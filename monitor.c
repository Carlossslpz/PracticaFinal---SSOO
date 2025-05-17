#include "comun.c"

//Definimos todas las funciones
void  escribirLog(char * mensaje);
void escribirBanco(char * mensaje);
void seniales(int seniales);
void * buscarIngresosMaximos(void * arg);
void * buscarTotalIngresos(void * arg);

void * buscarRetirosMaximos(void * arg);
void * buscarTotalRetiros(void * arg);

void * buscarTransferenciasMaximas(void * arg);
void * buscarTotalTransferencias(void * arg);
void * buscarTransferenciasEntreCuentas(void * arg);

void obtenerFechas(char * fichero_trans);
void leerMemoriaCompartida();



//Creamos las estructuras
typedef struct 
{
    int maximo_retiros;
    int maximo_tranferencias;
    int maximo_ingresos;
    int maximo_transferencias_entre_cuentas;

    float maxima_cantidad_retiro;
    float maxima_cantidad_transferencia;
    float maxima_cantidad_ingreso;
}MAXIMOS;

typedef struct 
{
    char  mensajes[20][400];
    int indice;
}PILA;


typedef struct 
{
    //datos del USUARIO_MONITOR
    int id; 
    //Lo que lleva ingresado
    float cantidad_ingresada;
    float cantidad_retirada;
    float cantidad_transferida;

    //El numero de veces que ha hecho cada transaccion
    int num_retiros;
    int num_transferencias;
    int numero_ingresos;

    //Aqui guardo una tupla con el id de USUARIO_MONITOR y las veces que ha tranferido ej;
    //[1][2] al USUARIO_MONITOR 1 le ha tranferido 2 veces , tal que mas de 3 por sesion es fraude 
    int usuarios_transferidos[200][2];
    int n_usus_trans;
    
}USUARIO_MONITOR;

//Iniciamos variables globales

typedef struct 
{
    USUARIO_MONITOR lista[255];
    int index;
}LISTAUSERS;

typedef struct
{
    char * lista[300];
    int index;
}LISTAFECHAS;

MEMORIA * listaUsers;
MAXIMOS maximos;
char *fichero_log_global;
LISTAFECHAS listaFechas;
pid_t pid_programa,pid_banco;
sem_t *semaforo_hilos,*semaforo_log,*semaforo_trans,*semaforo_fifo,*semaforo_memoria;
int semaforos_usados[4], fd_memoria;
USUARIO_MONITOR user;


int main(int argc , char * argv[])
{
    //Manejamos las señales
    signal(SIGINT ,seniales);
    signal(SIGHUP,seniales);
    signal(SIGTERM,seniales);
    signal(SIGUSR1,seniales);
    signal(SIGKILL,seniales);
   
    //Creamos las variables
    pthread_t hilos_busqueda[10];
    char mensaje[1024];
    int fd,i,j;

    //Configuramos las variables pasadas por parametro
    fichero_log_global = argv[1];
    pid_banco = (pid_t)atoi(argv[2]);
    maximos.maxima_cantidad_ingreso = atoi(argv[3]);
    maximos.maxima_cantidad_retiro = atoi(argv[4]);
    maximos.maxima_cantidad_transferencia = atoi(argv[5]);

    maximos.maximo_ingresos = atoi(argv[6]);
    maximos.maximo_retiros = atoi(argv[7]);
    maximos.maximo_tranferencias = atoi(argv[8]);
    maximos.maximo_transferencias_entre_cuentas = atoi(argv[9]);


    //Configuramos las variables globales
    pid_programa = getpid();
    
    
   //Iniciamos los semaforos
    semaforo_hilos= sem_open(SEMAFORO_CONTROL_HILOS,0);
    semaforo_log = sem_open(SEMAFORO_MUTEX_LOG,0);
    semaforo_trans = sem_open(SEMAFORO_MUTEX_TRANSACCIONES,0);
    semaforo_fifo = sem_open(SEMAFORO_MUTEX_FIFO,0);
    semaforo_memoria = sem_open(SEMAFORO_MUTEX_MEMORIA,0);
    //Contolamos el fallo de los semaforos
    //Si alguno falla, se debera morir monitor ya que sin semaforos no puede haber control 
    if (semaforo_hilos == SEM_FAILED)
    { 

        snprintf(mensaje,sizeof(mensaje),"Ha fallado el semaforo de transacciones del programa monitor, error code %d",errno);
        escribirLog(mensaje);
        //Activo una señal para que el banco se miuera
        kill(getpid(),SIGTERM);
        exit(0);
    }
    if (semaforo_memoria == SEM_FAILED)
    {
        snprintf(mensaje,sizeof(mensaje),"Ha fallado el semaforo de memoria del programa monitor, error code %d",errno);
        escribirLog(mensaje);
        //Activo una señal para que el banco se miuera
        kill(getpid(),SIGTERM);
        exit(0);
    }

    if (semaforo_fifo == SEM_FAILED)
    { 

        snprintf(mensaje,sizeof(mensaje),"Ha fallado el semaforo de FIFO del programa monitor, error code %d",errno);
        escribirLog(mensaje);
        //Activo una señal para que el banco se miuera
        kill(getpid(),SIGTERM);
        exit(0);
    }


    if (semaforo_log == SEM_FAILED)
    {
        snprintf(mensaje,sizeof(mensaje),"Ha fallado el semaforo de log del programa monitor, error code %d",errno);
        escribirLog(mensaje);
        //No me automato ya que eso rquiere el log sino que mato a banco 
        
        exit(0);
    }

    if (semaforo_trans == SEM_FAILED)
    {
        snprintf(mensaje,sizeof(mensaje),"Ha fallado el semaforo de log del programa monitor, error code %d",errno);
        escribirLog(mensaje);
        //No me automato ya que eso rquiere el log sino que mato a banco 
        
        exit(0);
    }

    //Avisamos al banco que estamos activos
    snprintf(mensaje,sizeof(mensaje),"0-%d",pid_programa);
    escribirBanco(mensaje);
    
    
    //Ponemos en el log que nos hemos iniciado
    snprintf(mensaje,sizeof(mensaje),"Monitor iniciado con pid %d",pid_programa);
    escribirLog(mensaje);

   leerMemoriaCompartida();

   

    for (i = 0; i<listaUsers->n_users;i++)
    {
       
        snprintf(mensaje,sizeof(mensaje),"Monitor con pid %d empieza a analizar al usuario %d",pid_programa,listaUsers->lista[i].id);
        escribirLog(mensaje);
        //Creamos el usuario para
        user.id = listaUsers->lista[i].id;

        obtenerFechas(listaUsers->lista[i].fichero);
        pthread_create(&hilos_busqueda[0],NULL,buscarIngresosMaximos,listaUsers->lista[i].fichero);
        pthread_create(&hilos_busqueda[1],NULL,buscarTotalIngresos,listaUsers->lista[i].fichero);
        pthread_create(&hilos_busqueda[2],NULL,buscarRetirosMaximos,listaUsers->lista[i].fichero);
        pthread_create(&hilos_busqueda[3],NULL,buscarTotalRetiros,listaUsers->lista[i].fichero);
        pthread_create(&hilos_busqueda[4],NULL,buscarTransferenciasMaximas,listaUsers->lista[i].fichero);
        pthread_create(&hilos_busqueda[5],NULL,buscarTotalTransferencias,listaUsers->lista[i].fichero);
        pthread_create(&hilos_busqueda[6],NULL,buscarTransferenciasEntreCuentas,listaUsers->lista[i].fichero);


        pthread_join(hilos_busqueda[0],NULL);
        pthread_join(hilos_busqueda[1],NULL);
        pthread_join(hilos_busqueda[2],NULL);
        pthread_join(hilos_busqueda[3],NULL);
        pthread_join(hilos_busqueda[4],NULL);
        pthread_join(hilos_busqueda[5],NULL);
        pthread_join(hilos_busqueda[6],NULL);
        //Una vez que hemos terminado de leer el fichero lo cerramos
        snprintf(mensaje,sizeof(mensaje),"Monitor con pid %d ha terminado de analizar al usuario %d",pid_programa,listaUsers->lista[i].id);
        escribirLog(mensaje);

        for (j = 0; j<listaFechas.index;j++)
        {
            free(listaFechas.lista[j]);
        }
      
    }
    

   close(fd_memoria);

    snprintf(mensaje,sizeof(mensaje),"Monitor con pid %d ha terminado",pid_programa);
    escribirLog(mensaje);
    //Avisamos al banco que hemos terminado
    snprintf(mensaje,sizeof(mensaje),"0-%d",pid_programa);
    escribirBanco(mensaje);



    return 0;
}

//Nombre: generarListasDatos.
//Retorno: void.
//Parámetros: Ninguno.
//Uso: Lee el fichero de transacciones global, extrae las fechas y los IDs de los USUARIO_MONITORs, 
//      y genera las listas correspondientes.
void obtenerFechas( char * fichero_trans)
{
    FILE * fichero;
    int i,j;
    char mensaje[1024],linea[300];
    bool esta_en_fecha;
    char *fecha,* id,*operacion,*cantidad;

    escribirLog("Se empieza a generar listas de datos...");
    //sem_wait(semaforo_trans);
    semaforos_usados[1]=1;
    fichero = fopen(fichero_trans,"r");
    if (fichero == NULL)
    {
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir el fichero %s",errno,fichero_trans);
        escribirLog(mensaje);
        //sem_post(semaforo_trans);
        semaforos_usados[1]=0;
        return;
    }

    //Inicializamos los indices para evitar fallos  
    listaFechas.index=  0;

    //Obtenemos los datos de las lineas
  
    while (fgets(linea,sizeof(linea),fichero) != NULL)
    {
        
        //Para cada linea veremos si la fecha y el id de USUARIO_MONITOR ya existen
        esta_en_fecha = false;

        linea[strcspn(linea, "\n")] = 0;
        fecha = strtok(linea,"-");
        
       //Vemos si la fecha ya existe en la lista de fechas
        for (i = 0; i<listaFechas.index;i++)
        {
            if (strcmp(listaFechas.lista[i],fecha) == 0)
            {
                esta_en_fecha = true;
                break;
            }      
        }
        //Si no existe la fecha la añadimos
        if (!esta_en_fecha) listaFechas.lista[listaFechas.index++] = strdup(fecha);
     
        
    }

    fclose(fichero);
    //sem_post(semaforo_trans);
    semaforos_usados[1]=0;
    escribirLog("Se han generado las listas de datos...");
    return;

   
}

//Nombre: seniales.
//Retorno: void.
//Parámetros: seniales (int).
//Uso: Maneja las señales recibidas (SIGINT, SIGHUP, SIGTERM, SIGKILL, SIGUSR1), 
//      notifica al banco y cierra el proceso si es necesario.
void seniales(int seniales)
{
    

    int fd;
    char mensaje[255];
    pthread_t hilo_comunicar;
    if (seniales == SIGINT || seniales == SIGHUP || seniales == SIGTERM || seniales == SIGKILL)
    {
       //Si recivo esto es que me han matado o he tenido algun fallo por tanto el banco debe morir 
       //ya que no tiene sentido que el funcione sin mi

       //Notifico al banco que me han matado para que me quite de la lista de procesos activos
        snprintf(mensaje,sizeof(mensaje),"0-%d",pid_programa);  
        escribirBanco(mensaje);
        

        //Notifico en el log que me he cerrdo
        escribirLog("Monitor se ha cerrado por un fallo");

        if ( semaforos_usados[0] == 1) sem_post(semaforo_hilos);
        if ( semaforos_usados[1] == 1) sem_post(semaforo_trans);
        if ( semaforos_usados[2] == 1) sem_post(semaforo_log);
        if ( semaforos_usados[3] == 1) sem_post(semaforo_fifo);

        //Obligo al banco a cerrase
        kill(pid_banco,SIGUSR2);
        exit(1);
    }
    else if (seniales == SIGUSR1)
    {

        //Escribo en el log que me he cerrado
        escribirLog("Monitor se ha cerrado por el banco");

        if ( semaforos_usados[0] == 1) sem_post(semaforo_hilos);
        if ( semaforos_usados[1] == 1) sem_post(semaforo_trans);
        if ( semaforos_usados[2] == 1) sem_post(semaforo_log);
        if ( semaforos_usados[3] == 1) sem_post(semaforo_fifo);

        exit(0);
    
    }

    return;
}


//----------------------------------------------------------INGRESOS---------------------------------------------------------------------
void * buscarIngresosMaximos(void * arg)
{
    //Bloqueamos un hilo
    sem_wait(semaforo_hilos);
    semaforos_usados[0]=1;

    //Creamos las variables
    int i,j;
    FILE * fichero;  
    char mensaje[400],linea[255];
    char *fecha,* id,*operacion,*cantidad;
    char *nombre_fichero;

    char * alertas[400];
    int alertas_index = 0;

    nombre_fichero = (char *)arg;

    escribirLog("Monitor empieza a buscar ingresos maximos");
    fichero = fopen(nombre_fichero,"r");
    if (fichero == NULL)
    {
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir el fichero %s",errno,nombre_fichero);
        escribirLog(mensaje);
        //En caso de error cerramos el programa ya que sin poder acceder al log no tiene sentido que funcione
        sem_post(semaforo_hilos);
        semaforos_usados[0]=0;
        //Si no puedo abrir el fichero de transacciones no puedo hacer nada asi que me muero
        kill(pid_programa,SIGTERM);
        return NULL;
    }

    
    //Buscamos el maximo de cada fecha
    for ( i = 0; i<listaFechas.index;i++)
    {
        //Reiniciamos el fichero para leerlo de nuevo
        rewind(fichero);
       
        //Reiniciamos los contadores de ingresos de USUARIO_MONITORpara cada fecha
        user.cantidad_ingresada = 0;

        while (fgets(linea,sizeof(linea),fichero) != NULL)
        {
            //Quito el \n
            linea[strcspn(linea,"\n")] = 0;

            //Obtenemos los datos de cada linea
            fecha = strtok(linea,"-");
            operacion = strtok(NULL,"-");
            id = strtok(NULL,"-");
            cantidad = strtok(NULL,"-");

            //Si la fecha no coincide con la que buscamos o la operacion no es un ingreso pasamos
            if (strcmp(fecha,listaFechas.lista[i]) != 0 || strcmp(operacion,"DEPOSITO") != 0) continue;
            
              //Vemos si el id de USUARIO_MONITOR ya existe en la lista de USUARIO_MONITORs
            
            //Si existe lo sumamos a la cantidad ingresada
            user.cantidad_ingresada += atof(cantidad);

                    //Si el conjunto de cantidad ingresada es mayor al maximo lo notificamos
            if (user.cantidad_ingresada >= maximos.maxima_cantidad_ingreso)
            {
                //Si el ingreso es mayor al maximo lo notificamos
                snprintf(mensaje,sizeof(mensaje),"1-ALERTA: USUARIO CON ID %s HA SUPERADO LA MAXIMA CANTIDAD DE INGRESOS EL DIA %s\n",id,fecha);
                alertas[alertas_index++] = strdup(mensaje);            
            }
                
            
            
        }
    }

    //Una vez que hemos terminado de leer el fichero lo cerramos
    fclose(fichero);

    for (i = 0; i<alertas_index;i++)
    {
        escribirBanco(alertas[i]);
        free(alertas[i]);
    }

    escribirLog("Monitor ha terminado de buscar ingresos maximos raros");
    //Liberamos el hilo
    sem_post(semaforo_hilos);
    semaforos_usados[0]=1;
    return NULL;

}

void * buscarTotalIngresos(void * arg)
{
    //Bloqueamos un hilo
    sem_wait(semaforo_hilos);
    semaforos_usados[0]=1;

    //Creamos las variables
    int i,j;
    FILE * fichero;  
    char mensaje[400],linea[255];
    char *fecha,* id,*operacion,*cantidad;
    char *nombre_fichero;

    char * alertas[400];
    int alertas_index = 0;
    nombre_fichero = (char *)arg;

    escribirLog("Monitor empieza a buscar ingresos totales sospechosos");
    fichero = fopen(nombre_fichero,"r");
    if (fichero == NULL)
    {
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir el fichero %s",errno,nombre_fichero);
        escribirLog(mensaje);
        //En caso de error cerramos el programa ya que sin poder acceder al log no tiene sentido que funcione
        sem_post(semaforo_hilos);
        semaforos_usados[0]=0;
        //Si no puedo abrir el fichero de transacciones no puedo hacer nada asi que me muero
        kill(pid_programa,SIGTERM);
        return NULL;
    }

    
    //Buscamos el maximo de cada fecha
    for ( i = 0; i<listaFechas.index;i++)
    {
        //Reiniciamos el fichero para leerlo de nuevo
        rewind(fichero);
       
        //Reiniciamos los contadores de ingresos de USUARIO_MONITORpara cada fecha
        user.numero_ingresos = 0;

        while (fgets(linea,sizeof(linea),fichero) != NULL)
        {
            //Quito el \n
            linea[strcspn(linea,"\n")] = 0;

            //Obtenemos los datos de cada linea
            fecha = strtok(linea,"-");
            operacion = strtok(NULL,"-");
            id = strtok(NULL,"-");
            cantidad = strtok(NULL,"-");

            //Si la fecha no coincide con la que buscamos o la operacion no es un ingreso pasamos
            if (strcmp(fecha,listaFechas.lista[i]) != 0 || strcmp(operacion,"DEPOSITO") != 0) continue;
            
              //Vemos si el id de USUARIO_MONITOR ya existe en la lista de USUARIO_MONITORs
            
            //Si existe lo sumamos a la cantidad ingresada
            user.numero_ingresos += 1;

            //Si el conjunto de cantidad ingresada es mayor al maximo lo notificamos
            if (user.numero_ingresos >= maximos.maximo_ingresos)
            {
                //Si el ingreso es mayor al maximo lo notificamos
                snprintf(mensaje,sizeof(mensaje),"1-ALERTA: USUARIO CON ID %s HA SUPERADO EL TOTAL DE INGRESOS EL DIA %s\n",id,fecha);
                alertas[alertas_index++] = strdup(mensaje);
            }
              
            
            
        }
    }

    //Una vez que hemos terminado de leer el fichero lo cerramos
    fclose(fichero);

    for (i = 0; i<alertas_index;i++)
    {
        escribirBanco(alertas[i]);
        free(alertas[i]);
        usleep(10000);
    }
    escribirLog("Monitor ha terminado de buscar ingresos maximos raros");
    //Liberamos el hilo
    sem_post(semaforo_hilos);
    semaforos_usados[0]=1;
    return NULL;

}

//----------------------------------------------------------RETIROS---------------------------------------------------------------------

void * buscarRetirosMaximos(void * arg)
{
    //Bloqueamos un hilo
    sem_wait(semaforo_hilos);
    semaforos_usados[0]=1;

    //Creamos las variables
    int i,j;
    FILE * fichero;  
    char mensaje[400],linea[255];
    char *fecha,* id,*operacion,*cantidad;
    char *nombre_fichero;

    char * alertas[400];
    int alertas_index = 0;

    nombre_fichero = (char *)arg;

    escribirLog("Monitor empieza a buscar retiros maximos sospechosos...");
    fichero = fopen(nombre_fichero,"r");
    if (fichero == NULL)
    {
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir el fichero %s",errno,nombre_fichero);
        escribirLog(mensaje);
        //En caso de error cerramos el programa ya que sin poder acceder al log no tiene sentido que funcione
        sem_post(semaforo_hilos);
        semaforos_usados[0]=0;
        //Si no puedo abrir el fichero de transacciones no puedo hacer nada asi que me muero
        kill(pid_programa,SIGTERM);
        return NULL;
    }

    
    //Buscamos el maximo de cada fecha
    for ( i = 0; i<listaFechas.index;i++)
    {
        //Reiniciamos el fichero para leerlo de nuevo
        rewind(fichero);
       
        //Reiniciamos los contadores de ingresos de USUARIO_MONITORpara cada fecha
        user.cantidad_retirada = 0;

        while (fgets(linea,sizeof(linea),fichero) != NULL)
        {
            //Quito el \n
            linea[strcspn(linea,"\n")] = 0;

            //Obtenemos los datos de cada linea
            fecha = strtok(linea,"-");
            operacion = strtok(NULL,"-");
            id = strtok(NULL,"-");
            cantidad = strtok(NULL,"-");

            //Si la fecha no coincide con la que buscamos o la operacion no es un ingreso pasamos
            if (strcmp(fecha,listaFechas.lista[i]) != 0 || strcmp(operacion,"RETIRO") != 0) continue;
            
            //Vemos si el id de USUARIO_MONITOR ya existe en la lista de USUARIO_MONITORs
            
            //Si existe lo sumamos a la cantidad ingresada
            user.cantidad_retirada += atof(cantidad);

                    //Si el conjunto de cantidad ingresada es mayor al maximo lo notificamos
            if (user.cantidad_retirada >= maximos.maxima_cantidad_retiro)
            {
                //Si el ingreso es mayor al maximo lo notificamos
                snprintf(mensaje,sizeof(mensaje),"1-ALERTA: USUARIO CON ID %s HA SUPERADO LA MAXIMA CANTIDAD DE RETIROS EL DIA %s\n",id,fecha);
                alertas[alertas_index++] = strdup(mensaje);              
            }
                
            
            
        }
    }

    //Una vez que hemos terminado de leer el fichero lo cerramos
    fclose(fichero);

    for (i = 0; i<alertas_index;i++)
    {
        escribirBanco(alertas[i]);
        free(alertas[i]);
        usleep(10000);
    }

    escribirLog("Monitor ha terminado de buscar retiros maximos sospechosos...");
    //Liberamos el hilo
    sem_post(semaforo_hilos);
    semaforos_usados[0]=1;
    return NULL;

}
void * buscarTotalRetiros(void * arg)
{
    //Bloqueamos un hilo
    sem_wait(semaforo_hilos);
    semaforos_usados[0]=1;

    //Creamos las variables
    int i,j;
    FILE * fichero;  
    char mensaje[400],linea[255];
    char *fecha,* id,*operacion,*cantidad;
    char *nombre_fichero;

    char * alertas[400];
    int alertas_index = 0;

    nombre_fichero = (char *)arg;

    escribirLog("Monitor empieza a buscar total de retiros sospechosos...");
    fichero = fopen(nombre_fichero,"r");
    if (fichero == NULL)
    {
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir el fichero %s",errno,nombre_fichero);
        escribirLog(mensaje);
        //En caso de error cerramos el programa ya que sin poder acceder al log no tiene sentido que funcione
        sem_post(semaforo_hilos);
        semaforos_usados[0]=0;
        //Si no puedo abrir el fichero de transacciones no puedo hacer nada asi que me muero
        kill(pid_programa,SIGTERM);
        return NULL;
    }

    
    //Buscamos el maximo de cada fecha
    for ( i = 0; i<listaFechas.index;i++)
    {
        //Reiniciamos el fichero para leerlo de nuevo
        rewind(fichero);
       
        //Reiniciamos los contadores de ingresos de USUARIO_MONITORpara cada fecha
        user.num_retiros = 0;

        while (fgets(linea,sizeof(linea),fichero) != NULL)
        {
            //Quito el \n
            linea[strcspn(linea,"\n")] = 0;

            //Obtenemos los datos de cada linea
            fecha = strtok(linea,"-");
            operacion = strtok(NULL,"-");
            id = strtok(NULL,"-");
            cantidad = strtok(NULL,"-");

            //Si la fecha no coincide con la que buscamos o la operacion no es un ingreso pasamos
            if (strcmp(fecha,listaFechas.lista[i]) != 0 || strcmp(operacion,"RETIRO") != 0) continue;
            
              //Vemos si el id de USUARIO_MONITOR ya existe en la lista de USUARIO_MONITORs
            
            //Si existe lo sumamos a la cantidad ingresada
            user.num_retiros += 1;

            //Si el conjunto de cantidad ingresada es mayor al maximo lo notificamos
            if (user.num_retiros >= maximos.maximo_retiros)
            {
                //Si el ingreso es mayor al maximo lo notificamos
                snprintf(mensaje,sizeof(mensaje),"1-ALERTA: USUARIO CON ID %s HA SUPERADO EL TOTAL DE RETIROS EL DIA %s\n",id,fecha);
                alertas[alertas_index++] = strdup(mensaje);
            }
            

                
            
            
        }
    }

    //Una vez que hemos terminado de leer el fichero lo cerramos
    fclose(fichero);

    for (i = 0; i<alertas_index;i++)
    {
        escribirBanco(alertas[i]);
        free(alertas[i]);
        usleep(10000);
    }

    escribirLog("Monitor ha terminado de buscar total de retiros sospechosos...");
    //Liberamos el hilo
    sem_post(semaforo_hilos);
    semaforos_usados[0]=1;
    return NULL;

}

//-----------------------------------------------------------TRANSFERENCIAS-------------------------------------------------------------



void * buscarTransferenciasMaximas(void * arg)
{
    //Bloqueamos un hilo
    sem_wait(semaforo_hilos);
    semaforos_usados[0]=1;

    //Creamos las variables
    int i,j;
    FILE * fichero;  
    char mensaje[400],linea[255];
    char *fecha,* id,*operacion,*cantidad;
    char *nombre_fichero;

    char * alertas[400];
    int alertas_index = 0;

    nombre_fichero = (char *)arg;

    escribirLog("Monitor empieza a buscar transferencias maximos sospechosas...");
    fichero = fopen(nombre_fichero,"r");
    if (fichero == NULL)
    {
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir el fichero %s",errno,nombre_fichero);
        escribirLog(mensaje);
        //En caso de error cerramos el programa ya que sin poder acceder al log no tiene sentido que funcione
        sem_post(semaforo_hilos);
        semaforos_usados[0]=0;
        //Si no puedo abrir el fichero de transacciones no puedo hacer nada asi que me muero
        kill(pid_programa,SIGTERM);
        return NULL;
    }

    
    //Buscamos el maximo de cada fecha
    for ( i = 0; i<listaFechas.index;i++)
    {
        //Reiniciamos el fichero para leerlo de nuevo
        rewind(fichero);
       
        //Reiniciamos los contadores de ingresos de USUARIO_MONITORpara cada fecha
        user.cantidad_retirada = 0;

        while (fgets(linea,sizeof(linea),fichero) != NULL)
        {
            //Quito el \n
            linea[strcspn(linea,"\n")] = 0;

            //Obtenemos los datos de cada linea
            fecha = strtok(linea,"-");
            operacion = strtok(NULL,"-");
            id = strtok(NULL,"-");
            cantidad = strtok(NULL,"-");

            //Si la fecha no coincide con la que buscamos o la operacion no es un ingreso pasamos
            if (strcmp(fecha,listaFechas.lista[i]) != 0 || strcmp(operacion,"TRANSFERENCIA") != 0) continue;
            
            //Vemos si el id de USUARIO_MONITOR ya existe en la lista de USUARIO_MONITORs
            
            //Si existe lo sumamos a la cantidad ingresada
            user.cantidad_transferida += atof(cantidad);

                    //Si el conjunto de cantidad ingresada es mayor al maximo lo notificamos
            if (user.cantidad_transferida >= maximos.maxima_cantidad_transferencia)
            {
                //Si el ingreso es mayor al maximo lo notificamos
                snprintf(mensaje,sizeof(mensaje),"1-ALERTA: USUARIO CON ID %s HA SUPERADO LA MAXIMA CANTIDAD DE TRANSFERENCIAS EL DIA %s\n",id,fecha);
                alertas[alertas_index++] = strdup(mensaje);              
            }
           
            
            
        }
    }

    //Una vez que hemos terminado de leer el fichero lo cerramos
    fclose(fichero);

    for (i = 0; i<alertas_index;i++)
    {
        escribirBanco(alertas[i]);
        free(alertas[i]);
        usleep(10000);
    }

    escribirLog("Monitor ha terminado de buscar transferencias maximas sospechosos...");
    //Liberamos el hilo
    sem_post(semaforo_hilos);
    semaforos_usados[0]=1;
    return NULL;

}

void * buscarTotalTransferencias(void * arg)
{
    //Bloqueamos un hilo
    sem_wait(semaforo_hilos);
    semaforos_usados[0]=1;

    //Creamos las variables
    int i,j;
    FILE * fichero;  
    char mensaje[400],linea[255];
    char *fecha,* id,*operacion,*cantidad;
    char *nombre_fichero;

    char * alertas[400];
    int alertas_index = 0;

    nombre_fichero = (char *)arg;

    escribirLog("Monitor empieza a buscar total de transferencias sospechosos...");
    fichero = fopen(nombre_fichero,"r");
    if (fichero == NULL)
    {
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir el fichero %s",errno,nombre_fichero);
        escribirLog(mensaje);
        //En caso de error cerramos el programa ya que sin poder acceder al log no tiene sentido que funcione
        sem_post(semaforo_hilos);
        semaforos_usados[0]=0;
        //Si no puedo abrir el fichero de transacciones no puedo hacer nada asi que me muero
        kill(pid_programa,SIGTERM);
        return NULL;
    }

    
    //Buscamos el maximo de cada fecha
    for ( i = 0; i<listaFechas.index;i++)
    {
        //Reiniciamos el fichero para leerlo de nuevo
        rewind(fichero);
       
        //Reiniciamos los contadores de ingresos de USUARIO_MONITORpara cada fecha
        user.num_transferencias = 0;

        while (fgets(linea,sizeof(linea),fichero) != NULL)
        {
            //Quito el \n
            linea[strcspn(linea,"\n")] = 0;

            //Obtenemos los datos de cada linea
            fecha = strtok(linea,"-");
            operacion = strtok(NULL,"-");
            id = strtok(NULL,"-");
            cantidad = strtok(NULL,"-");

            //Si la fecha no coincide con la que buscamos o la operacion no es un ingreso pasamos
            if (strcmp(fecha,listaFechas.lista[i]) != 0 || strcmp(operacion,"TRANFERENCIA") != 0) continue;
            
              //Vemos si el id de USUARIO_MONITOR ya existe en la lista de USUARIO_MONITORs
            
            //Si existe lo sumamos a la cantidad ingresada
            user.num_transferencias += 1;

            //Si el conjunto de cantidad ingresada es mayor al maximo lo notificamos
            if (user.num_transferencias >= maximos.maximo_tranferencias)
            {
                //Si el ingreso es mayor al maximo lo notificamos
                snprintf(mensaje,sizeof(mensaje),"1-ALERTA: USUARIO CON ID %s HA SUPERADO EL TOTAL DE TRANSFERENCIAS EL DIA %s\n",id,fecha);
                alertas[alertas_index++] = strdup(mensaje);            
            }

            
        }
    }

    //Una vez que hemos terminado de leer el fichero lo cerramos
    fclose(fichero);

    for (i = 0; i<alertas_index;i++)
    {
        escribirBanco(alertas[i]);
        free(alertas[i]);
        usleep(10000);
    }

    escribirLog("Monitor ha terminado de buscar total de transferencias sospechosos...");
    //Liberamos el hilo
    sem_post(semaforo_hilos);
    semaforos_usados[0]=1;
    return NULL;

}





void * buscarTransferenciasEntreCuentas(void * arg)
{
    //Bloqueamos un hilo
    sem_wait(semaforo_hilos);
    semaforos_usados[0]=1;

    //Creamos las variables
    int i,j,k;
    FILE * fichero;  
    char mensaje[400],linea[255];
    char *fecha,* id,*operacion,*cantidad,*id_tranf;
    bool encontrado = false;
    char * fichero_user = arg;
    char * alertas[255];
    int num_alertas = 0;
    

    escribirLog("Monitor empieza a buscar multiples transferencias entre cuentas en un dia");
    fichero = fopen(fichero_user,"r");
    if (fichero == NULL)
    {
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir el fichero %s",errno,fichero_user);
        escribirLog(mensaje);
        //En caso de error cerramos el programa ya que sin poder acceder al log no tiene sentido que funcione
        sem_post(semaforo_hilos);
        semaforos_usados[0]=0;
        //Si no puedo abrir el fichero de transacciones no puedo hacer nada asi que me muero
        kill(pid_programa,SIGTERM);
        return NULL;
    }

    
    //Buscamos el maximo de cada fecha
    for ( i = 0; i<listaFechas.index;i++)
    {
        //Reiniciamos el fichero para leerlo de nuevo
        rewind(fichero);
       
        //Reiniciamos los contadores de ingresos de USUARIO_MONITORpara cada fecha
        for ( j = 0 ; j<user.n_usus_trans;j++)
        {
            user.usuarios_transferidos[j][0] = 0;
            user.usuarios_transferidos[j][1] = 0;
        }
        user.n_usus_trans = 0;
        

        while (fgets(linea,sizeof(linea),fichero) != NULL)
        {
            //Quito el \n
            linea[strcspn(linea,"\n")] = 0;
           

            //Obtenemos los datos de cada linea
            fecha = strtok(linea,"-");
            operacion = strtok(NULL,"-");
            id = strtok(NULL,"-");
            cantidad = strtok(NULL,"-");
            id_tranf = strtok(NULL,"-");

            //Si la fecha no coincide con la que buscamos o la operacion no es un ingreso pasamos
            if (strcmp(fecha,listaFechas.lista[i]) != 0 || strcmp(operacion,"TRANSFERENCIA") != 0) continue;
            
            encontrado = false;
            //Para cada user miramos su lista de tranferidoas a ver si h atransferido a alguien
            for ( k = 0; k<user.n_usus_trans;k++)
            {
    
                if (user.usuarios_transferidos[k][0] == atoi(id_tranf))
                {
                    encontrado = true;
                    break;
                }
                
            }
            if (encontrado)
            {
                //Si ya existe le sumamos uno
                user.usuarios_transferidos[k][1] += 1;

                //Si el numero de transferencias entre cuentas es mayor al maximo lo notificamos
                if (user.usuarios_transferidos[k][1] >= maximos.maximo_transferencias_entre_cuentas)
                {
                    //Si el ingreso es mayor al maximo lo notificamos
                    snprintf(mensaje,sizeof(mensaje),"1-ALERTA: USUARIO CON ID %s HA SUPERADO EL MAXIMO NUMERO DE TRANSFERENCIAS ENTRE CUENTAS CON %s EL DIA %s\n",id,id_tranf,fecha);
                    alertas[num_alertas] = strdup(mensaje);
                    num_alertas++;
                    
                }
                
                //Si el numero de transferencias entre cuentas es mayor al maximo lo notificamos
                    
            }
            else 
            {
                
                //Sino esta le agregamos
                user.usuarios_transferidos[user.n_usus_trans][0] = atoi(id_tranf);
                user.usuarios_transferidos[user.n_usus_trans][1] = 1;
                user.n_usus_trans++;
            }
            
        }
            
            
        
    }

    //Una vez que hemos terminado de leer el fichero lo cerramos
    fclose(fichero);

    for ( j = 0; j<num_alertas;j++)
    {
        escribirBanco(alertas[j]);
        free(alertas[j]);
        usleep(10000); // 10ms de espera para dar tiempo a leer
    }

    escribirLog("Monitor ha terminado de buscar multiples transferencias entre cuentas");
    //Liberamos el hilo
    sem_post(semaforo_hilos);
    semaforos_usados[0]=0;
    return NULL;

}




//----------------------------------------------------------OTROS-------------------------------------------------------------
//Nombre: escribirLog.
//Retorno: void.
//Parámetros: mensaje (char *).
//Uso: Escribe un mensaje en el fichero de log especificado, incluyendo la fecha y hora, 
//      y protege el acceso al fichero con semáforo.
void  escribirLog(char * mensaje)
{
    
    //Creamos las variables
    FILE * file;
    char * hora;
    time_t tiempo;
    char error[255];
    
    
    //Abrimos el fichero que se pasa por parametro, se hace asi ya que como el fichero viene del main
    //si en el main se cambia el fichero no es necesario actualizar este programa
    sem_wait(semaforo_log);
    semaforos_usados[2]= 1;
    file = fopen(fichero_log_global,"a");

    if (file == NULL)
    {
        //En caso de error cerramos el programa ya que sin poder acceder al log no tiene sentido que funcione
        fprintf(stderr,"Fallo %d al abrir el archivo %s por parte de monitor",errno,fichero_log_global);
        snprintf(error,sizeof(error),"Fallo %d al abrir el archivo  %s por parte de monitor",errno,fichero_log_global);
        //Avisamos al banco por si el si que puede para que notifique el error
        escribirBanco(error);
        sem_post(semaforo_log);
        semaforos_usados[2]= 0;
        //Nos enviamos una señal para matarnos y que asi cierre tambien monitor
        kill(pid_programa,SIGTERM);
    }
    //Al ser un log hay que poner fecha y hora, lo cual se hace asi
    time(&tiempo);
    //Lo pasamos a formato string
    hora = ctime(&tiempo);
    //quitamos el \n que por defecto pone la liberia de la hora
    hora[strcspn(hora,"\n")] = '\0';
    //Excribimos el mensaje con la hora
    fprintf(file,"[%s] - %s\n",hora,mensaje);
    
    //Cerramos el fichero
    fclose(file);

    //Liberamos el hilo y el semaforor mutex del fichero
    sem_post(semaforo_log);
    semaforos_usados[2]= 0;
    
    return ;
}
//Nombre: escribirBanco.
//Retorno: void.
//Parámetros: mensaje (char *).
//Uso: Envia un mensaje al banco a través de un FIFO, protegiendo el acceso con semáforo, 
//      y maneja los errores si no se puede abrir el FIFO.
void  escribirBanco(char * mensaje)
{
   
    sem_wait(semaforo_fifo);
    semaforos_usados[3] =1;
    //Creamos las variables
    int fd;
    char error[255];
    pthread_t hilo_log;
  
    //Hacemos un cast del mensaje porque queremos que sea un char
    char copia[255];
    snprintf(copia,sizeof(copia),"%s",mensaje);

    fd = open(FIFO_PROGRAMAS_BANCO,O_WRONLY);
    //Si fd es -1 es que ha habido error asi que notificamos 
    if ( fd == -1)
    {
        
        snprintf(error,sizeof(error),"Error %d al abrir el fifo de monitor a banco",errno);
        escribirLog(error);
        fprintf(stderr,"%s\n",error);
        sem_post(semaforo_fifo);
        semaforos_usados[3] =0;
        //En este caso como es una funcion clave ya que sin una comunicacion entre ambos
        //Puede haber errores forzamos la salida
        kill(pid_programa,SIGTERM);
    }
    //enviamos el mensaje
    write(fd,copia,strlen(copia)+1);
    close(fd);
    sem_post(semaforo_fifo);
    semaforos_usados[3] =0;
    
    //Liberamos el hilo
    
    return;
}


void leerMemoriaCompartida()
{
    
    int i;
    char linea[255],mensaje[255];
    snprintf(mensaje,sizeof(mensaje),"Monitor con pid %d empieza a leer la memoria compartida",pid_programa);
    escribirLog(mensaje);
 
    //Creamos el semaforo para la memoria compartida
    sem_wait(semaforo_memoria);
    semaforos_usados[1] = 1;
    fd_memoria = shm_open(MEMORIA_COMPARTIDA , O_RDWR,0666);

    if (fd_memoria == -1 )
    {
        snprintf(mensaje, sizeof(mensaje),"Error al abrir la memoria compartida del usuario con pid %d",pid_programa);
        fprintf(stderr,"%s\n",mensaje);
        escribirLog(mensaje);
        sem_post(semaforo_memoria);
        semaforos_usados[1] = 0;
        kill(SIGTERM,pid_programa);
    } 

  
    listaUsers = mmap(0,sizeof(MEMORIA),PROT_READ | PROT_WRITE,MAP_SHARED,fd_memoria,0);
    if (listaUsers ==MAP_FAILED)
    {
        snprintf(mensaje, sizeof(mensaje),"Error al leer la memoria compartida del usuario con pid %d",pid_programa);
        fprintf(stderr,"%s\n",mensaje);
        escribirLog(mensaje);
        sem_post(semaforo_memoria);
        semaforos_usados[1] = 0;
        kill(SIGTERM,pid_programa);
    }

    

    //Liberamos el semaforo
    sem_post(semaforo_memoria);
    semaforos_usados[1] = 0;
    snprintf(mensaje,sizeof(mensaje),"Monitor con pid %d ha terminado de leer la memoria compartida",pid_programa);
    escribirLog(mensaje);
    

}