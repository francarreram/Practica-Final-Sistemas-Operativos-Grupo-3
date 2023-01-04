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

void nuevoClienteRed(int tipo);
void *accionesCliente(void *ptr);
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

    /*
     * -1 - si ya se marcha
     *  0 - si no ha sido atendido
     *  1 - si esta siendo atendido
     */
    int atendido;
    int tipo;
    int prioridad;
    int solicitud;
}

struct 
struct Cliente *listaClientes;

int *listaApp;
int *listaRed;

FILE *logFile;

int main(int argc, char const *argv[]){
    return 0;
}

/**
 * Metodo que crea un nuevo cliente
 * 
 * tipo: Grupo al que pertenece el cliente
 *  0: Cliente app
 *  1: Cliente red
 */
void nuevoCienteRed(int tipo) { //No se si esto lo he entendido bien, creo que sobrea el argumento ya que solo se crean los clientes de red.
    // Restablecer la señal
    if(signal(tipo, nuevoCliente) == SIG_ERR){
        perror("Llamada a signal");
        exit(-1);
    }
    // 1. Comprobar si hay espacio en la lista de clientes.
    pthread_mutex_lock(&mutexColaClientes);
    if (numSolicitudes < contadorRed){ //Creo que seria asi teniendo en cuenta que solo se crean clientes de tipo red
        //Caso a.
        // i. Añadir cliente a la lista
        struct Cliente *clienteNuevo;
        clienteNuevo=malloc(sizeof *clienteNuevo);

        clienteNuevo->sig = NULL;
        clienteNuevo->ant = NULL;

        if (primerCliente == NULL){ //Se añadiria el cliente como el primero de la cola
            primerCliente = clienteNuevo;
            ultimoCliente = primerCliente;
        } else { //Se añade el cliente despues del ultimo cliente, el nuevo cliente no es el primero
            clienteNuevo->ant = ultimoCliente;
            ultimoCliente->sig = clienteNuevo;
            ultimoCliente = clienteNuevo;
        }

        // ii. Aumentar contador clientes
        if(tipo == 0) {
            contadorApp++;
        } else {
            contadorRed++;
        }

        // iii. Establecer el id del cliente
        clienteNuevo->id=totalClientes; //No entiendo esto

        // iv. Establecer el flag de atendido
        clienteNuevo->atendido=0;

        // v. Establecer el tipo del cliente
        if(tipo == 0) {
            clienteNuevo->tipo = 0;
        } else {
            clienteNuevo->tipo = 1;
        }

        // vi. Establecer solicitud
        clienteNuevo->solicitud=0;

        // vii. Establecer prioridad
        int prioridadCliente = calculaAleatorios(1, 10);
        clienteNuevo->prioridad = prioridadCliente;

        // viii. Crear hilo cliente
        pthread_t threadNuevoCliente;
        pthread_create (&threadNuevoCliente, NULL, accionesCliente, (void *)clienteNuevo);
    }

    pthread_mutex_unlock(&mutexColaClientes);
}

void *accionesCliente(void *ptr) {
    //1y2. Escribir en logs
    struct Cliente *cliente;
    int atendido;
    int comportamiento;
    char type[100];
    char mensaje[100];

    pthread_mutex_lock(&mutexColaClientes);

    //Se coge la referencia a cliente que se pasa por parametro
    cliente = (struct Cliente *) ptr;

    switch(cliente->tipo){
        case 0:
            sprintf(type, "%s%d %s","Cliente ", cliente->id, "App");
            break;
        case 1:
            sprintf(type, "%s%d %s","Cliente ", cliente->id, "Red");
            break;
        default:
            sprintf(type, "%s%d %s","Cliente ", cliente->id, "Desconocido");
            break;
    }

    sprintf(mensaje, "Entra el cliente.");

    pthread_mutex_lock(&mutexFichero);
    writeLogMessage(type, mensaje);
    pthread_mutex_unlock(&mutexFichero);

    pthread_mutex_unlock(&mutexColaPacientes);

    //3. Comprobar si esta atendido
    do {
        pthread_mutex_lock(&mutexColaClientes);
        atendido = cliente->atendido;
        comportamiento = calculaAleatorios(1, 100);

        if (atendido == 1) {
            printf("El cliente: %d esta siendo atentido\n", cliente->id);
        }else if(atendido == 0){
            printf("El cliente: %d no esta siendo atentido\n", cliente->id);
            if (comportamiento <= 35) {
                if(cliente->atendido == 0) {
                    if(comportamiento <= 10){
                        sprintf(mensaje, "Encuentra dificil la aplicación y se va.");
                    } else if(comportamiento > 10 && comportamiento <= 30) {
                        sprintf(mensaje, "Se ha cansado de esperar y se va.");
                        sleep(8);
                    } else {
                        sprintf(mensaje, "Se le ha caido la conexion y se va.")
                    }

                    pthread_mutex_lock(&mutexFichero);
                    writeLogMessage(type, mensaje);
                    pthread_mutex_unlock(&mutexFichero);

                    paciente->atendido = -1;
                    eliminarCliente(&cliente);
                    pthread_exit(NULL);
                }
            }else{
                sleep(2);
            }
        }
        pthread_mutex_unlock(&mutexColaPacientes);
    }while (atendido == 0);

    // 4. Esperar a que termine de ser atendido, comprueba cada 2

    pthread_mutex_lock(&mutexColaClientes);

    comportamiento = calculaAleatorios(1, 100)
    while(atendido == 1) {
        if(cliente->tipo == 1 && comportamiento <= 30) {
            // 5.
            do {
                numSolicitudes++;

                // i.
                sprintf(mensaje, "El cliente espera ser atendido.");
                pthread_mutex_lock(&mutexFichero);
                writeLogMessage(type, mensaje);
                pthread_mutex_unlock(&mutexFichero);

                // ii.
                cliente->solicitud = 1;

                // iii. y iv. no se como hacerlo


                // v.
                sprintf(mensaje, "El cliente ha terminado de ser atendido.");
                pthread_mutex_lock(&mutexFichero);
                writeLogMessage(type, mensaje);
                pthread_mutex_unlock(&mutexFichero);

                sleep(2);

            }while(numSolicitudes < 4);
        } else {
            paciente->atendido = -1;
            eliminarCliente(&cliente);

            sprintf(mensaje, "El cliente ha terminado.")
            pthread_mutex_lock(&mutexFichero);
            writeLogMessage(type, mensaje);
            pthread_mutex_unlock(&mutexFichero);
            
            pthread_exit(NULL);
        }
        sleep(2);
    }
    
    pthread_mutex_unlock(&mutexColaClientes);

}

void eliminarCliente(struct Cliente **clienteAEliminar){
    pthread_mutex_lock(&mutexColaClientes);
    if(*cliienteAEliminar == NULL){
        printf("NULL\n");
    }
    printf("Eliminar Cliente %d\n", (*clieienteAEliminar)->id);

    if((*clienteAEliminar)->ant != NULL && (*clienteAEliminar)->sig != NULL){
        (*clienteAEliminar)->ant->sig=(*clienteAEliminar)->sig;
        (*clienteAEliminar)->sig->ant = (*clienteAEliminar)->ant;
    }else if((*clienteAEliminar)->ant != NULL){
        clientePaciente = (*clienteAEliminar)->ant;
        (*clienteAEliminar)->ant->sig = NULL;

    }else if((*clienteAEliminar)->sig != NULL){
        primerCliente = (*clienteAEliminar)->sig;
        (*clienteAEliminar)->sig->ant = NULL;
    }else{
        primerCliente = NULL;
        ultimoCliente = NULL;
    }

    contadorClientes --;
    free(*clienteAEliminar);
    printf("Eliminado el cliente \n");
    pthread_mutex_unlock(&mutexColaClientes);
}

int calculaAleatorios(int min, int max){
	return rand() % (max-min+1) + min;
}

void writeLogMessage(char *id, char *msg) {
    // Calculamos la hora actual
    time_t now = time(0);
    struct tm *tlocal = localtime(&now);
    char stnow[25];
    strftime(stnow, 25, "%d/%m/%y %H:%M:%S", tlocal);
    // Escribimos en el log
    logFile = fopen(logFileName, "a");
    fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg);
    fclose(logFile);
}



