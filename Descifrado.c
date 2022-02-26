#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>

#define CHAR_NF		32
#define CHAR_MAX 	127
#define CHAR_MIN 	33
#define COMPROBADOR 1
#define GENERADOR	2

char *leerPalabra(char *texto);

int main(int argc, char ** argv)
{
	int id, nprocs;
	int tipo;
	char *claro;
	MPI_Request request;

	MPI_Init(&argc, &argv);

	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &id);


	switch (id)	
	{
		case 0:
			//Enviar tipo a todos
			for (int i=1;i<nprocs;i++)
			{
				if (i%2==0){ tipo = COMPROBADOR; } else { tipo = GENERADOR; }
				printf("\nEnvío mensaje a %d", tipo);
				MPI_Isend(&tipo, 1, MPI_INT, i, 10, MPI_COMM_WORLD, &request);
			}	
			//Enviar palabra a comprobadores
			//Enviar longitud palabra a todos
			//Enviar id comprobador a generador (esto desata el descifrado)
			break;
		default:
			//Espera mensaje de 0 con su tipo, recv bloqueante
			MPI_Recv(&tipo, 1, MPI_INT, 0, 10, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			switch (tipo)
			{
				case COMPROBADOR:
					//Espera palabra bloqueante
					if (tipo == COMPROBADOR) {printf("\nSoy un comprobador");}
					//Inicio bucle de escucha
					break;
				case GENERADOR:
					//Espera longitud bloqueante
					if (tipo == GENERADOR) {printf("\nSoy un generador");}
					//Inicio bucle descifrado
					break;
			}
			break;
	}
	
	MPI_Finalize();
	return 0;
}

char *leerPalabra(char *texto)
{
	char buffer[100];
	char *palabra;

	printf("\n%s", texto);
	scanf("%s", buffer);
	palabra = (char*)malloc(strlen(buffer)+1);
	if (palabra == NULL){ return NULL; }
	strcpy(palabra,buffer);
	return palabra;
}
