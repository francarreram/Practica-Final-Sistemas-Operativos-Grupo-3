#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#define TRUE 1

void nuevoClienteRed();
void accionesCliente(void *ptr);
void accionesTecnico(void *ptr);
void accionesEncargado(void *ptr);
void accionesTecnicoDomiciliario(void *ptr);
void escribeEnLog(char *id, char *msg);
int calculaAleatorios(int min, int max);

pthread_mutex_t mutexFichero;
pthread_mutex_t mutexColaClientes;
pthread_mutex_t mutexSolicitudes;

pthread_cond_t condicionTecnicoDomiciliario; 
pthread_cond_t condicionClienteDomiciliario;

int contadorApp;
int contadorRed;

int numSolicitudes;

struct Cliente{
    int id;
    int atendido;
    int tipo;
    int prioridad;
    int solicitud;
}

struct Cliente *listaClientes;

int *listaApp;
int *listaRed;

FILE *logFile;

int main(int argc, char const *argv[]){
    return 0;
}

int calculaAleatorios(int min, int max){
	return rand() % (max-min+1) + min;
}