
Implementing Communicating Things
=================================

_An evolving tutorial on implementing "micro-actors" and related techniques
in C++. (Mostly applicable to other languages too.)_

The story so far:

1. [Boxed Function Pointers](01-boxed-function-pointers.cpp.md)
2. [Raw Actors](02-raw-actors.cpp.md)

Each part is writen as a stand-alone, runnable C++ program, then automatically
converted to markdown. You can read the formatted markdown files
by clicking the links above, or you can load the .cpp source files in your
favourite editor.

---

Aside from the main topic of micro-actors, the tutorial also serves as a
development context for [`emdeer.py`](emdeer.py): a
minimal literate programming tool for C++. `emdeer.py` converts `.cpp` source files
into markdown. It also compiles the `.cpp` code into an executable, runs it, and
merges the output of the executable into the markdown.  When used in conjunction
with [grip](https://github.com/joeyespo/grip) and [`grip_hook.py`](grip_hook.py)
you can edit the `.cpp` source code, then refresh your web browser to
automatically trigger recompilation of the executable and regeneration of the markdown.

Suggestions and patches are most welcome. Here are examples of
some of the kind of feedback that I would love to receive:
 - Copy editing, including grammar and typos.
 - Constructive criticism of the presentation (this is, as much as anything else, a writing excercise.)
 - Suggestions on how to clarify, simplify or improve the writing and/or the C++ code.

Please use Git Hub's pull requests and issue tracker. If you'd like to discuss
any aspect of this tutorial, please open an issue, I'll reply there.

Have fun :)

Ross B.  
Melbourne, May-June 2015
