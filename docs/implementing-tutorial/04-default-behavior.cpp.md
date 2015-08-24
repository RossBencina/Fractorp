```c++
#include <cassert>
#include <cstdio>
#include "emdeer.h" // (C++ to markdown conversion utils)
```

4. Default Initial Behavior
===========================

In this episode we let actors define a `receive()` method that is
automatically installed as the initial behavior.
It's not a big deal, just a convenience feature to save keystrokes and
hopefully make our code clearer.
We're aiming to write actor definitions that look like this:

```c++
struct BrieferActor : public ActorT<BrieferActor> {
    void receive() // called when we send() or inject() to `this`
    {
        printf("default behavior :)\n");
    }
};
```

For comparison, here's the same actor declared with the base code from
[part 3](03-cpp-actors.cpp.md):

```c++
// Part 3 version:
struct VerbosePart3Actor : public ActorT<VerbosePart3Actor> {
    typedef VerbosePart3Actor this_type;
    Part3Actor()
        : ActorT<this_type>(behavior_thunk<&this_type::receive>())
    {}

    void receive()
    {
        printf("default behavior :)\n");
    }
};
```

Notice that in both cases the `receive()` method is not virtual. Recall that
our actors use a single [boxed function pointer](01-boxed-function-pointers.cpp.md)
for polymorphic dispatch.

First let's review what we've already achieved. (Otherwise this file won't compile!)

## Recap: Actor Base Code

Here's the `Actor` base struct. There's nothing new here. It's just as
we defined it in [part 3](03-cpp-actors.cpp.md).

```c++
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
```

(Test hidden.)

```c++

} // end namespace cpp_actors_1
```


## Recap: ActorT<> behavior<>() Thunks

[Part 3's](03-cpp-actors.cpp.md) template base class
`ActorT<>` provides a version of `become<>()` that converts
member-function pointers to `BehaviorProc`-compatible free function pointers.
We'll extend `ActorT<>` with the default initial behavior mechanism.
For reference, here's the earlier version:

```c++
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
```


## Implementation

Our new feature has a formal specification:
> **Default Initial Behavior:** When an `ActorT<>` subclass is instantiated
> using the `ActorT::ActorT()` default constructor: If the derived class declares
> a `receive()` member function, that function will be installed as the initial behavior;
> else when the derived class does not define `receive()`, `Actor::do_nothing`
> will be installed as the initial behavior.

```c++
namespace default_behavior {
```

`ActorT<>` begins as it did in part 3:

```c++
    using namespace cpp_actors_1;

    template< typename DerivedActorType >
    struct ActorT : public Actor {
    private:
        typedef ActorT<DerivedActorType> actor_t_type;
        typedef DerivedActorType derived_actor_type;
```

Now for the good stuff: the `get_default_behavior()` function (defined
below in two template specializations) implements the default behavior mechanism.
If the derived class declares a `receive()` method, `get_default_behavior()`
returns a `behavior_thunk<>` to that method; if not, `get_default_behavior()`
returns `Actor::do_nothing`.

```c++
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
```

The mechanism detects whether the derived class implements `receive()`
by matching the type of the `receive()` function pointer against one of the
`get_default_behavior()` specializations.
If the derived class declares `receive()`, the type of
`&derived_actor_type::receive` is `(derived_actor_type::*)()`.
If the derived class does not declare `receive()`
the type will be `(actor_t_type::*)()`.

(If you're wondering why we're using type matching on template parameters
rather than on function parameters, it's because we need
to pass `&derived_actor_type::receive` as a type parameter to `behavior_thunk<>`.)

`ActorT`'s default constructor invokes `get_default_behavior()` to install
the initial behavior:
```c++
    protected:
        // NB: &derived_actor_type::receive refers to actor_t_type::receive if
        // the derived class did not implement receive().
        ActorT() : Actor(get_default_behavior_proc<&derived_actor_type::receive>()) {}

        ActorT(BehaviorProc bp) : Actor(bp) {} // construct with specified behavior.
```

The remainder of the `ActorT<>` class implements the `become<>()` thunk templates from part 3:

```c++
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
```

## Tests

First, test that we haven't broken anything by defining an actor that
explicitly specifies its initial behavior:

```c++
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
```

>[Test 2]:
> ```
> yes
> no
> yes
> no
> yes
> ```

Okay.

Now test the default initial behavior mechanism:

```c++
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
```

>[Test 3]:
> ```
> default behavior :)
> default behavior :)
> subsequently...
> subsequently...
> ```

It works.

I'll leave you with a re-write of the `SelfDestructor` idea from
[part 2](02-raw-actors.cpp.md):

```c++
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

        for (int i = 0; i < 3; ++i)
            inject(creator);
    }
```

>[Test 4]:
> ```
> creating.
> deleting.
> creating.
> deleting.
> creating.
> deleting.
> ```

```c++
} // end namespace default_behavior
```

Next time I'll probably tackle _messages_. Or maybe recursive sends.

```c++
int main(int, char *[])
{
    cpp_actors_1::test1();
    default_behavior::test2();
    default_behavior::test3();
    default_behavior::test4();
}
```

***
Next: _Coming soon..._ <br/>
Previous: [C++ Actors](03-cpp-actors.cpp.md) <br/>
Up: [README](README.md)

Generated from [`04-default-behavior.cpp`](04-default-behavior.cpp) by [`emdeer.py`](emdeer.py) at UTC 2015-08-24 13:22:39.432000
