/* SPDX-License-Identifier: 0BSD */
/* Copyright 2016 Alexander Kozhevnikov <mentalisttraceur@gmail.com> */

/* Standard C library headers */
#include <errno.h> /* errno */
#include <stdio.h> /* EOF, fputc, fputs, perror, stderr, stdout */
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include <string.h> /* strcmp */

/* Standard UNIX/Linux (POSIX/SUS base) headers */
#include <unistd.h> /* execvp */


char const version_text[] = "exec0 1.1.1\n";

char const help_text_prefix[] = "Usage: ";
char const help_text[] =
    " OPTION|COMMAND [NAME [ARGUMENT]...]\n"
    "\n"
    "Execute a command, controlling all arguments passed to it, including\n"
    "the name the command sees itself invoked as (the \"zeroth\" argument).\n"
    "\n"
    "  -h, --help    Print this help text and exit.\n"
    "  -V, --version Print version information and exit.\n"
;


static
int error_no_arguments(char * arg0)
{
    if(fputs(arg0, stderr) != EOF)
    {
        fputs(": need command or option argument\n", stderr);
    }
    return EXIT_FAILURE;
}


static
int error_bad_option(char * option, char * arg0)
{
    if(fputs(arg0, stderr) != EOF
    && fputs(": bad option: ", stderr) != EOF
    && fputs(option, stderr) != EOF)
    {
        fputc('\n', stderr);
    }
    return EXIT_FAILURE;
}


static
int error_writing_output(char * arg0)
{
    int errno_ = errno;
    if(fputs(arg0, stderr) != EOF)
    {
        errno = errno_;
        perror(": error writing output");
    }
    return EXIT_FAILURE;
}


static
int error_executing_command(char * command, char * arg0)
{
    int errno_ = errno;
    if(fputs(arg0, stderr) != EOF
    && fputs(": error executing command: ", stderr) != EOF)
    {
        errno = errno_;
        perror(command);
    }
    return EXIT_FAILURE;
}


static
int print_help(char * arg0)
{
    if(fputs(help_text_prefix, stdout) != EOF
    && fputs(arg0, stdout) != EOF
    && fputs(help_text, stdout) != EOF
    && fflush(stdout) != EOF)
    {
        return EXIT_SUCCESS;
    }
    return error_writing_output(arg0);
}


static
int print_version(char * arg0)
{
    if(fputs(version_text, stdout) != EOF
    && fflush(stdout) != EOF)
    {
        return EXIT_SUCCESS;
    }
    return error_writing_output(arg0);
}


int main(int argc, char * * argv)
{
    char * arg;
    char * arg0 = *argv;

    /* Need at least one argument (two, counting argv[0]): */
    if(argc < 2)
    {
        /* Many systems allow execution without even the zeroth argument: */
        if(!arg0)
        {
            arg0 = "";
        }
        return error_no_arguments(arg0);
    }

    /* The goal is to shift argv until it points to the command to execute: */
    argv += 1;

    /* First argument is either an option (starts with '-') or a command: */
    arg = *argv;

    if(*arg == '-')
    {
        arg += 1;
        if(!strcmp(arg, "-help") || !strcmp(arg, "h"))
        {
            return print_help(arg0);
        }
        if(!strcmp(arg, "-version") || !strcmp(arg, "V"))
        {
            return print_version(arg0);
        }

        /* If it is *not* the "end of options" ("--") "option": */
        if(strcmp(arg, "-"))
        {
            return error_bad_option(arg - 1, arg0);
        }

        /* The "--" is just skipped, allowing the command to start with '-'. */
        argv += 1;
        arg = *argv;
        /* But a "--" with no arguments after it is the same as (argc < 2): */
        if(!arg)
        {
            return error_no_arguments(arg0);
        }
    }

    /* Now arg should be the command to execute. */

    /* Shift argv so that the next argument is used as its zeroth argument. */
    argv += 1;

    execvp(arg, argv);
    /* If we're here, execvp failed to execute the command. */

    return error_executing_command(arg, arg0);
}
