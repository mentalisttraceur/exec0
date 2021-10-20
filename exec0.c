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

char const help_text[] =
    "Execute a command with an arbitrary argument array, including the\n"
    "\"zeroth\" argument - the name the command sees itself invoked as.\n"
    "\n"
    "Usage:\n"
    "    exec0 <command> [<name> [<argument>]...]\n"
    "    exec0 (--help | --version)\n"
    "\n"
    "    -h --help     show this help text\n"
    "    -V --version  show version information\n"
;


static
int error_need_command(char * arg0)
{
    if(fputs(arg0, stderr) != EOF)
    {
        fputs(": need command argument\n", stderr);
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
    if(fputs(help_text, stdout) != EOF
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
        return error_need_command(arg0);
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
            return error_need_command(arg0);
        }
    }

    /* Now arg should be the command to execute. */

    /* Shift argv so that the next argument is used as its zeroth argument. */
    argv += 1;

    execvp(arg, argv);
    /* If we're here, execvp failed to execute the command. */

    return error_executing_command(arg, arg0);
}
