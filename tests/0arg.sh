#!/bin/sh
./exec0
./exec0 --
./exec0 ./exec0
./exec0 ./exec0 q
./exec0 ./exec0 q --
./exec0 w
./exec0 -- --
./exec0 ./exec0 q w
./exec0 --help
./exec0 ./exec0 execFOO --help
./exec0 --version
./exec0 ./exec0 a/b/c/execFOO --version
