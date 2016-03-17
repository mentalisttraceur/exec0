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

No error-checking is done because if writing the error message fails, what's
next? Try to report an error about how we can't report errors? (We could set a
different exitcode, but we already set it to indicate failure if we're printing
an error, so at the moment that seems to be low-value).
\*/
void writeErrorMsgOfAnySize(struct iovec * msgParts, size_t msgPartsCount)
{
 /*\
 We need to track how much data is left to write across iterations, and when at
 that limit, we need to briefly hold the difference/remainder in how much a
 message part has left and how much we can still fit in a write. Using a union
 lets us use one temporary ssize_t variable, but two meaningful names for it.
 \*/
 union
 {
  ssize_t spaceLeft;
  ssize_t remainder;
 }
 temp;
 
 /* At the beginning, we have the full maximum space available for a writev: */
 temp.spaceLeft = SSIZE_MAX;
 
 unsigned int msgParts_i = 0;
 while(msgParts_i < msgPartsCount)
 {
  size_t msgPartSize = msgParts[msgParts_i].iov_len;
  if(msgPartSize >= temp.spaceLeft)
  {
   /* Reached limit of one writev. Compute remaining length in current part: */
   temp.remainder = msgPartSize - temp.spaceLeft;
   
   /* Set the current part length to the amount that fits, and do the write: */
   msgPartSize -= temp.remainder;
   msgParts[msgParts_i].iov_len = msgPartSize;
   if(writev(STDERR_FILENO, msgParts, msgParts_i + 1) == -1)
   {
    /* If a write error happened, just bail, no point writing more. */
    return;
   }
   
   /* If there's any remainder unwritten in this message part... */
   if(temp.remainder)
   {
    /* ..adjust the pointer forward by the amount we just printed: */
    msgParts[msgParts_i].iov_base
    = (char * )(msgParts[msgParts_i].iov_base) + msgPartSize;
    /* ..and set the size to that of portion remaining to print: */
    msgParts[msgParts_i].iov_len = temp.remainder;
   }
   else /* !temp.remainder */
   {
    /* Increment index past this message part because it's "used up". */
    msgParts_i += 1;
   }
   /* Reset spaceLeft back to the (full) remaining max length for writev */
   temp.spaceLeft = SSIZE_MAX;
   /* Adjust message parts variables to point past the fully-written parts. */
   msgParts += msgParts_i;
   msgPartsCount -= msgParts_i;
   /* Index has to be reset too, since we just incremented msgParts */
   msgParts_i = 0;
  }
  else /* msgPartSize < temp.spaceLeft */
  {
   /* In the normal case we just increment the index... */
   msgParts_i += 1;
   /* ..and decrement the space left */
   temp.spaceLeft -= msgPartSize;
  }
 }
 
 /* If there's any unwritten parts after the loop, do the final write. */
 if(msgPartsCount)
 {
  writev(STDERR_FILENO, msgParts, msgPartsCount);
 }
}


/* The actions to take in responce to explicitly requested "--help". */
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
 /* if we're here, execvp failed to even execute the command */
 
 return error_execFailure(arg);
}