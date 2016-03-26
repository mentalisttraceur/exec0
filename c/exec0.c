/*****************************************************************************\
 * exec0 1.0.0 - C implementation
 * Copyright (C) 2016-03-23 Alexander Kozhevnikov <mentalisttraceur@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
\*****************************************************************************/

/* This must be defined for limits.h to include SSIZE_MAX */
#define _POSIX_C_SOURCE 1

/* Standard C library headers */
#include <stdlib.h> /* size_t, EXIT_SUCCESS, EXIT_FAILURE */
#include <string.h> /* strlen */
#include <errno.h> /* errno, strerror */
#include <limits.h> /* SSIZE_MAX */

/* Standard UNIX/Linux (POSIX/SUSv3 base) headers */
#include <unistd.h> /* ssize_t, write, execvp, STDOUT_FILENO, STDERR_FILENO */

/* Common UNIX/Linux (POSIX/SUSv3 XSI) headers */
#include <sys/uio.h> /* struct iovec, writev */


char const version[] = "1.0.0\n";
char const stdoutWritingError[] = ": error writing to stdout: ";
char const colonSpaceSplit[] = ": ";
char const noArgumentsGiven[] = ": need command or option argument\n";
char const newline = '\n';

char const helpTextPrefix[] = "Usage: ";
char const helpText[] =
 " OPTION|COMMAND [NAME [ARGUMENT]...]\n"
 "\n"
 "Execute a command, specifying all arguments to pass to it, including the\n"
 "name the command sees itself invoked as (the \"zeroth\" argument).\n"
 "\n"
 "  -h, --help    Print this help text and exit.\n"
 "  -V, --version Print version information and exit.\n"
;


/*\
A basename function which does specifically what this program needs: returns an
"iovec" struct for use with writev. Does _not_ check for a null argument.
\*/
static
struct iovec basename(char const * str)
{
 struct iovec basename;
 char c;
 
 /*\
 This logic returns the same blank basename whether given a blank string, or if
 given a non-blank string where the last character is the path separator. This
 is not what the POSIX basename function does, but it's what's appropriate for
 the exec0 usecase of printing messages using the invoked name.
 \*/
 basename.iov_base = (void * )str;
 /* One scan gets both the start address, and (implicitly) the length. */
 while(c = *str)
 {
  str += 1;
  /*\
  Address increments before check, so it points behind the path separator,
  which is the correct spot.
  \*/
  if(c == '/')
  {
   basename.iov_base = (void * )str;
  }
 }
 /*\
 Compute length of basename from the difference between the pointer to the
 terminating null, and the pointer to the start of the basename.
 \*/
 basename.iov_len = str - (char const * )basename.iov_base;
 return basename;
}


/*\
write() can succeed, succeed partially then "fail", or just fail. However, if
it succeeds partially, we don't get any indication of an error. So we call it
in a loop: We want to relay a useful error message to the user if their write
fails, even after partially succeeding, and the write() API doesn't give us
the error information when it partially succeeds. It's noteworthy that most of
the time, this will only call write once in a successful case, once for errors
that are immediately apparent (e.g. bad file descriptor), and twice for most
other errors: even transient errors (e.g. full pipe) are unlikely to disappear
in the time window between two iterations of this loop.
\*/
static
int writeUntilError(int fd, void const * buf, size_t count)
{
 do
 {
  ssize_t result = write(1, buf, count);
  if(result == -1)
  {
   return errno;
  }
 #ifdef SUPPORT_HISTORICAL_WRITE_SEMANTICS
  /*\
  Some "historical" systems returned 0 where modern systems do -1 with errno
  set to EAGAIN/EWOULDBLOCK. The following if-statement accounts for the old
  way, while being compatible with modern specifications.
  \*/
  if(!result && count)
  {
   return EAGAIN;
  }
 #endif /* SUPPORT_HISTORICAL_WRITE_SEMANTICS */ 
  count -= result;
  buf = (char * )buf + result;
 }
 while(count);
 return 0;
}


/*\
This function is mainly there to account for the extremely unlikely possibility
that an error message is too big for SSIZE_MAX. This should never really happen
unless somehow the built-in error strings plus the command to invoke end up
spilling past SSIZE_MAX chars in length - which is huge. I'm not even sure if a
command line can be that long on most (any?) systems, given that the length of
a command line and environment passable to the execve(2) syscall has a length
limit too. Basically, it depends on if ARG_MAX is close to or bigger than
SSIZE_MAX. Anyway, I strongly believe in accounting for possible problems
unless there's sound evidence that it's definitely unnecessary.

No response to errors (besides the EINVAL due to too large of a write chunk) is
done because if writing the error message fails, what's next? Try to report an
error about how we can't report errors? (We could exit with a different status
code, but we already set it to indicate failure if we're printing an error, so
that seems to be low-value).
\*/
static
void writeErrorMsgOfAnySize(struct iovec * msg, unsigned int msgPartsToWrite)
{
 /*\
 We really only need two temporary variables: they have semantically different
 but related purposes at different parts of the code. Unions let us use two
 names for each, making the meaning of the values held clearer.
 \*/
 
 /*\
 Either the length of data remaining in the previously partially-writen message
 part, or the length of all parts as we count up to the maximum writev length.
 \*/
 union
 {
  size_t remainder;
  size_t total;
 }
 len;

 /*\
 Either the index into the message part array or the count of how many parts
 are to be written in the current writev attempt.
 \*/
 union
 {
  unsigned int i;
  unsigned int count;
 }
 part;
 
 /* When we start, there is no "remainder from a previously written part": */
 len.remainder = 0;
 /* First write optimistically attempts to write the entire message: */
 part.count = msgPartsToWrite;
 
 do
 {
  /* errno is set when we get here: resetting it makes it meaningful later: */
  errno = 0;
  
  if(writev(STDERR_FILENO, msg, part.count) != -1
  && (part.count < msgPartsToWrite || len.remainder))
  {
   /*\
   If writev succeeded _and_ we have something left (should only happen on the
   even iterations) we set up another full-write of the remaining message.
   \*/
   if(len.remainder)
   {
    /* Move i back to point at the partially-written message part: */
    part.i = part.count - 1;
    /* Move pointer forward and set length to just the unwritten remainder: */
    msg[part.i].iov_base = (char * )(msg[part.i].iov_base) + len.remainder;
    msg[part.i].iov_len = len.remainder;
    /* And now that we've accounted for the remainder, reset the variable: */
    len.remainder = 0;
   }
   /* Adjust values for the next write to cover just the unwritten parts: */
   msg += part.i;
   msgPartsToWrite -= part.i;
   part.count = msgPartsToWrite;
   continue;
  }
  /* EINVAL will be raised if the total message length was too big: */
  if(errno == EINVAL)
  {
   /* Invariant: len.remainder == 0 == (what we want len.total to be) */
   for(part.i = 0; len.total < SSIZE_MAX; part.i += 1)
   {
    if(part.i == msgPartsToWrite)
    {
     /* We only get here if EINVAL was raised for some other reason. */
     return;
    }
    len.total += msg[part.i].iov_len;
   }
   len.remainder = len.total - SSIZE_MAX;
   msg[part.i - 1].iov_len -= len.remainder;
   /* part.count == part.i == the number of parts we can write at once. */
   continue;
  }
  /*\
  If writev had neither of the above two cases of results, we're done. Either
  it succeeded with nothing left to write, or it had another error as we tried
  to print this error message, and there's no really useful way to handle that.
  \*/
 }
 while(0);
}


/* Write to stdout, if that fails write error to stderr. */
static
int writeStdOut_reportIfError(char const * buf, size_t len, char * arg0)
{
 int err = writeUntilError(STDOUT_FILENO, buf, len);
 if(!err)
 {
  /* Successfully wrote to stdout, just return. */
  return EXIT_SUCCESS;
 }
 
 /* Write failed: get error string, then compose and print error message. */
 char * errStr = strerror(err);
 struct iovec errMsg[4];
 errMsg[0] = basename(arg0);
 errMsg[1].iov_base = (void * )stdoutWritingError;
 errMsg[1].iov_len = sizeof(stdoutWritingError) - 1;
 errMsg[2].iov_base = errStr;
 errMsg[2].iov_len = strlen(errStr);
 errMsg[3].iov_base = (void * )&newline;
 errMsg[3].iov_len = 1;
 writev(STDERR_FILENO, errMsg, 4);
 return EXIT_FAILURE;
}


/* The action taken when there's no actionable arguments given. */
static
int error_noArguments(char * arg0)
{
 /* Construct error message including help text: */
 struct iovec errMsg[5];
 errMsg[0] = basename(arg0);
 errMsg[1].iov_base = (void * )noArgumentsGiven;
 errMsg[1].iov_len = sizeof(noArgumentsGiven) - 1;
 errMsg[2].iov_base = (void * )helpTextPrefix;
 errMsg[2].iov_len = sizeof(helpTextPrefix) - 1;
 errMsg[3] = errMsg[0];
 errMsg[4].iov_base = (void * )helpText;
 errMsg[4].iov_len = sizeof(helpText) - 1;

 writev(STDERR_FILENO, errMsg, 5);
 return EXIT_FAILURE;
}


/* The action taken when executing the given command fails. */
static
int error_execFailure(char * command, char * arg0)
{
 char * errStr = strerror(errno);
 struct iovec errMsg[6];
 errMsg[0] = basename(arg0);
 errMsg[1].iov_base = (void * )colonSpaceSplit;
 errMsg[1].iov_len = sizeof(colonSpaceSplit) - 1;
 errMsg[2].iov_base = command;
 errMsg[2].iov_len = strlen(command);
 errMsg[3] = errMsg[1];
 errMsg[4].iov_base = errStr;
 errMsg[4].iov_len = strlen(errStr);
 errMsg[5].iov_base = (void * )&newline;
 errMsg[5].iov_len = 1;
 writeErrorMsgOfAnySize(errMsg, 6);
 return EXIT_FAILURE;
}


int main(int argc, char * * argv)
{
 char * arg;
 char * arg0 = *argv;
 
 /*\
 There must be at least one argument (so two, counting argv[0]), to know what
 command to execute.
 \*/
 if(argc < 2)
 {
  if(!arg0)
  {
   /* If we don't even have arg0, we let the error print it as a blank arg0. */
   arg0 = "";
  }
  return error_noArguments(arg0);
 }
 
 /* We slide the start of argv past argv[0], because argv[0] is unused. */
 argv += 1;
 
 /* And inspect the next argument, which is either... */
 arg = *argv;
 
 /* ..the help-printing option: */
 if(!strcmp(arg, "-h") || !strcmp(arg, "--help"))
 {
  return writeStdOut_reportIfError(helpText, sizeof(helpText) -1, arg0);
 }
 /* .. the version printing option: */
 if(!strcmp(arg, "-V") || !strcmp(arg, "--version"))
 {
  return writeStdOut_reportIfError(version, sizeof(version) -1, arg0);
 }
 
 /* .. or arg is the "end of options" argument: */
 if(!strcmp(arg, "--"))
 {
  /*\
  ..in which case, we just skip it, and use the next argument as if nothing
  happened. This allows unambiguous use of command names starting with '-'.
  \*/
  argv += 1;
  arg = *argv;
  /* But a "--" with no consequent arguments is the same as (argc < 2): */
  if(!arg)
  {
   return error_noArguments(arg0);
  }
 }
 
 /* ..the command to actually invoke is in "arg" if we're here. */
 
 /* Slide the start of argv past that argument. */
 argv += 1;
 
 execvp(arg, argv);
 /* If we're here, execvp failed to even execute the command. */
 
 return error_execFailure(arg, arg0);
}
