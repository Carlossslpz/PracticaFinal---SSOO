#!/bin/bash

gcc banco.c -o banco -lpthread
gcc usuario.c -o usuario -lpthread
gcc init_cuenta.c -o init_cuenta -lpthread
gcc monitor.c -o monitor -lpthread