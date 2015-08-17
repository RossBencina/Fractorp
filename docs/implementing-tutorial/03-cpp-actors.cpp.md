```c++
#include <cstdio>
#include "emdeer.h" // (C++ to markdown conversion utils)
```

3. C++ Actors
=============

[Part 2](02-raw-actors.cpp.md) introduced a rudimentary implementation
of actors in a C-like coding style.
In this installment we're going to make it easier to define and use actors
with the help of a few C++ features. Here's what to expect:

1. Review the code from part 2.
2. Consider requirements for a more usable C++ implementation.
3. Refactor: make `send()` and `become()` member functions.
   Enforce intended usage by making some members protected or private.
4. Introduce a thunking mechanism so that member functions can be used as actor behaviors.
5. Factor the thunking mechanism into a reusable base class.

## 1. Review of C-like Code From Part 2

Here's the implementation from part 2, annotated with explanatory comments:

```c++
namespace part_02_raw_actors {

struct Actor; // forward declaration

typedef void (*BehaviorProc)(Actor *a); // Type for actor behavior procedures.

void do_nothing(Actor *) {} // A behavior procedure that does nothing.

struct Actor {
    BehaviorProc bp_; // Current behavior. Determines how the actor responds to next message.
    Actor() : bp_(do_nothing) {} // Default-constructed actors do nothing.
    Actor(BehaviorProc bp) : bp_(bp) {} // Construct with an initial behavior.
};

// Specify the behavior that `a` will use to handle the next message.
void become(Actor *a, BehaviorProc bp) { a->bp_ = bp; }

// Send an (empty) message to actor `a`.
void send(Actor& a) { a.bp_(&a); }

// Send an (empty) message to actor `a`. (For use from outside the actor network.)
void inject(Actor& a) { a.bp_(&a); }
```

The following test code excercises each `Actor` constructor, `become()`,
`send()`, and `inject()`:

```c++
struct AlternatingSender : public Actor {
    Actor *target0_, *target1_;

    AlternatingSender(Actor* a, Actor* b)
        : Actor(sender_0)
        , target0_(a)
        , target1_(b) {}

    static void sender_0(Actor *a)
    {
        send(*static_cast<AlternatingSender*>(a)->target0_); // downcast to concrete type
        become(a, sender_1);
    }

    static void sender_1(Actor *a)
    {
        send(*static_cast<AlternatingSender*>(a)->target1_);
        become(a, sender_0);
    }
};

void print_yes(Actor *) { std::printf("yes\n"); }
void print_no(Actor *) { std::printf("no\n"); }

void test()
{
    MD_BEGIN_OUTPUT("part_02_raw_actors Test");

    // Test default ctor
    Actor inoperative;
    inject(inoperative); // does nothing

    // Test Actor::Actor(bp), send(), become(), inject()
    Actor affirmative(print_yes);
    Actor negative(print_no);
    AlternatingSender alternately(&affirmative, &negative);
    for (int i = 0; i < 5; ++i)
        inject(alternately); // prints yes, no, yes, no...
}
```

>[part_02_raw_actors Test]:
> ```
> yes
> no
> yes
> no
> yes
> ```

```c++
} // end namespace part_02_raw_actors
```

In part 2 we also looked at a `self_destruct` mechanism supporting
polymorphic destruction. I'll return to that at the end of
this installment.

## 2. Desiderata: A Clearer Interface

Our implementation should reflect the semantics of
[The Actor Model](http://en.wikipedia.org/wiki/Actor_model).
In particular, the scope of operations should reflect their intended use:

- `send()` is used to send messages from one actor to another.
  Therefore `send()` should only be callable from within actor behaviors,
   not from top level functions.

- `become()` is used by a behavior to specificy the actor's next behavior.
  Therefore `become()` should only be callable from within actor behaviors.

- The value of `bp_`, the behavior procedure pointer, is an internal
  detail of each actor. We would like to protect against `bp_` being modified
  indiscriminately. Actors should only be able to modify their own `bp_` --
  either at construction time, or from within a behavior by calling `become()`.

- `inject()` is intended to be called by code running outside the actor
  network. It should be difficult (ideally impossible) for actor behaviors
  to call `inject()`.

In addition, the following features are desirable:

- It should be possible to define behaviors using C++ member functions.

- It would be nice to install a _default_ behavior method (named
  `receive()` for example). This would simplify the code for single-behavior actors.

For now, we won't cover all of these requirements, but we'll get close.

## 3. Refactor for Encapsulation

Let's restrict the scope and visibility of `bp_`, `send()` and `become()`:

```c++
namespace cpp_actors_1 { // first cut at C++ actors
```

Define `Actor` as a `struct` so that we can put public declarations first.

```c++
    struct Actor {
```
As before, leave the `BehaviorProc` type and the constructors public.
This allows us to define actor behaviors using static members or free-functions.

```c++
        typedef void(*BehaviorProc)(Actor& a); // Type for actor behavior procedures.

        Actor() : bp_(do_nothing) {} // Default-constructed actors do nothing.
        Actor(BehaviorProc bp) : bp_(bp) {} // Construct with initial behavior.
```

Make `bp_` private. Once constructed, the only way to modify an actor's
behavior will be using `become()`.
```c++
    private:
        BehaviorProc bp_; // Current behavior. Determines how the actor responds to messages.
```

`inject()` remains a free function (see below). Make it a friend so that it can access `bp_`.
```c++
        friend void inject(Actor& a);
```

Make `become()` and `send()` protected to restrict them to being called
from within Actors. (In a later installment we'll restrict them to only being
callable from within behavior procedures.)
```c++
    protected:
        // Specify behavior for `this`'s next activation.
        void become(BehaviorProc bp) { bp_ = bp; }

        // Send an (empty) message to actor `a`.
        void send(Actor& a) { a.bp_(a); }
```


Arguably `do_nothing()` is an implementation detail and should be private.
However, a derived class might like to `become(do_nothing)`, so make it protected.
```c++
        static void do_nothing(Actor&) {} // A behavior procedure that does nothing.
    };
```

`inject()` remains a free function.

```c++
// Send an (empty) message to actor 'a'. (For use from outside the actor network.)
void inject(Actor& a) { a.bp_(a); }
```


Here's the test code; rewritten to use the above interface.

```c++
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
```

Make our behaviors protected. They'll only be referenced by other behaviors.
```c++
protected:
```

Now that `send()` and `become()` are protected we can only call
them from member functions.

```c++
        void sender_0()
        {
            send(target0_);
            become(sender_1_thunk_);
        }
```

`BehaviorProc` is still a plain old function pointer (and always will be),
hence we can't pass a member function pointer to `become()`.
We need some kind of adapter. The simplest solution is to define separate
static  [thunk](http://en.wikipedia.org/wiki/Thunk) functions to convert
calls to `bp_` into calls to our member functions.

```c++
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
```

(I haven't checked, but I'm expecting that the compiler will elide the thunk
and its callee. There should be no overhead to this indirection.)

```c++
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
```

>[Test 1]:
> ```
> yes
> no
> yes
> no
> yes
> ```

```c++
} // end namespace cpp_actors_1
```

## 4. Member Function Thunks

The code in the previous section used static thunk functions
to adapt between the free-function interface of `BehaviorProc`
and member functions of an actor class (`sender_0_thunk_` and `sender_1_thunk_` above).
That's tedious.
We can avoid the tedium by defining our thunk as a template function
parameterised by a member function pointer. Here's a demonstration:

```c++
namespace cpp_actors_2 {

    using namespace cpp_actors_1;

    struct AlternatingSenderTemplateThunks1 : public Actor {
```
Save keystrokes by providing `this_type` representing the type of the current class:
```c++
        typedef AlternatingSenderTemplateThunks1 this_type;

        Actor &target0_, &target1_;
```

Here's the thunk template. When instantiated it defines a static `BehaviorProc`
that casts the actor reference and calls the supplied member function template argument.
```c++
        template< typename T, void (T::*BehaviorMemberFn)() >
        static void behavior_thunk(Actor& a)
        {
            (static_cast<T&>(a).*BehaviorMemberFn)(); // downcast to concrete type
        }
```

Now use it to automatically generate thunks for the
constructor and the calls to `become()`:
```c++
        AlternatingSenderTemplateThunks1(Actor& a, Actor& b)
            : Actor(behavior_thunk<this_type, &this_type::sender_0>)
            , target0_(a)
            , target1_(b) {}


    private:
        // non-copyable
        AlternatingSenderTemplateThunks1(const AlternatingSenderTemplateThunks1&);
        AlternatingSenderTemplateThunks1& operator=(const AlternatingSenderTemplateThunks1&);

    protected:
        void sender_0()
        {
            send(target0_);
            become(behavior_thunk<this_type, &this_type::sender_1>);
        }

        void sender_1()
        {
            send(target1_);
            become(behavior_thunk<this_type, &this_type::sender_0>);
        }
    };

    void test2a()
    {
        MD_BEGIN_OUTPUT("Test 2a");

        // Test Actor::Actor(bp), send(), become(), inject()
        Actor affirmative(print_yes);
        Actor negative(print_no);

        // behavior_thunk<> version:
        AlternatingSenderTemplateThunks1 alternately1(affirmative, negative);
        for (int i = 0; i < 5; ++i)
            inject(alternately1); // prints yes, no, yes, no...
    }
```

>[Test 2a]:
> ```
> yes
> no
> yes
> no
> yes
> ```

We can take this idea a step further by eliminating the explicit thunk
instantiation using an overloaded version of `become()`.

```c++
    struct AlternatingSenderTemplateThunks2 : public Actor {
        typedef AlternatingSenderTemplateThunks2 this_type;

        Actor &target0_, &target1_;

        AlternatingSenderTemplateThunks2(Actor& a, Actor& b)
            : Actor(behavior_thunk<this_type, &this_type::sender_0>)
            , target0_(a)
            , target1_(b) {}

    private:
        // non-copyable
        AlternatingSenderTemplateThunks2(const AlternatingSenderTemplateThunks2&);
        AlternatingSenderTemplateThunks2& operator=(const AlternatingSenderTemplateThunks2&);

    protected:
        template< typename T, void (T::*BehaviorMemberFn)() >
        static void behavior_thunk(Actor& a)
        {
            (static_cast<T&>(a).*BehaviorMemberFn)(); // downcast to concrete type
        }
```

Here's `become()`. Note that we have to specify the concrete `this_type`.
The compiler can't infer it.

```c++
        template<void (this_type::*BehaviorMemberFn)() >
        void become()
        {
            Actor::become(behavior_thunk<this_type, BehaviorMemberFn>);
        }
```

Here's how it's used:
```c++
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
```

Notice that the member function pointer is a _type_ parameter to `become<>()`.
This allows the thunk to be generated at compile time. Otherwise we'd need
to store a member function pointer somewhere and call it at runtime, which
in our case is an unnecessary extra level of indirection.

```c++
    void test2b()
    {
        MD_BEGIN_OUTPUT("Test 2b");

        // Test Actor::Actor(bp), send(), become(), inject()
        Actor affirmative(print_yes);
        Actor negative(print_no);

        // become<>() version:
        AlternatingSenderTemplateThunks2 alternately2(affirmative, negative);
        for (int i = 0; i < 5; ++i)
            inject(alternately2); // prints yes, no, yes, no...
    }
```

>[Test 2b]:
> ```
> yes
> no
> yes
> no
> yes
> ```

```c++
} // end namespace cpp_actors_2
```

## 5. Member Function Thunks, Refactored

Let's refactor the thunk template mechanism to support reuse.

```c++
namespace cpp_actors_3 {

    using namespace cpp_actors_1;
```

I'll introduce an intermediate base class `ActorT` parameterised by
the derived type. This lets us define `behavior_thunk` and `become()`
templates that depend on the type of the derived class.
(Please get in touch if you have a simpler solution!)

```c++
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
```

Now we can use `ActorT` as the base class. The code for
`behavior_thunk` and `become` is the same as in the previous section:

```c++
    struct AlternatingSenderTemplateThunks : public ActorT<AlternatingSenderTemplateThunks> {
        typedef AlternatingSenderTemplateThunks this_type;

        Actor &target0_, &target1_;

        AlternatingSenderTemplateThunks(Actor& a, Actor& b)
            : ActorT(behavior_thunk<&this_type::sender_0>)
            , target0_(a)
            , target1_(b) {}

    private:
        // non-copyable
        AlternatingSenderTemplateThunks(const AlternatingSenderTemplateThunks&);
        AlternatingSenderTemplateThunks& operator=(const AlternatingSenderTemplateThunks&);

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

    void test3()
    {
        MD_BEGIN_OUTPUT("Test 3");

        // Test Actor::Actor(bp), send(), become(), inject()
        Actor affirmative(print_yes);
        Actor negative(print_no);
        AlternatingSender alternately(affirmative, negative);
        for (int i = 0; i < 5; ++i)
            inject(alternately); // prints yes, no, yes, no...
    }
```

>[Test 3]:
> ```
> yes
> no
> yes
> no
> yes
> ```

```c++
} // end namespace cpp_actors_3
```

With the member function thunking mechanism in place, it's easy
to express the polymorphic delete-later mechanism from part 2:

```c++
namespace cpp_actors_3 { // extend namespace

// An actor that destroys itself when it receives its first message.
struct SelfDestructor : public ActorT<SelfDestructor> {
    typedef SelfDestructor this_type;
    SelfDestructor()
        : ActorT(behavior_thunk<&this_type::self_destruct>)
    {}

    ~SelfDestructor()
    {
        std::printf("deleting.\n");
    }

protected:
    void self_destruct()
    {
        delete this;
    }
};

struct Creator : public ActorT<Creator> {
    typedef Creator this_type;
    Creator()
        : ActorT(behavior_thunk<&this_type::receive>)
    {}

protected:
    // Create and immediately delete actors by sending them a message.
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
} // end namespace cpp_actors_3
```

In the next part I'll add support for defining a default behavior,
thus eliminating four lines of boilerplate from many actor definitions.

```c++
int main(int, char *[])
{
    part_02_raw_actors::test();
    cpp_actors_1::test1();
    cpp_actors_2::test2a();
    cpp_actors_2::test2b();
    cpp_actors_3::test3();
    cpp_actors_3::test4();
}
```

***
Next: [Default Behavior](04-default-behavior.cpp.md) <br/>
Previous: [Raw Actors](02-raw-actors.cpp.md) <br/>
Up: [README](README.md)

Generated from [`03-cpp-actors.cpp`](03-cpp-actors.cpp) by [`emdeer.py`](emdeer.py) at UTC 2015-08-17 10:15:02.347000
