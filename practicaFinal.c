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
void eliminarCliente(struct Cliente **clienteAEliminar);
void *accionesTecnico(void *ptr);
void *accionesEncargado(void *ptr);
void *accionesTecnicoDomiciliario(void *ptr);
void terminar(int señal);
void writeLogMessage(char *id, char *msg);
int calculaAleatorios(int min, int max);

pthread_mutex_t mutexFichero;
pthread_mutex_t mutexColaClientes;
pthread_mutex_t mutexSolicitudesDom;
pthread_mutex_t mutexSolicitudesTecnicos;
pthread_mutex_t mutexSolicitudesEncargado;

pthread_cond_t condicionTecnicoDomiciliario; 
pthread_cond_t condicionClienteDomiciliario;

int contadorApp;
int contadorRed;

int numSolicitudesDom;

struct Cliente{
    int id;

    /*
     * -1 - Si ya se marcha
     *  0 - Si no ha sido atendido
     *  1 - Si esta siendo atendido
     */
    int atendido;
    int tipo;
    int prioridad;
    int solicitud;
};

struct Cliente *listaClientes;

struct Tecnico{
    /*
     *  0 - App
     *  1 - Red
     *  2 - Encargado
     *  3 - Domiciliario
     */
    int tipo;
    int solicitudes;

    /*
     *  0 - No
     *  1 - Si
     */
    int atendiendo;
    pthread_t hiloTecnico;
};

struct Tecnico *listaTecnicos;

FILE *logFile;

int main(int argc, char const *argv[]){
    //Plantar semilla
    srand(getpid());

    //Definir estructuras de clientes y enmascarar señales
    struct sigaction clienteApp = {0};
    clienteApp.sa_handler = nuevoClienteRed;
    struct sigaction clienteRed = {0};
    clienteRed.sa_handler = nuevoClienteRed;
    if(-1 == sigaction(SIGUSR1, &clienteApp, NULL)  || -1 == sigaction(SIGUSR2, &clienteRed, NULL) ){
		perror("Error: LLamada a sigaction nuevo cliente.");
		exit(-1);
	}

    //Definir estructura de fin y enmascar señales
    struct sigaction fin = {0};
	fin.sa_handler = terminar;
	if( -1 == sigaction(SIGINT, &fin, NULL) ){
		perror("Error: LLamada a sigaction terminar.");
		exit(-1);
	}

    //Inicializar semáforos
    if (pthread_mutex_init(&mutexFichero, NULL) != 0) exit(-1);
    if (pthread_mutex_init(&mutexColaClientes, NULL) != 0) exit(-1);
    if (pthread_mutex_init(&mutexSolicitudesDom, NULL) != 0) exit(-1);
    if (pthread_mutex_init(&mutexSolicitudesTecnicos, NULL) != 0) exit(-1);
    if (pthread_mutex_init(&mutexSolicitudesEncargado, NULL) != 0) exit(-1);

    //Inicializar contadores
    contadorApp = 0;
    contadorRed = 0;

    //Inicializar lista de clientes
    listaClientes = (struct Cliente*)malloc(sizeof(struct Cliente)*(20));

    for (int i = 0; i < 20; i++){
        (listaClientes+i)->id = 0;
        (listaClientes+i)->atendido = 0;
        (listaClientes+i)->tipo = 0;
        (listaClientes+i)->prioridad = 0;
        (listaClientes+i)->solicitud = 0;
    }

    //Inicializar lista de tecnicos (2 tecnico app, 2 tecnico red, 1 encargado y 1 tecnico domiciliario)
    listaTecnicos = (struct Tecnico*)malloc(sizeof(struct Tecnico)*6);

    for (int i = 0; i < 6; i++) {
        switch(1){

            //Tecnicos app
            case 0:
            (listaTecnicos+i)->tipo = 0;
                (listaTecnicos+i)->solicitudes = 0;
                (listaTecnicos+i)->atendiendo = 0;
                break;
            case 1:
                (listaTecnicos+i)->tipo = 0;
                (listaTecnicos+i)->solicitudes = 0;
                (listaTecnicos+i)->atendiendo = 0;
                break;
            
            //Encargado
            case 4:
                (listaTecnicos+i)->tipo = 2;
                (listaTecnicos+i)->solicitudes = 0;
                (listaTecnicos+i)->atendiendo = 0;
                break;
            
            //Tecnico dommiciliario
            case 5:
                (listaTecnicos+i)->tipo = 3;
                (listaTecnicos+i)->solicitudes = 0;
                (listaTecnicos+i)->atendiendo = 0;
                break;

            //Tecnicos red
            default:
                (listaTecnicos+i)->tipo = 1;
                (listaTecnicos+i)->solicitudes = 0;
                (listaTecnicos+i)->atendiendo = 0;
        }
    }

    //Inicializar fichero de log
    logFile = fopen("registroTiempos.log" , "wt");
	fclose(logFile);

    //Inicializar variable domicialiaria
    numSolicitudesDom = 0;

    //Inicializar variables condicion
    if (pthread_cond_init(&condicionTecnicoDomiciliario, NULL) != 0) exit(-1);
    if (pthread_cond_init(&condicionClienteDomiciliario, NULL) != 0) exit(-1);

    //Crear los 6 hilos 
    int pos=0;
    pthread_create(&listaTecnicos[0].hiloTecnico, NULL, accionesTecnico, (void*)(intptr_t)pos++);
    pthread_create(&listaTecnicos[1].hiloTecnico, NULL, accionesTecnico, (void*)(intptr_t)pos++);
    pthread_create(&listaTecnicos[2].hiloTecnico, NULL, accionesTecnico, (void*)(intptr_t)pos++);
    pthread_create(&listaTecnicos[3].hiloTecnico, NULL, accionesTecnico, (void*)(intptr_t)pos++);
    pthread_create(&listaTecnicos[4].hiloTecnico, NULL, accionesEncargado, NULL);
    pthread_create(&listaTecnicos[5].hiloTecnico, NULL, accionesTecnicoDomiciliario, NULL);

    //Esperar señales de forma infinita
    while(TRUE){
		pause();
	}

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
                        sprintf(mensaje, "Se le ha caido la conexion y se va.");
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

void *accionesTecnico(void *ptr){
    //El tecnico esta constantemente atendiento a clientes
    while(TRUE){

        int posTecnico = (intptr_t)ptr;

        //Si esta en la posicion 0 o 1 es un tecnico de app
        if( (posTecnico==0 || posTecnico==1) && listaTecnicos[posTecnico].atendiendo==0 ){

            pthread_mutex_lock(&mutexSolicitudesTecnicos);
            
            //Obtememos posicion del ciente de tipo app con mayor prioridad o si hay empate, de que haya llegado antes
            int posCliente=-1;
            int prioMax=0;
            for(int i=0; i<contadorApp+contadorRed && i<20; i++){
                    
                if(listaClientes[i].tipo==0 && listaClientes[i].atendido==0 && listaClientes[i].prioridad>prioMax){
                    posCliente=i;
                    prioMax=listaClientes[i].prioridad;
                }             
            }

            //Si el tecnico no encuentra clientes espera 1s y vuelve al principio
            if(posCliente==-1){
                sleep(1);
                pthread_mutex_unlock(&mutexSolicitudesTecnicos);

            //Si lo encuentra, el tecnico lo atiende
            }else{

                //Cambiamos flag de atendido
                listaClientes[posCliente].atendido=1;

                //Cambiamos flag de atendiendo
                listaTecnicos[posTecnico].atendiendo=1;

                //Calculamos el tipo de atencion del cliente
                int num = calculaAleatorios(0,100);
                char mensaje[100];
                int numSolicitud = contadorApp+contadorRed;
                sprintf(mensaje, "Solicitud numero %d de tipo %d", numSolicitud, listaClientes[posCliente].tipo);
                writeLogMessage(mensaje, "Comienza atencion");
                //- 80% tiene todo en orden
                if(num<80){
                    sleep(calculaAleatorios(1,4));
                    writeLogMessage(mensaje, "Finaliza la atencion teniendo todo en orden");

                    //Calcula si manda o no la solicitud domiciliaria
                    pthread_mutex_lock(&mutexSolicitudesDom);
                    num = calculaAleatorios(0,1);
                    if(num==1){
                        //Envio señal del cliente
                        pthread_cond_signal(condicionClienteDomiciliario);
                        //Espero a recibir la señal del tecnico domiciliario
                        pthread_cond_wait(&condicionTecnicoDomiciliario, &mutexSolicitudesDom);
                    }
                    pthread_mutex_unlock(&mutexSolicitudesDom);

                //- 10% mal identificados
                }else if(num<90){
                    sleep(calculaAleatorios(2,6));
                    writeLogMessage(mensaje, "Finaliza la atencion y tiene errores en sus datos de identificacion");

                    //Calcula si manda o no la solicitud domiciliaria
                    pthread_mutex_lock(&mutexSolicitudesDom);
                    num = calculaAleatorios(0,1);
                    if(num==1){
                        //Envio señal del cliente
                        pthread_cond_signal(condicionClienteDomiciliario);
                        //Espero a recibir la señal del tecnico domiciliario
                        pthread_cond_wait(&condicionTecnicoDomiciliario, &mutexSolicitudesDom);
                    }
                    pthread_mutex_unlock(&mutexSolicitudesDom);

                //- 10% se ha confunfido de compañia
                }else{
                    sleep(calculaAleatorios(1,2));
                    writeLogMessage(mensaje, "Finaliza la atencion por confundirse de compañia");
                }

                //Incrementamos el numero de solicutudes atendidas
                listaTecnicos[posTecnico].solicitudes++;

                //Tecnico mira si tiene que descansar
                if(listaTecnicos[posTecnico].solicitudes>=5){
                    sleep(5);
                }
                
                //Cambiamos flag de atendiendo
                listaTecnicos[posTecnico].atendiendo=0;

                pthread_mutex_unlock(&mutexSolicitudesTecnicos);
            }
 
        }

        //Si esta en la posicion 2 o 3 es un tecnico de app
        if( (posTecnico==2 || posTecnico==3) && listaTecnicos[posTecnico].atendiendo==0 ){

            pthread_mutex_lock(&mutexSolicitudesTecnicos);;

            //Obtememos posicion del ciente de tipo red con mayor prioridad o si hay empate, de que haya llegado antes
            int posCliente=-1;
            int prioMax=0;
            for(int i=0; i<contadorApp+contadorRed && i<20; i++){
                  
                if(listaClientes[i].tipo==1 && listaClientes[i].atendido==0 && listaClientes[i].prioridad>prioMax){
                        posCliente=i;
                        prioMax=listaClientes[i].prioridad;
                }             
            }

            //Si el tecnico no encuentra clientes espera 1s y vuelve al principio
            if(posCliente==-1){
                sleep(1);
                pthread_mutex_unlock(&mutexSolicitudesTecnicos);

            //Si lo encuentra, el tecnico lo atiende
            }else{

                //Cambiamos flag de atendido
                listaClientes[posCliente].atendido=1;

                //Cambiamos flag atendiendo
                listaTecnicos[posTecnico].atendiendo=1;

                //Calculamos el tipo de atencion del cliente
                int num = calculaAleatorios(0,100);
                char mensaje[100];
                int numSolicitud = contadorApp+contadorRed+1;
                sprintf(mensaje, "Solicitud numero %d de tipo %d", numSolicitud, listaClientes[posCliente].tipo);
                writeLogMessage(mensaje, "Comienza atencion");
                //- 80% tiene todo en orden
                if(num<80){
                    sleep(calculaAleatorios(1,4));
                    writeLogMessage(mensaje, "Finaliza la atencion teniendo todo en orden");

                    //Calcula si manda o no la solicitud domiciliaria
                    pthread_mutex_lock(&mutexSolicitudesDom);
                    num = calculaAleatorios(0,1);
                    if(num==1){
                        //Envio señal del cliente
                        pthread_cond_signal(condicionClienteDomiciliario);
                        //Espero a recibir la señal del tecnico domiciliario
                        pthread_cond_wait(&condicionTecnicoDomiciliario, &mutexSolicitudesDom);
                    }
                    pthread_mutex_unlock(&mutexSolicitudesDom);

                //- 10% mal identificados
                }else if(num<90){
                    sleep(calculaAleatorios(2,6));
                    writeLogMessage(mensaje, "Finaliza la atencion y tiene errores en sus datos de identificacion");

                    //Calcula si manda o no la solicitud domiciliaria
                    pthread_mutex_lock(&mutexSolicitudesDom);
                    num = calculaAleatorios(0,1);
                    if(num==1){
                        //Envio señal del cliente
                        pthread_cond_signal(condicionClienteDomiciliario);
                        //Espero a recibir la señal del tecnico domiciliario
                        pthread_cond_wait(&condicionTecnicoDomiciliario, &mutexSolicitudesDom);
                    }
                    pthread_mutex_unlock(&mutexSolicitudesDom);

                //- 10% se ha confunfido de compañia
                }else{
                    sleep(calculaAleatorios(1,2));
                    writeLogMessage(mensaje, "Finaliza la atencion por confundirse de compañia");
                }

                //Incrementamos el numero de solicutudes atendidas
                listaTecnicos[posTecnico].solicitudes++;

                //Tecnico mira si tiene que descansar
                if(listaTecnicos[posTecnico].solicitudes>=5){
                    sleep(5);
                }
                
                //Cambiamos flag de atendiendo
                listaTecnicos[posTecnico].atendiendo=0;

                pthread_mutex_unlock(&mutexSolicitudesTecnicos);
            }
        }
    }
}



void *accionesEncargado(void *ptr){

    //El encargado esta constantemente atendiendo clientes
    while(TRUE){
        
        pthread_mutex_lock(&mutexSolicitudesEncargado);

        //Obtememos posicion del ciente de tipo red con mayor prioridad o si hay empate, de que haya llegado antes
        int posClienteRed=-1;
        int prioMax=0;
        for(int i=0; i<contadorApp+contadorRed && i<20; i++){
                  
            if(listaClientes[i].tipo==1 && listaClientes[i].atendido==0 && listaClientes[i].prioridad>prioMax){
                posClienteRed=i;
                prioMax=listaClientes[i].prioridad;
            }             
        }

        //Si el encargado no encuentra clientes de tipo red busca si hay de tipo app
        int posClienteApp=-1;
        if(posClienteRed==-1){

            prioMax=0;
            for(int i=0; i<contadorApp+contadorRed && i<20; i++){
                  
                if(listaClientes[i].tipo==0 && listaClientes[i].atendido==0 && listaClientes[i].prioridad>prioMax){
                    posClienteRed=i;
                    prioMax=listaClientes[i].prioridad;
                }             
            }
        }
        //Si el tecnico no encuentra clientes espera 3s y vuelve al principio
        if(posClienteRed==-1 && posClienteApp==-1){
            sleep(3);
            pthread_mutex_unlock(&mutexSolicitudesEncargado);

        //Si lo encuentra, el encargado lo atiende
        }else{

            //Guardamos la posicion del cliente
            int posCliente=posClienteApp;
            if(posClienteRed!=-1){
                posCliente=posClienteRed;
            }

            //Cambiamos flag de atendido
            listaClientes[posCliente].atendido=1;

            //Cambiamos flag atendiendo
            listaTecnicos[4].atendiendo=1;

            //Calculamos el tipo de atencion del cliente
            int num = calculaAleatorios(0,100);
            char mensaje[100];
            int numSolicitud = contadorApp+contadorRed+1;
            sprintf(mensaje, "Solicitud numero %d de tipo %d", numSolicitud, listaClientes[posCliente].tipo);
            writeLogMessage(mensaje, "Comienza atencion");
            //- 80% tiene todo en orden
            if(num<80){
                sleep(calculaAleatorios(1,4));
                writeLogMessage(mensaje, "Finaliza la atencion teniendo todo en orden");

                //Calcula si manda o no la solicitud domiciliaria
                pthread_mutex_lock(&mutexSolicitudesDom);
                num = calculaAleatorios(0,1);
                if(num==1){
                    //Envio señal del cliente
                    pthread_cond_signal(condicionClienteDomiciliario);
                    //Espero a recibir la señal del tecnico domiciliario
                    pthread_cond_wait(&condicionTecnicoDomiciliario, &mutexSolicitudesDom);
                }
                pthread_mutex_unlock(&mutexSolicitudesDom);
                
            //- 10% mal identificados
            }else if(num<90){
                sleep(calculaAleatorios(2,6));
                writeLogMessage(mensaje, "Finaliza la atencion y tiene errores en sus datos de identificacion");

                //Calcula si manda o no la solicitud domiciliaria
                pthread_mutex_lock(&mutexSolicitudesDom);
                num = calculaAleatorios(0,1);
                if(num==1){
                    //Envio señal del cliente
                    pthread_cond_signal(condicionClienteDomiciliario);
                    //Espero a recibir la señal del tecnico domiciliario
                    pthread_cond_wait(&condicionTecnicoDomiciliario, &mutexSolicitudesDom);
                }
                pthread_mutex_unlock(&mutexSolicitudesDom);

            //- 10% se ha confunfido de compañia
            }else{
                sleep(calculaAleatorios(1,2));
                writeLogMessage(mensaje, "Finaliza la atencion por confundirse de compañia");
            }

            //Cambiamos flag de atendido
            listaClientes[posCliente].atendido=1;

            //Cambiamos flag de atendiendo
            listaTecnicos[4].atendiendo=0;

            pthread_mutex_unlock(&mutexSolicitudesEncargado);
        }
    }
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

void accionesTecnicoDomiciliario (void *ptr){

while(TRUE){

// se queda bloqueado el mutex si hay menos de 4 solicitudes

pthread_mutex_lock(&mutexSolicitudesDom);
while(numSolicitudesDom<4){
pthreah_cond_wait(&condicionTecnicoDomicialrio, & mutexSolicitudesDom);
}
// guardamos en el log que va a comenzar la atencion

char msg[100];
pritnf(" Va comenzar la atención\n");
sprintf(msg, " Va a comenzar la atención\n");
escribeEnLog( "AVISO", msg);
for(int i=0; i<5; i++){

// guardamos en el log que el cliente ha sido atendido

char atendido[100];
pritnf(" cliente atendido\n");
sprintf(atendido, " cliente atendido\n");
escribeEnLog( "AVISO", atendido);

// cambiamos el flag de solicitud del cliente

listaClientes[i].solicitud==0;
int idTecnico = *(int *)ptr;
listaTecnicos[idTecnico].atendiendo=1;
sleep(1);
}

// ponemos las solicitudes a 0

solicitudesDom=0;

// guardamos en el log que se ha finalizado la atencion

char final[100];
pritnf(" se ha finalizado la atencion domicialria\n");
sprintf(final, " se ha finalizado la atencion domiciliaria\n");
escribeEnLog( "AVISO", final);

pthread_cond_signal(&condicionTecnicodomiciliario);

pthread_mutex_unlock(&mutexSolicitudesDom);

}
}


void terminar(int señal){
    char mensaje[100];
    printf("Se va a finalizar el programa.\n");
    sprintf(mensaje, "Se va a finalizar el programa.\n");
    escribeEnLog("AVISO", mensaje);

    //Bloquear el mutex de las solicitudes domiciliarias
    pthread_mutex_lock(&mutexSolicitudesDom);

    //Cerrar solicitudes domiciliarias
    numSolicitudesDom = 0;

    for (int i = 0; i < 20; i++){
        (listaClientes+i)->solicitud = 0;
    }

    //Desbloquear los mutex de las solicitudes domiciliarias
    pthread_mutex_unlock(&mutexSolicitudesDom);

    while(TRUE) {
        //Bloquear mutex de clientes para saber si se han acabado
        pthread_mutex_lock(&mutexColaClientes);
        if (contadorApp + contadorRed == 0){
            //Liberar punteros
            free(listaClientes);
            free(listaTecnicos);

            printf("Fin del programa.\n");
            sprintf(mensaje, "Fin del programa.\n");
            escribeEnLog("AVISO", mensaje);
            
            exit(0);
        } else{
            //Desbloquear el mutex hasta que se acaben los clientes
            pthread_mutex_unlock(&mutexColaClientes);
			sleep(1);
        }
    }
}

void writeLogMessage(char *id, char *msg) {
    // Calculamos la hora actual
    time_t now = time(0);
    struct tm *tlocal = localtime(&now);
    char stnow[25];
    strftime(stnow, 25, "%d/%m/%y %H:%M:%S", tlocal);
    // Escribimos en el log
    logFile = fopen("registroTiempos.log", "a");
    fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg);
    fclose(logFile);
}
