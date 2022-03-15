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
}EstadisticasComprobador;

char *leerPalabra(char * texto);
void fuerza_espera(unsigned long peso);
void construirSolucion(MPI_Datatype * MPI_Solucion);
void construirEstadisticas(MPI_Datatype * MPI_Estadisticas);
void construirEstadisticasComprobador(MPI_Datatype * MPI_Estadisticas);

/*
 * @param ncomp pista1/0
 * Dudas:	debo enviar longitud o longitud+1
 * 			Si hago malloc en 0 y envío esa cadena a un puntero de otro, ¿es la misma zona de memoria y no puedo modificar? 
 * */

	char *pista; //aquí va, su puta madre
int main(int argc, char ** argv)
{
	int id, nprocs;
	int tipo;
	int nintentos;
	int npistas;
	int ncomp;
	int micomp;
	int pistas;
	int mejora;
	long longitud;
	int flag;
	char *claro;
	char *intento;
	char *mejorSol;
	char palabra[MAX_PALABRA];
	double tInicio;
	double tInicioComprueba;
	double tInicioGenera;
	MPI_Request request;
	MPI_Status *estado;

	Solucion sol;
	Estadisticas est;
	Estadisticas *listaEst;
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

	listaEst = malloc(sizeof(Estadisticas)*(nprocs-1-ncomp));

	switch (id)	
	{
		case ES:
			//No va bien, ya lo arreglaré 
			claro = malloc(sizeof(char)*MAX_PALABRA);
			//printf("\nIntroduce la palabra: ");
			//fflush(stdout); //bien 
			//scanf("%s", claro);
			claro = leerPalabra("\nIntroduce la palabra a descubrir: ");
			//claro = (char*)malloc(sizeof(char)*12);
			//sprintf(claro,"MPIUSAL2022");

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
			puts("");

			//Inicio escuchas de descifrado	
			intento = malloc(sizeof(char)*longitud);
			estado = malloc(sizeof(MPI_Status));
			pista = malloc(sizeof(char)*longitud);
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
					//Quiero hacer bcast pero no sé como, porque hago probe y bcast no tiene etiqueta. Puede que se considerase una mala práctica de programación asumir que si el tag no coincide con el resto, es este envío.
					for (int i=1;i<nprocs;i++)
					{
						//Da igual lo que mande, solo leo la etiqueta
						MPI_Isend(&longitud, 1, MPI_INT, i, TAG_TERM, MPI_COMM_WORLD, &request);
					}	
					puts("");
					for (int i=(1+ncomp);i<nprocs;i++)
					{
						MPI_Probe(MPI_ANY_SOURCE, TAG_EST, MPI_COMM_WORLD, estado);
						MPI_Recv(&listaEst[(estado->MPI_SOURCE)-(1+ncomp)], 1, MPI_Estadisticas, estado->MPI_SOURCE, TAG_EST, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
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
				printf("SIN PISTAS");
			printf("\n============= Estadísticas Generadores ============\nProceso\tT_Total\tT_Generación\tT_Espera\tNcomprobaciones\tNpistas");
			for (int i=0; i<(nprocs-ncomp-1); i++)
			{
				printf("\n%d)\t%lf\t%lf\t%lf\t%d\t%d", listaEst[i].id, listaEst[i].tTotal, listaEst[i].tGenera, listaEst[i].tComprueba, listaEst[i].intentos, listaEst[i].pistas);
			}

			free(intento);
			free(estado);
			free(mejorSol);
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
					pista = malloc(sizeof(char)*longitud);
					intento = malloc(sizeof(char)*longitud);
					estado = malloc(sizeof(MPI_Status)); //Con malloc porque si no da error
					while (1)
					{
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
							sol.generador = estado->MPI_SOURCE;
							sprintf(sol.palabra, "%s", intento);
							MPI_Isend(&sol, 1, MPI_Solucion, ES, TAG_SOL, MPI_COMM_WORLD, &request);
							MPI_Isend(intento, longitud, MPI_CHAR, estado->MPI_SOURCE, TAG_INTENTO, MPI_COMM_WORLD, &request);
						}
						else if (estado->MPI_TAG==TAG_TERM){
							//Envíar estadísitcas y cerrar
							break;
						}else
							break;
					}
					break;
				case GENERADOR:
					tInicio = MPI_Wtime();
					//Espera longitud bloqueante
					MPI_Recv(&longitud, 1, MPI_LONG, ES, TAG_LONG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);	
					//Espera micomp
					MPI_Recv(&micomp, 1, MPI_INT, ES, TAG_COMP, MPI_COMM_WORLD, MPI_STATUS_IGNORE);	

					estado = malloc(sizeof(MPI_Status));
					intento = malloc(sizeof(char)*longitud);
			pista = malloc(sizeof(char)*longitud);
					//Cutre pero no me deja declar en incio de programa un puntero y usar malloc aquí, ni puta idea por qué

					for (int i=0;i<longitud;i++)
						intento[i]=CHAR_NF;
					intento[longitud-1]='\0';
					srand(id+time(0));
					npistas=0;
					nintentos=0;
					est.tGenera = 0;
					est.tComprueba = 0;

					//Inicio bucle descifrado
					while (1)
					{
						//Recorro una matriz de longitud palabra, si espacio en blanco: rand
						tInicioGenera = MPI_Wtime();
						for (int i=0;i<longitud-1;i++)	
						{
							while (intento[i]==CHAR_NF || intento[i]<CHAR_MIN || intento[i]>CHAR_MAX)
							{
								intento[i] = (rand()%(CHAR_MAX+1)+CHAR_MIN);//revisar, tengo sueño
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
							est.tComprueba = MPI_Wtime() - tInicioComprueba;
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
							//Enviar estadísitcas y salir
							est.tTotal = MPI_Wtime() - tInicio;
							est.id = id;
							est.intentos = nintentos;
							est.pistas = npistas;
							MPI_Isend(&est, 1, MPI_Estadisticas, ES, TAG_EST, MPI_COMM_WORLD, &request);
							break;
						}
					}
					break;
			}
			break;
	}
	
	MPI_Type_free(&MPI_Solucion);
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
	int longitudes[2];
	MPI_Datatype tipos[2] = { MPI_DOUBLE, MPI_DOUBLE };
	MPI_Aint desplazamiento[2];

	longitudes[0]=1;
	longitudes[1]=1;

	desplazamiento[0] = offsetof(Estadisticas, tTotal);
	desplazamiento[1] = offsetof(Estadisticas, tGenera);

	MPI_Type_create_struct(2, longitudes, desplazamiento, tipos, MPI_Estadisticas);
	MPI_Type_commit(MPI_Estadisticas);
}

char *leerPalabra(char * texto)
{
	char buffer[100];
	char *palabra;

	//Falta asegurarse que los caracteres introducidos están dentro del rango aceptado
	printf("%s", texto);
	fflush(stdout);
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
