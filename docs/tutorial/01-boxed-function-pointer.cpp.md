```c++
#include <cstdio>
```

Boxed Function Pointers
=======================

Our story begins with a "boxed" function pointer: a function pointer
inside in a struct.

```c++
struct Box; // forward declaration
```

The function takes a pointer to its containing box as an argument.

```c++
typedef void (*BoxProc)(Box *a);
```

Since it doesn't return a value, technically it's a procedure; hence `BoxProc`.

Here's the proc in its box:

```c++
struct Box {
    BoxProc bp;
};
```

So far, so good. Let's define a procedure and test it:

```c++
void hello(Box *)
{
    std::printf("hello world!\n");
}

void example_1()
{
    std::printf("@OUTPUT-1:\n\n"); // (You can ignore these @OUTPUT lines. They serve 
                                   // to delimit each example's output so that it can 
                                   // be automatically inserted in to the markdown 
                                   // rendering of this file.)
    Box a = {hello};
    a.bp(&a);
}
```

Which outputs:
> ```
> @OUTPUT-1:
> 
> hello world!
> ```


Each instance of `Box` can reference a different procedure.

Let's define an second procedure:

```c++
void goodbye(Box *)
{
    std::printf("goodbye world!\n");
}

void example_2()
{
    std::printf("@OUTPUT-2:\n\n");

    Box a = { hello };
    Box b = { hello };
    Box c = { goodbye };

    a.bp(&a);
    b.bp(&b);
    c.bp(&c);
}
```

Which outputs:
> ```
> @OUTPUT-2:
> 
> hello world!
> hello world!
> goodbye world!
> ```

Typing the box name twice for each `bp` call is error-prone. Let's define 
a function to invoke the box's proc:

```c++
void invoke(Box& a)
{
    a.bp(&a);
}

void example_3()
{
    std::printf("@OUTPUT-3:\n\n");

    Box a = { hello };
    Box b = { goodbye };
    // Uniform interface, different behaviors...
    invoke(a);
    invoke(b);
}
```

> ```
> @OUTPUT-3:
> 
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
    std::printf("@OUTPUT-4:\n\n");

    Box a = { open };
    for (int i=0; i < 6; ++i)
        invoke(a);
}
```

> ```
> @OUTPUT-4:
> 
> opened
> closed
> opened
> closed
> opened
> closed
> ```

That's it for now. Rest assured there is more to come :)

```c++
int main(int, char *[])
{
    example_1();
    example_2();
    example_3();
    example_4();
}
```

Generated from [`01-boxed-function-pointer.cpp`](01-boxed-function-pointer.cpp) by [`emdeer.py`](emdeer.py) at UTC 2015-05-20 16:32:24.631000
