#!/bin/bash

gcc -pedantic -Og -ggdb -std=c99 -Wall -Werror -Wextra -fdiagnostics-color=always ./invalid.c ./client-kerberos.c ./client-sockets.c ./shared.c -o bin/invalid -lgssapi_krb5  && ./bin/invalid ; echo $?
