#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <mpi.h>
#include <stddef.h>

#define MAX_PALABRA		101

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
#define TAG_SOL			16
#define TAG_PISTA		17

#define TAG_TERM		18
#define TAG_EST			19
#define TAG_ESTC		20

typedef struct solucion{
	int generador;
	char palabra[MAX_PALABRA];	
}Solucion;

typedef struct estadisticas{
	double tTotal;
	double tGenera; 
	double tComprueba;
	int intentos;
	int pistas;
	int id;
}Estadisticas;

typedef struct estadisticasComprobador{
	double tTotal;
	double tComprueba;
	int id;
}EstadisticasComprobador;

char *leerPalabra(char * texto);
void fuerza_espera(unsigned long peso);
void construirSolucion(MPI_Datatype * MPI_Solucion);
void construirEstadisticas(MPI_Datatype * MPI_Estadisticas);
void construirEstadisticasComprobador(MPI_Datatype * MPI_Estadisticas);

/*
 * El programa requiere de dos parámetros de entrada:
 *  - El número de comprobadores 
 *  - Un valor que indique si se ejecuta con o sin pistas (1/0)
 * @param ncomp pista1/0
 * */
int main(int argc, char ** argv)
{
	int id, nprocs, tipo, ncomp, micomp, pistas;
	int nintentos, npistas;
	int mejora, contadorEstC, contadorEst, flag;
	long longitud;
	double tInicio, tInicioComprueba, tInicioGenera;
	char *claro;
	char *intento;
	char *mejorSol;
	char pista[MAX_PALABRA];
	char palabra[MAX_PALABRA];

	MPI_Request request;
	MPI_Status *estado;

	Solucion sol;
	Estadisticas est;
	EstadisticasComprobador estC;
	Estadisticas *listaEst;
	EstadisticasComprobador *listaEstComp;
	MPI_Datatype MPI_Solucion;
	MPI_Datatype MPI_Estadisticas;
	MPI_Datatype MPI_EstadisticasComprobador;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &id);

	construirSolucion(&MPI_Solucion);
	construirEstadisticas(&MPI_Estadisticas);
	construirEstadisticasComprobador(&MPI_EstadisticasComprobador);

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
			listaEst = malloc(sizeof(Estadisticas)*(nprocs-1-ncomp));
			listaEstComp = malloc(sizeof(EstadisticasComprobador)*ncomp);
			claro = leerPalabra("\nIntroduce la palabra a encontrar ");

			printf("\nNÚMERO DE PROCESOS: Total %d: E/S: %d, Comprobadores: %d, Generadores: %d", 1, nprocs,ncomp, (nprocs-ncomp-1));

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
			intento = malloc(sizeof(char)*longitud);
			estado = malloc(sizeof(MPI_Status));
			mejorSol = malloc(sizeof(char)*longitud);

			for (int i=0;i<longitud;i++)
				mejorSol[i]=CHAR_NF;
			mejorSol[longitud-1]='\0';

			while (1)
			{
				MPI_Probe(MPI_ANY_SOURCE, TAG_SOL, MPI_COMM_WORLD, estado);
				MPI_Recv(&sol, 1, MPI_Solucion, estado->MPI_SOURCE, estado->MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				if (strcmp(sol.palabra, claro)==0)
				{
					for (int i=1;i<nprocs;i++)
					{
						MPI_Isend(&longitud, 1, MPI_INT, i, TAG_TERM, MPI_COMM_WORLD, &request);
					}	
					puts("");
					contadorEstC=0;
					contadorEst=0;

					for (int i=0; i<ncomp; i++)
					{
						MPI_Probe(MPI_ANY_SOURCE, TAG_ESTC, MPI_COMM_WORLD, estado);
						MPI_Recv(&listaEstComp[contadorEstC], 1, MPI_EstadisticasComprobador, estado->MPI_SOURCE, TAG_ESTC, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
							contadorEstC++;
							printf("\n%d) Fin comprobador", estado->MPI_SOURCE);
					}

					for (int i=(ncomp+1);i<nprocs;i++)
					{
						MPI_Probe(MPI_ANY_SOURCE, TAG_EST, MPI_COMM_WORLD, estado);
						MPI_Recv(&listaEst[contadorEst], 1, MPI_Estadisticas, estado->MPI_SOURCE, TAG_EST, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
						contadorEst++;
						printf("\n%d) Fin generador", estado->MPI_SOURCE);
					}
					break;
				}else
				{
					mejora = 0;
					for (int i=0; i<longitud-1; i++)
					{
						if (sol.palabra[i]!=CHAR_NF && mejorSol[i]==CHAR_NF)
						{
							mejorSol[i]=sol.palabra[i];
							mejora=1;
						}
					}
					if (mejora == 1)
					{
						if (pistas == 1)
						{
							for (int i=ncomp+1; i<nprocs; i++)
								MPI_Isend(mejorSol, longitud, MPI_CHAR, i, TAG_PISTA, MPI_COMM_WORLD, &request);					
							printf("\n%d) PISTA...: %s", sol.generador, mejorSol);
						}else{ printf("\n%d) SIN PISTA: %s", sol.generador, mejorSol); }
					}
				}
			}
			printf("\n\nTERMINADO - LA CONTRASEÑA ERA: %s\n", sol.palabra);
			
			printf("\n\nPROCESOS: %d\tCOMPROBADORES: %d\tGENERADORES: %d", nprocs, ncomp, (nprocs-ncomp-1));
			if (pistas==1)
				printf("\tCON PISTAS");
			else
				printf("\tSIN PISTAS");
			printf("\n=========================== Estadísticas Generadores =======================\nProceso\tT_Generación\tT_Espera\tT_Total\ttNcomprobaciones\tNpistas");
			for (int i=0; i<(nprocs-ncomp-1); i++)
			{
				printf("\n%d)\t%lf\t%lf\t%lf\t%d\t\t%d", listaEst[i].id, listaEst[i].tGenera, listaEst[i].tComprueba, listaEst[i].tTotal, listaEst[i].intentos, listaEst[i].pistas);
			}

			fflush(stdout);
			printf("\n== Estadísticas comprobadores ==\nProceso\tT_Comprueba\tT_Espera");
			for (int i=0; i<ncomp; i++)
				printf("\n%d)\t%lf\t%lf", listaEstComp[i].id, listaEstComp[i].tComprueba, listaEstComp[i].tTotal);

			puts("\n");
			free(listaEst);
			free(listaEstComp);
			free(intento);
			free(estado);
			free(mejorSol);
			free(claro);
			break;
		default:
			MPI_Recv(&tipo, 1, MPI_INT, ES, TAG_TIPO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			switch (tipo)
			{
				case COMPROBADOR:
					tInicio = MPI_Wtime();
					MPI_Recv(&longitud, 1, MPI_LONG, ES, TAG_LONG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);	
					claro = (char*)malloc(sizeof(char)*longitud);
					MPI_Recv(claro, longitud, MPI_CHAR, ES, TAG_CLARO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

					intento = malloc(sizeof(char)*longitud);
					estado = malloc(sizeof(MPI_Status));
					estC.tComprueba = 0;

					while (1)
					{
						MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, estado);
						if (estado->MPI_TAG==TAG_CON)
						{
							tInicioComprueba = MPI_Wtime();
							MPI_Recv(intento, longitud, MPI_CHAR, estado->MPI_SOURCE, estado->MPI_TAG, MPI_COMM_WORLD, estado);
							for (int i=0; i<longitud-1; i++)
							{
								if (intento[i]!=claro[i])
									intento[i]=CHAR_NF;
							}
							sol.generador = estado->MPI_SOURCE;
							sprintf(sol.palabra, "%s", intento);
							MPI_Isend(&sol, 1, MPI_Solucion, ES, TAG_SOL, MPI_COMM_WORLD, &request);
							MPI_Isend(intento, longitud, MPI_CHAR, estado->MPI_SOURCE, TAG_INTENTO, MPI_COMM_WORLD, &request);
							estC.tComprueba += MPI_Wtime() - tInicioComprueba;
						}
						else if (estado->MPI_TAG==TAG_TERM){
							estC.tTotal = MPI_Wtime() - tInicio;
							estC.id = id;
							MPI_Isend(&estC, 1, MPI_EstadisticasComprobador, ES, TAG_ESTC, MPI_COMM_WORLD, &request);
							break;
						}else
							break;
					}
					free(intento);
					free(estado);
					free(claro);
					break;
				case GENERADOR:
					tInicio = MPI_Wtime();
					MPI_Recv(&longitud, 1, MPI_LONG, ES, TAG_LONG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);	
					MPI_Recv(&micomp, 1, MPI_INT, ES, TAG_COMP, MPI_COMM_WORLD, MPI_STATUS_IGNORE);	

					estado = malloc(sizeof(MPI_Status));
					intento = malloc(sizeof(char)*longitud);

					for (int i=0;i<longitud;i++)
						intento[i]=CHAR_NF;
					intento[longitud-1]='\0';
					srand(id+time(0));
					npistas=0;
					nintentos=0;
					est.tGenera = 0;
					est.tComprueba = 0;

					while (1)
					{
						tInicioGenera = MPI_Wtime();
						for (int i=0;i<longitud-1;i++)	
						{
							while (intento[i]==CHAR_NF || intento[i]<CHAR_MIN || intento[i]>CHAR_MAX)
							{
								intento[i] = (rand()%(CHAR_MAX - CHAR_MIN +1)+CHAR_MIN);
							}
						}
						fuerza_espera(PESO_GENERAR);
						est.tGenera += MPI_Wtime() - tInicioGenera;
						tInicioComprueba = MPI_Wtime();
						MPI_Send(intento, longitud, MPI_CHAR, micomp, TAG_CON, MPI_COMM_WORLD);
						MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, estado);
						if (estado->MPI_TAG==TAG_INTENTO && estado->MPI_SOURCE == micomp)
						{
							MPI_Recv(intento, longitud, MPI_CHAR, micomp, TAG_INTENTO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);	
							est.tComprueba += MPI_Wtime() - tInicioComprueba;
							nintentos++;
						}else if (estado->MPI_TAG==TAG_PISTA)
						{
							MPI_Recv(pista, longitud, MPI_CHAR, ES, TAG_PISTA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);	
							npistas++;
							for (int i=0; i<longitud; i++)
							{
								if (pista[i]!=CHAR_NF)
									intento[i]=pista[i];
							}
						}else if (estado->MPI_TAG==TAG_TERM)
						{
							est.tTotal = MPI_Wtime() - tInicio;
							est.id = id;
							est.intentos = nintentos;
							est.pistas = npistas;
							MPI_Isend(&est, 1, MPI_Estadisticas, ES, TAG_EST, MPI_COMM_WORLD, &request);
							break;
						}
					}

					free(intento);
					free(estado);
					break;
			}
			break;
	}
	
	MPI_Type_free(&MPI_Solucion);
	MPI_Type_free(&MPI_Estadisticas);
	MPI_Type_free(&MPI_EstadisticasComprobador);
	MPI_Finalize();
	return 0;
}

void construirSolucion(MPI_Datatype * MPI_Solucion)
{
	int longitudes[2];
	MPI_Datatype tipos[2] = { MPI_INT, MPI_CHAR };
	MPI_Aint desplazamiento[2];

	longitudes[0]=1;
	longitudes[1]=MAX_PALABRA;

	desplazamiento[0] = offsetof(Solucion, generador);
	desplazamiento[1] = offsetof(Solucion, palabra);

	MPI_Type_create_struct(2, longitudes, desplazamiento, tipos, MPI_Solucion);
	MPI_Type_commit(MPI_Solucion);
}

void construirEstadisticas(MPI_Datatype * MPI_Estadisticas)
{
	int longitudes[6];
	MPI_Datatype tipos[6] = { MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE,MPI_INT, MPI_INT, MPI_INT };
	MPI_Aint desplazamiento[6];

	longitudes[0]=1;
	longitudes[1]=1;
	longitudes[2]=1;
	longitudes[3]=1;
	longitudes[4]=1;
	longitudes[5]=1;

	desplazamiento[0] = offsetof(Estadisticas, tTotal);
	desplazamiento[1] = offsetof(Estadisticas, tGenera);
	desplazamiento[2] = offsetof(Estadisticas, tComprueba);
	desplazamiento[3] = offsetof(Estadisticas, intentos);
	desplazamiento[4] = offsetof(Estadisticas, pistas);
	desplazamiento[5] = offsetof(Estadisticas, id);

	MPI_Type_create_struct(6, longitudes, desplazamiento, tipos, MPI_Estadisticas);
	MPI_Type_commit(MPI_Estadisticas);
}

void construirEstadisticasComprobador(MPI_Datatype * MPI_Estadisticas)
{
	int longitudes[3];
	MPI_Datatype tipos[3] = { MPI_DOUBLE, MPI_DOUBLE, MPI_INT };
	MPI_Aint desplazamiento[3];

	longitudes[0]=1;
	longitudes[1]=1;
	longitudes[2]=1;

	desplazamiento[0] = offsetof(EstadisticasComprobador, tTotal);
	desplazamiento[1] = offsetof(EstadisticasComprobador, tComprueba);
	desplazamiento[2] = offsetof(EstadisticasComprobador, id);

	MPI_Type_create_struct(3, longitudes, desplazamiento, tipos, MPI_Estadisticas);
	MPI_Type_commit(MPI_Estadisticas);
}

char *leerPalabra(char * texto)
{
	char buffer[MAX_PALABRA];
	char *palabra;
	char prueba[] = "MPIUSAL2021GRUPOA1a16deMarzode2022ConUnCodigoEscritoEnCYUnaPalabraLargaConNumerosAleatorios087831231";

	printf("%s [INTRO para cadena de prueba]: ", texto);
	fflush(stdout);
	scanf("%100[^\n]", buffer);

	if (buffer[0]=='|' || buffer[0]=='\0' || buffer[0]=='\n')
	{
		palabra = (char*)malloc(strlen(prueba));
		if (palabra == NULL){ return NULL; }
		strcpy(palabra,prueba);

	}else
	{
		palabra = (char*)malloc(strlen(buffer)+1);
		if (palabra == NULL){ return NULL; }
		strcpy(palabra,buffer);
	}
	return palabra;
	
}

void fuerza_espera(unsigned long peso)
{
	for (unsigned long i=1; i<1*peso; i++) sqrt(i);
}
