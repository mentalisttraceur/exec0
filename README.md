# exec0

exec0 is a small project born as a workaround for the fact that shell scripts
generally have no way to execute another program with explicit control over the
zeroth argument (the name that the program sees itself invoked as).





# Goal

This project aims to provide several implementations of an `exec0` command line
program, which simply executes a program with the given list of arguments, with 
the noteworthy detail being that the list of arguments starts at the zeroth
argument.



### Benefits of `exec0` Functionality

Ideally, the full flexibility of the underlying `execve` system call (or your
system's equivalent/next-best-thing, like Windows' `CreateProcess` function)
would be exposed. This brings the ability to test how programs react to such
invocation oddities like being executed with literally _no_ arguments, not even
the zeroth argument (on systems which allow such behavior: some don't), and the
ability to control commands which change behavior based on the zeroth argument,
into easy command-line reach.

Unfortunately, several languages' functionality does not allow you to invoke a
command with with _no_ arguments, and in the worst case I know of, it's not
possible to invoke a command with a zero-length zeroth argument either. But
those are pedantic corner-cases: the majority of the benefits apply to every
`exec0` implementation.



### Benefits of Multiple Implementations

1. You can take whichever implementation is easiest for you to install.

2. We can take all of the interpreted languages' `exec0` implementations, and
roll them up into a polyfill for Bourne shell `exec` `-a` and `-l` options,
giving shell scripts this functionality without a reliance on an `exec0`
implementation being installed.

3. Having a bunch of different "how to execute another process in `language`
with full control of the arguments" implementations in one place can be useful
and/or educational.





# Current State

At this time, only a C implementation is fully written, tested, and committed.
It's using my not-really-idiomatic approach to C coding, including cutting out
all unnecessary execution overhead (e.g. using UNIX IO primitives instead of
stdio.h IO, etc). This is the reference implementation in case anyone wants to
submit others.

I've got a few more in the works (Bourne shell, Perl) and several others
planned (Python, etc).


#### Idle Contemplation / "Plans"

As more of an academic pursuit, I'd also like to add native Windows portability
to the C implementation using `WriteFile`/`WriteFileGather`, `CreateProcess`,
etc. This will require some non-trivial work since `CreateProcess` was designed
to take a command-line string parsed into arguments, instead of just an array
of arguments, and `WriteFileGather` is asynchronous (sometimes in practice,
always in semantics/syntax), so they're not strict drop-in replacements. And if
I do that, I may also replace `execvp` to a lower-level `execve` implementation
with a manual `PATH` search - but that seems like a portability nightmare due
to inconsistent OS behavior with e.g. shell scripts, and generally worse in
every way unless I use the execve syscall more directly. Anyway, I don't know
how soon the Windows support will get done: until then it should work fine with
either MinGW or Cygwin, and most other `exec0` implementations will be portable
because of their nature as languages with better abstraction for this. I even
wouldn't be against adding native C code for any other system that lacks either
of those.





# Usage / "Stable API"

Basically, the typical use is this:

    exec0 [--] program [arg0 [arg1 [arg2 ...]]]

If "program" starts with a `-` character, you should put the end-of-options
argument (`--`) as a separate argument in front of the "program" argument to
make the invocation unambiguous and future-safe - otherwise it's not necessary.
Other useful invocations are:

    exec0 --help
    exec0 --version

You can replace `--help` with the short option `-h`, and `--version` with the
short option `-V` (note that's a _capital_ V, **not** lowercase v), but note
that the short versions of options are not future-safe (a future change might
repurpose the short, single-letter options, whereas the long options are part
of the stable API, so use the long options in scripts).

That's all you need to know for basic usage. Examples are at the bottom of this
README. If you want to write future-proof scripts with `exec0`, read the
following sub-sections.



### Invocation/Future-Safety

No other invocations than the ones in code blocks above are considered part of
the "stable API". If you invoke `exec0` in a way that works but isn't part of
the stable API, you shouldn't rely on future versions doing the same thing.

##### Examples

`exec0 --help foo bar` works as of 1.0.0 in a way equivalent to `exec0 --help`.
But it might do something else in a future version: `--help` might take some
optional arguments in the future, or any number of other things.

Also, in many of the commits to this repository before this formal `exec0`
specification was documented, `exec0 --foo` would try to execute a command with
the name `--foo`, instead of reporting a bad option. And in the future, `--foo`
could be a new option. (Per the above, `exec0 -- --foo` was always correct, of
course.)



### Execution

`exec0` executes the program specified with the remaining argument list being
the argument list passed to the executed program.

On platforms that allow this, `exec0` process will "become" the executed
program (replaces its own process image). On platforms/implementation where
this is not supported, `exec0` will attempt to act as equivalently as possible:
wait until the executed process finishes, exit with the same exit code, etc.

When execution fails (e.g. the command is not found, isn't executable, etc),
exec0 exits with an error.

The behavior of `exec0` will be mostly comparable to how the system it is
running on would normally determine which program to execute, although at this
time no specific behavior is promised. As a general trend, the PATH is searched
for the command given, unless the command given already contains the system's
path separator (e.g. `/` on *nix). No behavior is currently promised if `exec0`
would do a PATH search but PATH is unset, though more concrete behavior might
be specified.

As much as possible, `exec0` will pass all process state to the executed
program without any alterations.



### Output

##### Errors

If there's an error, `exec0` tries to print a message to stderr. Those messages
are not currently explicitly intended to be machine parseable, though that's an
area of interest for possible future improvement. Exit status for errors does
not currently reflect the exact error reason - just that an error occurred:
this is also an area for possible future improvements.

##### Help

The output of the "help" option **is not** part of the stable API. The stable
API just guarantees that the output of the help option will help a thinking
entity learn/remember how to use the tool.

##### Version

The output of the "version" option **is** part of the stable API. Currently
`exec0` uses [Semantic Versioning 2.0.0](http://semver.org/spec/v2.0.0.html)
for its version numbers, so the output contains the semver version number.

Note that the output also contains the name of the program, `exec0`, so that if
it's installed under a different name, you know what it is and _where it came
from_ (this means that I'd appreciate it if other implementations/projects not
part of this repo that might be installed as `exec0` used some other string
instead, unless you were using the name first or become much more widely used).

You are guaranteed that the _first_ word of the _first_ line of version output
is the name of the project (in this case, `exec0`), and the _second_ word of
the _first_ line of the output is the semver version number.

Future-safe scripts should not assume that the current output contents will be
the _only_ output. A future change may add additional version information to
the output. See the examples section for how you can parse the version output
in a future-safe way.





# Examples



### Executing Other Commands

Execute `bash` as an interactive, "login" POSIX-y `sh`:

    exec0 bash -sh -i

The program `bash` will be looked up in the `PATH` and will see `argv[0]` as
`-sh`, and `argv[1]` as `-i`. (The `-` as the first character of the zeroth
argument tells `bash` that it is a login shell, and the rest of the name being
just `sh` makes it work more like a POSIX shell.)



### Extracting Version Info

Here is a broadly-portably Bourne shell command to get the semver version
number:

    exec0 --version | head -n 1 | cut -d ' ' -f 2

..and another to get the "name", which should always be `exec0` if it came from
this repo/project.

    exec0 --version | head -n 1 | cut -d ' ' -f 1

..of course in most modern Bourne-descendent shells you can skip the double
pipeline and just use the substitution capabilities of the shell, and other
programming/scripting languages have their own way to split strings on spaces
and/or newlines.
