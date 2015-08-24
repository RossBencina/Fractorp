```c++
#include <cstdio>
#include "emdeer.h" // (C++ to markdown conversion utils)
```

1. Boxed Function Pointers
==========================

Our story begins with a "boxed" function pointer: a function pointer
stored inside a `struct`. We declare the type of the function pointer using a
`typedef`:

```c++
struct Box; // forward declaration needed by BoxProc

typedef void (*BoxProc)(Box *a);
```

The function takes a pointer to its containing box as an argument.
It doesn't return a value, so technically it's a procedure; hence `BoxProc`.

Here's the proc in its box:

```c++
struct Box {
    BoxProc bp;
};
```

So far, so good. Let's define a procedure and try it out:

```c++
void hello(Box *)
{
    std::printf("hello world!\n");
}

void example_1()
{
    MD_BEGIN_OUTPUT("Output 1");    // (MD_BEGIN_OUTPUT lines mark each
                                    // example's output. This lets the emdeer
                                    // markdown generator automatically insert
                                    // program output into the markdown
                                    // rendering of this file.)
    Box a = {hello};
    a.bp(&a);
}
```

Which outputs:
>[Output 1]:
> ```
> hello world!
> ```


Each instance of `Box` can reference a different procedure:

```c++
void goodbye(Box *)
{
    std::printf("goodbye world!\n");
}

void example_2()
{
    MD_BEGIN_OUTPUT("Output 2");

    Box a = { hello };
    Box b = { hello };
    Box c = { goodbye };

    a.bp(&a);
    b.bp(&b);
    c.bp(&c);
}
```

>[Output 2]:
> ```
> hello world!
> hello world!
> goodbye world!
> ```

Typing the box name twice for each `bp` call is tedious and error-prone.
Let's define a function to invoke the box's proc:

```c++
void invoke(Box& a)
{
    a.bp(&a);
}

void example_3()
{
    MD_BEGIN_OUTPUT("Output 3");

    Box a = { hello };
    Box b = { goodbye };
    // Uniform interface, different behaviors...
    invoke(a);
    invoke(b);
}
```

>[Output 3]:
> ```
> hello world!
> goodbye world!
> ```

We have created a form of "behavioral polymorphism," which is a fancy
way to say that not all Boxes behave alike. So far we have `hello` boxes
and `goodbye` boxes.

Unlike conventional "objects," which typically run the same code
each time you invoke a method, our boxes can vary their behavior
by modifying their `bp` pointers.

Here is an example of a box that alternates between two different behaviors:

```c++
void close(Box *a); // forward declaration

void open(Box *a)
{
    std::printf("opened\n");
    a->bp = close;
}

void close(Box *a)
{
    std::printf("closed\n");
    a->bp = open;
}

void example_4()
{
    MD_BEGIN_OUTPUT("Output 4");

    Box a = { open };
    for (int i=0; i < 6; ++i)
        invoke(a);
}
```

>[Output 4]:
> ```
> opened
> closed
> opened
> closed
> opened
> closed
> ```

That's it for the first installment. Next time we'll begin to explore
using boxed function pointers to implement something called _Actors_.

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
Next: [Raw Actors](02-raw-actors.cpp.md) <br/>
Up: [README](README.md)

Generated from [`01-boxed-function-pointers.cpp`](01-boxed-function-pointers.cpp) by [`emdeer.py`](emdeer.py) at UTC 2015-08-24 13:22:57.275000
