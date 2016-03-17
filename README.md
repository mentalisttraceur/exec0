# exec0

exec0 is a small project born as a workaround for the fact that shell scripts
generally have no way to execute another program with explicitly control over
the zeroth argument (the name that the program sees itself invoked as).


## Goal

This project aims to provide several implementations of an `exec0` command line
program, which simply executes a program with the given list of arguments, with 
the noteworthy detail being that the list of arguments starts at the zeroth
argument.

#### Benefits of `exec0` Functionality

Ideally, the full flexibility of the underlying `execve` system call (or your
system's equivalent/next-best-thing, like Windows' `CreateProcess` function)
would be exposed. This brings the ability to test how programs react to such
invocation oddities like being executed with literally _no_ arguments, not even
the zeroth argument, and the ability to control commands which change behavior
based on the zeroth argument, into easy command-line reach.

Unfortunately, several languages' functionality does not allow you to invoke a
command with with _no_ arguments, and in the worst case I know of, it's not
possible to invoke a command with a zero-length zeroth argument either. But
those are pedantic corner-cases: the majority of the benefits apply to every
`exec0` implementation.


#### Benefits of Multiple Implementations

1. You can take whichever implementation is easiest for you to install.

2. We can take all of the interpreted languages' `exec0` implementations, and
roll them up into a polyfill for Bourne shell `exec` `-a` and `-l` options,
giving shell scripts this functionality without a reliance on an `exec0`
implementation being installed.

3. Having a bunch of different "how to execute another process in `language`
with full control of the arguments" implementations in one place can be useful
and/or educational.


## Current State

At this time, only a C implementation is fully written, tested, and committed.
It's using my not-really-idiomatic approach to C coding, including cutting out
all unnecessary execution overhead (e.g. using UNIX IO primitives instead of
stdio.h IO, etc). This is the reference implementation in case anyone wants to
submit others.

I've got a few more in the works (Bourne shell, Perl) and several others
planned (Python, etc).

As more of an academic pursuit, I'd also like to add native Windows portability
to the C implementation using `WriteFile`/`WriteFileGather`, `CreateProcess`,
etc. This will require some non-trivial work since `CreateProcess` was designed
to take a command-line string parsed into arguments, instead of just an array
of arguments, and `WriteFileGather` is asynchronous (sometimes in practice,
always in semantics/syntax), so they're not strict drop-in replacements. So I
don't know how soon that will get done: until then it should work fine with
either mingw or cygwin, and most other `exec0` implementations will be portable
because of their nature as languages with better abstraction for this. I even
wouldn't be against adding native C code for any other system that lacks either
of those.
