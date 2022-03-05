#!/bin/bash
if test $# -ne 2; then
	echo "./compila.sh nprocs ncomp"
	exit 1
fi
mpicc Descifrado.c -o descifrado -lm
mpirun -np $1 -oversubscribe descifrado $2 1
