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
#define TAG_INTENTO		15
#define TAG_PISTA		16
#define TAG_TERM		17

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
	int mejora;
	long longitud;
	char *claro;
	char *intento;
	char *mejorSol;
	MPI_Request request;
	MPI_Status *estado;

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
		case ES:
			//claro = leerPalabra("Introduce la palabra clara: ");
			claro = (char*)malloc(sizeof(char)*12);
			sprintf(claro,"MPIUSAL2022");

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
			longitud = strlen(claro) + 1;
			for (int i=1;i<nprocs;i++)
			{
				MPI_Send(&longitud, 1, MPI_INT, i, TAG_LONG, MPI_COMM_WORLD);
				if (i<=ncomp) 
				{ 
					printf("\n%d) %s, %ld", id, claro, longitud);
					MPI_Send(claro, longitud, MPI_CHAR, i, TAG_CLARO, MPI_COMM_WORLD);
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

			//Inicio escuchas de descifrado	
			intento = malloc(sizeof(char)*longitud);
			estado = malloc(sizeof(MPI_Status));
			mejorSol = malloc(sizeof(char)*longitud);

			for (int i=0;i<longitud;i++)
				mejorSol[i]=CHAR_NF;
			MPI_Probe(MPI_ANY_SOURCE, TAG_INTENTO, MPI_COMM_WORLD, estado);
			MPI_Recv(intento, longitud, MPI_CHAR, estado->MPI_SOURCE, estado->MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			if (strcmp(intento, claro)==0)
			{
				//Envío fin a todos, espero estadísiticas y cierro, pero no hago aquí el MPI_Finalize
				break;
			}else
			{
				mejora = 0;
				for (int i=0; i<longitud-1; i++)
				{
					if (intento[i]!=CHAR_NF && mejorSol[i]==CHAR_NF)
					{
						mejorSol[i]=intento[i];
						mejora=1;
					}
				}
				if (mejora == 1)
				{
					if (pistas == 1)
					{
						for (int i=ncomp+1; i<nprocs; i++)
							MPI_Isend(intento, longitud, MPI_CHAR, i, TAG_PISTA, MPI_COMM_WORLD, &request);					
						printf("\n%d) PISTA...: %s", estado->MPI_SOURCE, intento);
					}else{ printf("\n%d) SIN PISTA: %s", estado->MPI_SOURCE, intento); }
				}
			}
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
					claro = (char*)malloc(sizeof(char)*longitud);
					MPI_Recv(claro, longitud, MPI_CHAR, ES, TAG_CLARO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
					//Inicio bucle de escucha
					intento = malloc(sizeof(char)*longitud);
					estado = malloc(sizeof(MPI_Status)); //Con malloc porque si no da error
					//MPI_Recv(intento, longitud, MPI_CHAR, MPI_ANY_SOURCE, TAG_CON, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
					MPI_Probe(MPI_ANY_SOURCE, TAG_CON, MPI_COMM_WORLD, estado);
					//Inicio bucle
					//MPI_Recv(intento, longitud, MPI_CHAR, estado->MPI_SOURCE, estado->MPI_TAG, MPI_COMM_WORLD, estado);
					//printf("\nTengo el claro: %s de long: %ld y llegó un intento: %s\n", claro, longitud, intento);
					
					/*
					 * Primero Iprobe para ver quien envía mensaje, y actuar en función a eso:
					 * * Si 0 envía termina: envía estadísiticas y cierra
					 * * Si generadores envían palabra: responder 
					 * */
					intento = malloc(sizeof(char)*longitud);
					estado = malloc(sizeof(MPI_Status));
					MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, estado);
					if (estado->MPI_TAG==TAG_CON)
					{
						MPI_Recv(intento, longitud, MPI_CHAR, estado->MPI_SOURCE, estado->MPI_TAG, MPI_COMM_WORLD, estado);
						//compruebo la construcción de la palabra, intento se debe sobreescribir, solo ES deberá tener la cadena con todas las pistas
						for (int i=0; i<longitud-1; i++)
						{
							if (intento[i]!=claro[i])
								intento[i]=CHAR_NF;

						}
						//Envío palabra posiciones "corrección" a 0 y al generador
						//MAL al ES le tiene que mandar una estructura con la cadena y el identificador del proceso que ha hecho el intento. 
						MPI_Isend(intento, longitud, MPI_CHAR, 0, TAG_INTENTO, MPI_COMM_WORLD, &request);
						MPI_Isend(intento, longitud, MPI_CHAR, estado->MPI_SOURCE, TAG_INTENTO, MPI_COMM_WORLD, &request);
					}
					else if (estado->MPI_TAG==TAG_TERM){
						//Envíar estadísitcas y cerrar
						break;
					}else
						break;
					break;
				case GENERADOR:
					//Espera longitud bloqueante
					MPI_Recv(&longitud, 1, MPI_LONG, ES, TAG_LONG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);	
					//Espera micomp
					MPI_Recv(&micomp, 1, MPI_INT, ES, TAG_COMP, MPI_COMM_WORLD, MPI_STATUS_IGNORE);	

					estado = malloc(sizeof(MPI_Status));
					intento = malloc(sizeof(char)*longitud);
					//Cutre pero no me deja declar en incio de programa un puntero y usar malloc aquí
					char pista[longitud];

					for (int i=0;i<longitud;i++)
						intento[i]=CHAR_NF;
					intento[longitud-1]='\0';
					srand(id+time(0));

					//Inicio bucle descifrado
						//Recorro una matriz de longitud palabra, si espacio en blanco: rand
						for (int i=0;i<longitud-1;i++)	
						{
							while (intento[i]==CHAR_NF || intento[i]<CHAR_MIN || intento[i]>CHAR_MAX)
							{
								intento[i] = (rand()%(CHAR_MAX+1)+CHAR_MIN);//revisar, tengo sueño
							}
						}
						fuerza_espera(PESO_GENERAR);
						MPI_Send(intento, longitud, MPI_CHAR, micomp, TAG_CON, MPI_COMM_WORLD);
						MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, estado);
							if (estado->MPI_TAG==TAG_INTENTO && estado->MPI_SOURCE == micomp)
							{
								MPI_Recv(intento, longitud, MPI_CHAR, micomp, TAG_INTENTO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);	
							}else if (estado->MPI_TAG==TAG_PISTA)
							{
								MPI_Recv(pista, longitud, MPI_CHAR, 0, TAG_PISTA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);	
								for (int i=0; i<longitud; i++)
								{
									if (pista[i]!=CHAR_NF)
										intento[i]=pista[i];
								}
							}else if (estado->MPI_TAG==TAG_TERM)
							{
								//Enviar estadísitcas y salir
								break;
							}


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
