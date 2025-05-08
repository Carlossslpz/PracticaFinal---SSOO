#!/bin/bash

gcc banco.c -o banco -lpthread -lm
gcc usuario.c -o usuario -lpthread -lm
gcc monitor.c -o monitor -lpthread -lm
gcc init_cuenta.c -o init_cuenta -lpthread -lm
