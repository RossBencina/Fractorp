This folder contains an envolving tutorial about techniques for
implementing "micro-actors" in C++. (Most of the techniques apply
to other languages too.)

Here are the parts that have been posted so far:

1. [Boxed Function Pointers](01-boxed-function-pointers.cpp.md)
2. [Raw Actors](02-raw-actors.cpp.md)

The tutorial also serves as a development platform for [`emdeer.py`](emdeer.py), which is a
minimal literate programming tool for C++. `emdeer.py` converts .cpp source files
into markdown. It also compiles the .cpp code into an executable, runs it, and
merges the output of the executable into the markdown.  When used in conjunction
with [grip](https://github.com/joeyespo/grip) and [`grip_hook.py`](grip_hook.py)
you can edit the .cpp source code, then refresh your web browser to
automatically trigger recompilation of the executable and regeneration of the markdown.

Suggestions and patches welcome. Especially if they simplify or clarify things :)

Have fun.
