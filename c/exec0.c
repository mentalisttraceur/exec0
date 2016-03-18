/*****************************************************************************\
 * exec0 1.0.0 - C implementation
 * Copyright (C) 2016-03-17 Alexander Kozhevnikov <mentalisttraceur@gmail.com>
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


char const stdoutWritingError[] = "exec0: error writing to stdout: ";
char const msgHeader[] = "exec0: ";
char const colonSpaceSplit[] = ": ";
char const noArgumentsGiven[] = "exec0: need command or option argument\n";
char const newline = '\n';

char const helpText[] =
 "Usage: exec0 OPTION|COMMAND [NAME [ARGUMENT]...]\n"
 "\n"
 "Execute a command, specifying all arguments to pass to it, include the\n"
 "name the command sees itself invoked as (the \"zeroth\" argument).\n"
 "\n"
 "  -h, --help    Print this help text and exit.\n"
;


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
void writeErrorMsgOfAnySize (struct iovec * msg, unsigned int msgPartsToWrite)
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


/* The actions to take in responce to explicitly requested "--help". */
static
int help()
{
 /* If writing the help text fails, report the error and exit accordingly. */
 int err = writeUntilError(STDOUT_FILENO, helpText, sizeof(helpText) - 1);
 if(err)
 {
  /* Get error string, the compose and print the error message with it. */
  char * errStr = strerror(err);
  struct iovec errMsg[3];
  errMsg[0].iov_base = (void * )stdoutWritingError;
  errMsg[0].iov_len = sizeof(stdoutWritingError) - 1;
  errMsg[1].iov_base = errStr;
  errMsg[1].iov_len = strlen(errStr);
  errMsg[2].iov_base = (void * )&newline;
  errMsg[2].iov_len = 1;
  writev(STDERR_FILENO, errMsg, 3);
  return EXIT_FAILURE;
 }
 /* Successfully printed help info; exit accordingly. */
 return EXIT_SUCCESS;
}


/* The action taken when there's no actionable arguments given. */
static
int error_noArguments()
{
 /* Construct error message including help text. */
 struct iovec errMsg[2];
 errMsg[0].iov_base = (void * )noArgumentsGiven;
 errMsg[0].iov_len = sizeof(noArgumentsGiven) - 1;
 errMsg[1].iov_base = (void * )helpText;
 errMsg[1].iov_len = sizeof(helpText) - 1;
 writev(STDERR_FILENO, errMsg, 2);
 return EXIT_FAILURE;
}


/* The action taken when executing the given command fails. */
static
int error_execFailure(char * command)
{
 char * errStr = strerror(errno);
 struct iovec errMsg[5];
 errMsg[0].iov_base = (void * )msgHeader;
 errMsg[0].iov_len = sizeof(msgHeader) - 1;
 errMsg[1].iov_base = command;
 errMsg[1].iov_len = strlen(command);
 errMsg[2].iov_base = (void * )colonSpaceSplit;
 errMsg[2].iov_len = sizeof(colonSpaceSplit) - 1;
 errMsg[3].iov_base = errStr;
 errMsg[3].iov_len = strlen(errStr);
 errMsg[4].iov_base = (void * )&newline;
 errMsg[4].iov_len = 1;
 writeErrorMsgOfAnySize(errMsg, 5);
 return EXIT_FAILURE;
}


int main(int argc, char * * argv)
{
 char * arg;
 
 /*\
 There must be at least one argument (so two, counting argv[0]), to know what
 command to execute.
 \*/
 if(argc < 2)
 {
  return error_noArguments();
 }
 
 /* We slide the start of argv past argv[0], because argv[0] is unused. */
 argv += 1;
 
 /* And inspect the next argument, which is either... */
 arg = *argv;
 
 /* ..the help-printing option: */
 if(!strcmp(arg, "-h") || !strcmp(arg, "--help"))
 {
  return help();
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
   return error_noArguments();
  }
 }
 
 /* ..the command to actually invoke is in "arg" if we're here. */
 
 /* Slide the start of argv past that argument. */
 argv += 1;
 
 execvp(arg, argv);
 /* If we're here, execvp failed to even execute the command. */
 
 return error_execFailure(arg);
}
