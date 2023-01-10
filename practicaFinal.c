#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

#define TRUE 1

pthread_mutex_t mutexFichero;
pthread_mutex_t mutexColaClientes;
pthread_mutex_t mutexCliente;
pthread_mutex_t mutexSolicitudesDom;
pthread_mutex_t mutexSolicitudesTecnicos;
pthread_mutex_t mutexSolicitudesEncargado;

pthread_cond_t condicionTecnicoDomiciliario; 
pthread_cond_t condicionClienteDomiciliario;

int contadorApp;
int contadorRed;
int contadorSolicitudes;
int contadorSolicitudesDom;

struct Cliente{
    /*
     * Numero que ocupa en la cola de clientes de su tipo
     */
    int id;
    /*
     * -1 - Si ya se marcha
     *  0 - Si no ha sido atendido
     *  1 - Si esta siendo atendido
     */
    int atendido;
    /*
     *  0 - App
     *  1 - Red
     */
    int tipo;
    /*
     * 0 - todo en regla(mandara o no solicitud dom)
     * 1 - mal identificado(mandara o no solicitud dom)
     * 2 - se ha confundido de compañia(abandona el sistema)
     */
     int tipoAtencion;
     /*
     * Prioridad del 1 al 10 para ser atendido
     */
    int prioridad;
    /* 
     * 0 - no ha solicitado atencion domiciliaria
     * 1 - ha solicitado atencion domiciliaria
     */
    int solicitud;
    /*
     * Hilo que ejecuta cada cliente
     */
    pthread_t hiloCliente;
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
    /*
     * Num de solicitudes atendidas
     */
    int solicitudes;
    /*
     *  0 - No
     *  1 - Si
     */
    int atendiendo;
     /*
     * Hilo que ejecuta cada tecnico
     */
    pthread_t hiloTecnico;
};

struct Tecnico *listaTecnicos;

void nuevoClienteRed(int signal);
void *accionesCliente(void *ptr);
void eliminarCliente(int pos);
void *accionesTecnico(void *ptr);
void *accionesEncargado(void *ptr);
void *accionesTecnicoDomiciliario(void *ptr);
void terminar(int signal);
void writeLogMessage(char *id, char *msg);
int calculaAleatorios(int min, int max);

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
    if (pthread_mutex_init(&mutexCliente, NULL) != 0) exit(-1);
    if (pthread_mutex_init(&mutexSolicitudesDom, NULL) != 0) exit(-1);
    if (pthread_mutex_init(&mutexSolicitudesTecnicos, NULL) != 0) exit(-1);
    if (pthread_mutex_init(&mutexSolicitudesEncargado, NULL) != 0) exit(-1);

    //Inicializar contadores
    contadorApp = 0;
    contadorRed = 0;

    //Definir los punteris de Clientes y tecnicos
    listaClientes = (struct Cliente*)malloc(sizeof(struct Cliente)*(20));
    listaTecnicos = (struct Tecnico*)malloc(sizeof(struct Tecnico)*6);
    

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
    contadorSolicitudesDom = 0;

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

    //Crear menú inicial
    printf("GESTIÓN DE AVERIAS LUZECITA\n");
    writeLogMessage("INICIO", "Gestión de Averías LuZECita");
    printf("Si tiene problemas en la app introduzca el comando 'kill -SIGUSR1 %d'\n", getpid());
    printf("Si tiene problemas en la red introduzca el comando 'kill -SIGUSR2 %d'\n", getpid());
    printf("Si desea cerrar la app introduzca el comando 'kill -SIGINT %d'\n", getpid());
    
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
void nuevoClienteRed(int signal){

    //Comprobamos si hay espacio en la lista de clientes
    if(contadorApp+contadorRed<20){
        pthread_mutex_lock(&mutexColaClientes);
        

        //Calculamos prioridad del cliemte
        int prioridadCliente = calculaAleatorios(1, 10);
        listaClientes[contadorApp+contadorRed].prioridad=prioridadCliente;

        
        //Dependiendo de la señal calculamos el id y el tipo, tambien incrementamos el contador
        switch(signal){
            case SIGUSR1:
            listaClientes[contadorApp+contadorRed].id=contadorApp+1;
            listaClientes[contadorApp+contadorRed].tipo=0;
            listaClientes[contadorApp+contadorRed].atendido=0;
            listaClientes[contadorApp+contadorRed].solicitud=0;
            printf("Creo hilo app\n");
            //Creamos el hilo y pasamos como parametro la posicion del cliente en listaClientes
            pthread_create(&listaClientes[contadorApp+contadorRed].hiloCliente, NULL, accionesCliente, (void *)(intptr_t)contadorApp+contadorRed);
            contadorApp++;
            break;

            case SIGUSR2:
            listaClientes[contadorApp+contadorRed].id=contadorRed+1;
            listaClientes[contadorApp+contadorRed].tipo=1;
            listaClientes[contadorApp+contadorRed].atendido=0;
            listaClientes[contadorApp+contadorRed].solicitud=0;
            printf("Creo hilo red\n");
            //Creamos el hilo y pasamos como parametro la posicion del cliente en listaClientes
            pthread_create(&listaClientes[contadorApp+contadorRed].hiloCliente, NULL, accionesCliente, (void *)(intptr_t)contadorApp+contadorRed);  
            contadorRed++;
            break;
        }

        //Incrementamos contador
        contadorSolicitudes++;

        pthread_mutex_unlock(&mutexColaClientes);
    }
   
}

void *accionesCliente(void *ptr) {
  
    int comportamiento;
    char idCliente[100];
    char mensaje[100];
    int posCliente = (intptr_t)ptr;

    pthread_mutex_lock(&mutexCliente);
    
    int tipo = listaClientes[(intptr_t)ptr].tipo;
    switch(tipo){
        case 0:
            sprintf(idCliente,"Cliapp_%d",listaClientes[(intptr_t)ptr].id);
            break;
        case 1:
            sprintf(idCliente,"Clired_%d",listaClientes[(intptr_t)ptr].id);
            break;
    }
    writeLogMessage(idCliente, "Ha entrado");
    printf("La solicitud %d de tipo %d ha entrado\n", contadorSolicitudes, tipo);

    //Mienstras no es atendido se calcula su comportamiento y se va o permanece
    while(listaClientes[posCliente].atendido==0){

        comportamiento = calculaAleatorios(1, 100);

        if(comportamiento<=10){
            sprintf(mensaje, "Encuentra dificil la aplicación y se va.");
            writeLogMessage(idCliente,mensaje);
            printf("La solicitud %d encuentra dificil la aplicacion y se va\n", contadorSolicitudes);
            eliminarCliente((intptr_t)ptr);
            pthread_mutex_unlock(&mutexCliente);
            pthread_exit(NULL);
        }else if(comportamiento<=20){
            sleep(8);
            listaClientes[posCliente].atendido=1;
            printf(mensaje, "Se ha cansado de esperar y se va.");
            writeLogMessage(idCliente,mensaje);
            printf("La solicitud %d se cansa de esperar y se va\n", contadorSolicitudes);
            eliminarCliente((intptr_t)ptr);
            pthread_mutex_unlock(&mutexCliente);
            pthread_exit(NULL);
        }else if(comportamiento<=37){//7 de 100== 5 de 70
            sprintf(mensaje, "Se le ha caido la conexion y se va.");
            writeLogMessage(idCliente,mensaje);
            printf("La solicitud %d ha perdido la conexion\n", contadorSolicitudes);
            eliminarCliente((intptr_t)ptr);
            pthread_mutex_unlock(&mutexCliente);
            pthread_exit(NULL);
        }else{
            sleep(2);
        }
    }
    
    //Esperamos hasta que deje de estar siendo atendido por el tecnico
    while(listaClientes[posCliente].atendido!=-1){
        sleep(2);
    }
    
    //Una vez atendido por el tecnico se va si se ha confundido de compañia(tipoAtencion=2)
    if(listaClientes[posCliente].tipoAtencion==2){
        pthread_mutex_unlock(&mutexCliente);
        pthread_exit(NULL);
    }

    //Si es de tipo red puede solicitar o no solicitud domiciliaria
    if(listaClientes[(intptr_t)ptr].tipo==1){

        int solicitaDom = calculaAleatorios(0,1);
        if(solicitaDom=1){

        bool clienteAtendido=false;
            while(!clienteAtendido){
                if(contadorSolicitudesDom<4){
                    contadorSolicitudesDom++;

                    sprintf(mensaje, "Espera al domiciliario");
                    writeLogMessage(idCliente, mensaje);

                    //Cambiiamos flag atendido
                    listaClientes[posCliente].atendido=1;
                }else{
                    sleep(3);
                }
                if(contadorSolicitudesDom==4){
                    pthread_mutex_lock(&mutexSolicitudesDom);

                    pthread_cond_signal(&condicionTecnicoDomiciliario);
                    pthread_cond_wait(&condicionClienteDomiciliario, &mutexSolicitudesDom);

                    sprintf(mensaje, "Ha sido atendido por el domiciliario");
                    writeLogMessage(idCliente, mensaje);
                    printf("La solicitud %d ha sido atendida por el domiciliario\n", contadorSolicitudes);
                    pthread_mutex_unlock(&mutexSolicitudesDom);
                    clienteAtendido=true;
                }
            }
   
        }
    } 

    sprintf(mensaje, "Ha terminado\n");
    writeLogMessage(idCliente, mensaje);
    printf("El cliente %d ha terminado\n", contadorSolicitudes);
    eliminarCliente(posCliente);
    pthread_mutex_unlock(&mutexCliente);
    pthread_exit(NULL);

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
                char mensajeTipoAtencion[100];
                char mensajeTecnico[100];
                char idTecnico[10];
                sprintf(idTecnico,"Tecapp_%ld",(intptr_t)ptr);
                sprintf(mensajeTecnico,"Va a atender la solicitud %d", contadorSolicitudes);
                writeLogMessage(idTecnico,mensajeTecnico);
                printf("El tecnico %ld va a atender la solicitud %d \n", (intptr_t)ptr, contadorSolicitudes);

                //- 80% tiene todo en orden
                if(num<80){
                    sleep(calculaAleatorios(1,4));
                    sprintf(mensajeTipoAtencion, "La solicitud  %d tiene todo en orden", contadorSolicitudes);
                    printf("La solicitud numero %d tiene todo en orden\n", contadorSolicitudes);
                    listaClientes[posCliente].tipoAtencion=0;

                //- 10% mal identificados
                }else if(num<90){
                    sleep(calculaAleatorios(2,6));
                    sprintf(mensajeTipoAtencion, "La solicitud %d se ha identificado mal", contadorSolicitudes);
                    printf("La solicitud numero %d se ha identificado mal\n", contadorSolicitudes);
                    listaClientes[posCliente].tipoAtencion=1;

                //- 10% se ha confunfido de compañia
                }else{
                    sleep(calculaAleatorios(1,2));
                    sprintf(mensajeTipoAtencion, "La solicitud %d se ha confundido de compañia", contadorSolicitudes);
                    printf("La solicitud numero %d se confundido de compañia\n", contadorSolicitudes);
                    listaClientes[posCliente].tipoAtencion=2;
                }

                sprintf(mensajeTecnico,"Atencion a solicitud %d finalizada", contadorSolicitudes);
                writeLogMessage(idTecnico,mensajeTecnico);
                printf("El tecnico %ld ha atendido la solicitud %d \n", (intptr_t)ptr, contadorSolicitudes);

                writeLogMessage(idTecnico,mensajeTipoAtencion);

                //Cambiamos flag de atendido
                listaClientes[posCliente].atendido=-1;

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

                //Cambiamos flag de atendiendo
                listaTecnicos[posTecnico].atendiendo=1;

                //Calculamos el tipo de atencion del cliente
                int num = calculaAleatorios(0,100);
                char mensajeTipoAtencion[100];
                char mensajeTecnico[100];
                char idTecnico[10];
                sprintf(idTecnico,"Tecapp_%ld",(intptr_t)ptr);
                sprintf(mensajeTecnico,"Va a atender la solicitud %d", contadorSolicitudes);
                writeLogMessage(idTecnico,mensajeTecnico);
                printf("El tecnico %ld va a atender la solicitud %d \n", (intptr_t)ptr, contadorSolicitudes);

                //- 80% tiene todo en orden
                if(num<80){
                    sleep(calculaAleatorios(1,4));
                    sprintf(mensajeTipoAtencion, "La solicitud %d tiene todo en orden", contadorSolicitudes);
                    printf("La solicitud numero %d tiene todo en orden\n", contadorSolicitudes);
                    listaClientes[posCliente].tipoAtencion=0;

                //- 10% mal identificados
                }else if(num<90){
                    sleep(calculaAleatorios(2,6));
                    sprintf(mensajeTipoAtencion, "La solicitud %d se ha identificado mal", contadorSolicitudes);
                    printf("La solicitud numero %d se ha identificado mal\n", contadorSolicitudes);
                    listaClientes[posCliente].tipoAtencion=1;

                //- 10% se ha confunfido de compañia
                }else{
                    sleep(calculaAleatorios(1,2));
                    sprintf(mensajeTipoAtencion, "La solicitud %d se ha confundido de compañia y se va", contadorSolicitudes);
                    printf("La solicitud numero %d se confundido de compañia\n", contadorSolicitudes);
                    listaClientes[posCliente].tipoAtencion=2;
                }

                sprintf(mensajeTecnico,"Atencion a solicitud %d finalizada", contadorSolicitudes);
                writeLogMessage(idTecnico,mensajeTecnico);
                printf("El tecnico %ld ha atendido la solicitud %d \n", (intptr_t)ptr, contadorSolicitudes);

                writeLogMessage(idTecnico,mensajeTipoAtencion);

                //Cambiamos flag de atendido
                listaClientes[posCliente].atendido=-1;

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
            char mensajeTipoAtencion[100];
            char mensajeEncargado[100];
            sprintf(mensajeEncargado,"Va a atender la solicitud %d", contadorSolicitudes);
            writeLogMessage("Encargado",mensajeEncargado);
            printf("El encargado va a atender la solicitud %d \n", contadorSolicitudes);

            //- 80% tiene todo en orden
            if(num<80){
                sleep(calculaAleatorios(1,4));
                sprintf(mensajeTipoAtencion, "La solicitud %d tiene todo en orden", contadorSolicitudes);
                printf("La solicitud numero %d tiene todo en orden\n", contadorSolicitudes);
                listaClientes[posCliente].tipoAtencion=0;
                    
            //- 10% mal identificados
            }else if(num<90){
                sleep(calculaAleatorios(2,6));
                sprintf(mensajeTipoAtencion, "La solicitud %d se ha identificado mal", contadorSolicitudes);
                printf("La solicitud numero %d se ha identificado mal\n", contadorSolicitudes);
                listaClientes[posCliente].tipoAtencion=1;

            //- 10% se ha confunfido de compañia
            }else{
                sleep(calculaAleatorios(1,2));
                sprintf(mensajeTipoAtencion, "La solicitud %d se ha confundido de compañia", contadorSolicitudes);
                printf("La solicitud numero %d se ha confundido de compañia\n", contadorSolicitudes);
                listaClientes[posCliente].tipoAtencion=2;
            }

            sprintf(mensajeEncargado,"Atencion a solicitud %d finalizada", contadorSolicitudes);
            writeLogMessage("Encargado",mensajeEncargado);
            printf("El encargado ha atendido la solicitud %d \n", contadorSolicitudes);

            writeLogMessage("Encargado",mensajeTipoAtencion);

            //Cambiamos flag de atendido
            listaClientes[posCliente].atendido=-1;

            //Cambiamos flag de atendiendo
            listaTecnicos[4].atendiendo=0;

            pthread_mutex_unlock(&mutexSolicitudesEncargado);
        }
    }
}


void eliminarCliente(int pos){
    int tipo=listaClientes[pos].tipo;
	pthread_mutex_lock(&mutexColaClientes);
	for(int i=pos;i<contadorApp+contadorRed-1;i++){
		listaClientes[i] = listaClientes[i+1];  
	}
	if(tipo==0)
        contadorApp--;
    if(tipo==1)
        contadorRed--;
  
	pthread_mutex_unlock(&mutexColaClientes);
}

int calculaAleatorios(int min, int max){
    return rand() % (max-min+1) + min;
}

void *accionesTecnicoDomiciliario (void *ptr){

      while(TRUE){
    
        pthread_mutex_lock(&mutexSolicitudesDom);

        // se queda bloqueado el mutex si hay menos de 4 solicitudes
        while(contadorSolicitudesDom<4){
            pthread_cond_wait(&condicionTecnicoDomiciliario, & mutexSolicitudesDom);
        }

        // guardamos en el log que va a comenzar la atencion
        char numSolicitud[100];
        char idTecDom[10];
        sprintf(idTecDom,"TecDom");
        writeLogMessage(idTecDom, "Va comenzar la atención");

        printf("Tecnico domiciliario empieza atencion domiciliaria\n");
        for(int i=0; i<contadorApp+contadorRed && i<20; i++){
            sleep(1);
            // cambiamos el flag de solicitud del cliente
            if(listaClientes[i].solicitud==1){
                // guardamos en el log que el cliente ha sido atendido
                sprintf(numSolicitud, "Atiende la solicitud nº %d", i);
                writeLogMessage(idTecDom, numSolicitud);
                // cambiamos el flag de solicitud del cliente
                listaClientes[i].solicitud=0;
            }
                
        }
        
        // ponemos las solicitudes a 0
        contadorSolicitudesDom=0;

        // guardamos en el log que se ha finalizado la atencion
        writeLogMessage(idTecDom, "Termina atencion");
        printf("Tecnico domiciliario termina atencion\n");
    
        pthread_cond_signal(&condicionClienteDomiciliario);

        pthread_mutex_unlock(&mutexSolicitudesDom);
    }
}


void terminar(int signal){
    char mensaje[100];
    printf("Se va a finalizar el programa.\n");
    sprintf(mensaje, "Se va a finalizar el programa.\n");
    writeLogMessage("AVISO", mensaje);

    //Bloquear el mutex de las solicitudes domiciliarias
    pthread_mutex_lock(&mutexSolicitudesDom);

    //Cerrar solicitudes domiciliarias
    contadorSolicitudesDom = 0;

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
            writeLogMessage("AVISO", mensaje);
            
            exit(0);
        } else{
            //Desbloquear el mutex hasta que se acaben los clientes
            pthread_mutex_unlock(&mutexColaClientes);
            sleep(1);
        }
    }
}

void writeLogMessage(char *id, char *msg) {
    pthread_mutex_lock(&mutexFichero);
    // Calculamos la hora actual
    time_t now = time(0);
    struct tm *tlocal = localtime(&now);
    char stnow[25];
    strftime(stnow, 25, "%d/%m/%y %H:%M:%S", tlocal);
    // Escribimos en el log
    logFile = fopen("registroTiempos.log", "a");
    fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg);
    fclose(logFile);
    pthread_mutex_unlock(&mutexFichero);
}
