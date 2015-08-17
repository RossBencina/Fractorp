
Implementing Communicating Things
=================================

_An evolving tutorial about implementing light-weight actors and related
techniques in C++. Mostly applicable to other languages too._

The story so far:

1. [Boxed Function Pointers](01-boxed-function-pointers.cpp.md)
2. [Raw Actors](02-raw-actors.cpp.md)
3. [C++ Actors](03-cpp-actors.cpp.md)
4. [Default Initial Behavior (Convenience Feature)](04-default-behavior.cpp.md) *__NEW!__*

Each part is writen as a stand-alone runnable C++ program, then automatically
converted to markdown. Read the formatted markdown by clicking the links
above, or load the `.cpp` source files in your favourite editor.

Suggestions and patches are most welcome. I would love to receive feedback
including:
 - Constructive criticism of the presentation (this is, as much as anything else, a writing excercise).
 - Suggestions on how to clarify, simplify or improve the writing and/or the C++ code.
 - Copy editing, including grammar and typos.

Please use Git Hub's pull requests and issue tracker. If you'd like to discuss
any aspect of this tutorial, feel free to open an issue.
You can also reach me on Twitter: @RossBencina.

---

The main topic here is light-weight actors, but the tutorial also serves as a
development context for [`emdeer.py`](emdeer.py): a minimal literate programming
tool for C++. `emdeer.py` converts `.cpp` source files
into markdown. It also compiles the `.cpp` code into an executable, runs it, and
merges the program output into the markdown.  When used in conjunction
with [grip](https://github.com/joeyespo/grip) and [`grip_hook.py`](grip_hook.py)
you can edit the `.cpp` source code, then refresh your web browser to
trigger recompilation of the executable and regeneration of the markdown.

Have fun :)

Ross B.  
Melbourne, May-August 2015

