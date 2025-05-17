#include "comun.c"

//Definimos las funciones
void  escribirBanco(char * arg);
char * crearDirectorio(int cuentaId);
void  menu(char * fichero);
void  generarDatos(char * ficherParam);
void seniales(int senial);
void  escribirLog(char * mensaje);
void leerMemoriaCompartida();
int obtenerNumeroCuenta();
int contarLineas(char * fichero);




//Declaramos las variables globales
bool activo;
char *fichero_log;
pid_t pid_programa;
sem_t *semaforo_cuentas,*semaforo_log,*semaforo_fifo,*semaforo_memoria;
int semaforos_usados[4],fd_memoria;
MEMORIA * listaUsers;


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
    semaforo_memoria = sem_open(SEMAFORO_MUTEX_MEMORIA,0);


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
    if (semaforo_memoria == SEM_FAILED)
    {
        //Si no puede abrir el semaforo la logica del programa se ve afectada por lo que salimos
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir semaforo de memoria en init_cuenta",errno);
        escribirLog(mensaje);
        fprintf(stderr,"%s\n",mensaje);
        //Genero un señal de muerte
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
        leerMemoriaCompartida();
    
        menu(argv[1]);

        //Notificamos al banco que se ha cerrado el programa

        close(fd_memoria);
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


void leerMemoriaCompartida()
{
    
    int i;
    char linea[255],mensaje[255];

    snprintf(mensaje,sizeof(mensaje),"Init cuentas ha abierto la memoria compartida");
    escribirLog(mensaje);
    //Creamos el semaforo para la memoria compartida

    sem_wait(semaforo_memoria);
    fd_memoria = shm_open(MEMORIA_COMPARTIDA , O_RDWR,0666);
    i = 0;
    if (fd_memoria == -1 )
    {
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir la memoria compartida",errno);
        escribirLog(mensaje);
        fprintf(stderr,"%s\n",mensaje);
        //Liberamos el semaforo para no retener recursos
        sem_post(semaforo_memoria);
        //Generamos una señal para automatarnos
        kill(pid_programa,SIGTERM);
        
    } 

  
    listaUsers = mmap(0,sizeof(MEMORIA),PROT_READ | PROT_WRITE,MAP_SHARED,fd_memoria,0);
    if (listaUsers ==MAP_FAILED)
    {
        snprintf(mensaje,sizeof(mensaje),"Error %d al mapear la memoria compartida",errno);
        escribirLog(mensaje);
        fprintf(stderr,"%s\n",mensaje);
        //Liberamos el semaforo para no retener recursos
        sem_post(semaforo_memoria);
        //Generamos una señal para automatarnos
        kill(pid_programa,SIGTERM);
    }

    sem_post(semaforo_memoria);
    snprintf(mensaje,sizeof(mensaje),"Init cuentas ha leido la memoria compartida");
    escribirLog(mensaje);
    return;

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


//Nombre: menu.
//Retorno: void.
//Parámetros: ficheroParam (char *).
//Uso: Crea una nueva cuenta de usuario, pidiendo los datos por terminal
//escribiéndola en el fichero especificado y registrando los eventos en el log.
void  menu(char * file)
{
    //Creamos las variables
    int fd,n_cuenta,i,j;
    char*fichero;
    char mensaje[255],nombre[250];

   

    
    printf("CREAR CUENTA\n");
    printf("Cual es tu nombre: ");
    //Uso fgets para que luego getchar() no me de problemas
    fgets(nombre,100,stdin);
    nombre[strcspn(nombre,"\n")] = '\0';

    //Simulamos que esta pensando el programa
    printf("\nCreando cuenta...\n");
    sleep(2);
    sem_wait(semaforo_memoria);
    //Asgino el numero de cuentas

    //Aplicamos la politica de sustitucion en memoria en caso de que haya llegado al maximo de usuarios
    if ( listaUsers->n_users >= MAX_USUARIO)
    {
        for ( j = 0; j<MAX_USUARIO;j++)
        {
            if ( listaUsers->lista[j].activo == 0) break;
        }
        i = j;
    }
    else
    {
        i = listaUsers->n_users++;
    }
    n_cuenta = contarLineas(file);
    //Vemos si ha habido errores al abrir el fichero
    if (n_cuenta == -1) return;


    listaUsers->lista[i].id = n_cuenta;
    strcpy( listaUsers->lista[i].nombre , nombre);
    listaUsers->lista[i].operaciones = 0;
    listaUsers->lista[i].saldo = 0;
    

    //Notificamos que se ha cerrado el fichero y lo liberamos
    sem_post(semaforo_memoria);
    semaforos_usados[0]=0;

   
    escribirLog("Se ha creado un nuevo usuario");

    fichero = crearDirectorio(n_cuenta);
    strncpy(listaUsers->lista[i].fichero, fichero, sizeof(listaUsers->lista[i].fichero) - 1);
    listaUsers->lista[i].fichero[sizeof(listaUsers->lista[i].fichero) - 1] = '\0';
    free(fichero);
    escribirBanco("2");
   
    //Datos para el usuario importantes
    printf("Cuenta creada...\n");
    printf("IMPORTANTE\n");
    printf("Tu numero de usuario es %d, no lo olvides se te pedira para inciar sesion\n",n_cuenta);
    printf("Pulsa una  tecla para salir...\n");
    getchar();

    
    
 
    return;
}
//Nombre: contarLineas.
//Retorno: int.
//Parámetros: file (char *).
//Uso: Cuenta el número de líneas en un fichero, protegiendo el acceso con un semáforo.
//      Si no puede abrir el fichero, notifica el error y cierra el programa.
//      Devuelve el número de líneas o -1 en caso de error.
int contarLineas(char * file)
{
    int contador = 0;
    FILE *fichero;
    char linea[255];
    char mensaje[255];

    //Abrimos el fichero y bloqueamos su semaforo para protegerlo
    sem_wait(semaforo_cuentas);
    semaforos_usados[0]=1;
    if ((fichero = fopen(file,"r")) == NULL)
    {
        //Notificamos al user y al log
        snprintf(mensaje,sizeof(mensaje),"Error %d al abrir el fichero %s",errno,fichero_log);
        escribirLog(mensaje);
        fprintf(stderr,"%s\n",mensaje);
        //Liberamos los semaforos para no retener recursos
        sem_post(semaforo_cuentas);
        semaforos_usados[0]=0;
        //Generamos una señal para automatarnos, ya que si no puede abrir el fichero la logica del programa se ve afectada 
        kill(pid_programa,SIGTERM);
        return -1;
    }
    //Contamos las lineas del fichero
    contador = 0;
    while (fgets(linea,sizeof(linea),fichero) != NULL)
    {
        contador++;
    }
    fclose(fichero);
    //Notificamos que se ha cerrado el fichero
    sem_post(semaforo_cuentas);
    semaforos_usados[0]=0;
    //Devolvemos el numero de lineas
    return contador;
}
//Nombre: obtenerNumeroCuenta.
//Retorno: int.
//Parámetros: Ninguno.
//Uso: Devuelve el siguiente número de cuenta disponible, buscando en la lista de usuarios activos.
//      Protege el acceso con un semáforo.
int obtenerNumeroCuenta()
{
    int i,max;
    max = 0;
    for ( i = 0; i<listaUsers->n_users;i++)
    {
      if ( listaUsers->lista[i].id > max) max = listaUsers->lista[i].id;
    }
    return max+1;
}
//Nombre: crearDirectorio.
//Retorno: char *.
//Parámetros: cuentaId (int).
//Uso: Crea un directorio para el usuario con su ID y un fichero de transacciones,
//      protegiendo el acceso con un semáforo. Si hay error, se notifica y se cierra el programa.
char * crearDirectorio(int cuentaId  )
{
    char directorio[500],archivo[700],mensaje[900];
    FILE *fichero;
    snprintf(directorio,sizeof(directorio),"./ficheros/usuario_%d",cuentaId);
    if (mkdir(directorio,0777) == -1)
    {
        //Notificamos al user y al log
        snprintf(mensaje,sizeof(mensaje),"Error %d al crear el directorio %s",errno,directorio);
        escribirLog(mensaje);
        fprintf(stderr,"%s\n",mensaje);
        //Generamos una señal para automatarnos, ya que si no puede abrir el fichero la logica del programa se ve afectada 
        kill(pid_programa,SIGTERM);
        return NULL;
    }
    //Generamos el fichero de cuentas
    snprintf(archivo,sizeof(archivo),"%s/transacciones_%d.log",directorio,cuentaId);
    fichero = fopen(archivo,"a");
    if (fichero == NULL)
    {
        
        //Notificamos al user y al log
        snprintf(mensaje,sizeof(mensaje),"Error %d al crear el fichero %s",errno,archivo);
        escribirLog(mensaje);
        fprintf(stderr,"%s\n",mensaje);
        //Generamos una señal para automatarnos, ya que si no puede abrir el fichero la logica del programa se ve afectada 
        kill(pid_programa,SIGTERM);
        return NULL; 
    }
    escribirLog("Se ha creado el directorio para el nuevo usuario");
    return strdup(archivo);

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
    fprintf(fichero, "andres;6;890.00;15\n");
    fprintf(fichero,"laura;5;0.00;0\n");
    fprintf(fichero, "sofia;7;25.75;2\n");
    fprintf(fichero, "miguel;8;100.00;1\n");
    fprintf(fichero, "valentina;9;1340.60;50\n");
    fprintf(fichero, "david;10;430.20;12\n");
    fprintf(fichero, "camila;11;980.00;27\n");
    fprintf(fichero, "santiago;12;0.00;0\n");
    fprintf(fichero, "isabella;13;760.30;19\n");
    fprintf(fichero, "diego;14;115.10;4\n");
    fprintf(fichero, "antonella;15;2464.00;67\n");

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
