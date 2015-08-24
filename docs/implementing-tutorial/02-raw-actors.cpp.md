```c++
#include <cstdio>
#include "emdeer.h" // (C++ to markdown conversion utils)
```

2. Raw Actors
=============

[Part 1](01-boxed-function-pointers.cpp.md) introduced *boxed function
pointers*. We'll continue to use boxed function pointers, but we'll
rename things to be suggestive of the
[_actor model_](http://en.wikipedia.org/wiki/Actor_model).

Our protagonists are now `Actors`:

```c++
struct Actor; // forward declaration
```

`Actors` contain a function pointer named `BehaviorProc`:
a procedure that determines how the actor behaves.

```c++
typedef void (*BehaviorProc)(Actor *a);
```

For example, here's a procedure for a non-acting actor:

```c++
void do_nothing(Actor *)
{
}
```

Here's the `Actor` type. It's a box containing a function pointer:

```c++
struct Actor {
    BehaviorProc bp_; // Underscore-suffix is a convention that I use
                      // to indicate local/encapsulated member data.
```

We'll define a constructor to make setting the initial behavior easier:
```c++
    Actor(BehaviorProc bp) : bp_(bp) {}
```

And a default constructor (for non-acting actors):
```c++
    Actor() : bp_(do_nothing) {}
};
```

`inject()` invokes an actor's behavior. I'll explain why it's called `inject` shortly.

```c++
void inject(Actor& a) // invoke the actor's current behavior
{
    a.bp_(&a);
}
```

Let's try it out:

```c++
void hello(Actor *)
{
    std::printf("hello world!\n");
}

void example_1()
{
    MD_BEGIN_OUTPUT("Output 1"); // mark output section for insertion into markdown

    Actor inoperative;
    inject(inoperative); // does nothing

    Actor greeter(hello);
    inject(greeter);
}
```

Which outputs:

>[Output 1]:
> ```
> hello world!
> ```

It works. Good.

(`Actor` is now the central character in our story.
Even so, keep in mind that we're still dealing with boxed function pointers.
For now it should be self-evident; later we'll be layering up
additional machinery.)

Actors conform to three "axioms":

 1. Actors can specify the behavior that they will use at their next activation
 2. Actors can send messages to other actors
 3. Actors can create new actors

Let's make an initial implementation of these axioms...


## Axiom 1: Actors can specify the behavior that they will use at their next activation ##

Define a function `become()` to specify an actor's behavior.
It sets the actor's behavior proc:

```c++
void become(Actor *a, BehaviorProc bp)
{
    a->bp_ = bp;
}
```

We'll test `become()` by defining an actor that alternates between printing `yes` and `no`:

```c++
void no(Actor *a);

void yes(Actor *a)
{
    std::printf("yes\n");
    become(a, no);
}

void no(Actor *a)
{
    std::printf("no\n");
    become(a, yes);
}

void example_2()
{
    MD_BEGIN_OUTPUT("Output 2");

    Actor indecisive(yes);
    for(int i=0; i < 6; ++i)
        inject(indecisive);
}
```

>[Output 2]:
> ```
> yes
> no
> yes
> no
> yes
> no
> ```

Looks good.

(By the way, I'm intentionally writing in a C-like style to suppress irrelevant detail.
I'll move towards idiomatic C++ in due course.)

## Axiom 2: Actors can send messages to other actors ##

We haven't met _messages_ yet. I'll introduce them in a later episode.
For now, "sending a message" is synonymous with invoking an actor's behavior.
You can think of it as sending an empty message.
We'll define a placeholder `send()` method:

```c++
void send(Actor& a)
{
    a.bp_(&a);
}
```

You might have noticed that `send()` looks a lot like `inject()`.
We'll adopt the convention that `send()` is only used within actor behaviors.
i.e. for actors to send messages to each other. `inject()` will be used
to _inject_  messages from outside the actor network. e.g. to invoke actors from
top level functions. The distinction may seem unnecessary, but later
we will need different code for each case.

(Sidenote: I'm tempted to name `send` `sendto`. We'll see. In the meantime
_one should not agonise over that which can easily be changed later._)


Now the fun really begins. We can subclass `Actor` to introduce
additional data members. We'll define an actor that alternately sends
to one of two targets:

```c++
struct AlternatingSender : public Actor {
    Actor *target0_, *target1_;

    AlternatingSender(Actor* a, Actor* b)
        : Actor(sender_0)
        , target0_(a)
        , target1_(b)
    {
    }
```

I've defined `AlternatingSender`'s behavior procs inside the struct:

```c++
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
```
(Without the `static` keyword they'd be member functions and couldn't
be used as `BehaviorProcs`. I'd like them to be member functions.
Those `static_cast` downcasts sure are ugly. I'll fix them next time.)
```c++
};
```

Now to test it:

```c++
void example_3()
{
    MD_BEGIN_OUTPUT("Output 3");

    Actor initiallyAffirmative(yes);
    Actor initiallyNegative(no);
    AlternatingSender alternately(&initiallyAffirmative, &initiallyNegative);

    for(int i=0; i < 8; ++i)
        inject(alternately);
}
```

>[Output 3]:
> ```
> yes
> no
> no
> yes
> yes
> no
> no
> yes
> ```

Make sure that you understand the output.
Recall that the `yes` and `no` behaviors are set up to alternate.
We are alternately invoking two actors each with alternating behaviors.


By the way, if you're getting uneasy about the weird static functions, `static_casts`,
non-idiomatic C++ and so on, it's okay, you're not alone. I'm uneasy too.
I'll work on that in the next episode. Promise.

## Axiom 3: Actors can create new actors ##

Something like the following should suffice to create new actors:

> ```C++
> Actor *a = new Actor;
>```

Now, if we're going to support creating new actors we also need to provide a way to delete them.
This raises a subtle point: our actors are polymorphic. Each subclass may need its own
destructor (e.g. to clean up  local data, and to ensure that member objects have their
destructors invoked). Typically this is what a virtual destructor is for, however I'm loath
to introduce the space overhead of virtual functions just to deal with destruction.

One way to handle polymorphic destruction is for actors to delete themselves:

```c++
struct SelfDestructor : public Actor {
    SelfDestructor()
        : Actor(self_destruct)
    {}

    ~SelfDestructor()
    {
        std::printf("deleting.\n");
    }

    static void self_destruct(Actor *a)
    {
#if 1
        // NB: downcast to ensure that the correct destructor is called.
        // Without this cast only ~Actor is called.
        // This is how we get polymorphic destruction without a virtual dtor.
        delete static_cast<SelfDestructor*>(a);
#else
        delete a; // Set the #if above to 0 and check the output. It won't say "deleting."
#endif
    }
};
```

`SelfDestructor` deletes itself the first time it receives a message.
This is not very useful, except to demonstrate creating new actors and
immediately asking them to destroy themselves...

```c++
struct Creator : public Actor {
    Creator()
        : Actor(receive)
    {}

    // I like `receive` as the behavior name for
    // actors that never change their behavior.
    static void receive(Actor *)
    {
        std::printf("creating.\n");
        Actor *transient = new SelfDestructor;
        send(*transient);
    }
};

void example_4()
{
    MD_BEGIN_OUTPUT("Output 4");

    Creator creator;

    for(int i=0; i < 6; ++i)
        inject(creator);
}
```

>[Output 4]:
> ```
> creating.
> deleting.
> creating.
> deleting.
> creating.
> deleting.
> creating.
> deleting.
> creating.
> deleting.
> creating.
> deleting.
> ```

It proves the concept.

In the next installment we'll C++-ify things.

```c++
int main(int, char *[])
{
    example_1();
    example_2();
    example_3();
    example_4();
}
```

***
Next: [C++ Actors](03-cpp-actors.cpp.md) <br/>
Previous: [Boxed Function Pointers](01-boxed-function-pointers.cpp.md) <br/>
Up: [README](README.md)

Generated from [`02-raw-actors.cpp`](02-raw-actors.cpp) by [`emdeer.py`](emdeer.py) at UTC 2015-08-24 13:22:45.958000
