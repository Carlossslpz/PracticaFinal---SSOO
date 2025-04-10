#include "comun.c"

//Definimos las funciones
void  escribirBanco(char * arg);
int localizarUltimo(char * ficheroParam);
void  menu(char * ficheroParam);
void  generarDatos(char * ficherParam);
void seniales(int senial);
void  escribirLog(char * mensaje);



//Declaramos las estructuras
typedef struct 
{
    char nombre[100];
    int nCuenta;
    int saldo;
    int transaciones;
}USUARIO;


//Declaramos las variables globales
bool activo;
char *fichero_log;
pid_t pid_programa;
sem_t *semaforo_cuentas,*semaforo_log,*semaforo_fifo;
int semaforos_usados[4];


int main(int argc, char *argv[])
{
    
    //Controlamos las señales
    signal(SIGINT ,seniales);
    signal(SIGHUP,seniales);
    signal(SIGTERM,seniales);
    signal(SIGUSR1,seniales);

    //Creamos las variables
    char mensaje[255];
    int modo_ejecucion;

    //Asigamos los parametros pasados y las variables globales
    modo_ejecucion = atoi(argv[2]);
    fichero_log = argv[3];
    pid_programa = getpid();

  
    //Iniciamos los semaforos
    semaforo_cuentas = sem_open(SEMAFORO_MUTEX_CUENTAS,0);
    semaforo_log = sem_open(SEMAFORO_MUTEX_LOG,0);
    semaforo_fifo = sem_open(SEMAFORO_MUTEX_FIFO,0);


    //Vemos si ha habido fallos con los semaforos
    if (semaforo_cuentas == SEM_FAILED)
    {
        //Si no puede abrir el semaforo la logica del programa se ve afectada por lo que salimos
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir semaforo de cuentas en init_cuenta",errno);
        escribirLog(mensaje);
        fprintf(stderr,"%s\n",mensaje);
        //Genero un señal de muerte para matarme
        kill(pid_programa,SIGTERM);
    }
    if (semaforo_fifo == SEM_FAILED)
    {
        //Si no puede abrir el semaforo la logica del programa se ve afectada por lo que salimos
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir semaforo de FIFO en init_cuenta",errno);
        escribirLog(mensaje);
        fprintf(stderr,"%s\n",mensaje);
        //Genero un señal de muerte para matarme
        kill(pid_programa,SIGTERM);
       
    }    

    if (semaforo_log == SEM_FAILED)
    {
        //Si no puede abrir el semaforo la logica del programa se ve afectada por lo que salimos
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir semaforo de log en init_cuenta",errno);
        escribirBanco(mensaje);
        fprintf(stderr,"%s\n",mensaje);
        //No me mato ya que necesito acceso al log para eso cosa que no tendo
        kill(pid_programa,SIGTERM);
       
    }  
    
   

    //Reflejamos en el log que se ha iniciado el programa
    snprintf(mensaje,sizeof(mensaje),"Se ha iniciado init cuenta en modo %d",modo_ejecucion);
    escribirLog(mensaje);

    //El modo de ejecucion permite polivalencia , ya que:
    //Si es 0 - El programa sera una intefaz para añadir un nuevo usuario
    //SI es 1 - El programa generara un fichero cuentas.dat con una serie de datos
    activo = true;
    if ( modo_ejecucion == 0)
    {
        //Avisamos al banco de que hemos iniciado el programa
     
        snprintf(mensaje,sizeof(mensaje),"0-%d",pid_programa);
        escribirBanco(mensaje);
    
        menu(argv[1]);

        //Notificamos al banco que se ha cerrado el programa

        snprintf(mensaje,sizeof(mensaje),"0-%d",pid_programa);
        escribirBanco(mensaje);

         
    }
    else
    {
        //Aqui no tiene sentido avisar al banco, ya que cuando se llama es este modo todavia no esta activo el
        //control del procesos del banco
        generarDatos(argv[1]);
    }
    activo = false;

    //El programa ha acabado asi que actualizamos el log             
    snprintf(mensaje,sizeof(mensaje),"Se ha cerrado init cuenta en modo %d",modo_ejecucion);
    escribirLog(mensaje);


    return 0;
    
}

//Nombre: escribirBanco.
//Retorno: void.
//Parámetros: mensaje (char *).
//Uso: Envía un mensaje al proceso del banco a través de un FIFO, protegiendo el acceso con un semáforo.
void  escribirBanco(char * mensaje)
{
    
    //Creamos las variables
    int fd;
    char *aux[255];
    char error[255];


    //Abrimos el fifo para escribir el mensaje
    
    sem_wait(semaforo_fifo);
    semaforos_usados[2]=1;
    fd = open(FIFO_PROGRAMAS_BANCO,O_WRONLY);
   
   
    //Vemos si ha habido algun errro al abrilo
    if (fd == -1)
    {
        //Avisamos al usuario y lo ponemos en el log
        snprintf(error,sizeof(error),"Error %d de init_cuenta al abrir el fifo programas banco",errno);
        fprintf(stderr,"%s\n",error);
        escribirLog(mensaje);
        sem_post(semaforo_fifo);
        semaforos_usados[2]=0;
     
        //Generamos una señal para automatarnos
        kill(pid_programa,SIGTERM);
    }
    
    write(fd,mensaje,strlen(mensaje)+1);
    //Cerramops el fifo
    close(fd);
   
    sem_post(semaforo_fifo);
    semaforos_usados[2]=0;
   
    return;
}


//Nombre: localizarUltimo.
//Retorno: int.
//Parámetros: ficheroParam (char *).
//Uso: Localiza la última línea del fichero especificado para determinar el siguiente ID de usuario disponible.

int localizarUltimo(char * ficheroParam)
{
    //Esta funcion lo uncio que hace es localizar la ultima linea del fichero para asignar luego un id al usuario
   
 

    //Creamos las variables
    FILE * fichero;
    int contador;
    char linea[255],mensaje[255];

    //Bloqueamos el semaforo de la exclusion muta de cuentas
    sem_wait(semaforo_cuentas);
    semaforos_usados[0]=1;
    if ((fichero = fopen(ficheroParam,"r") )== NULL)
    {
        //Si no puede abrir el fichero la logica del programa se ve afectada por lo que salimos
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir el fichero %s",errno,ficheroParam);
        escribirLog(mensaje);
        fprintf(stderr,"%s\n",mensaje);
        //Liberamos el semaforo del hilo y de exclusion mutua del fhcero
        sem_post(semaforo_cuentas);
        semaforos_usados[0]=0;
        //Generamos una señal para automatarnos
        kill(pid_programa,SIGTERM);
    }


    contador =0;
    //Leemos el fichero y contamos las lineas, de tal manera que la ultima linea sera el id del cliente
    //ya que las lineas empiezan en 1 y los id en 0
    while(fgets(linea,255,fichero) != NULL) contador++;
    
    //Cerramos el fichero  
    fclose(fichero);
    //Notificamos que se ha abierto el fichero
    sem_post(semaforo_cuentas);
    semaforos_usados[0]=0;

   
    //Actualizamos el log
    snprintf(mensaje,sizeof(mensaje),"Se ha abierto el fichero %s",ficheroParam);
    escribirLog(mensaje);

    escribirLog("Se ha localizado el ultimo");

    snprintf(mensaje,sizeof(mensaje),"Se ha cerrado el fichero %s",ficheroParam);
    escribirLog(mensaje);  
 
    return contador;
    
}
//Nombre: menu.
//Retorno: void.
//Parámetros: ficheroParam (char *).
//Uso: Crea una nueva cuenta de usuario, pidiendo los datos por terminal
//escribiéndola en el fichero especificado y registrando los eventos en el log.
void  menu(char * ficheroParam)
{
   
 

    //Creamos las variables
    int fd;
    FILE *fichero;
    USUARIO user = {"",0,0,0};
    char mensaje[255],nombre[250];

   

    
    printf("CREAR CUENTA\n");
    printf("Cual es tu nombre: ");
    //Uso fgets para que luego getchar() no me de problemas
    fgets(user.nombre,100,stdin);
    user.nombre[strcspn(user.nombre,"\n")] = '\0';

    //Simulamos que esta pensando el programa
    printf("\nCreando cuenta...\n");
    sleep(2);
    //Asgino el numero de cuentas
    user.nCuenta = localizarUltimo(ficheroParam);

    //Abrimos el fichero para escribir bloqueando su semaforo de exclusion mutua
    sem_wait(semaforo_cuentas);
    semaforos_usados[0]=1;
    if ((fichero = fopen(ficheroParam,"a") )== NULL)
    {
        //Si no puede abrir el fichero la logica del programa se ve afectada por lo que salimos
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir el fichero %s",errno,ficheroParam);
        escribirLog(mensaje);
        fprintf(stderr,"%s\n",mensaje);
        //Generamos una señal para automatarnos
        kill(pid_programa,SIGTERM);
    }

   

    //Agregamos el usuario al fichero
    snprintf(mensaje,sizeof(mensaje),"%s;%d;%d;%d\n",user.nombre,user.nCuenta,user.saldo,user.transaciones);
    fprintf(fichero,"%s",mensaje);

    //Cerramos el fichero
    fclose(fichero);
    //Notificamos que se ha cerrado el fichero y lo liberamos
    sem_post(semaforo_cuentas);
    semaforos_usados[0]=0;

    //Actualizamos el log
    snprintf(mensaje,sizeof(mensaje),"Se ha abierto el fichero %s",ficheroParam);
    escribirLog(mensaje);

   escribirLog("Se ha creado un usuario");

    snprintf(mensaje,sizeof(mensaje),"Se ha cerrado el fichero %s",ficheroParam);
    escribirLog(mensaje);

   
    //Datos para el usuario importantes
    printf("Cuenta creada...\n");
    printf("IMPORTANTE\n");
    printf("Tu numero de usuario es %d, no lo olvides se te pedira para inciar sesion\n",user.nCuenta);
    printf("Pulsa una  tecla para salir...\n");
    getchar();

    
  
 
    return;
}

//Nombre: generarDatos.
//Retorno: void.
//Parámetros: ficheroParam (char *).
//Uso: Genera datos iniciales de prueba y los escribe en el fichero de cuentas, protegiendo el acceso con un semáforo.
void  generarDatos(char * ficheroParam)
{
   

    //Creamos las variables
    FILE * fichero;
    char mensaje[255];
    pthread_t hilo_log;

    //Abrimos el fichero y bloqueamos su semaforo para protegerlo
    sem_wait(semaforo_cuentas);
    semaforos_usados[0]=1;
    if ((fichero = fopen(ficheroParam,"a")) == NULL)
    {
        //Notificamos al user y al log
        snprintf(mensaje,sizeof(mensaje),"Error al crear el fichero %s",ficheroParam);
        escribirLog(mensaje);
        
        fprintf(stderr,"%s\n",mensaje);
        //Liberamos los semaforos para no retener recursos
        sem_post(semaforo_cuentas);
        semaforos_usados[0]=0;
        //Generamos una señal para automatarnos, ya qee si no puede abrir el fichero la logica del programa se ve afectada 
        kill(pid_programa,SIGTERM);
        return;
    }
    //Generamos unos datos preestablecidos
    fprintf(fichero ,"nombre;numerocuenta;saldo;transacciones\n");
    fprintf(fichero ,"carlos;1;500;3\n");
    fprintf(fichero ,"juan;2;20;10\n");
    fprintf(fichero ,"sebastian;3;0;0\n");
    fprintf(fichero ,"maria;4;1200;40\n");
    fclose(fichero);
    //Notificamos que se ha cerrado el fichero
    sem_post(semaforo_cuentas);
    semaforos_usados[0]=0;
    //liberamos el hilo
    return;
}


//Nombre: seniales.
//Retorno: void.
//Parámetros: senial (int).
//Uso: Maneja señales del sistema (SIGINT, SIGHUP, SIGTERM, SIGUSR1), 
//realiza cierre controlado y libera recursos si es necesario.
void seniales(int senial)
{
    //Creamos las variables
    char mensaje[255];

    if ( senial == SIGINT || senial == SIGHUP || senial == SIGTERM || senial == SIGUSR1)
    {
        //vemos si estamos activos o no para tomar una decision
        if ( activo != 0)
        {
            printf("Se ha detectado un intento de matar el programa init cuenta,espere mientras se cierran los procesos...\n");
            snprintf(mensaje,sizeof(mensaje),"Se ha detectado un intento de matar el programa init cuenta, cerrando procesos...");
            escribirLog(mensaje);
            sleep(2);
            if (activo!= 0)
            {
                if ( semaforos_usados[0] == 1) sem_post(semaforo_cuentas);
                if ( semaforos_usados[1] == 1) sem_post(semaforo_log);
                if ( semaforos_usados[2] == 1) sem_post(semaforo_fifo);

                snprintf(mensaje,sizeof(mensaje),"No se han liberado los recursos pero si los semaforos de init_cuenta...");
                printf("%s\n",mensaje);
                escribirLog(mensaje);
            }
            else
            {
                
                snprintf(mensaje,sizeof(mensaje),"Se han liberado los recursos de init_cuenta...");
                printf("%s\n",mensaje);
                escribirLog(mensaje);
            }
            //Si llega la señal de usu 1 es que el nos esta matando asi que no le notificamos la muerte
            if (senial != SIGUSR1)
            {   snprintf(mensaje,sizeof(mensaje),"0-%d",pid_programa);  
                escribirBanco(mensaje);
            }
            _exit(0);

        }
        
        
    }
}

//Nombre: escribirLog.
//Retorno: void.
//Parámetros: mensaje (char *).
//Uso: Esta función escribe el mensaje pasado por parametroen el fichero de log 
//con la hora actual, protegiendo el acceso con un semáforo.
void escribirLog(char * mensaje)
{
    //Creamos las variables
    char *hora;
    time_t tiempo;
    FILE  * fichero;
    char error[255];
   

    //Protegemos el acceso a la variable global para que no haya condiciones de carrera no cosas raras
    sem_wait(semaforo_log);
    semaforos_usados[1]=1;
    if ((fichero = fopen(fichero_log,"a"))== 0)
    {
        //Notificamos en caso de error
        snprintf(error,sizeof(error),"Error %d al abrir el fichero %s",errno,fichero_log);
        fprintf(stderr,"%s",mensaje);
        //Avisamos al banco por si el puedira poner le mensaje
        escribirBanco(mensaje);
        //Liberamos el semaforo para indicar que la variable global ya esta disponible
        sem_post(semaforo_log);
        semaforos_usados[1]=0;
        
        //Nos automatamos
        kill(pid_programa,SIGTERM);
    }


    //Creamos la hora para el log
    time(&tiempo);
    //Lo pasamos a formato string
    hora = ctime(&tiempo);
    //quitamos el \n que por defecto pone la liberia de la hora
    hora[strcspn(hora,"\n")] = '\0';

    fprintf(fichero,"[%s] - %s\n",hora,mensaje);
    fclose(fichero);
    //Liberamos el acceso al fichero 
    sem_post(semaforo_log);
    semaforos_usados[1]=0;

    return;
}
