/*
    Fractorp by Ross Bencina

    "You cannot find peace by avoiding life."  -- Virgina Wolf
*/

#include "Actor.h"

#include <cstdio>

using namespace Fractorp;

typedef void* shared_context_type;
typedef void* message_type;
typedef ActorSpace<shared_context_type, message_type> AS1;


// The simplest type of actor has an unchanging behavior.
struct HelloActor : public Fractorp::ActorT<AS1, HelloActor> {

    // By default, the initial() method defines the actor's initial behavior. It needs to be public.
    void initial(self_type& self, int port, message_type message)
    {
        std::printf("hello! shared-context: %p port: %d message: %p\n", self.shared_context(), port, message);
    }
};


struct SenderActor : public Fractorp::ActorT <AS1, SenderActor> {
    actor_type& a;

    explicit SenderActor(Actor& dest)
        : a(dest) {}

    void initial(self_type& self, int /*port*/, message_type /*message*/)
    {
        // there are 5 overloads of the send function
        // the most general is:

        self.send(a, 2, (void*)300); // send message 300 to actor a on port 2

        self.send(a); // empty message on port 0

        self.send(a, (void*)200); // message 200 on port 0

        self.send(endpoint_type(a,1)); // empty message to (actor,port) specified by endpoint

        self.send(endpoint_type(a,2), (void*)100); // send message 100 to (actor,port) specified by endpoint
    }
};


// An actor can send messages to other actors.
struct SendN : public Fractorp::ActorT<AS1, SendN> {
    typedef SendN this_type;

    explicit SendN(endpoint_type other, int N)
        : actor_base_type(behavior<&this_type::sendN>) // you can also specify a different method for the initial behacior
        , other_(other)
        , N_(N) {}

private:
    endpoint_type other_;
    int N_;

    void sendN(self_type& self, int /*port*/, message_type /*message*/)
    {
        for (int i=0; i < N_; ++i)
            self.send(other_, (message_type)i);
    }
};


// An actor can send messages to itself.
// Although sends to other actors may invoke behaviors directly, recursive sends never re-enter behaviors.
// (Notice that "> in" and "< out" alternate in the output.)
struct SendNRecursive : public Fractorp::ActorT<AS1, SendNRecursive> {
    typedef SendNRecursive this_type;

    explicit SendNRecursive(endpoint_type other, int N)
        : other_(other)
        , i_(N) {}

    endpoint_type other_;
    int i_;

    void initial(self_type& self, int /*port*/, message_type /*message*/)
    {
        std::printf("> in\n");
        self.send( other_, (message_type)i_);
        if (--i_ > 0)
            self.send(*this, 0);
        std::printf("< out\n");
    }
};


// Actors can change their behavior. This is useful for building state machines and coroutines.
struct AlternatingActor : public Fractorp::ActorT<AS1, AlternatingActor> {
    typedef AlternatingActor this_type;

    AlternatingActor()  : ActorT(behavior<&this_type::yes>) {}
    
private:
    void yes(self_type& self, int /*port*/, void* /*message*/)
    {
        std::printf("yes\n");
        self.become<&this_type::no>();
    }

    void no(self_type& self, int /*port*/, void* /*message*/)
    {
        std::printf("no\n");
        self.become<&this_type::yes>();
    }
};


// Log messages and forward them on 
struct Log : public Fractorp::ActorT<AS1, Log> {
    Actor& a_;
    const char *s_;

    Log(Actor& a, const char *s)
        : a_(a)
        , s_(s) {}

    void initial(self_type& self, int port, message_type message)
    {
        std::printf("> %s shared-context: %p actor: %p port: %d message: %p\n", s_, self.shared_context(), &a_, port, message);
        self.send(a_, port, message);
        std::printf("< %s\n", s_);
    }
};


// Actors can delete themselves. Sending a message to a deleted actor will crash, of course.
struct SelfDeleting : public Fractorp::ActorT<AS1, SelfDeleting> {
    ~SelfDeleting()
    {
        std::printf("SelfDeleting::~SelfDeleting() called\n");
    }

    void initial(self_type& self, int /*port*/, void* /*message*/)
    {
        return self.delete_later();
    }
};

//////////////////////////////////////////////////////////////////////////

void test1()
{
    typedef AS1::endpoint_type endpoint_type;
    AS1::world_type world;

    // We can store a shared context in the world. This context is accessible to all actors.
    world.set_shared_context((void*)0x4AC70AAA);

    // To send messages from inside actor behaviors we use send() (see above).
    // From outside we use inject():
    HelloActor a;
    world.inject(a);

    // the same 5 function signatures are available:

    world.inject(a, 2, (void*)300); // send message 300 to actor a on port 2

    world.inject(a); // empty message on port 0

    world.inject(a, (void*)200); // message 200 on port 0

    world.inject(AS1::endpoint_type(a, 1)); // empty message to (actor,port) specified by endpoint

    world.inject(AS1::endpoint_type(a, 2), (void*)100); // send message 100 to (actor,port) specified by endpoint

    
    // See the SenderActor above for examples of different send() options
    SenderActor s(a);
    world.inject(s);

    AlternatingActor b;
    for (int i = 0; i < 10; ++i)
        world.inject(b);

    endpoint_type e = endpoint_type(a);
    SendN sendTen(e, 10);
    world.inject(sendTen);

    Log log(b, "*send to alternating*");
    SendN sendTenB(endpoint_type(log), 10);
    world.inject(sendTenB);

    SendNRecursive sendTenRecursive(e, 10);
    world.inject(sendTenRecursive);

    // Actors allocated on the heap may delete themeslves
    SelfDeleting *selfDeleting = new SelfDeleting;
    world.inject(*selfDeleting);

    // actors are very small
    printf("sizeof(HelloActor) = %d\n", sizeof(HelloActor));
    printf("sizeof(SendNRecursive) = %d\n", sizeof(SendNRecursive));
    
    // communications can address a small number of actor ports
    // this allows side-channel control to be sent to actors
    for (int i=0; i < 4; ++i)
        world.inject(a, i, (void*)(i*100));
}


//////////////////////////////////////////////////////////////////////////

// Recursive Factorial example from Gul Agha's "Actors" book.

typedef void* shared_context_type; // not used
struct FactMessageType;
typedef ActorSpace<shared_context_type, FactMessageType> AS2;

// In the Actors book, messages are arbitrary tuples. Different
// actors can then receive different messages (e.g. like different
// functions have different signatures).
// We only support a single message type for all actors, so we 
// shoehorn the data into the following struct:
struct FactMessageType{
    int i;
    AS2::actor_type *u;
};

struct RecCustomer : public Fractorp::ActorT < AS2, RecCustomer > {
    int n;
    AS2::actor_type &u;

    RecCustomer(int n_, AS2::actor_type &u_) : n(n_), u(u_) {}

    void initial(self_type& self, int, message_type communication)
    {
        int k = communication.i;
        self.send(u, { n*k, 0 });
        self.delete_later();
    }
};

struct RecFactorial : public Fractorp::ActorT < AS2, RecFactorial > {
   
    void initial(self_type& self, int, message_type communication)
    {
        int n = communication.i;
        AS2::actor_type *u = communication.u;

        if (n == 0) {
            // finally, kick off the calculation
            self.send(*u, {1, (AS2::actor_type*)0} );
        } else {
            // recursively assemble a pipeline. 
            // The first RecCustomer created is the one that sends to PrintResult
            AS2::actor_type *c = new RecCustomer(n, *u);
            self.send(*this, {n-1, c} );
        }
    }
};

struct PrintResult : public Fractorp::ActorT < AS2, PrintResult > {
    void initial(self_type& self, int, message_type communication)
    {
        int y = communication.i;
        printf("%d\n", y);
    }
};


void test2()
{
    std::printf("running recursive factorial algorithm:");

    AS2::world_type world;
    RecFactorial recFactorial;
    PrintResult printResult;
    for (int i=0; i < 15; ++i)
        world.inject(recFactorial, {i, &printResult});
}

//////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
    (void)argc, argv;

    test1();
    test2();

    return 0;
}
