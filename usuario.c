#include "comun.c"


//Creamos las estructuras
typedef struct
{
    char *nombre;
    int num_cuenta;
    float saldo;
    int transacciones;
    int operacionactiva;
    char * ficheroOperaciones;
    //char * ficheroCuentas;
    char * ficheroLog;
    int codigo_identificacion_operacion;
}CONFIGURACION_USER;

//Esta estrutura se usa a para actulizar datos 
typedef struct
{
    int idCliente;
    int idTranferencia;
    float cantidad;
    
}DATA;


//Definimos las funciones
void Menu();
char* obtener_fecha();
bool  buscarUser(int id);
void * Retiro(void *args);
void seniales(int senial);
void escribirBanco(char *);
void * Deposito(void *args);
void * Transferencia(void *args);
void escribirLog(char * mensaje);
void * ConsultarSaldo(void *args); 
void escribirTransacciones(DATA d);
void modificarFicheroCuentas(DATA d);
void leerMemoriaCompartida();



//Iniciamos las variables globales
MEMORIA * listaUsers;
pid_t pid_programa;
bool senial_recibida;
CONFIGURACION_USER User;
sem_t * semaforo_hilos, *semaforo_memoria,*semaforo_trans,*semaforo_log,*semaforo_fifo;
//Esto lo uso para la hora del cierre 
int semaforos_usados[5];




int main(int argc, char *argv[])
{
    //Manejamos las señales
    signal(SIGINT ,seniales);
    signal(SIGHUP,seniales);
    signal(SIGTERM,seniales);
    signal(SIGUSR1,seniales);
    senial_recibida = false;
    //Creamos las variables
    pid_programa = getpid();
    char  mensaje[255];

    //Cargamos los parametros de configuracion como el nombre y demas que se han pasado por parametro 
   
    User.nombre = argv[1];
    User.num_cuenta = atoi(argv[2]);
    User.saldo = atoi(argv[3]);
    User.transacciones = atoi(argv[4]);

    User.ficheroOperaciones= argv[5];
    User.ficheroLog = argv[6];
  
   
   
    //Inicializamos los semaforos
    semaforo_hilos = sem_open(SEMAFORO_CONTROL_HILOS,0);
    semaforo_memoria = sem_open(SEMAFORO_MUTEX_MEMORIA,0);
    semaforo_trans = sem_open(SEMAFORO_MUTEX_TRANSACCIONES,0);
    semaforo_log = sem_open(SEMAFORO_MUTEX_LOG,0);
    semaforo_fifo = sem_open(SEMAFORO_MUTEX_FIFO,0);
    //Controlamos si ha habido algun fallo con los semaforos
    //en dicho caso, nos salimos ya que sin semaforos no se puede controlar el programa y puede dar fallos
    if (semaforo_trans == SEM_FAILED)
    { 
        snprintf(mensaje,sizeof(mensaje),"Ha fallado el semaforo de transacciones del programa usuario, error code %d",errno);
        fprintf(stderr,"%s\n",mensaje);
        escribirLog(mensaje);
        kill(pid_programa,SIGTERM);
    }

    if (semaforo_fifo == SEM_FAILED)
    { 
        snprintf(mensaje,sizeof(mensaje),"Ha fallado el semaforo de FIFO del programa usuario, error code %d",errno);
        fprintf(stderr,"%s\n",mensaje);
        escribirLog(mensaje);
        kill(pid_programa,SIGTERM);
    }

    if (semaforo_memoria== SEM_FAILED)
    {
        snprintf(mensaje,sizeof(mensaje),"Ha fallado el semaforo de memoria del programa usuario, error code %d",errno);
        escribirLog(mensaje);
        fprintf(stderr,"%s\n",mensaje);
        kill(pid_programa,SIGTERM);
    }

    if (semaforo_hilos == SEM_FAILED)
    {
       
        snprintf(mensaje,sizeof(mensaje),"Ha fallado el semaforo de hilo del programa usuario, error code %d",errno);
        escribirLog(mensaje);
        fprintf(stderr,"%s\n",mensaje);
        kill(pid_programa,SIGTERM);
    }

    if (semaforo_log == SEM_FAILED)
    {
        snprintf(mensaje,sizeof(mensaje),"Ha fallado el semaforo de log del programa usuario, error code %d",errno);
        //Si no podemos acceder al semaforo de log, no podemos usar la funcion, por tanto ,a
        //avisamos al banco para que el lo pongoa el el log
        escribirBanco(mensaje);
        fprintf(stderr,"%s\n",mensaje);
        exit(1);//No mandamos señal de automuerte ya que eso implica escribir en el log cosa que no podemos 
    }

    
    //Indicamos que estamos activos
    snprintf(mensaje,sizeof(mensaje),"0-%d",pid_programa);
    escribirBanco(mensaje);
  
    snprintf(mensaje,sizeof(mensaje),"El usuario con PID %d ha inciado sesion",pid_programa);
    escribirLog(mensaje);
   
    //Cargamos los datos de memoria 
    leerMemoriaCompartida();
    //Inciamos el menu
    Menu();
    
   
    return 0;
}

//Nombre: seniales
//Retorno: void
//Parametros: senial
//Uso: Esta funcion se encarga de manejar las señales que le llegan en mayor caso de apagado
void seniales(int senial)
{
    int i;
    //Controlamos que le llegue una vez la señal
    if ( senial_recibida) return;
    senial_recibida = true;
    //Creamos las variables
    char mensaje[255];
    i = 0;
    if ( senial == SIGINT || senial == SIGHUP || senial == SIGTERM )
    {
        //Poenmos el mensaje que toque
        if ( User.operacionactiva != 0) 
        {
            snprintf(mensaje,sizeof(mensaje),"Intento de matar el programa usuario con PID %d, quedan %d operaciones activas,cerrandolas...",pid_programa,User.operacionactiva);
            escribirLog(mensaje);
            printf("%s\n",mensaje);
            sleep(5); //Esperamos un poco a ver si se cierran
            //Si sigue activa es que se ha quedado bloqueada asi que liberamos los recursos
            if (User.operacionactiva != 0)
            {
                if ( semaforos_usados[0] == 1) sem_post(semaforo_hilos);
                if ( semaforos_usados[1] == 1) sem_post(semaforo_memoria);
                if ( semaforos_usados[2] == 1) sem_post(semaforo_trans);
                if ( semaforos_usados[3] == 1) sem_post(semaforo_log);
                if ( semaforos_usados[4] == 1) sem_post(semaforo_fifo);
                snprintf(mensaje,sizeof(mensaje),"No se han cerrado las operaciones pendientes del user con PID %d,pero se han liberado recursos",pid_programa);
                escribirLog(mensaje);
                printf("%s\n",mensaje);
            }
            else
            {
                snprintf(mensaje,sizeof(mensaje),"Se han cerrado las operaciones pendientes del user con PID %d",pid_programa);
                escribirLog(mensaje);
                printf("%s\n",mensaje);
            }
        }
        else{ snprintf(mensaje,sizeof(mensaje),"Usuario con PID %d saliendo de manera forzada...",pid_programa);
        
        //Lo motramos por pantalla y lo ponemos en el log
        printf("%s\n",mensaje);
        escribirLog(mensaje);}

            
        //Como no ha sido el banco quien me ha matado le aviso
        snprintf(mensaje,sizeof(mensaje),"0-%d",pid_programa);
        escribirBanco(mensaje);
        
        sleep(1);    
        exit(0);

    }
    else if (senial == SIGUSR1)
    {
        if ( User.operacionactiva != 0) 
        {
            snprintf(mensaje,sizeof(mensaje),"El banco mata al programa usuario con PID %d, quedan %d operaciones activas,cerrandolas...",pid_programa,User.operacionactiva);
            escribirLog(mensaje);
            printf("%s\n",mensaje);
            sleep(5); //Esperamos un poco a ver si se cierran
            //Si sigue activa es que se ha quedado bloqueada asi que liberamos los recursos
            if (User.operacionactiva != 0)
            {
                if ( semaforos_usados[0] == 1) sem_post(semaforo_hilos);
                if ( semaforos_usados[1] == 1) sem_post(semaforo_memoria);
                if ( semaforos_usados[2] == 1) sem_post(semaforo_trans);
                if ( semaforos_usados[3] == 1) sem_post(semaforo_log);
                if ( semaforos_usados[4] == 1) sem_post(semaforo_fifo);
                snprintf(mensaje,sizeof(mensaje),"No se han cerrado las operaciones pendientes del user con PID %d,pero se han liberado recursos",pid_programa);
                escribirLog(mensaje);
                printf("%s\n",mensaje);
            }
            else
            {
                snprintf(mensaje,sizeof(mensaje),"Se han cerrado las operaciones pendientes del user con PID %d",pid_programa);
                escribirLog(mensaje);
                printf("%s\n",mensaje);
            }
        }
        
        else
        { snprintf(mensaje,sizeof(mensaje),"Usuario con PID %d saliendo de manera forzada por motivo del banco...",pid_programa);

        //Lo motramos por pantalla y lo ponemos en el log
        printf("%s\n",mensaje);
        escribirLog(mensaje);}

        sleep(1);
        exit(0);
    }
}

// Nombre: Menu
// Retorno: void*
// Parámetros: void *args
// Uso: Esta función gestiona el menú principal del banco, permitiendo al usuario realizar operaciones como depósito, retiro, 
//transferencia y consulta de saldo. 
// Utiliza hilos y sincronización con semáforos y mutex para controlar el acceso a recursos compartidos.
void Menu()
{
    

    //Creamos las variables
    int opcion;   
    char mensaje[255];
    pthread_t thread2;
    

    //Inicamos la variable a cero para que no de fallos 
    opcion = 0;
    while(opcion != 5)
    {
        
        printf("-- Bienvenido al banco %s--\n1.Deposito\n2.Retiro\n3.Transferencia\n4.Consultar Saldo\n5.Salir\nIntroduzca la opcion: ",User.nombre);
        scanf("%d",&opcion);
        //en funcion de la opcion que elija llamamos a una funcion o a otra
        switch (opcion)
        {
            case 1:
               
                User.operacionactiva ++;
                pthread_create(&thread2,NULL,Deposito,NULL);
                pthread_join(thread2,NULL);
               
                User.operacionactiva --;
            break;
            case 2:
               
                User.operacionactiva ++;
                pthread_create(&thread2,NULL,Retiro,NULL);
                pthread_join(thread2,NULL);
               
                User.operacionactiva --;
                break;
            case 3:
               
                User.operacionactiva ++;
                pthread_create(&thread2,NULL,Transferencia,NULL);
                pthread_join(thread2,NULL);
               
                User.operacionactiva --;
                break;
            case 4:
               
                User.operacionactiva ++;
                pthread_create(&thread2,NULL,ConsultarSaldo,NULL);
                pthread_join(thread2,NULL);
               
                User.operacionactiva --;
                break;
            case 5:
                printf("Saliendo...");

                //Escribimos en el log
                snprintf(mensaje,sizeof(mensaje),"El usuario con PID %d ha cerrado sesion",pid_programa);
                escribirLog(mensaje);              
                
                //Nos quitamos de la lista de activos del banco
                snprintf(mensaje,sizeof(mensaje),"0-%d",pid_programa);
                escribirBanco(mensaje);

                
            break;

            default:
                snprintf(mensaje,sizeof(mensaje),"El usuario con PID %d ha elegido una opcion invalida",pid_programa);
                escribirLog(mensaje);
                //Controlamos que solo se ponga una opcion del menu
                printf("Introduce un numero valido...\n");
                sleep(2);
                system("clear");
            break;
        }
    }
    
    return ;
}

// Nombre: Deposito
// Retorno: void*
// Parámetros: void *args
// Uso: Esta función gestiona el proceso de depósito de dinero en la cuenta del usuario. 
//      Verifica que la cantidad ingresada sea válida, comunica la transacción al monitor, 
//      actualiza los archivos de cuentas y transacciones, y sincroniza el acceso a los datos 
//      con semáforos y mutex.
void *Deposito(void *args)
{
  
    sem_wait(semaforo_hilos);
    semaforos_usados[0]=1;

    //Creamos las variables
    DATA d;
    float cantidad;
    char mensaje[255], *respuesta;

    snprintf(mensaje,sizeof(mensaje),"El usuario con PID %d ha iniciado un deposito",pid_programa);
    escribirLog(mensaje);


    printf("Introduce la cantidad a depositar: ");
    scanf("%f",&cantidad);

    //Verificamos un par de cosas antes de enviarlas al monitor
    if (cantidad < 0)
    {
        snprintf(mensaje,sizeof(mensaje),"El usuario con PID %d ha intentado meter dinero negativo",pid_programa);
        escribirLog(mensaje);
        printf("No se puede meter una cantidad negativa...\n");
        sleep(2);
        system("clear");
        sem_post(semaforo_hilos);
        return NULL;
    }


    
    //Preparamos la estrutura para actualizar los datos
    
    d.cantidad = cantidad;
    d.idCliente = User.num_cuenta;
    d.idTranferencia = -2;
   
    
    
    //Actualizamoos los ficheros
  
    modificarFicheroCuentas(d);
    escribirTransacciones(d);
    snprintf(mensaje,sizeof(mensaje),"El usuario con PID %d ha depositado dinero",pid_programa);
    escribirLog(mensaje);
    printf("Depositando su dinero...\n");
    sleep(4);
    printf("Se ha realizado el ingreso...\n");
    sleep(1);
    system("clear");
    escribirBanco("2");
        
   
    
   
   
    
    sem_post(semaforo_hilos);
    semaforos_usados[0]=0;
    return NULL;
}

// Nombre: Retiro
// Retorno: void*
// Parámetros: void *args
// Uso: Esta función gestiona el retiro de dinero de la cuenta del usuario. 
//      Verifica que haya saldo suficiente y que la cantidad ingresada sea válida, 
//      comunica la transacción al monitor, actualiza los archivos de cuentas y transacciones, 
//      y sincroniza el acceso a los datos mediante semáforos y mutex.
void *Retiro(void *args)
{
    //Bloqueamos un hilo
    sem_wait(semaforo_hilos);
    semaforos_usados[0]=1;

    //Creamos las variables
    DATA d;
    char mensaje[255];
    char *respuesta;
    float cantidad;
   
    snprintf(mensaje,sizeof(mensaje),"El usuario con PID %d ha iniciado un retiro",pid_programa);
    escribirLog(mensaje);
    
    
   
    printf("Introduce la cantidad a retirar: ");
    scanf("%f",&cantidad);

    
    //Hacemos comprobaciones basicas de lado del usuario
    if (User.saldo - cantidad < 0)
    {
        snprintf(mensaje,sizeof(mensaje),"El usuario con PID %d ha intentado sacar mas dinero del que tiene",pid_programa);
        escribirLog(mensaje);
        printf("No tienes suficiente saldo...\n");
        sleep(2);
        system("clear");
        sem_post(semaforo_hilos);
        semaforos_usados[0]=0;
        return NULL;
    }
    if (cantidad < 0)
    {
        snprintf(mensaje,sizeof(mensaje),"El usuario con PID %d ha intentado sacar dinero negativo",pid_programa);
        escribirLog(mensaje);
        printf("No puedes sacar una cantidad negativa...\n");
        sleep(2);
        system("clear");
        sem_post(semaforo_hilos);
        semaforos_usados[0]=0;
        return NULL;
    }
    


   
    //Iniciamos los datos de la estrutura que luego actualizaran los ficheros  
   
    d.cantidad = cantidad;
    d.idCliente = User.num_cuenta;
    d.idTranferencia = -1;

    //Actualizamos los ficheros
    modificarFicheroCuentas(d);
    escribirTransacciones(d);

    snprintf(mensaje,sizeof(mensaje),"El usuario con PID %d ha retirado dinero",pid_programa);
    escribirLog(mensaje);
    printf("Retirando...\n");
    sleep(2);
    printf("Operacion realizada...\n");
    sleep(2);
    system("clear");
    escribirBanco("2");
   
    //Liberamos el hilo
    sem_post(semaforo_hilos);
    semaforos_usados[0]=0;
           
    return NULL;  
}

// Nombre: Transferencia
// Retorno: void*
// Parámetros: void *args
// Uso: Esta función gestiona la transferencia de dinero entre cuentas. 
//      Verifica la validez del ID del destinatario, que el usuario tenga fondos suficientes 
//      y que la cantidad sea válida. Luego, comunica la transacción al monitor, 
//      actualiza los archivos de cuentas y transacciones, y sincroniza el acceso a los datos 
//      mediante semáforos y mutex.

void *Transferencia(void *args)
{
    //Bloqueamos un hilo
    sem_wait(semaforo_hilos);
    semaforos_usados[0]=1;

    //Creamos las variables
    DATA d;
    int idTransfer;
    char mensaje[255];
    float cantidadTransfer;
   
    snprintf(mensaje,sizeof(mensaje),"El usuario con PID %d ha iniciado una tranferencia",pid_programa);
    escribirLog(mensaje);



   
   
   //Iniciamos las variables para que no haya fallos
    idTransfer = 0;
    cantidadTransfer = 0;

    printf("Introduce el ID del cliente al que quieres hacerle la transferencia: ");
    scanf("%d", &idTransfer);

    //Validamos que no te tranfieras a ti mismo
    if ( idTransfer == User.num_cuenta)
    {
        snprintf(mensaje,sizeof(mensaje),"El usuario con PID %d ha intentado tranferirse a si mismo",pid_programa);
        escribirLog(mensaje);
        printf("No te puedes tranferir a ti mismo...\n");
        sleep(2);
        system("clear");
        sem_post(semaforo_hilos);
        semaforos_usados[0]=0;
        return NULL;

    } 
    //Vemos si el usuario marcado existe
    if (! buscarUser(idTransfer))
    {
        snprintf(mensaje,sizeof(mensaje),"El usuario con PID %d ha intentado tranferir a un usuario que no existe",pid_programa);
        escribirLog(mensaje);
        printf("No existe un usuario con id %d\n",idTransfer);
        sleep(2);
        system("clear");
        
        sem_post(semaforo_hilos);
        semaforos_usados[0]=0;
        return NULL;

    }
    printf("Introduce la cantidad a transferir: ");
    scanf("%f",&cantidadTransfer);

    //Validamos que la cantidad no sea negativa o haya fondos
    if ( cantidadTransfer < 0)
    {
        snprintf(mensaje,sizeof(mensaje),"El usuario con PID %d ha intentado tranferir una cantidad negativa",pid_programa);
        escribirLog(mensaje);
        printf("No se puede tranferir dinero negativo...\n");
        sem_post(semaforo_hilos);
        sleep(2);
        system("clear");
        
        return NULL;
    }

    if (User.saldo - cantidadTransfer < 0)
    {
        snprintf(mensaje,sizeof(mensaje),"El usuario con PID %d ha intentado tranferir mas dinero del que tiene",pid_programa);
        escribirLog(mensaje);
        sem_post(semaforo_hilos);
        printf("No tienes fondos suficientes...\n");
        sleep(2);
        system("clear");
        semaforos_usados[0]=0;
        return NULL;
    }
        
  
    //Iniciamos los datos de la estrutura que luego actualizaran los ficheros
  
    d.cantidad = cantidadTransfer;
    d.idCliente = User.num_cuenta;
    d.idTranferencia = idTransfer;
   

   

   
    //Actualizamos los ficheros
    escribirTransacciones(d);
    modificarFicheroCuentas(d);
   
    

    //Lo ponemos el el log
    snprintf(mensaje,sizeof(mensaje),"El usuario con PID %d ha tranferido dinero",pid_programa);
    escribirLog(mensaje);

    printf("Transfiriendo...\n");
    sleep(4);
    printf("Se ha tranferido el dinero...\n");
    sleep(2);
    system("clear");
    escribirBanco("2");

   
    //Liberamos el hilo
    sem_post(semaforo_hilos);
    semaforos_usados[0]=0;
    return NULL;   

}

// Nombre: ConsultarSaldo
// Retorno: void*
// Parámetros: void *args
// Uso: Esta función permite al usuario consultar su saldo disponible en la cuenta. 
//      y muestra el saldo en pantalla, asegurando el acceso seguro a la variable global mediante un mutex.

void *ConsultarSaldo(void *args)
{
    
    sem_wait(semaforo_hilos);
    semaforos_usados[0]=1;

    
    //Creamos las variables
    int i;
    DATA d;
    char mensaje[255];


    //Notificamos al log
    snprintf(mensaje,sizeof(mensaje),"El usuario con PID %d ha consultado su saldo",pid_programa);
    escribirLog(mensaje);

    
    d.cantidad = User.saldo;
    d.idCliente = User.num_cuenta;
    d.idTranferencia = -3;
   
    escribirTransacciones(d);
    //Simulamos que el sistema hace algo
    printf("Contando billetes...\n");
    sleep(3);
    //Bloqueamos el acceso a la memoria para evitar probelmas
    sem_wait(semaforo_memoria);
    semaforos_usados[1] = 1;
    //Buscamos al usuario en la memoria compartida
    for ( i = 0; i<listaUsers->n_users;i++)
    {
        if (listaUsers->lista[i].id == User.num_cuenta) break;
    }
    printf("Tu saldo es: %.2f\n", listaUsers->lista[i].saldo);
    sem_post(semaforo_memoria);
    semaforos_usados[1] = 0;
    printf("Pulse una tecla para continuar...\n");
    //Limpiamos el buffer para posibles problemas con getchar()
    while(getchar() != '\n');
    getchar();
    system("clear");
    //Liberamos un hilo
    sem_post(semaforo_hilos);
    semaforos_usados[0]=0;
    return NULL;
}

//donde si es un ingreso, idTrans = -2 si es retirada = -1 y si ew tranferencia el id del que quieres hacer la treanferencia
// Nombre: modificarFicheroCuentas
// Retorno: void*
// Parámetros: void *arg aunque se le pasa una estrutura de tipo DATA
// Uso: Esta funcion se encarga de modificar el fichero cuentas.dat tras cada operacion ,puesto que el usuario lee el mismo 
//      antes de cada operacion para tener los datos de memoria lo mas reciente posible.
//      primero de la estrutura DATA mira el idTrans que representa el usuario al que quiero tranferir, por tanto si es < 0 es que
//      debo actualizar dos cuentas, ya que es un id real , si es -1 es que es una retirada del usuario y si es un -2 es que es un ingreso
//      Se crea un fichero temporal y se lee el fichero de cuentas, si la linea leida no es de ningun usuario implicado , la copio tal cual 
//      en el fichero temporal , en caso contrario , cojo los datos de la linea, le sumo los que me indican en la estrutura y sumo +1 a las transacciones
//      de dicho usuario, esto se copia en el fichero temporal. Una vez acabado se remplaza el fichero temporal por el cuentas.dat
//      Utiliza semáforos para asegurar el acceso exclusivo al archivo y sincronizar 
//      con otros hilos.
void  modificarFicheroCuentas(DATA d)
{
    

    //Creamos las variables
    int i;

    //Bloqueamos el acceso a la memoria compartida
    sem_wait(semaforo_memoria);
    semaforos_usados[1] = 1;
    for ( i = 0; i<listaUsers->n_users;i++)
    {
        //Esto es una transferencia
        if (d.idTranferencia > 0)
        {
            //Este es el que tranfiere
            if (d.idCliente == listaUsers->lista[i].id )
            {
                listaUsers->lista[i].saldo -= d.cantidad;
                listaUsers->lista[i].operaciones ++;
            }
            //Este es el que recibe la tranferecnia
            if (d.idTranferencia == listaUsers->lista[i].id )
            {
                //Actualizo los datos
                listaUsers->lista[i].saldo += d.cantidad;
                listaUsers->lista[i].operaciones ++;
            }
          
        }
        else
        {
            //en este caso solo busco al usuario que o mete dinero o lo saca
            if (d.idCliente == listaUsers->lista[i].id )
            {
               
                //Si lo saca se le quita
                if (d.idTranferencia == -1 ) listaUsers->lista[i].saldo -= d.cantidad;
                //Si lo mete se le suma
                else if ( d.idTranferencia == -2) listaUsers->lista[i].saldo += d.cantidad;
               
                listaUsers->lista[i].operaciones ++;
        
              
            }
            
        }
        
    }
    //Liberamos el semaforo
    sem_post(semaforo_memoria);
    semaforos_usados[1] = 0;
 
   
   
  
    return;
}

// Nombre: escribirBanco
// Retorno: void*
// Parámetros: void *arg aunque es un char*
// Uso: Esta función se encarga de escribir un mensaje en un canal FIFO (First In, First Out) 
//      para comunicar información a al banco
//      Utiliza semáforos para controlar los hilos del programa

void escribirBanco(char * mensaje)
{
    sem_wait(semaforo_hilos);
    semaforos_usados[4] = 1;
    int fd;
    char error[255];

    fd = open(FIFO_PROGRAMAS_BANCO,O_WRONLY);
    if ( fd == -1)
    {
        snprintf(error,sizeof(error),"Error %d al abrir el fifo de programas por parte del usuario con PID %d",errno,pid_programa);
        fprintf(stderr,"%s\n",error);
        escribirLog(error);
        sem_post(semaforo_fifo);
        semaforos_usados[4] = 0;
        //Si no puede escribir al banco es un fallo critico ya que no puede saber de mi asi que me cierro
        kill(pid_programa,SIGTERM);
    }
    //escribimos el mensaje
    write(fd,mensaje,strlen(mensaje)+1);
    close(fd);
    sem_post(semaforo_fifo);
    semaforos_usados[4] = 0;
  
    return ;
}


// Nombre: buscarUser
// Retorno: void*
// Parámetros: void *arg, aunque se le pasa una estrutura DATAUSER
// Uso: Esta función busca un usuario en el archivo de cuentas utilizando el ID proporcionado, 
//      si encuentra el usuario, actualiza los datos del usuario en la estructura correspondiente. 
//      Usa semaforos para sincronizar los recursos y evitar fallos

bool buscarUser(int id)
{
    
   //creamos las variables
    int i;

    
    sem_wait(semaforo_memoria);
    semaforos_usados[1] = 1;
    for (i = 0; i<listaUsers->n_users;i++)
    {
      
        //veo si hay coincidencias y devuelvo un true 
        if ( id == listaUsers->lista[i].id)
        {
            sem_post(semaforo_memoria);
            semaforos_usados[1] = 0;
            return true;
        } 
       
    }
    //Si no ha encontrado nada, devuelvo un false
    sem_post(semaforo_memoria);
    semaforos_usados[1] = 0;
    return false;
}

// Nombre: escribirTransacciones
// Retorno: void*
// Parámetros: void *arg, auqnue se le pasa un aestrutura del tipo DATA
// Uso: Esta función escribe una transacción (depósito, retiro o transferencia) en el archivo de operaciones. 
//      ,donde se incluye la hora de la transacción y los detalles del cliente que realizó la operación. 
//      Los datos se obtienen del idTransf de la estrutura pasada por parametro donde -1 es un retiro,-2 ingreso 
//      y <0 una transferencia
//  Se usan semaforos para garantizar la exlcusion mutua
void escribirTransacciones(DATA d) 
{
    //Bloqueamos un hilo
    //sem_wait(semaforo_hilos);

    //Creamos las variables 
    char * hora;
    FILE *archivo;
    time_t tiempo;
    char error[255];
    
    //Blqueams el acceso al fichero para evitar cosas raras
    sem_wait(semaforo_trans);
    semaforos_usados[2]=1;
    archivo = fopen(User.ficheroOperaciones, "a");
    if (archivo == NULL) 
    {
        snprintf(error,sizeof(error),"Error %d del usuario con PID %d al abrir el fichero %s",errno,pid_programa,User.ficheroOperaciones);
        fprintf(stderr,"%s\n",error);
        escribirLog(error);
        //Liberamos los recursos
        sem_post(semaforo_trans);
        semaforos_usados[2]=0;
        
        //Si no puedo encontrar usuarios , no puedo hacer escribir transacciones
        kill(pid_programa,SIGTERM);
    }
    //creamos las hora
    time(&tiempo);
    //Lo pasamos a formato string
    hora = ctime(&tiempo);
    //quitamos el \n que por defecto pone la liberia de la hora
    hora[strcspn(hora,"\n")] = '\0';
 
    //En funcion del tipo de operacion hacemos una csoa u otra
    if (d.idTranferencia == -2)  fprintf(archivo, "[%s]-DEPOSITO-%d-%.2f\n",obtener_fecha() ,User.num_cuenta, d.cantidad);
    else if (d.idTranferencia == -1) fprintf(archivo, "[%s]-RETIRO-%d-%.2f\n",obtener_fecha() ,User.num_cuenta, d.cantidad);
    else if (d.idTranferencia == -3) fprintf(archivo, "[%s]-CONSULTA_SALDO-%d-%.2f-%d\n",obtener_fecha(),User.num_cuenta, d.cantidad,d.idTranferencia);
    else fprintf(archivo, "[%s]-TRANSFERENCIA-%d-%.2f-%d\n",obtener_fecha() ,User.num_cuenta, d.cantidad,d.idTranferencia);
    
 
    fclose(archivo);
    //Liberamos los recursos
    sem_post(semaforo_trans);
    semaforos_usados[2]=0;
   
    return ;
}

// Nombre: escribirLog
// Retorno: void*
// Parámetros: void *arg, anque se le pasa un char* 
// Descripción: Esta función escribe un mensaje de log en el archivo de log del usuario, añadiendo la fecha y hora en que se registró la acción. 
//              Si no puede acceder al archivo de log, envía un mensaje al banco


void escribirLog(char * mensaje)
{
    
    //Creamos las variables
    FILE * file;
    time_t tiempo;
    char error[255];
    char * hora;
    
    //Abrimos el fichero que se pasa por parametro, se hace asi ya que como el fichero viene del main
    //si en el main se cambia el fichero no es necesario actualizar este programa
    //Bloqueamos el acceso al fichero
    sem_wait(semaforo_log);
    semaforos_usados[3] = 0;
    file = fopen(User.ficheroLog,"a");

    if (file == NULL)
    {
        //En caso de error cerramos el programa ya que sin poder acceder al log no tiene sentido que funcione

        snprintf(error,sizeof(error),"Fallo %d del usuario con PID %d al abrir el archivo %s",errno,pid_programa,User.ficheroLog);
        fprintf(stderr,"%s\n",error);
        //Avisamos al banco por si el si que puede para que notifique el error
        escribirBanco(error);
        sem_post(semaforo_log);
        semaforos_usados[3]=0;
       
        //Nos enviamos una señal para matarnos ya que es un fallo critico
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
    semaforos_usados[3]=0;
  
    return ;
}



char* obtener_fecha() 
{
    static char fecha[255]; // DD/MM/AAAA + \0
    time_t tiempo_actual;
    struct tm *info_tiempo;

    time(&tiempo_actual);
    info_tiempo = localtime(&tiempo_actual);

    snprintf(fecha, sizeof(fecha), "%02d/%02d/%04d",
             info_tiempo->tm_mday,
             info_tiempo->tm_mon + 1,
             info_tiempo->tm_year + 1900);

    return fecha;
}


void leerMemoriaCompartida()
{
    
    int i,fd_memoria;
    char linea[255],mensaje[255];

    

    snprintf(mensaje,sizeof(mensaje),"El usuario con PID %d ha abierto la memoria compartida",pid_programa);
    escribirLog(mensaje);
    sem_wait(semaforo_memoria);
    semaforos_usados[0] = 1;
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

    sem_post(semaforo_memoria);
    semaforos_usados[1] = 0;

    snprintf(mensaje,sizeof(mensaje),"El usuario con PID %d ha leido la memoria compartida",pid_programa);
    escribirLog(mensaje);

}