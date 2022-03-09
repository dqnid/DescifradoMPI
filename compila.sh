#!/bin/bash
if test $# -ne 3; then
	echo "./compila.sh nprocs ncomp pista"
	exit 1
fi
mpicc Descifrado.c -o descifrado -lm
mpirun -np $1 descifrado $2 $3
