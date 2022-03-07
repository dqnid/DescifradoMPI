#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <mpi.h>

#define CHAR_NF			32
#define CHAR_MAX 		127
#define CHAR_MIN 		33
#define PESO_COMPROBAR	5000000
#define PESO_GENERAR	10000000
#define ES				0
#define COMPROBADOR 	1
#define GENERADOR		2
#define TAG_TIPO		10
#define TAG_CLARO		11
#define TAG_LONG		12
#define TAG_COMP		13
#define TAG_CON			14

char *leerPalabra(char *texto);
void fuerza_espera(unsigned long peso);

/*
 * @param ncomp pista1/0
 * Dudas:	debo enviar longitud o longitud+1
 * 			Si hago malloc en 0 y envío esa cadena a un puntero de otro, ¿es la misma zona de memoria y no puedo modificar? 
 * */

int main(int argc, char ** argv)
{
	int id, nprocs;
	int tipo;
	int ncomp;
	int micomp;
	int pistas;
	long longitud;
	char *claro;
	MPI_Request request;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &id);

	if (argc!=3)
	{
		if (id==0) { printf("\nError en los parámetros de ejecución.\nDebe seguir el siguiente formato:\nmpirun -np X descifrado numComprobadores PistasSioNo(1/0)\nEjemplo:\nmpirun -np 7 descifrado 2 1\n"); }
		MPI_Finalize();
		return 1;	
	} 

	ncomp = atoi(argv[1]);
	pistas = atoi(argv[2]);	

	if ((nprocs-ncomp-1)<ncomp){
		if (id == 0) { printf("Número de procesos incorrecto\nNo puede haber más comprobadores que generadores\n"); }
		MPI_Finalize();
		return 1;	
	}

	switch (id)	
	{
		case 0:
			//claro = leerPalabra("Introduce la palabra clara: ");
			claro = (char*)malloc(sizeof(char)*5);
			sprintf(claro,"algo");

			printf("\nNÚMERO DE PROCESOS: Total %d: E/S: %d, Comprobadores: %d, Generadores: %d", 1, nprocs,ncomp, (nprocs-ncomp-1));

			//Enviar tipo a todos
			printf("\n\nNotificación tipo:");
			for (int i=1;i<nprocs;i++)
			{
				if (i<=ncomp){ tipo = COMPROBADOR; } else { tipo = GENERADOR; }
				printf("\n%d) ", i);
				switch (tipo)
				{
					case COMPROBADOR:
						printf("COMPROBADOR");
						break;
					case GENERADOR:
						printf("GENERADOR");
						break;
				}
				MPI_Isend(&tipo, 1, MPI_INT, i, TAG_TIPO, MPI_COMM_WORLD, &request);
			}	
			//Enviar palabra a comprobadores
			printf("\n\nNotificación palabra a los comprobadores y tamaño a comprobadores y generadores.");
			for (int i=1;i<nprocs;i++)
			{
				longitud = strlen(claro);
				MPI_Isend(&longitud, 1, MPI_INT, i, TAG_LONG, MPI_COMM_WORLD, &request);
				if (i<=ncomp) 
				{ 
					printf("\n%d) %s, %ld", id, claro, longitud);
					MPI_Isend(&claro, longitud+1, MPI_CHAR, i, TAG_CLARO, MPI_COMM_WORLD, &request); 
				}
			}
			//Enviar id comprobador a generador (esto desata el descifrado)
			printf("\n\nNotificación a los generadores de su comprobador asociado.");
			micomp = 1;
			for (int i=ncomp+1; i<nprocs; i++)
			{
				printf("\n%d genera para %d", i, micomp);
				MPI_Isend(&micomp, 1, MPI_INT, i, TAG_COMP, MPI_COMM_WORLD, &request);				
				if (micomp >= ncomp) { micomp = 1; }
				else { micomp++; }
			}
			puts("");
			//Inicio escuchas de descifrado
			free(claro);
			break;
		default:
			//Espera mensaje de 0 con su tipo, recv bloqueante
			MPI_Recv(&tipo, 1, MPI_INT, ES, TAG_TIPO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			switch (tipo)
			{
				case COMPROBADOR:
					//Espera longitud y palabra bloqueante
					MPI_Recv(&longitud, 1, MPI_LONG, ES, TAG_LONG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);	
					MPI_Recv(&claro, longitud+1, MPI_CHAR, ES, TAG_CLARO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
					//Inicio bucle de escucha
					break;
				case GENERADOR:
					//Espera longitud bloqueante
					MPI_Recv(&longitud, 1, MPI_LONG, ES, TAG_LONG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);	
					//Espera micomp
					MPI_Recv(&micomp, 1, MPI_INT, ES, TAG_COMP, MPI_COMM_WORLD, MPI_STATUS_IGNORE);	
					//Inicio bucle descifrado
					/*
					char intento[longitud+1];
					for (int i=0;i<longitud;i++)
						intento[i]=CHAR_NF;
					intento[longitud]='\0';
					srand(id+time(0));
					//Recorro una matriz de longitud palabra, si espacio en blanco: rand
					while (1)
					{
						for (int i=0;i<longitud;i++)	
						{
							if (intento[i]==CHAR_NF)
							{
								intento[i] = (rand()%(CHAR_MAX-CHAR_MIN+1)+CHAR_MIN);//revisar, tengo sueño
							}
						}
						//Espero para dar peso al cálculo
						fuerza_espera(PESO_GENERAR);
						MPI_Send(intento, longitud, MPI_CHAR, micomp, TAG_CON, MPI_COMM_WORLD);
					}
					*/
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

void fuerza_espera(unsigned long peso)
{
	for (unsigned long i=1; i<1*peso; i++) sqrt(i);
}
