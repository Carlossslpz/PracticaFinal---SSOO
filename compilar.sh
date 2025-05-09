#!/bin/bash

gcc banco.c -o banco -lpthread -lm -pthread
gcc usuario.c -o usuario -lpthread -lm -pthread
gcc monitor.c -o monitor -lpthread -lm -pthread
gcc init_cuenta.c -o init_cuenta -lpthread -lm -pthread
