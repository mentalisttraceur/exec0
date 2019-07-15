# exec0

`exec0` is a small command line program which executes another program
with the given list of arguments, with the noteworthy detail being that the
list of arguments starts at the zeroth argument (the name that the program sees
itself invoked as).





# Why?

It was born as a workaround for the fact that shell scripts generally
have no way to execute another program with explicit control over the
zeroth argument (the `-a` option to `exec` isn't portable). A few
other scripting and programming languages don't provide such control.

Being able to do this is useful for:

1. Writing wrapper scripts around programs which use the zeroth argument to
determine their own behavior.

2. Testing programs which use the zeroth argument - many at least use it for
printing messages, and most of those assume there is always a zeroth argument,
failing poorly when there are literally *no* arguments.

3. More creative usecases can be imagined, such as running two instances of a
program with different zeroth arguments, so that their messages are distinct
when printed.

A couple of operating systems don't allow invoking a command without the zeroth
argument, but besides not allowing exercising that pedantic corner-case, exec0
still provides the rest of the benefits of controlling the zeroth argument on
those platforms too.





# Current State

At this time, version 1.0.0 is released, written in C. It supports all modern
*nix systems. A stable "API" for the `exec0` command-line tool is defined for
major version 1 (see below).

It's written in my not-really-idiomatic approach to C coding, including cutting
out all unnecessary execution overhead (e.g. using UNIX IO primitives instead
of stdio.h IO, etc).

##### Future Plans

Add native Windows support. This will require some non-trivial work since
`CreateProcess` was designed to take a command-line string parsed into
arguments, instead of just an array of arguments, and `WriteFileGather` is
asynchronous (sometimes in practice, always in semantics/syntax), so they're
not strict drop-in replacements. And if I do that, I may also replace `execvp`
to a lower-level implementation with a manual `PATH` search and `execve`.
Anyway, I don't know how soon the Windows support will get done: until then it
should work fine with either MinGW or Cygwin, as far as I know.





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
short option `-V` (note that's a *capital* V, **not** lowercase v), but note
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
optional arguments in the future which change behavior, etc.

Also, in many of the commits to this repository before this formal `exec0`
specification was documented, `exec0 --foo` would try to execute a command with
the name `--foo`, instead of reporting a bad option. And in the future, `--foo`
could be a new option. (Per the above, `exec0 -- --foo` was always correct, of
course.)



### Execution

`exec0` executes the program specified with the remaining argument list being
the argument list passed to the executed program.

On platforms that allow this, `exec0` process will "become" the executed
program (replaces its own process image). On platforms where this is not
supported, `exec0` will attempt to act as equivalently as possible: wait until
the executed process finishes, exit with the same exit code, etc.

When execution fails (e.g. the command is not found, isn't executable, etc),
exec0 exits with an error.

The behavior of `exec0` will be mostly comparable to how the system it is
running on would normally determine which program to execute, although at this
time no specific behavior is promised. As a general trend, the PATH is searched
for the command given, unless the command given already contains the system's
path separator (e.g. `/` on *nix). No behavior is currently promised if `exec0`
would do a PATH search but PATH is unset, though more concrete behavior might
be specified in the future.

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

Note that the output also contains the name of the program, `exec0`, so that
even if it's installed under a different name, you know it is `exec0`.
(this means that I'd appreciate it if other implementations/projects not
part of this repo that might be installed as `exec0` used some other string
instead, unless you were using the name first or become much more widely used).

You are guaranteed that the *first* word of the *first* line of version output
is the name of the project (in this case, `exec0`), and the *second* word of
the *first* line of the output is the semver version number.

Future-safe scripts should not assume that the current output contents will be
the *only* output. A future change may add additional version information to
the output. See the examples section for how you can parse the version output
in a future-safe way.





# Examples



### Executing Other Commands

Execute `bash` as an interactive, "login" shell`:

    exec0 bash -bash -i

The program `bash` will be looked up in the `PATH` and will see `argv[0]` as
`-bassh`, and `argv[1]` as `-i`. (The `-` as the first character of the
zeroth argument tells `bash` that it is a login shell.)

Execute `bash` as an interactive *POSIX* shell:

    exec0 bash -sh -i

Same as the last example, except `argv[0]` is `sh` instead of `-bash`
(when `bash` is invoked as `sh`, it conforms more to POSIX).


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
