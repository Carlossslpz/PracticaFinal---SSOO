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

void generarListasDatos();



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
    int USUARIO_MONITORs_transferidos[2][200];
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

MAXIMOS maximos;
char *fichero_log_global,*fichero_trans_global;
LISTAUSERS listaUSUARIO_MONITORs;
LISTAFECHAS listaFechas;
pid_t pid_programa,pid_banco;
sem_t *semaforo_hilos,*semaforo_log,*semaforo_trans,*semaforo_fifo;
pthread_mutex_t sem_fifo;
int semaforos_usados[4];



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
    char mensaje[255];
    int fd,i;

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
    fichero_trans_global = argv[10];

    //Configuramos las variables globales
    pid_programa = getpid();
    
    
   //Iniciamos los semaforos
    semaforo_hilos= sem_open(SEMAFORO_CONTROL_HILOS,0);
    semaforo_log = sem_open(SEMAFORO_MUTEX_LOG,0);
    semaforo_trans = sem_open(SEMAFORO_MUTEX_TRANSACCIONES,0);
    semaforo_fifo = sem_open(SEMAFORO_MUTEX_FIFO,0);
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

   
    generarListasDatos();

    //Bloqueamos los esemafors para que nadie modifique el fichero de cuentas
    //Como solo van a leer me da igual que varios hilos estan a la vez sobre el fichero
    sem_wait(semaforo_trans);
    pthread_create(&hilos_busqueda[0],NULL,buscarTotalIngresos,NULL);
    pthread_create(&hilos_busqueda[1],NULL,buscarIngresosMaximos,NULL);
    pthread_create(&hilos_busqueda[2],NULL,buscarTransferenciasMaximas,NULL);
    pthread_create(&hilos_busqueda[3],NULL,buscarTotalTransferencias,NULL);
    pthread_create(&hilos_busqueda[4],NULL,buscarTotalRetiros,NULL);
    pthread_create(&hilos_busqueda[5],NULL,buscarRetirosMaximos,NULL);
    pthread_create(&hilos_busqueda[6],NULL,buscarTransferenciasEntreCuentas,NULL);

    
    pthread_join(hilos_busqueda[0],NULL);
    pthread_join(hilos_busqueda[1],NULL);
    pthread_join(hilos_busqueda[2],NULL);
    pthread_join(hilos_busqueda[3],NULL);
    pthread_join(hilos_busqueda[4],NULL);
    pthread_join(hilos_busqueda[5],NULL);
    pthread_join(hilos_busqueda[6],NULL);
    sem_post(semaforo_trans);

   //Liberamos la memoriad e las fechas 
    for (int i = 0; i<listaFechas.index;i++)
    {
        free(listaFechas.lista[i]);
    }

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
void generarListasDatos()
{
    FILE * fichero;
    int i,j;
    char mensaje[255],linea[300];
    bool esta_en_fecha,esta_en_users;
    char *fecha,* id,*operacion,*cantidad;

    escribirLog("Se empieza a generar listas de datos");
    sem_wait(semaforo_trans);
    semaforos_usados[1]=1;
    fichero = fopen(fichero_trans_global,"r");
    if (fichero == NULL)
    {
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir el fichero %s",errno,fichero_trans_global);
        escribirLog(mensaje);
        sem_post(semaforo_trans);
        semaforos_usados[1]=0;
        return;
    }

    //Inicializamos los indices para evitar fallos  
    listaUSUARIO_MONITORs.index = 0;
    listaFechas.index=  0;

    //Obtenemos los datos de las lineas
    while (fgets(linea,sizeof(linea),fichero) != NULL)
    {
        //Para cada linea veremos si la fecha y el id de USUARIO_MONITOR ya existen
        esta_en_fecha = false;
        esta_en_users = false;

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
        
        
        operacion = strtok(NULL,"-");
        id = strtok(NULL,"-");

        //Vemos si el id de USUARIO_MONITOR ya existe en la lista de USUARIO_MONITORs
        for ( i = 0; i<listaUSUARIO_MONITORs.index;i++)
        {
            if(listaUSUARIO_MONITORs.lista[i].id == atoi(id))
            {
                esta_en_users = true;
                break;
            }                   
        }
        //Si no existe el id de USUARIO_MONITOR lo añadimos
      
        if (!esta_en_users) listaUSUARIO_MONITORs.lista[listaUSUARIO_MONITORs.index++].id = atoi(id); 
            

        
     
        
    }

    fclose(fichero);
    sem_post(semaforo_trans);
    semaforos_usados[1]=0;
    escribirLog("Se han generado las listas de datos");
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

    escribirLog("Monitor empieza a buscar ingresos maximos");
    fichero = fopen(fichero_trans_global,"r");
    if (fichero == NULL)
    {
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir el fichero %s",errno,fichero_trans_global);
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
        for ( j = 0 ; j<listaUSUARIO_MONITORs.index;j++)
        {
            listaUSUARIO_MONITORs.lista[j].cantidad_ingresada = 0;
        }

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
            
            for ( j = 0; j<listaUSUARIO_MONITORs.index;j++)
            {
                //Vemos si el id de USUARIO_MONITOR ya existe en la lista de USUARIO_MONITORs
                if (listaUSUARIO_MONITORs.lista[j].id == atoi(id))
                {
                    //Si existe lo sumamos a la cantidad ingresada
                    listaUSUARIO_MONITORs.lista[j].cantidad_ingresada += atof(cantidad);

                    //Si el conjunto de cantidad ingresada es mayor al maximo lo notificamos
                    if (listaUSUARIO_MONITORs.lista[j].cantidad_ingresada >= maximos.maxima_cantidad_ingreso)
                    {
                        //Si el ingreso es mayor al maximo lo notificamos
                        snprintf(mensaje,sizeof(mensaje),"1-ALERTA: USUARIO_MONITOR CON ID %s HA SUPERADO LA MAXIMA CANTIDAD DE INGRESOS EL DIA %s\n",id,fecha);
                        escribirBanco(mensaje);                   
                    }
                }
            }
            
        }
    }

    //Una vez que hemos terminado de leer el fichero lo cerramos
    fclose(fichero);

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

    escribirLog("Monitor empieza a buscar numerosos ingresos");
    fichero = fopen(fichero_trans_global,"r");
    if (fichero == NULL)
    {
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir el fichero %s",errno,fichero_trans_global);
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
        for ( j = 0 ; j<listaUSUARIO_MONITORs.index;j++)
        {
            listaUSUARIO_MONITORs.lista[j].cantidad_ingresada = 0;
        }

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
            
            for ( j = 0; j<listaUSUARIO_MONITORs.index;j++)
            {
                //Vemos si el id de USUARIO_MONITOR ya existe en la lista de USUARIO_MONITORs
                if (listaUSUARIO_MONITORs.lista[j].id == atoi(id))
                {
                    //Si existe lo sumamos a la cantidad ingresada
                    listaUSUARIO_MONITORs.lista[j].numero_ingresos +=1;

                    //Si el conjunto de cantidad ingresada es mayor al maximo lo notificamos
                    if (listaUSUARIO_MONITORs.lista[j].numero_ingresos >= maximos.maximo_ingresos)
                    {
                        //Si el ingreso es mayor al maximo lo notificamos
                        snprintf(mensaje,sizeof(mensaje),"1-ALERTA: USUARIO_MONITOR CON ID %s HA SUPERADO EL MAXIMO NUMERO DE INGRESOS EL DIA %s\n",id,fecha);
                        escribirBanco(mensaje);    
                    }
                }
            }
            
        }
    }

    //Una vez que hemos terminado de leer el fichero lo cerramos
    fclose(fichero);

    escribirLog("Monitor ha terminado de buscar ingresos maximos raros");
    //Liberamos el hilo
    sem_post(semaforo_hilos);
    semaforos_usados[0]=0;
    return NULL;

}

//-------------------------------------RETIRADAS-------------------------------------

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

    escribirLog("Monitor empieza a buscar gran cantidad de dinero en retiradas");
    fichero = fopen(fichero_trans_global,"r");
    if (fichero == NULL)
    {
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir el fichero %s",errno,fichero_trans_global);
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
        for ( j = 0 ; j<listaUSUARIO_MONITORs.index;j++)
        {
            listaUSUARIO_MONITORs.lista[j].cantidad_retirada = 0;
        }

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
            for ( j = 0; j<listaUSUARIO_MONITORs.index;j++)
            {
                //Vemos si el id de USUARIO_MONITOR ya existe en la lista de USUARIO_MONITORs
                if (listaUSUARIO_MONITORs.lista[j].id == atoi(id))
                {                 
                    //Si existe lo sumamos a la cantidad retirada
                    listaUSUARIO_MONITORs.lista[j].cantidad_retirada += atof(cantidad);

                    //Si el conjunto de cantidad retirada es mayor al maximo lo notificamos
                    if (listaUSUARIO_MONITORs.lista[j].cantidad_retirada >= maximos.maxima_cantidad_retiro)
                    {
                        //Si el ingreso es mayor al maximo lo notificamos
                        snprintf(mensaje,sizeof(mensaje),"1-ALERTA: USUARIO_MONITOR CON ID %s HA SUPERADO LA MAXIMA CANTIDAD DE RETIROS EL DIA %s\n",id,fecha);
                        escribirBanco(mensaje);                         
                    }
                }
            }
            
        }
    }

    //Una vez que hemos terminado de leer el fichero lo cerramos
    fclose(fichero);

    escribirLog("Monitor ha terminado de buscar grandes cantidades de retiro");
    //Liberamos el hilo
    sem_post(semaforo_hilos);
    semaforos_usados[0]=0;
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

    escribirLog("Monitor empieza a buscar multiples retiradas en un dia");
    fichero = fopen(fichero_trans_global,"r");
    if (fichero == NULL)
    {
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir el fichero %s",errno,fichero_trans_global);
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
        for ( j = 0 ; j<listaUSUARIO_MONITORs.index;j++)
        {
            listaUSUARIO_MONITORs.lista[j].num_retiros = 0;
        }

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
            
            for ( j = 0; j<listaUSUARIO_MONITORs.index;j++)
            {
                //Vemos si el id de USUARIO_MONITOR ya existe en la lista de USUARIO_MONITORs
                if (listaUSUARIO_MONITORs.lista[j].id == atoi(id))
                {
                    //Si existe lo suamnos el intento de retiro
                    listaUSUARIO_MONITORs.lista[j].num_retiros +=1;

                    //Si el conjunto de cantidad ingresada es mayor al maximo lo notificamos
                    if (listaUSUARIO_MONITORs.lista[j].num_retiros >= maximos.maximo_retiros)
                    {
                        //Si el numero de retiros al dia el mayor al mexmio
                        snprintf(mensaje,sizeof(mensaje),"1-ALERTA: USUARIO_MONITOR CON ID %s HA SUPERADO EL MAXIMO NUMERO DE RETIROS EL DIA %s\n",id,fecha);
                        escribirBanco(mensaje);             
                    }
                 
                }
            }
            
        }
    }

    //Una vez que hemos terminado de leer el fichero lo cerramos
    fclose(fichero);

    escribirLog("Monitor ha terminado de buscar nuemerosas retiradas en un dia");
    //Liberamos el hilo
    sem_post(semaforo_hilos);
    semaforos_usados[0]=0;
    return NULL;

}


//--------------------------------------------------------TRANSFERENCIAS------------------------------------------------

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

    escribirLog("Monitor empieza a grandes cantidades de dinero en transferencias");
    fichero = fopen(fichero_trans_global,"r");
    if (fichero == NULL)
    {
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir el fichero %s",errno,fichero_trans_global);
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
        for ( j = 0 ; j<listaUSUARIO_MONITORs.index;j++)
        {
            listaUSUARIO_MONITORs.lista[j].cantidad_transferida= 0;
        }

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
            
            for ( j = 0; j<listaUSUARIO_MONITORs.index;j++)
            {
                //Vemos si el id de USUARIO_MONITOR ya existe en la lista de USUARIO_MONITORs
                if (listaUSUARIO_MONITORs.lista[j].id == atoi(id))
                {
                    //Si existe lo sumamos a la cantidad tranferida
                    listaUSUARIO_MONITORs.lista[j].cantidad_transferida += atof(cantidad);

                    //Si el conjunto de cantidad trabferida es mayor al maximo lo notificamos
                    if (listaUSUARIO_MONITORs.lista[j].cantidad_transferida>= maximos.maxima_cantidad_transferencia)
                    {
                        //Si el la cantidad tranferida es mayor al maximo lo notificamos
                        snprintf(mensaje,sizeof(mensaje),"1-ALERTA: USUARIO_MONITOR CON ID %s HA SUPERADO LA MAXIMA CANTIDAD DE DINERO TRANSFERIBLE EL DIA %s\n",id,fecha);
                        escribirBanco(mensaje);                 
                    }
                }
            }
            
        }
    }

    //Una vez que hemos terminado de leer el fichero lo cerramos
    fclose(fichero);

    escribirLog("Monitor ha terminado de buscar ingresos maximos raros");
    //Liberamos el hilo
    sem_post(semaforo_hilos);
    semaforos_usados[0]=0;
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

    escribirLog("Monitor empieza a buscar gran cantidad de transferencias al dia "); 
    fichero = fopen(fichero_trans_global,"r");
    if (fichero == NULL)
    {
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir el fichero %s",errno,fichero_trans_global);
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
        for ( j = 0 ; j<listaUSUARIO_MONITORs.index;j++)
        {
            listaUSUARIO_MONITORs.lista[j].cantidad_ingresada = 0;
        }

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
            
            for ( j = 0; j<listaUSUARIO_MONITORs.index;j++)
            {
                //Vemos si el id de USUARIO_MONITOR ya existe en la lista de USUARIO_MONITORs
                if (listaUSUARIO_MONITORs.lista[j].id == atoi(id))
                {
                    //Si existe lo sumamos al total de transferencias
                    listaUSUARIO_MONITORs.lista[j].num_transferencias +=1;

                    //Si la suma de transferencias es mayor al maximo lo notificamos
                    if (listaUSUARIO_MONITORs.lista[j].num_transferencias>= maximos.maximo_tranferencias)
                    {
                    
                        snprintf(mensaje,sizeof(mensaje),"1-ALERTA: USUARIO_MONITOR CON ID %s HA SUPERADO EL MAXIMO NUMERO DE TRANSFERENCIAS EL DIA %s\n",id,fecha);
                        escribirBanco(mensaje);              
                    }
                }
            }
            
        }
    }

    //Una vez que hemos terminado de leer el fichero lo cerramos
    fclose(fichero);

    escribirLog("Monitor ha terminado de buscar multiples transferencias al dia");
    //Liberamos el hilo
    sem_post(semaforo_hilos);
    semaforos_usados[0]=0;
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

    escribirLog("Monitor empieza a buscar multiples transferencias entre cuentas en un dia");
    fichero = fopen(fichero_trans_global,"r");
    if (fichero == NULL)
    {
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir el fichero %s",errno,fichero_trans_global);
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
        for ( j = 0 ; j<listaUSUARIO_MONITORs.index;j++)
        {
            listaUSUARIO_MONITORs.lista[j].cantidad_ingresada = 0;
        }

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
            if (strcmp(fecha,listaFechas.lista[i]) != 0 || strcmp(operacion,"TRANFERENCIA") != 0) continue;
            
            for ( j = 0; j<listaUSUARIO_MONITORs.index;j++)
            {
                //VIdentificamos si el id de USUARIO_MONITOR ya existe en la lista de USUARIO_MONITORs
                if (listaUSUARIO_MONITORs.lista[j].id == atoi(id))
                {
                    //Para cada USUARIO_MONITOR miramos su lista de tranferidoas a ver si h atransferido a alguien
                    for ( k = 0; k<=listaUSUARIO_MONITORs.lista[j].n_usus_trans;k++)
                    {
                        if (listaUSUARIO_MONITORs.lista[j].USUARIO_MONITORs_transferidos[k][0] == atoi(id_tranf))
                        { 
                            encontrado = true;
                            break;
                        }
                        
                    }
                    if (encontrado)
                    {
                        //Si ya existe le sumamos uno
                        listaUSUARIO_MONITORs.lista[j].USUARIO_MONITORs_transferidos[k][1] += 1;

                        //Si el numero de transferencias entre cuentas es mayor al maximo lo notificamos
                        if (listaUSUARIO_MONITORs.lista[j].USUARIO_MONITORs_transferidos[k][1] >= maximos.maximo_transferencias_entre_cuentas)
                        {
                            snprintf(mensaje,sizeof(mensaje),"1-ALERTA: USUARIO_MONITOR CON ID %s HA SUPERADO EL MAXIMO NUMERO DE TRANSFERENCIAS ENTRE CUENTAS CON %s EL DIA %s\n",id,id_tranf,fecha);
                            escribirBanco(mensaje);
                                           
                        }
                            
                    }
                    else 
                    {
                        //Sino esta le agregamos 
                        listaUSUARIO_MONITORs.lista[j].USUARIO_MONITORs_transferidos[listaUSUARIO_MONITORs.lista[j].n_usus_trans++][0] = atoi(id_tranf);
                        listaUSUARIO_MONITORs.lista[j].USUARIO_MONITORs_transferidos[listaUSUARIO_MONITORs.lista[j].n_usus_trans][1] += 1;
                    }
                    break;
                }
            }
            
        }
    }

    //Una vez que hemos terminado de leer el fichero lo cerramos
    fclose(fichero);

    escribirLog("Monitor ha terminado de buscar multiples transferencias entre cuentas");
    //Liberamos el hilo
    sem_post(semaforo_hilos);
    semaforos_usados[0]=0;
    return NULL;

}



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


