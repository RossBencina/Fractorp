/*
    Fractorp by Ross Bencina

    "There is no way to peace; peace is the way." -- A. J. Muste
*/

#ifndef INCLUDED_FRACTORP_ACTOR_H
#define INCLUDED_FRACTORP_ACTOR_H

#include <cassert>
#include <cstdint>
#include <list>

namespace Fractorp {

// An Actor system is parameterised by two types:
//
// S or shared_context_type is the type of an object accessible to all actors via self.shared_context()
//
// M or message_type is the type of messages communicated between actors using send() or inject()
// /message/ is treated as a value type. we need to be able to store it in a run queue.
// (but its value could be "pointer to Request object" or whatever.
// REVIEW: Agha calls this a communication rather than a message. that may be more appropriate.

template<typename S, typename M>
struct Actor;

template<typename A>
struct Endpoint;

template<typename S, typename M>
class World;


template<typename S, typename M>
struct Actor {
    typedef S shared_context_type;
    typedef M message_type;
    typedef Actor<shared_context_type, message_type> actor_type;
    typedef Endpoint<actor_type> endpoint_type;
    typedef World<shared_context_type, message_type> world_type;
    
    typedef void (*behavior_fn_ptr_type)(world_type&, actor_type&, int port, message_type message);

    behavior_fn_ptr_type behaviorFn_;

    static Actor& null() { static Actor nullActor; return nullActor; }

    Actor() : behaviorFn_(&actor_type::nullBehavior) {}
    explicit Actor(const Actor& src) : behaviorFn_(src.behaviorFn_) {}
    explicit Actor(behavior_fn_ptr_type behaviorFn) : behaviorFn_(behaviorFn) {}
    Actor& operator=(const Actor& rhs) { behaviorFn_ = rhs.behaviorFn_; return *this; }
    
    // It's fatal to send a message to an uninitialized Actor or actor ref.
    // Actor::null() and Endpoint::null() are like NULL pointers, not like /dev/null.
    static void nullBehavior(world_type&, actor_type&, int /*port*/, message_type /*message*/)
    {
        assert(false && "sending message to uninitialized actor or endpoint");
    }
};


template<typename actor_type>
struct Endpoint {
    enum { PORT_INDEX_MASK = 0x3 }; // Assume Actors are word-aligned. pack a port index into the low bits

    intptr_t packed_;

    static Endpoint null() { return Endpoint(actor_type::null()); }

    Endpoint() : packed_(Endpoint::null().packed_) {}
    explicit Endpoint(actor_type& a) : packed_(reinterpret_cast<std::intptr_t>(&a)) {}
    Endpoint(actor_type& a, int port) : packed_(reinterpret_cast<std::intptr_t>(&a) | port)
    {
        assert((port&PORT_INDEX_MASK) == port); // check for overflow of available port bits.
    }

    actor_type& actor() const { return *reinterpret_cast<actor_type*>(packed_&(~static_cast<std::intptr_t>(PORT_INDEX_MASK))); }
    int port() const { return static_cast<int>(packed_&PORT_INDEX_MASK); }
};


// Implementation of DeferredSendQueue is just a detail. Might be most efficient
// to use a stack-allocated fixed size ringbuffer: and pass a pointer to it into world.
// FIXME runQueue_ doesn't need to be an instance variable.
//  it could be pointer to queue allocated on the stack in inject().
//  and we don't need a runqueue at all if there is no cyclic sending.
// on the other hand, when we do need a runQueue_ we need to make it available
// to all actors. hence why we pass World around.


template<typename S, typename M>
class DeferredSendQueue {
    typedef S shared_context_type;
    typedef M message_type;
    typedef Actor<shared_context_type,message_type> actor_type;
    typedef Endpoint<actor_type> endpoint_type;
    typedef World<shared_context_type,message_type> world_type;

    struct DeferredSend {
        endpoint_type endpoint;
        message_type message;

        DeferredSend() {}
        DeferredSend(endpoint_type e, message_type m)
            : endpoint(e)
            , message(m) {}
    };

    std::list<DeferredSend> q_;

public:

    void push(actor_type& a, int port, message_type m)
    {
        q_.push_front((DeferredSend(endpoint_type(a, port), m)));
    }

    void send_all(world_type& world)
    {
        // NOTE: dispatching may cause additional entries to be queued
        // REVIEW: consider optionally performing send non-deterministically
        while (!q_.empty()) {
            DeferredSend &deferredSend = q_.back();
            actor_type& a = deferredSend.endpoint.actor();
            int port = deferredSend.endpoint.port();
            a.behaviorFn_(world, a, port, deferredSend.message);
            q_.pop_back();
        }
    }
};

template<typename S, typename M>
class World {
    typedef S shared_context_type;
    typedef M message_type;
    typedef Actor<shared_context_type, message_type> actor_type;
    typedef Endpoint<actor_type> endpoint_type;
    typedef World<shared_context_type, message_type> world_type;

    DeferredSendQueue<S, M> deferredSendQueue_;

    shared_context_type sharedContext_;

public:

    // inject() sends messages to actors. should only be called from outside actor behaviors.

    void inject(actor_type& a) { // sends value-initialized message on port 0
        inject(a, 0, message_type());
    }

    void inject(const endpoint_type& e) { // sends value-initialized message on port 0
        inject(e.actor(), e.port(), message_type());
    }

    void inject(actor_type& a, message_type m) { // sends message m on port 0
        inject(a, 0, m);
    }

    void inject(const endpoint_type &e, message_type m) {
        inject(e.actor(), e.port(), m);
    }

    void inject(actor_type& a, int port, message_type m) {
        // REVIEW: we should probably use an assert to guard against re-entering inject
        a.behaviorFn_(*this, a, port, m);
        deferredSendQueue_.send_all(*this);
    }


    // user-specified shared context available to all actors

    void set_shared_context(const shared_context_type& s) { sharedContext_ = s; }
    const shared_context_type& shared_context() const { return sharedContext_; }
    shared_context_type& shared_context() { return sharedContext_; }


    // defer_behavior is used by the implementation for deferring recursive sends.

    static void defer_behavior(world_type& world, actor_type& a, int port, message_type m)
    {
        world.deferredSendQueue_.push(a, port, m);
    }
};



// Self is a scoped guard object intended to span the activation of a single behavior invocation.
// The self object serves a number of purposes:
//
//  * Restricts access to only those operations of World that are
//    appropriate to be called from within a behavior (defer(), context())
//
//  * Prevents access to send() and become() from outside actor behaviors.
//
//  * Implements a recursion guard deferral mechanism: While the Scope instance is active,
//    the actor takes on the World::defer behavior, which queues messages for later execution.
//    This ensures that behavior functions are not re-entered, but that cycles may occur in the
//    graph of sending actors.
//
// As an implementation detail: the main reason that Self was defined was to implement
// the recursion guard. From this followed the need to implement become() and delete_later()
// within Self -- since they depend on installing a new behavior while retaining deferral of 
// new messages until the behavior returns.
template <typename concrete_actor_type>
struct Self {
    typedef typename concrete_actor_type::shared_context_type shared_context_type;
    typedef typename concrete_actor_type::message_type message_type;
    typedef typename concrete_actor_type::behavior_fn_ptr_type behavior_fn_ptr_type;
    typedef typename concrete_actor_type::actor_type actor_type;
    typedef typename concrete_actor_type::endpoint_type endpoint_type;
    typedef typename concrete_actor_type::world_type world_type;

    world_type& world_;
    actor_type& myself_;
    behavior_fn_ptr_type behaviorFnToRestore_;

    Self& operator=(const Self&);
    Self(const Self&);
    Self();

    //friend actor_base_type; // only allow creation of a Self from the base actor, not from the derived
    Self(world_type& world, actor_type& myself)
        : world_(world)
        , myself_(myself)
        , behaviorFnToRestore_(myself_.behaviorFn_)
    {
        // REVIEW: Possible optimisation:
        //
        // Message deferral re-entrance guard is only needed if cycles in the message
        // send graph can occur. In some use-cases this is predictable statically, for example: 
        //  * Actors that don't send messages can't be re-entered.
        //  * Actor graphs that are guaranteed cycle-free (e.g. pipelines) can't be re-entered
        // 
        // As an optimisation, we may wish to provide the following alternative implementations of
        // ActorT with different implementations of self:
        //
        // ActorT: the current implementation
        //
        // ActorT_nosend:
        //  Remove send() functions from ActorT_nosend::Self;
        //  Don't use defer behavior or save behaviorFnToRestore_;
        //  delete_later() and become() can store new behaviors directly into myself_->behaviorFn_
        //
        // ActorT_nocycles:
        //  Debug version: as for current impl but replace World::defer with a function that 
        //  asserts false if a recursive message invocation is made.
        //  Release version: similar to ActorT_nosend (except with send functions defined).

        myself_.behaviorFn_ = world_type::defer_behavior;
    }

    ~Self()
    {
        // stop deferring requests. restore old (or install new) behavior. 
        myself_.behaviorFn_ = behaviorFnToRestore_;
    }

public:

    // delete_later() should only be called if the Actor is certain that 
    // no further messages will be delivered to it.
    void delete_later()
    {
        behaviorFnToRestore_ = concrete_actor_type::delete_behavior; // become the delete behavior
        world_type::defer_behavior(world_, myself_, 0, message_type()); // enqueue deferred message to self, which will cause the delete behavior to be invoked
    }
    
    template < void (concrete_actor_type::*f)(Self&, int, message_type) >
    void become()
    {
        behaviorFnToRestore_ = concrete_actor_type::behavior<f>;
    }
    

    // send() may only be used to send to other actors from within a behavior. 
    // Use World::inject() to send from non-behavior code.

    void send(actor_type& a) { // sends value-initialized message on port 0
        send(a, 0, message_type());
    }

    void send(const endpoint_type& e) { // sends value-initialized message on port 0
        send(e.actor(), e.port(), message_type());
    }

    void send(actor_type& a, message_type m) { // sends message m on port 0
        send(a, 0, m);
    }

    void send(const endpoint_type &e, message_type m) {
        send(e.actor(), e.port(), m);
    }

    void send(actor_type& a, int port, message_type m) {
        a.behaviorFn_(world_, a, port, m);
    }

    shared_context_type& shared_context() { return world_.shared_context(); }
};


// ActorT is the most general base class for concrete Actor implementations.
template <typename AS, typename DerivedT>
struct ActorT : public Actor<typename AS::shared_context_type, typename AS::message_type> {

    typedef typename AS::shared_context_type shared_context_type;
    typedef typename AS::message_type message_type;

    typedef typename AS::actor_type root_actor_type;
    typedef ActorT<AS, DerivedT> actor_base_type;
    typedef DerivedT concrete_actor_type;    
    
    typedef Self<concrete_actor_type> self_type;

    static concrete_actor_type* downcast_to_concrete_actor_type(Actor *a)
    {
        return static_cast<concrete_actor_type*>(a);

        // Rationale for using static_cast:
        //
        // """
        // static_cast <new_type> (expression)
        // 2) If new_type is a pointer or reference to some class D and the type of expression 
        // is a pointer or reference to its non - virtual base B, static_cast performs a downcast.
        // Such static_cast makes no runtime checks to ensure that the object's runtime type is 
        // actually D, and may only be used safely if this precondition is guaranteed by other means, 
        // such as when implementing static polymorphism. Safe downcast may be done with dynamic_cast.
        // """
        //
        // We know that a really is an instance of concrete_actor_type, because we only install an concrete_actor_type
        // thunk in an concrete_actor_type actor. Therefore we can safely use static_cast provided that Actor is
        // not a virtual base class of concrete_actor_type.
        //
        // The alternative is dynamic_cast<> which is not even usable if concrete_actor_type is not
        // a polymorphic object.
    }

    static void delete_behavior(world_type&, root_actor_type& a, int, message_type)
    {
        delete downcast_to_concrete_actor_type(&a);
    }

    // behavior<f>() is a thunk from member function to actor behavior function.
    // A distinct thunk is instantiated for each used behavior member function.
    // The intention is that this is a lightweight wrapper. Hopefully the compiler will inline the behavior method.
    // /Self/ implements a recursion guard (see above).
    template < void (concrete_actor_type::*f)(self_type&, int, message_type) >
    static void behavior(world_type& world, root_actor_type& a, int port, message_type m)
    {
        self_type self(world, a);
        // invoke behavior method f (template parameter) on instance of concrete_actor_type
        (downcast_to_concrete_actor_type(&a)->*f)(self, port, m);
    }

protected:
    
    void initial(self_type& self, int port, message_type message)
    {
        assert(false & "no initial behavior defined");
    }

    // Derived classes have two options for initializing the base class:
    // (1) Use the default base-class ctor, and define an initial() behavior that hides the base class error implementation
    ActorT() : root_actor_type( &behavior<&concrete_actor_type::initial> ) {}
    // (2) Specify an initial behavior
    ActorT(behavior_fn_ptr_type initialBehaviorFn) : root_actor_type(initialBehaviorFn) {}
};


template< typename S = void*, typename M = void*>
struct ActorSpace {
    typedef S shared_context_type;
    typedef M message_type;

    typedef Actor<S,M> actor_type;
    typedef Endpoint<actor_type> endpoint_type;
    typedef World<S,M> world_type;
};

} // end namespace Fractorp

#endif /* INCLUDED_FRACTORP_ACTOR_H */
