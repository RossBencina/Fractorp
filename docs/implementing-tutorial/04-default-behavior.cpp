#include <cassert>
#include <cstdio>
#include "emdeer.h" // (C++ to markdown conversion utils)

/// 4. Default Initial Behavior
/// ===========================

/// In this episode we let actors define a `receive()` behavior that is
/// automatically installed as the initial behavior.
/// It's a convenience feature intended to save keystrokes.
/// We're aiming for actor definitions that look like the following:
///
/// ```c++
/// struct BrieferActor : public ActorT<BrieferActor> {
///     void receive() // called when we send() or inject() to `this`
///     {
///         printf("default behavior :)\n");
///     }
/// };
/// ```
///
/// For comparison, with the code from [part 3](03-cpp-actors.cpp.md), we'd have to write:
///
/// ```c++
/// // Part 3 version:
/// struct VerbosePart3Actor : public ActorT<VerbosePart3Actor> {
///     typedef VerbosePart3Actor this_type;
///     Part3Actor()
///         : ActorT<this_type>(behavior_thunk<&this_type::receive>())
///     {}
///
///     void receive()
///     {
///         printf("default behavior :)\n");
///     }
/// };
/// ```

/// In both cases note that `receive()` is *not* a virtual method. Remember that
/// our actors only use a single boxed function pointer for polymorphic dispatch.

/// First lets review what we've already achieved. (Otherwise this file won't compile!)

/// ## Recap: Actor Base Code

/// Here's the `Actor` base struct that we defined in [part 3](03-cpp-actors.cpp.md).
/// There's nothing new here.

namespace cpp_actors_1 {

    struct Actor {
        typedef void(*BehaviorProc)(Actor& a); // Type for actor behavior procedures.

        Actor() : bp_(do_nothing) {} // Default-constructed actors do nothing.
        Actor(BehaviorProc bp) : bp_(bp) {} // Construct with initial behavior.
    private:
        BehaviorProc bp_; // Current behavior. Determines how the actor responds to messages.
        friend void inject(Actor& a);

    protected:
        // Specify behavior for `this`'s next activation.
        void become(BehaviorProc bp) { bp_ = bp; }

        // Send an (empty) message to actor `a`.
        void send(Actor& a) { a.bp_(a); }

        static void do_nothing(Actor&) {} // A behavior procedure that does nothing.
    };

    // Send an (empty) message to actor 'a'. (For use from outside the actor network.)
    void inject(Actor& a) { a.bp_(a); }

/// (Test hidden.)

MD_BEGIN_HIDE

/// Test code:

    struct AlternatingSender : public Actor {
        Actor &target0_, &target1_;

        AlternatingSender(Actor& a, Actor& b)
            : Actor(sender_0_thunk_) // see below
            , target0_(a)
            , target1_(b) {}

    private:
        // non-copyable
        AlternatingSender(const AlternatingSender&);
        AlternatingSender& operator=(const AlternatingSender&);

    protected:

        void sender_0()
        {
            send(target0_);
            become(sender_1_thunk_);
        }

        static void sender_0_thunk_(Actor& a)
        {
            static_cast<AlternatingSender&>(a).sender_0(); // downcast to concrete type
        }

        void sender_1()
        {
            send(target1_);
            become(sender_0_thunk_);
        }

        static void sender_1_thunk_(Actor& a)
        {
            static_cast<AlternatingSender&>(a).sender_1();
        }
    };

    void print_yes(Actor&) { std::printf("yes\n"); }
    void print_no(Actor&) { std::printf("no\n"); }

    void test1()
    {
        MD_BEGIN_OUTPUT("Test 1");

        // Test default ctor
        Actor inoperative;
        inject(inoperative); // does nothing

        // Test Actor::Actor(bp), send(), become(), inject()
        Actor affirmative(print_yes);
        Actor negative(print_no);
        AlternatingSender alternately(affirmative, negative);
        for (int i = 0; i < 5; ++i)
            inject(alternately); // prints yes, no, yes, no...
    }

/// [Test 1]

MD_END_HIDE

} // end namespace cpp_actors_1


/// ## Recap: ActorT<> behavior<>() Thunks

/// [Part 3](03-cpp-actors.cpp.md) defined a template base class
/// `ActorT<>`. `ActorT<>` provides a version of `become<>()` which
/// thunks member-function pointers to free-function pointers compatible with
/// the `BehaviorProc` function pointer type.
/// We'll extend `ActorT<>` with our default initial behavior mechanism.
/// For reference, here's the old `ActorT<>`:

namespace cpp_actors_3 {
    using namespace cpp_actors_1;

    template< typename DerivedActorType >
    struct ActorT : public Actor {
    private:
        typedef DerivedActorType derived_actor_type;

    protected:
        ActorT() {}
        ActorT(BehaviorProc bp) : Actor(bp) {}

        template< void (derived_actor_type::*BehaviorMemberFn)() >
        static void behavior_thunk(Actor& a)
        {
            (static_cast<derived_actor_type&>(a).*BehaviorMemberFn)();
        }

        template<void (derived_actor_type::*BehaviorMemberFn)() >
        void become()
        {
            Actor::become(behavior_thunk<BehaviorMemberFn>);
        }
    };
} // end namespace cpp_actors_3


/// ## Implementation

/// Here's the spec for our new feature:
/// > **Default Initial Behavior:** When an `ActorT<>` subclass is instantiated
/// > using the `ActorT::ActorT()` default constructor: If the derived class implements
/// > a **`receive()`** method, that method will be installed as the initial behavior;
/// > else if the subclass does not implement `receive()`, `Actor::do_nothing`
/// > will be installed as the initial behavior.

namespace default_behavior {

/// `ActorT<>` begins as it did in part 3:

    using namespace cpp_actors_1;

    template< typename DerivedActorType >
    struct ActorT : public Actor {
    private:
        typedef ActorT<DerivedActorType> actor_t_type;
        typedef DerivedActorType derived_actor_type;

/// Now for the good stuff: the `get_default_behavior()` function (defined
/// below in two template specializations) implements the default behavior mechanism.
/// If the derived class defines a `receive()` method, `get_default_behavior()`
/// returns a `behavior_thunk<>` to that method; if not, `get_default_behavior()`
/// returns `Actor::do_nothing`.

        // match derived class `receive()`:
        template< void (derived_actor_type::*BehaviorMemberFn)() >
        static BehaviorProc get_default_behavior_proc()
        {
            return behavior_thunk<BehaviorMemberFn>;
        }

        void receive() { assert(false); /* dummy implementation. should never be called. */ }

        // match base-class dummy `receive()`:
        template< void (actor_t_type::*BehaviorMemberFn)() >
        static BehaviorProc get_default_behavior_proc()
        {
            return Actor::do_nothing;
        }

/// The mechanism detects whether the derived class implements `receive()`
/// by matching the type of the `receive()` function pointer against the
/// specializations of `get_default_behavior()`.
/// If the derived class defines `receive()`, the type of
/// `&derived_actor_type::receive` is `(derived_actor_type::*)()`.
/// On the other hand, if the derived class does not implement `receive()`
/// the type will be `(actor_t_type::*)()`.

/// (If you're curious why we're using type matching on template parameters
/// rather than on normal function overloads, it's because we need
/// `&derived_actor_type::receive` as a type parameter to pass to `behavior_thunk<>`.)

/// `ActorT`'s default constructor invokes `get_default_behavior()`:
    protected:
        // NB: &derived_actor_type::receive refers to actor_t_type::receive if
        // the derived class did not implement receive().
        ActorT() : Actor(get_default_behavior_proc<&derived_actor_type::receive>()) {}

        ActorT(BehaviorProc bp) : Actor(bp) {} // construct with specified behavior.

/// The remainder of the class implements the `become<>()` thunk templates from part 3:

        template< void (derived_actor_type::*BehaviorMemberFn)() >
        static void behavior_thunk(Actor& a)
        {
            (static_cast<derived_actor_type&>(a).*BehaviorMemberFn)(); // downcast to concrete type
        }

        template<void (derived_actor_type::*BehaviorMemberFn)() >
        void become()
        {
            Actor::become(behavior_thunk<BehaviorMemberFn>);
        }
    };

/// ## Tests

/// First, test that we haven't broken anything:

    struct ActorTAlternatingSender : public ActorT<ActorTAlternatingSender> {
        typedef ActorTAlternatingSender this_type;

        Actor &target0_, &target1_;

        ActorTAlternatingSender(Actor& a, Actor& b)
            : ActorT(behavior_thunk<&this_type::sender_0>) // explicit initial behavior
            , target0_(a)
            , target1_(b) {}

    private:
        // non-copyable
        ActorTAlternatingSender(const ActorTAlternatingSender&);
        ActorTAlternatingSender& operator=(const ActorTAlternatingSender&);

    protected:

        void sender_0()
        {
            send(target0_);
            become<&this_type::sender_1>();
        }

        void sender_1()
        {
            send(target1_);
            become<&this_type::sender_0>();
        }
    };

    void test2()
    {
        MD_BEGIN_OUTPUT("Test 2");

        // Test ActorT::ActorT(bp), send(), become(), inject()
        Actor affirmative(print_yes);
        Actor negative(print_no);
        ActorTAlternatingSender alternately(affirmative, negative);
        for (int i = 0; i < 5; ++i)
            inject(alternately); // prints yes, no, yes, no...
    }

/// [Test 2]

/// Okay.

/// Now test the default initial behavior mechanism:

    struct DefaultDoNothing : public ActorT<DefaultDoNothing> {
    };

    struct DefaultReceive1 : public ActorT<DefaultReceive1> {
        void receive()
        {
            printf("default behavior :)\n");
        }
    };

    struct DefaultReceive2 : public ActorT <DefaultReceive2> {
        typedef DefaultReceive2 this_type;

        void receive()
        {
            printf("default behavior :)\n");
            become<&this_type::subsequent>();
        }

        void subsequent()
        {
            printf("subsequently...\n");
        }
    };

    void test3()
    {
        MD_BEGIN_OUTPUT("Test 3");

        DefaultDoNothing defaultDoNothing;
        inject(defaultDoNothing); // should do nothing

        DefaultReceive1 defaultReceive1;
        inject(defaultReceive1); // should print "default behavior :)"

        DefaultReceive2 defaultReceive2;
        inject(defaultReceive2); // should print "default behavior :)"
        inject(defaultReceive2); // should print "subsequently...)"
        inject(defaultReceive2); // should print "subsequently..."
    }

/// [Test 3]

/// It works.

/// To finish for today, here's a re-write of the `SelfDestructor` from
/// [part 2](02-raw-actors.cpp.md):

    struct SelfDestructor : public ActorT<SelfDestructor> {
        ~SelfDestructor()
        {
            std::printf("deleting.\n");
        }

        void receive()
        {
            delete this;
        }
    };

    struct Creator : public ActorT<Creator> {
        void receive()
        {
            std::printf("creating.\n");
            Actor *transient = new SelfDestructor;
            send(*transient);
        }
    };

    void test4()
    {
        MD_BEGIN_OUTPUT("Test 4");

        Creator creator;

        for (int i = 0; i < 6; ++i)
            inject(creator);
    }

/// [Test 4]

} // end namespace default_behavior

/// Next time we'll get on to _messages_;

int main(int, char *[])
{
    cpp_actors_1::test1();
    default_behavior::test2();
    default_behavior::test3();
    default_behavior::test4();
}

/// ***
/// Next: _Coming soon..._ <br/>
/// Previous: [C++ Actors](03-cpp-actors.cpp.md) <br/>
/// Up: [README](README.md)