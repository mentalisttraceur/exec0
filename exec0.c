/*****************************************************************************\
 * Copyright 2016, 2019 Alexander Kozhevnikov <mentalisttraceur@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
\*****************************************************************************/

/* Standard C library headers */
#include <stdio.h> /* EOF, fputc, fputs, perror, stderr, stdout */
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include <string.h> /* strcmp */

/* Standard UNIX/Linux (POSIX/SUSv3 base) headers */
#include <unistd.h> /* execvp */


char const version_text[] = "exec0 1.1.0\n";
char const no_arguments_given[] = ": need command or option argument\n";
char const unrecognized_option[] = ": unrecognized option: ";
char const colon_space[] = ": ";
char const newline = '\n';

char const help_text_prefix[] = "Usage: ";
char const help_text[] =
    " OPTION|COMMAND [NAME [ARGUMENT]...]\n"
    "\n"
    "Execute a command, specifying all arguments to pass to it, including\n"
    "the name the command sees itself invoked as (the \"zeroth\" argument).\n"
    "\n"
    "  -h, --help    Print this help text and exit.\n"
    "  -V, --version Print version information and exit.\n"
;


static
int error_no_arguments(char * arg0)
{
    if(fputs(arg0, stderr) == EOF)
    {
        return EXIT_FAILURE;
    }
    fputs(no_arguments_given, stderr);
    return EXIT_FAILURE;
}


static
int error_unrecognized_option(char * option, char * arg0)
{
    if(fputs(arg0, stderr) == EOF)
    {
        return EXIT_FAILURE;
    }
    if(fputs(unrecognized_option, stderr) == EOF)
    {
        return EXIT_FAILURE;
    }
    if(fputs(option, stderr) == EOF)
    {
        return EXIT_FAILURE;
    }
    fputc('\n', stderr);
    return EXIT_FAILURE;
}


static
int error_exec(char * command, char * arg0)
{
    if(fputs(arg0, stderr) == EOF)
    {
        return EXIT_FAILURE;
    }
    if(fputs(colon_space, stderr) == EOF)
    {
        return EXIT_FAILURE;
    }
    perror(command);
    return EXIT_FAILURE;
}


static
int print_help(char * arg0)
{
    if(fputs(help_text_prefix, stdout) == EOF
    || fputs(arg0, stdout) == EOF
    || fputs(help_text, stdout) == EOF
    || fflush(stdout) == EOF)
    {
        perror(arg0);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


static
int print_version(char * arg0)
{
    if(fputs(version_text, stdout) == EOF
    || fflush(stdout) == EOF)
    {
        perror(arg0);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


int main(int argc, char * * argv)
{
    char * arg;
    char * arg0 = *argv;
 
    /* Need at least one argument (two, counting argv[0]): */
    if(argc < 2)
    {
        /* If argc is 0, then arg0 is null: make it an empty string instead: */
        if(!arg0)
        {
            arg0 = "";
        }
        return error_no_arguments(arg0);
    }
 
    /* Shift argv past argv[0], to argv[1]... */
    argv += 1;
 
    /* ..and inspect the next argument, which is either... */
    arg = *argv;
 
    if(*arg == '-')
    {
        arg += 1;
        /* ..the help-printing option: */
        if(!strcmp(arg, "-help") || !strcmp(arg, "h"))
        {
            return print_help(arg0);
        }
        /* .. the version printing option: */
        if(!strcmp(arg, "-version") || !strcmp(arg, "V"))
        {
            return print_version(arg0);
        }
  
        /* .. or *not* the "end of options" ("--") argument: */
        if(strcmp(arg, "-"))
        {
            return error_unrecognized_option(arg - 1, arg0);
        }
  
        /*\
        ..or the "end of options" argument, in which case
        just skip it and continue the logic. This allows
        unambiguous use of command names starting with '-'.
        \*/
        argv += 1;
        arg = *argv;
        /* But a "--" with no arguments after it is the same as (argc < 2): */
        if(!arg)
        {
            return error_no_arguments(arg0);
        }
    }
 
    /* ..the command to actually invoke is in "arg" if we're here. */
 
    /* Shift the start of argv past that argument. */
    argv += 1;
 
    execvp(arg, argv);
    /* If we're here, execvp failed to even execute the command. */
 
    return error_exec(arg, arg0);
}
