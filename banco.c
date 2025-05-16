#include "comun.c"
//Definimos las funciones
void  escribirLog(char * mensaje);
void  modifificarArrayProcesos(char * );
void * leerMensajes(void * arg);
void  leerConfiguracion(char *);
void loginUsuario(int id_user);
void matarProcesos();
void menu();
void iniciarSemaforos();
void terminarSemaforos();
void seniales(int senial);
void cerrarBanco();
void  listarPila();
void  agregarPila(char *);
void generarMemoriaCompartida();
char * crearArchivoUsuario(int id_usuario);
void * Buffer(void * arg);
void inicializarArchivosUsuarios();
void listarUsuariosEnMemoria();
int remplazarUsuario(char * datos);
char * buscarUserEnFichero(int user_id);
void guardarFichero();
void listarPids();
//Declaramos las variables globales asi como las estruturas necesarias

typedef struct propiedades
{
 
    int limite_retiro;
    int limite_tranferecnia;
    int limite_ingreso;

    int max_retiros;
    int max_tranferecnias;
    int max_ingreso;
    int max_tranfers_cuentas;

    int num_hilos;

    char archivo_cuentas[200];
    char archivo_log[255];
    
    pid_t arraypids[255];
    int n_procesos_activos;
    int tiempo_buffer;
    
    
}PROPIEDADES;

typedef struct pila
{
    char * mensajes[300];
    int indice;
}PILA;



PROPIEDADES PROPS;
PILA pilaAlertas;
MEMORIA * listaUsers;
pid_t pidgeneral;
sem_t * semaforo_hilos,*semaforo_config,*semaforo_cuentas,*semaforo_trans,*semaforo_log,*semaforo_fifo,*semaforo_memoria;
sem_t semaforo_control;
int fd_memoria;
int buff_start;
//Variables para el cerrado forzado
pthread_t hilo_mensajes,hilo_buffer;
int modo_cerrado = 0;
int opcion = 0;

int main(int argc, char* argv[])
{
    //Si no se ha pasado el fichero .conf el programa no puede funcionar 
    //asi que se sale 
    if (argc != 2)
    {
        printf("Necesito un solo parametro y es la ruta del fichero .conf como parametro\n");
        exit(1);
    }
    //Contorlamos las señales
    signal(SIGINT ,seniales);
    signal(SIGHUP,seniales);
    signal(SIGTERM,seniales);
    signal(SIGUSR2,seniales);


    //Creamos las variables
    pid_t pid;
    struct  stat st;
    char mensaje[300];
    pidgeneral = getpid();
    pilaAlertas.indice = -1;
    //Iniciamos los semaforos
    iniciarSemaforos();
    //Leemos la configuracion del .conf
    leerConfiguracion(argv[1]);
 
   
    //Creamos los fifos para la comunicacion
    mkfifo(FIFO_PROGRAMAS_BANCO,0666);
     
  
   sem_wait(&semaforo_control);
    //Vemos si el fichero de cuentas existe
    if (stat(PROPS.archivo_cuentas,&st) == -1)
    {
        //Si no existe lo creamos
        if (fork()==0) execl("./init_cuenta","init_cuenta",PROPS.archivo_cuentas,"1",PROPS.archivo_log,(char *)NULL);
        
        
       
    }
    else  if (st.st_size == 0)
    {
        //En caso de que asi lo sea es que el programa se acaba de instalar, asi que llamamos a init_cuenta ,con parametro 1
        //Lo que le indica que tiene que cargar la cabecera, ademas de una serie de usarios por  defecto 
        if (fork()==0) execl("./init_cuenta","init_cuenta",PROPS.archivo_cuentas,"1",PROPS.archivo_log,(char *)NULL);
    }
    sem_post(&semaforo_control);
    sleep(2);
    //Inciamos el hilo que escucha mensajes
    pthread_create(&hilo_mensajes,NULL,leerMensajes,NULL);
    

    
    sem_wait(&semaforo_control);
    generarMemoriaCompartida();
    sem_post(&semaforo_control);

    pthread_create(&hilo_buffer,NULL,Buffer,NULL);
    inicializarArchivosUsuarios();
    
    //Inicamos el menu del banco
    menu(NULL);
    //Cuando este hilo se acaba es que se ha pulsado la opcion de salir, asiq que iniciamos los procesos para cerrar
    //todo el sisstema de manera segura y sin dejar archivos residuales 

    cerrarBanco();
  
    return 0;
}

//Nombre: cerrarBanco
//Retorno: void
//Parametro: ninguno
//Uso: Esto cierra de manera segura el banco.Detiene los procesos que esten activos , 
//      cierra los hilos , limpia todos los recursos y elimina los archivos FIFO.
void cerrarBanco()
{
    int fd;
   
    matarProcesos();
    //Cerramos de manera segura el hilo que leera los mensajes
    
    fd = open(FIFO_PROGRAMAS_BANCO,O_WRONLY);
    write(fd,"salir",5);
    close(fd);
    
    //Esperamos a que el hilo de mensajes se cierre
    pthread_join(hilo_mensajes,NULL);

    if (!modo_cerrado) escribirLog("Cerrando banco de manera segura");
    else escribirLog("Cerrando banco de manera forzada");

    if (!modo_cerrado) printf("Cerrando banco de manera segura\n");
    else escribirLog("Cerrando banco de manera forzada\n");
 
    sleep(2);
    escribirLog("----------------------");
    
    guardarFichero();
    //Cerramos y borramos tanto los semaforos como los archivos de los mismos
    terminarSemaforos();
    //Borramos los fifos para que no dejar archivos residuales
    unlink(FIFO_PROGRAMAS_BANCO);
    munmap(listaUsers,sizeof(MEMORIA));
    close(fd_memoria);
    shm_unlink(MEMORIA_COMPARTIDA);
    
    

    exit(0);   
}
//Nombre: seniales.
//Retorno: void.
//Parametros: seniales (int)
//Uso: En esta función maneja las señales que pueden llegar a los procesos
void seniales(int seniales)
{
    int fd;
    if (seniales == SIGINT || seniales == SIGHUP || seniales == SIGTERM)
    {
        //Si se recibe esta señal es que el banco se ha forzado a cerrar
        //Cerramos el menu
        opcion = 6;
        modo_cerrado = 1;
        cerrarBanco();
    }
    else if (seniales == SIGUSR2)
    {
        //Si recibo esto es que monitor ha muerto asi que debo morir
        opcion = 6;
        modo_cerrado = 1;
        cerrarBanco();
    
    }
    return;
}
//Nombre: AgregarPila.
//Retorno: void.
//Parametros: char * aux
//Uso: La función se encarga de agregar un mensaje a la pila de alertas.
void agregarPila(char * aux)
{

    //Creamos las variables
    char copia[1024];
    char *prefijo,*mensaje;
    

    //Copiamos el mensaje para no modificar el original y evitar  probelmas ya que son mensajes temporales
    strncpy(copia, aux, sizeof(copia));
    copia[sizeof(copia) - 1] = '\0';  // Por seguridad


    //Obtenemos el mensaje 
    prefijo = strtok(copia,"-");
    mensaje = strtok(NULL,"-");

    //Vemos si la pila esta llena
    if (pilaAlertas.indice + 1 >300) 
    {
        printf("Ya no caben mas mensajes en la pila...");
        escribirLog("La pila esta llena...");
        return;
    }

    //sino esta llena ponemos el mensaje
    pilaAlertas.indice ++;
    pilaAlertas.mensajes[pilaAlertas.indice]=strdup(mensaje);
    return ;

}
//Nombre: listarPila
//Retorno:: void
//Parametros: ninguno
//Uso: Esta funcion se encarga de listar los mensajes almacenados en la pila de alertas.
void listarPila()
{

    //Creamos las variables
    int i ;
    //Si la pila esta vacia damos error
    if (pilaAlertas.indice == -1 )printf("La pila de mensajes esta vacia\n");
    else 
    {
        for (i = 0;i<=pilaAlertas.indice;i++) printf("%s\n",pilaAlertas.mensajes[i]);
    }
    return;
    
}

//Nombre: modifificarArrayProcesos.
//Retorno: void.
//Parametros:char *.
//Uso: Esta función se encarga  de gestionar el array de los procesos activos.
void  modifificarArrayProcesos(char * codigo)
{
    
    //Creamos las variables
    int i;
    bool eliminar;
    pid_t pid,aux;
    char *prefijo,*pid_char;
    char mensaje[255];

    //Copiamos el mensaje para no modificar el original y evitar  problemas ya que son mensajes temporales
    strncpy(mensaje,codigo,sizeof(mensaje));
    mensaje[sizeof(mensaje) - 1] = '\0';  // Por seguridad


    //Primero separamos el prefijo del pid del programa hijo
    prefijo=strtok(codigo,"-");
    pid_char = strtok(NULL,"-");

    pid = (pid_t) atoi(pid_char);
    eliminar = false;
    for (i = 0; i<PROPS.n_procesos_activos;i++)
    {
        
        if (pid == PROPS.arraypids[i])
        {
            //En caso de que el pid coincida es que el proceso se ha cerrado por lo que hay que eliminar dicho proceso del array
            
            eliminar = true;
            break;
        }
    }
    //Si no ha habido coincidencias es que el pid es nuevo por tanto se agrega
    if (!eliminar) PROPS.arraypids[PROPS.n_procesos_activos ++ ] = pid;  
    else
    {
        //hacemos como en una pila, ponemos el que hay que eliminar al final y decrementamos el indice de tal manera
        //que aunque siga ahí no se contabilizara y cuando llegue uno nuevo lo sobreescribira
        aux = PROPS.arraypids[i];
        PROPS.arraypids[i] = PROPS.arraypids[PROPS.n_procesos_activos - 1];
        PROPS.arraypids[PROPS.n_procesos_activos - 1] = aux;
        PROPS.n_procesos_activos --;
    }
   

    
    return; 

}

void listarPids()
{
    int i;
    for (i = 0; i<PROPS.n_procesos_activos;i++)
    {
        printf("El pid %d esta activo\n",PROPS.arraypids[i]);
    }
}

void * leerMensajes(void * arg)
{
    //Creamos todas las variables
    bool salir;
    int * cerrar;
    int fd_lectura;
    char mensaje[1024];
    ssize_t bytes_leidos;
    pthread_t hilo_monitor,hilo_array,hilo_pila;
    
  
    //Esta varibel nos ayuda a cerrar el hilo de manera segura
    salir = false;
    

    //Creamos un doble bucle por si alguien cierra el extremo de escritura que no se cierre el hilo y se vuelva a abrir el extremo
    while(1)
    {
        //Abrimos el extremo de lectura
        fd_lectura = open(FIFO_PROGRAMAS_BANCO, O_RDONLY);
        if (fd_lectura == -1)
        {
            //Notificamos en caso de error 
            snprintf(mensaje,sizeof(mensaje),"Error %d al abrir el fifo de lectura del banco",errno);
            fprintf(stderr,"%s\n",mensaje);
            escribirLog(mensaje);
            //Es un error critico , sin poder comunicarmen no sirvo asi que me muero
            kill(pidgeneral,SIGTERM);
        }
        
        
        
        //Este bucle se encarga de leer todo el rato
        while (true)
        {
            //en este caso queremos cerrar el hilo , por tanto activamos la variable para salir y salimos de este bucle
            //como el otro bucle cumprueba la variable al estar en true , saldremos de ambos acabando el hilo
            

            bytes_leidos = read(fd_lectura,mensaje,sizeof(mensaje));
            //Si se ha leido algo , en funcion del mensaje, se hace una cosa u otra
            if (bytes_leidos > 0)
            {
                mensaje[bytes_leidos] = '\0';
                //En funcion del codigo haremos una cosa u otra , estos codigos sirven para catalogar los mensajes
                //ya que solo hay un hilo y varios programas, por tanto no se sabe de donde viene el mensaje, pero 
                //0-x pid para controlar los procesos activos
                //1-x mensaje de alerta del monitor
                //2-x mensaje de inicio del buffer
                //3-x mensaje para remplazar un usuario en memoria por el que le llega
                //nada - mensaje que nos envian porgramas cuando no pueden escribir en sus logs
                //por si el banco puede para reflejar el error
                if ( mensaje[0] == '0') modifificarArrayProcesos(mensaje);
                
                else if (mensaje[0] == '1') agregarPila(mensaje);
                else if (mensaje[0] == '2') buff_start = 1;
                else if (mensaje[0]== '3')
                {
                   
                    char *aux,*aux2;
                    aux = strtok(mensaje,"-");
                    aux2 = strtok(NULL,"-");

                    remplazarUsuario(aux2);
                } 
                            
                else if (strcmp(mensaje,"salir")==0 )
                {
                    //Cerramos el extremo de lectura, ya que se ha acabado el programa y nadie mas nos va a escribir
                    close(fd_lectura);
                    buff_start = 2;
                    return NULL;
                }
                else escribirLog(mensaje);

            }
            //Este caso se dara cuando alguien cierre el extremo de escritue, por lo que salimos del bucle
            //Pero como hay otro bucle, se volvera a abir el extremo de lectura
            else if (bytes_leidos == 0) break;
            
        }
    }

    return NULL;
}

//Nombre :escribirLog
//retorno: void
//Parametros: mensaje (char *)
//Uso: Esta funcion , escribe un mensaje en el fichero log, junto con la hora en la que se realiza la entrada.
//A demás de esto se asegura de que no suceda condiciones de carrera mediante semáforos.

void escribirLog(char * mensaje)
{


    //Creamos las variables
    char *hora;
    char error[300];
    time_t tiempo;
    FILE  * fichero;
    //Bloqueamos el acceso al recurso
    sem_wait(semaforo_log);
    if ((fichero = fopen(PROPS.archivo_log,"a"))== 0)
    {
        //Notificamos en caso de error
        snprintf(error,sizeof(error),"Error %d al abrir el fichero %s",errno,PROPS.archivo_log);
        fprintf(stderr,"%s\n",error);
    
       //Liberamos el recurso
        sem_post(semaforo_log);
        
        return;
    }


    //Creamos la hora para el log
    time(&tiempo);
    //Lo pasamos a formato string
    hora = ctime(&tiempo);
    //quitamos el \n que por defecto pone la liberia de la hora
    hora[strcspn(hora,"\n")] = '\0';

    fprintf(fichero,"[%s] - %s\n",hora,mensaje);
    fclose(fichero);
    //Liberamos el semaforo para que otros procesos puedan acceder al log
    sem_post(semaforo_log);
    
    return;
}
//Nombre: leerConfiguracion.
//Retorno: void
//Parámetros: char * .
//Uso: Esta función se encarga de leer un fichero de configuración, el cual se pasara como 
//      parametro de la funcion, con parámetros como clave,valor.
//      Ademas los asigna a la estrutura PROPS, para configurar bien el programa

void leerConfiguracion(char * nombre_fichero)
{
    //Creamos las variables
    FILE* fichero;
    char *clave, *valor;
    char linea[1024], mensaje[255];
   
 
   
    sem_wait(semaforo_config);
    //Abrimos el fichero con la configuracion
    if ((fichero =fopen(nombre_fichero,"r"))== NULL)
    {
        //En caso de error cerramos el programa ya que sin configuracion no puede funcionar
        printf("Error %d al abrir el fichero %s\n",errno,nombre_fichero);
        sem_post(semaforo_config);
        exit(1);
    }
    
    while (fgets(linea,1024,fichero) != NULL)
    {
       //Quitamos el salto de linea y caracteres raros
       linea [strcspn(linea,"\n")] =0;

      
    

        //Para cada linea , dividimos la clave y el valor los cuales se separan por =, ej
        //Si tenemos fichero=fichero.txt clave=fichero y valor=fichero.txt
        clave = strtok(linea,"=");
        valor = strtok(NULL,"=");
       

        //verificamos que ambos valores sean validos
        if (clave != NULL && valor != NULL)
        {
          
            //Vemos que tipo de clave es y en funcion a eso asignamos los valores
            if (strcmp(clave,"LIMITE_RETIRO") == 0 ) PROPS.limite_retiro = atoi(valor);
            else if (strcmp(clave, "LIMITE_TRANSFERENCIAS") == 0) PROPS.limite_tranferecnia = atoi(valor); 
            else if (strcmp(clave, "LIMITE_INGRESO") == 0) PROPS.limite_ingreso = atoi(valor); 
            else if (strcmp(clave, "MAXIMO_TRANSFERENCIAS_ENTRE_CUENTAS") == 0) PROPS.max_tranfers_cuentas = atoi(valor);
            else if (strcmp(clave, "MAXIMO_RETIROS") == 0) PROPS.max_retiros = atoi(valor);          
            else if (strcmp(clave, "MAXIMO_TRANSFERENCIAS") == 0) PROPS.max_tranferecnias = atoi(valor);
            else if (strcmp(clave, "MAXIMO_INGRESO") == 0) PROPS.max_ingreso = atoi(valor);
            else if (strcmp(clave, "NUM_HILOS") == 0) PROPS.num_hilos = atoi(valor);
            else if (strcmp(clave, "TIEMPO_BUFFER") == 0) PROPS.tiempo_buffer = atoi(valor);
            else if (strcmp(clave, "ARCHIVO_CUENTAS") == 0)
            {   
                strncpy(PROPS.archivo_cuentas, valor, sizeof(PROPS.archivo_cuentas) - 1);
                PROPS.archivo_cuentas[sizeof(PROPS.archivo_cuentas) - 1] = '\0'; 
              
            }
            else if (strcmp(clave, "ARCHIVO_LOG") == 0)
            {              
                strncpy(PROPS.archivo_log, valor, sizeof(PROPS.archivo_log) - 1);
                PROPS.archivo_log[sizeof(PROPS.archivo_log) - 1] = '\0';
               
            }
            //Si una clave del fichero no coincide con los parametros es que esta mal,
            //Por tanto cerramos el programa , ya que o hay un parametro inecesario o estal mal escrito en el fichero
            else
            {
                fprintf(stderr,"Error al leer el fichero de configuracion hay claves que no coinciden, revise los parametros del mismo");
                exit(1);
            }
    
        }
        //Si alguna es null es que ha habido algun error por tanto salimos del programa ya que hay datos corruptos
        else
        {
            fprintf(stderr,"Alguna de las claves esta vacia\n");
            exit(1);
        }        
    }

    //Cerramos el fichero
    fclose(fichero);
    //Liberamos el semaforo que controla la exclusion mutua del fichero
    sem_post(semaforo_config);
   

    //Comprobamos si la configuracion de hilos es optima
    if (PROPS.num_hilos < 1)
    {
        //Si no hay hilos configurados no tiene sentido que el programa incie, asi que lo cerramos
        printf("Tiene que haber al menos un hilo cambia el .conf \n");
        exit(1);
    }
    //Por el numero de funciones que puede llegar a haber en paralelo, menos de 10 hilos pueden dar problemas
    //pero esto no es un error, simplemente se avisa pero se permite que el usuario siga con el programa
    if (PROPS.num_hilos < 10)
    {
        printf("Por temas de rendimiento se recomienda poner mas de 10 hilos , cierre el programa y cambie el .conf\n");
    }
   

    //Este semaforo se crea despues ya que necesitamos saber parte de la configuracion
    semaforo_hilos = sem_open(SEMAFORO_CONTROL_HILOS,O_CREAT | O_EXCL ,0666,PROPS.num_hilos);
    if (semaforo_hilos == SEM_FAILED)
    {
        //si falla un semaforo, el programa no se puede contolar , por tanto no puede ejecutarse asi que se cierra
        snprintf("Error %d al abrir el semaforo %s",errno,SEMAFORO_CONTROL_HILOS);
        fprintf(stderr,"%s\n",mensaje);
        escribirLog(mensaje);
     
        //Nos automatamos
        kill(pidgeneral,SIGTERM);
    }
  

  
    //Ponemos en el log la secuencia para indicar que se ha iniciado el banco  
    escribirLog("---------------------------");
    escribirLog("Inciando banco");
    escribirLog("Se esta cargando la configuracion del banco");

    snprintf(mensaje,sizeof(mensaje),"Se ha abierto el fichero %s",nombre_fichero);
    escribirLog(mensaje);

    escribirLog("Se ha cargado la configuracion del programa");
   
    snprintf(mensaje,sizeof(mensaje),"Se ha cerrado el fichero %s",nombre_fichero);
    escribirLog(mensaje);

    return ;

    
    
}
//Nombre: verificarUsuario
//Retorno : void
//Parametros : char *.
//Uso :Esta función lo que realiza es verificar si un usuario  existe, y de ser asi la abriremos
//      una nueva terminal con su sesion
//      Este tambien se encarga de asegurarse de que no hayan problemas de carrera con semáforos

void loginUsuario(int id_user) {


    // Variables locales
    pid_t pid;
    int n_cuenta;
    char *datos_user;
    bool encontrado = false;
    int i;
    while (1) 
    {
       

        if (id_user == -1) 
        {
            printf("LOGIN DE USUARIO\n");
            printf("Introduzca el ID de usuario: ");
            scanf("%d", &n_cuenta);
        } else 
        {
            n_cuenta = id_user;
        }

        // Buscar en lista en memoria
        for (i = 0; i < listaUsers->n_users; i++) 
        {
            if (n_cuenta == listaUsers->lista[i].id) 
            {
                encontrado = true;

                // Ejecutar el proceso de usuario
                char comando[1024];
                char *parametros[6];

                snprintf(comando, sizeof(comando), "./usuario %s %d %f %d %s %s %s",
                         listaUsers->lista[i].nombre,
                         listaUsers->lista[i].id,
                         listaUsers->lista[i].saldo,
                         listaUsers->lista[i].operaciones,
                         listaUsers->lista[i].fichero,
                         PROPS.archivo_log,
                         PROPS.archivo_cuentas
                    );

                parametros[0] = "gnome-terminal";
                parametros[1] = "--";
                parametros[2] = "bash";
                parametros[3] = "-c";
                parametros[4] = comando;
                parametros[5] = NULL;

                listaUsers->lista[i].activo = 1;

                pid = fork();
                if (pid == 0) 
                {
                    execvp("gnome-terminal", parametros);
                    escribirLog("Inicio de sesion fallido: error al abrir el programa de usuario");
                    perror("execvp failed");
                    exit(1);
                } else if (pid < 0) 
                {
                    escribirLog("Inicio de sesion fallido: fork de usuario ha fallado");
                    perror("Fork ha fallado");
                    return;
                }

                return; // Terminar tras login exitoso
            }
        }

        // Si no está en memoria, intentamos cargarlo
        datos_user = buscarUserEnFichero(n_cuenta);
        if (datos_user == NULL) {
            printf("Usuario no encontrado\n");
            escribirLog("Inicio de sesion fallido: no existe el usuario");
            sleep(1);
            return;
        }

        id_user = remplazarUsuario(datos_user);
        if (id_user == -1) {
            printf("No hay espacio para crear el nuevo usuario\n");
            escribirLog("Inicio de sesion fallido: no hay espacio para crear el nuevo usuario");
            return;
        }

    
        
    }
}

//Nombre: matarProcesos.
//Retorno: void.
//Parametros: Ninguno 
//Uso: Esta funcion todos los procesos activos del sistema.
void  matarProcesos()
{
    //Creamos las variables
    int i;
    char comando[255];
    //Protegemos el acceso a la variable global para que no haya condiciones de carrera no cosas raras
   
    for (i = 0 ; i<PROPS.n_procesos_activos ;i++)
    {
        //Para cada proceso de la lista, lo matamos y asi no dejamos ninguna sesion abierta
        kill(PROPS.arraypids[i],SIGUSR1);
    }
    //Limpiamos el indice por si acaso
    PROPS.n_procesos_activos = 0;
  
    return;
}


//Nombre: menu.
//Retorno: void
//Parametrod: char *
//Uso: Esta funcion se encarga de mostrar un menu interactivo al usuario con varias opciones.
void menu()
{
    

    //Creamos las variables
    int i;
    pid_t pid;
    char * params[6];
    char comando[1024], mensaje[255],salir;
    pthread_t hilo_verficar,hilo_comunicar;
    
    while (opcion != 7)
    {
        system("clear");
        printf("\nBienvenido al Banco\n");
        printf("1-Login\n");
        printf("2-Crear Cuenta\n");
        printf("3-Detectar anomalias con monitor\n");
        printf("4-Ver arbol procesos\n");
        printf("5-Ver anomalias (usar despues monitor)\n");
        printf("6-Ver usuarios en memoria\n");
        printf("7-Salir\n");
        printf("Que desea hacer: ");
        scanf("%d",&opcion);
        //Pedimos la opcion 
        
        //evaluamos y actuamos en funcion de lo pedido
        switch (opcion)
        {   
            case 1:
                //Si elige el incicio de sesion , se llama a la funcion correpondeinte
                loginUsuario(-1);
            break;

            case 2:
                //En caso de crear una cuenta lanzamos el programa init_cuenta con parametro 0 para indicar
                //que se debe registrar un nuevo usuario
                params[0] = "gnome-terminal";
                params[1] = "--";
                params[2] = "bash";
                params[3] = "-c";
                //Protegemos el acceso a la variable global para que no haya condiciones de carrera no cosas raras
             
                snprintf(comando, sizeof(comando), "./init_cuenta %s 0 %s" ,PROPS.archivo_cuentas,PROPS.archivo_log); 
                //Liberamos el semaforo para indicar que la variable global ya esta disponible
            
                params[4] = comando;
                
                params[5] = NULL;
                pid = fork();
                //Lanzamos el programa en un proceso independiente
                if (pid == 0)
                {
                        
                    escribirLog("Se ha iniciado el proceso de registro de un nuevo usuario");
                  
                    if (execvp("gnome-terminal",params)==-1)
                    {
                        //Si devuelve -1 es que ha habido error, por tanto lo notificamos
                        escribirLog("Ha fallado el proceso de creacion de un nuevo usuario");
                        perror("execvp failed");
                        return ;
                    }
                        
                        
                }

            break;
            case 3:
               pilaAlertas.indice = -1;
            //Lazamos el monitor
                if (fork()== 0)
                {
                    char pid_str[10], ingreso_str[10], retiro_str[10], transferencia_str[10];
                    char max_ingreso_str[10], max_retiros_str[10], max_transf_str[10],max_tranfer_cuentas_str[10];
            
                    sprintf(pid_str, "%d", pidgeneral);
                    sprintf(ingreso_str, "%d", PROPS.limite_ingreso);
                    sprintf(retiro_str, "%d", PROPS.limite_retiro);
                    sprintf(transferencia_str, "%d", PROPS.limite_tranferecnia);
                    sprintf(max_ingreso_str, "%d", PROPS.max_ingreso);
                    sprintf(max_retiros_str, "%d", PROPS.max_retiros);
                    sprintf(max_transf_str, "%d", PROPS.max_tranferecnias);
                    sprintf(max_tranfer_cuentas_str, "%d", PROPS.max_tranfers_cuentas);
                    
                    if (execl("./monitor", "monitor", PROPS.archivo_log, 
                        pid_str, ingreso_str, retiro_str, transferencia_str, 
                        max_ingreso_str, max_retiros_str, max_transf_str, max_tranfer_cuentas_str,(char *)NULL) == -1)
                    {
                        //Si monitor falla no tiene sentido que banco funcione asiq eu cerramos
                        kill(pidgeneral,SIGUSR2);
                    }
                    
                }
            break;
            

            case 4:
                //Por si el la defensa nos pide el arbol de procesos
                snprintf(comando,sizeof(comando),"pstree -p %d",pidgeneral);
                system(comando);
                printf("Pulsa una tecla para continuar...");
                while(getchar() != '\n');
                getchar();
            break;

            case 5:
               
                printf("Listando alertas:\n");
                listarPila();
                printf("Pulsa una tecla para continuar...");
                while(getchar() != '\n');
                getchar();
            break;

            case 6:
                listarUsuariosEnMemoria();
                printf("Pulsa una tecla para continuar...");
                while(getchar() != '\n');
                getchar();
            break;

            case 7:
               
               
                //Vemos si todavia hay procesos activos para notificar al usuario y que sea consciente
                if ((PROPS.n_procesos_activos ) == 0)  printf("Saliendo...\n");
                 
                //Si hay sesiones activas avisamos para que el usuario decida
                else
                {
                    
                    printf ("Todavia quedan %d sesiones abiertas \nSi sale ahora se cerraran todas ¿Desea salir? (s/n):",(PROPS.n_procesos_activos ));
                     
                    //Evitamos errores del getchar como espacion almacenados en el buffer
                    do {
                       salir = getchar(); // lee un carácter
                    } while (salir == '\n' || salir== ' ');
                    //Si desea salir de todas maneras, nos aseguramos de cerrar todos los procesos
                    if ( salir == 's') sleep(1);
                    //Si se arrepiente, cambiamos la variable opcion para que no salga del bucle
                    else opcion = 0;
                }
               
            break;    
            
            default:
                //Cualquier otra opcion no es valida , lo que seria un errror, esto se notifica por seguridad
                printf("Ingrese una opcion valida\n");
                escribirLog("Se ha ingresado una opcion no valida");
                
            break;
        }
           
    }
    
    return ;
}

void iniciarSemaforos()
{
    char mensaje[255];

    //Primero borramos los semaforos por si quedara alguno activo que diera problemas
    sem_unlink(SEMAFORO_CONTROL_HILOS);
    sem_unlink(SEMAFORO_MUTEX_CONFIGURACION);
    sem_unlink(SEMAFORO_MUTEX_CUENTAS);
    sem_unlink(SEMAFORO_MUTEX_LOG);
    sem_unlink(SEMAFORO_MUTEX_TRANSACCIONES);
    sem_unlink(SEMAFORO_MUTEX_FIFO);
    sem_unlink(SEMAFORO_MUTEX_MEMORIA);

    semaforo_log = sem_open(SEMAFORO_MUTEX_LOG,O_CREAT | O_EXCL ,0666,1);
    if (semaforo_log == SEM_FAILED)
    {
        snprintf("Error %d al abrir el semaforo %s",errno,SEMAFORO_MUTEX_LOG);
        fprintf(stderr,"%s\n",mensaje);
        //Este no se puede automatar ya que requiere acceso al log cosa que no podraimos porque
        //ha fallado su semaforo 
        exit(0);
    }
    //Al ser semaforos mutex, seran binarios iniciados a 1
    semaforo_config = sem_open(SEMAFORO_MUTEX_CONFIGURACION,O_CREAT | O_EXCL,0666,1);
    if (semaforo_config == SEM_FAILED)
    {
        snprintf("Error %d al abrir el semaforo %s",errno,SEMAFORO_MUTEX_CONFIGURACION);
        fprintf(stderr,"%s\n",mensaje);
        escribirLog(mensaje);
        //Nos automatamos
        kill(pidgeneral,SIGTERM);
    }

    
    semaforo_memoria= sem_open(SEMAFORO_MUTEX_MEMORIA,O_CREAT | O_EXCL,0666,1);
    if (semaforo_memoria == SEM_FAILED)
    {
        snprintf("Error %d al abrir el semaforo %s",errno,SEMAFORO_MUTEX_MEMORIA);
        fprintf(stderr,"%s\n",mensaje);
        escribirLog(mensaje);
        //Nos automatamos
        kill(pidgeneral,SIGTERM);
    }

    semaforo_fifo = sem_open(SEMAFORO_MUTEX_FIFO,O_CREAT | O_EXCL,0666,1);
    if (semaforo_fifo == SEM_FAILED)
    {
        snprintf("Error %d al abrir el semaforo %s",errno,SEMAFORO_MUTEX_FIFO);
        fprintf(stderr,"%s\n",mensaje);
        escribirLog(mensaje);
        //Nos automatamos
        kill(pidgeneral,SIGTERM);
    }


    semaforo_cuentas = sem_open(SEMAFORO_MUTEX_CUENTAS,O_CREAT | O_EXCL ,0666,1);
    if (semaforo_cuentas == SEM_FAILED)
    {
        snprintf("Error %d al abrir el semaforo %s",errno,SEMAFORO_MUTEX_CUENTAS);
        fprintf(stderr,"%s\n",mensaje);
      escribirLog(mensaje);
        //Nos automatamos
        kill(pidgeneral,SIGTERM);
    }

  

    semaforo_trans = sem_open(SEMAFORO_MUTEX_TRANSACCIONES,O_CREAT | O_EXCL,0666,1);
    if (semaforo_trans == SEM_FAILED)
    {
        snprintf("Error %d al abrir el semaforo %s",errno,SEMAFORO_MUTEX_TRANSACCIONES);
        fprintf(stderr,"%s\n",mensaje);
       escribirLog(mensaje);
        //Nos automatamos
        kill(pidgeneral,SIGTERM);
    }

    sem_init(&semaforo_control,1,1);
    //Si un semaforo fallara el programa no se podria controlar, por lo que no tiene sentido que se pueda 
    //ejecutar asi que se cierra
    return;
}

//Nombre: terminarSemaforos
//Retorno: void
//Parametros: ninguno
//Uso: Esta funcion se encarga de cerrar y destruir todos los semaforos creados por el programa
void terminarSemaforos()
{
    //Primero cerramos los semaforos
    sem_close(semaforo_hilos);
    sem_close(semaforo_config);
    sem_close(semaforo_cuentas);
    sem_close(semaforo_log);
    sem_close(semaforo_trans);
    sem_close(semaforo_fifo);
    sem_close(semaforo_memoria);
    sem_destroy(&semaforo_control);

    //Luego borramos los fichero asociados para dejar el sistema limpio
    sem_unlink(SEMAFORO_CONTROL_HILOS);
    sem_unlink(SEMAFORO_MUTEX_CONFIGURACION);
    sem_unlink(SEMAFORO_MUTEX_CUENTAS);
    sem_unlink(SEMAFORO_MUTEX_LOG);
    sem_unlink(SEMAFORO_MUTEX_TRANSACCIONES);
    sem_unlink(SEMAFORO_MUTEX_FIFO);
    sem_unlink(SEMAFORO_MUTEX_MEMORIA);

    return;
}

//Nombre: generarMemoriaCompartida
//Retorno: void
//Parametros: ninguno
//Uso: Esta funcion se encarga de crear la memoria compartida y cargar los datos de los usuarios
//      en ella, para que los procesos hijos puedan acceder a la misma sin problemas
//      Ademas de esto se encarga de asegurarse de que no haya problemas de carrera mediante semáforos
//      y de que la memoria compartida se haya creado correctamente
//      En caso de error se cierra el programa
void generarMemoriaCompartida()
{
    FILE * fichero;
    int i;
    bool saltar;
    char linea[255],mensaje[255];
    char *id,*nombre,*saldo,*trasn;

    escribirLog("Iniciando memoria compartida...");

    //Abrimos el semaforo para que nadie acceda a la memoria compartida
    sem_wait(semaforo_cuentas);
    //Abrimos el fichero de cuentas para leer los datos de los usuarios
    fichero = fopen(PROPS.archivo_cuentas,"r");
    if (fichero == NULL)
    {
        snprintf(mensaje,sizeof(mensaje),"Error al abrir el fichero %s",PROPS.archivo_cuentas);
        escribirLog(mensaje);
        fprintf(stderr,"%s\n",mensaje);
        sem_post(semaforo_cuentas);
        sem_post(semaforo_memoria);
        kill(SIGKILL,pidgeneral);
    }
    //Creamos la memoria compartida
    sem_wait(semaforo_memoria);
    fd_memoria = shm_open(MEMORIA_COMPARTIDA,O_CREAT | O_RDWR,0666);
    i = 0;
    if (fd_memoria == -1 )
    {
        escribirLog("Fallo al crear la memoria compartida");
        fprintf(stderr,"Error al crear la memoria compartida\n");
        sem_post(semaforo_cuentas);
        sem_post(semaforo_memoria);
        kill(SIGKILL,pidgeneral);
    } 
    //Le asignamos el tamaño a la memoria compartida en este caso solo podra almacenar el maximo de usuarios 10 por defecto
    if (ftruncate(fd_memoria,sizeof(MEMORIA)) == -1)
    {
        escribirLog("Fallo al asignar memoria compartida");
        fprintf(stderr,"Error al asignar memoria compartida\n");
        sem_post(semaforo_cuentas);
        sem_post(semaforo_memoria);
        kill(SIGKILL,pidgeneral);
    }

    //Mapeamos la memoria compartida para poder acceder a ella
    listaUsers = mmap(0,(sizeof(MEMORIA) * MAX_USUARIO),PROT_WRITE | PROT_READ,MAP_SHARED,fd_memoria,0);
    if ( listaUsers == MAP_FAILED)
    {
        escribirLog("Fallo al mapear memoria compartida");
        fprintf(stderr,"Error al mapear memoria compartida\n");
        sem_post(semaforo_cuentas);
        sem_post(semaforo_memoria);
        kill(SIGKILL,pidgeneral);
    } 

    saltar = true;
    while (fgets(linea,255,fichero) != NULL)
    {
        //saltamos la cabecera
        if (saltar)
        {
            saltar = false;
            continue;
        }
        //Si hemos llegado al maximo de usuarios, salimos
        if (i >= MAX_USUARIO)
        {
            escribirLog("Se ha superado el maximo de usuarios en memoria");
            break;
        }
        //Quitamos el \n 
        linea [strcspn(linea,"\n")] = '\0';

        //Sacamos los datos de cada linea
        nombre = strtok(linea,";");
        id = strtok(NULL,";");
        saldo = strtok (NULL,";");
        trasn = strtok(NULL,";");
    
        //Guardamos los datos en la memoria compartida
        listaUsers->lista[i].id = atoi(id);
        strncpy(listaUsers->lista[i].nombre, nombre, sizeof(listaUsers->lista[i].nombre) - 1);
        listaUsers->lista[i].nombre[sizeof(listaUsers->lista[i].nombre) - 1] = '\0'; // Seguridad
        listaUsers->lista[i].saldo = atof(saldo);
        listaUsers->lista[i].operaciones = atoi(trasn);
        listaUsers->lista[i].activo = 0;
   
        i++;
    }

    //Actualizamos el indice de ususarios totales
    listaUsers->n_users = i;

    //Liberamos los recursos
    fclose(fichero);
    sem_post(semaforo_cuentas);
    sem_post(semaforo_memoria);
    escribirLog("Se ha creado la memoria compartida");
    return;
}


//Nombre: crearArchivoUsuario
//Retorno: char *
//Parametros: int
//Uso: Esta funcion se encarga de crear el archivo de transacciones de cada usuario en funcion a su ID
//      Este archivo se crea en un directorio propio con mismo ID del usuario
//      Ademas de esto se encarga de asegurarse de que no hayan problemas de carrera con semáforos
//      Devuelve el nombre del archivo creado
//      En caso de error devuelve NULL
char * crearArchivoUsuario(int id_usuario)
{
    char directorio[256];
    char archivo[800];
    if (archivo == NULL) 
    {
        escribirLog("Error al asignar memoria para el nombre del archivo");
        return NULL;
    }
    struct stat st;

    // 1) Directorio: "trans_<id>"
    snprintf(directorio, sizeof(directorio), "./ficheros/usuario_%d", id_usuario);
    if (stat(directorio, &st) != 0) 
    {
        if (mkdir(directorio, 0755) != 0) 
        {
            escribirLog("Error creando directorio de transacciones");
            fprintf(stderr, "mkdir('%s') failed: %s\n", directorio, strerror(errno));
            return NULL;
        }
    } 
    else if (!S_ISDIR(st.st_mode)) 
    {

        escribirLog("Ruta de transacciones existe pero no es directorio");
        fprintf(stderr, "'%s' existe y no es un directorio\n", directorio);
        return NULL;
    }

    // 2) Fichero: "trans_<id>/transacciones<id>.dat"
    snprintf(archivo, sizeof(archivo), "%s/transacciones_%d.log", directorio, id_usuario);
    if (stat(archivo, &st) != 0) 
    {
        FILE *fp = fopen(archivo, "a");
        if (!fp) 
        {
            escribirLog("Error creando fichero de transacciones");
            fprintf(stderr, "fopen('%s') failed: %s\n", archivo, strerror(errno));
            return NULL;
        }
        fclose(fp);
    }

    return strdup(archivo);
}
//Nombre: inicializarArchivosUsuarios
//Retorno: void
//Parametros: void
//Uso: Esta funcion se encarga de inicializar los archivos de transacciones de cada usuario

void inicializarArchivosUsuarios(void)
{
    int i;
    char * fichero;
    for ( i = 0; i < listaUsers->n_users; i++){
       fichero = crearArchivoUsuario(listaUsers->lista[i].id);
        if (fichero == NULL)
        {
            escribirLog("Error al crear el archivo de transacciones");
            fprintf(stderr, "Error al crear el archivo de transacciones\n");
            return;
        }
        strncpy(listaUsers->lista[i].fichero, fichero, sizeof(listaUsers->lista[i].fichero) - 1);
        listaUsers->lista[i].fichero[sizeof(listaUsers->lista[i].fichero) - 1] = '\0';
        free(fichero);
    }
    

    
}

//Nombre: Buffer
//Retorno: void *
//Parametros: void *
//Uso: Esta funcion se encarga de guardar los datos en el fichero de transacciones cada cierto tiempo
//      Este tiempo se define en el fichero de configuracion
//      Ademasel buffer se encarga de asegurarse de que no hayan problemas de carrera con semáforos
void *Buffer(void * arg)
{
    int t_espera = PROPS.tiempo_buffer;
    buff_start = 0;
    while (1)
    {
        
        if (buff_start == 2) // Si se recibe la señal de detener el buffer, salir
        {
            break;
        }
        escribirLog("Se ha iniciado el buffer");

        // Guardar los datos en el archivo
        guardarFichero();
        
        escribirLog("El buffer ha acabado de guardar los datos en el archivo");
        // Reiniciar el temporizador
        t_espera = PROPS.tiempo_buffer;
        while (t_espera > 0)
        {
            sleep(1);
            t_espera--;

            // Verificar si se ha recibido una nueva señal para activar el buffer
            if (buff_start == 1)
            {
                buff_start = 0; // Reiniciar la señal
                break;
            }
            else if (buff_start == 2)
            {
                break;
            }
        }
    }

    return NULL;
}




//Nombre: listarUsuariosEnMemoria
//Retorno: void
//Parametros: Ninguno
//Uso: Esta funcion se encarga listar los usuarios en memoria compartida
//      mostrando ademas su estado 
void listarUsuariosEnMemoria()
{
    int i;
    char mensaje[255];
    sem_wait(semaforo_memoria);
    if (listaUsers->n_users == 0)
    {
        snprintf(mensaje, sizeof(mensaje), "No hay usuarios en memoria compartida");
        escribirLog(mensaje);
        fprintf(stderr, "%s\n", mensaje);
        sem_post(semaforo_memoria);
        return;
    }
    for (i = 0; i < listaUsers->n_users; i++)
    {
        printf("Usuario %d: %s - Activo: %d\n", listaUsers->lista[i].id, listaUsers->lista[i].nombre,listaUsers->lista[i].activo);
    }
    sem_post(semaforo_memoria);
}




//Nombre: buscarUserEnFichero
//Retorno: char *
//Parametros: int
//Uso: Esta funcion se encarga de buscar un usuario en el fichero de cuentas
//     Se usa en caso de que el usuario no este en memoria compartida
//     Devolvemos la cadena con los datos del usuario
//     En caso de error devolvemos NULL
//     Ademas de esto se encarga de asegurarse de que no hayan problemas de carrera con semáforos
char * buscarUserEnFichero(int id_buscar)
{
    FILE * fichero;
    bool saltar;
    char mensaje[255],linea[255],copio[255];
    char *nombre, *id, *saldo, *transacciones,*aux;

    //Abrimos el fichero de cuentas
    sem_wait(semaforo_cuentas);
  
    if ((fichero = fopen(PROPS.archivo_cuentas, "r")) == NULL)
    {
        snprintf(mensaje, sizeof(mensaje), "Error al abrir el fichero %s", PROPS.archivo_cuentas);
        escribirLog(mensaje);
        fprintf(stderr, "%s\n", mensaje);
        sem_post(semaforo_cuentas);
        return NULL;
    }

    saltar = true;
    while (fgets(linea, 255, fichero) != NULL)
    {
        if (saltar)
        {
            saltar = false;
            continue;
        }
        //Quitamos el \n 
        linea[strcspn(linea, "\n")] = '\0';

        strcpy(copio,linea);
     
        //Sacamos los datos de cada linea
        nombre = strtok(linea, ";");
        id = strtok(NULL, ";");
        saldo = strtok(NULL, ";");
        transacciones = strtok(NULL, ";");

        if (atoi(id) == id_buscar)
        {
            sem_post(semaforo_cuentas);
            return strdup(copio);
        }
    }
    
    fclose(fichero);
    sem_post(semaforo_cuentas);
    return NULL;
}

//Nombre: remplazarUsuario
//Retorno: int
//Parametros: char *.
//Uso: Esta funcion se encarga de remplazar un usuario en memoria compartida por otro
//      Este usuario es el que se ha leido del fichero de cuentas
//      Este remplazo se hace en caso de que el usuario no este activo
//      Este tambien se encarga de asegurarse de que no hayan problemas de carrera con semáforos
//      Devuelve el id del usuario que se ha remplazado
//      En caso de error devuelve -1
int  remplazarUsuario(char * datos)
{
    int i;
    char *nombre, *id, *saldo, *transacciones,*fichero;
    
  

   
    //Bloqueamos el acceso a la memoria compartida para evitar problemas de carrera
    sem_wait(semaforo_memoria);
    //Buscamos al primer usuario que no este activo
    for ( i = 0; i< listaUsers->n_users; i++)
    {
       
        //Si el usuario no esta activo, lo podemos remplazar
        if (listaUsers->lista[i].activo == 0)
        {
            //Saco los datos de la cadena
            nombre = strtok(datos, ";");
            id = strtok(NULL, ";");
            saldo = strtok(NULL, ";");
            transacciones = strtok(NULL, ";");


          
            strncpy(listaUsers->lista[i].nombre, nombre, sizeof(listaUsers->lista[i].nombre) - 1);
            listaUsers->lista[i].nombre[sizeof(listaUsers->lista[i].nombre) - 1] = '\0'; // Seguridad
            listaUsers->lista[i].id = atoi(id);
            listaUsers->lista[i].saldo = atof(saldo);
            listaUsers->lista[i].operaciones = atoi(transacciones);
            listaUsers->lista[i].activo = 0;
           

            fichero = crearArchivoUsuario(listaUsers->lista[i].id);
           
            strncpy(listaUsers->lista[i].fichero, fichero, sizeof(listaUsers->lista[i].fichero) - 1);
            listaUsers->lista[i].fichero[sizeof(listaUsers->lista[i].fichero) - 1] = '\0';
            free(fichero);
            sem_post(semaforo_memoria);
            //Guardamos el id del usuario para que el padre pueda abrir la sesion
            //y no se confunda con el id de la memoria compartida
            return atoi(id);
        }
    }
    sem_post(semaforo_memoria);
    //Si no hemos encontrado un hueco para el nuevo usuario, devolvemos -1
    return -1;
}

//Nombre: guardarFichero
//Retorno: void
//Parametros: Ninguno
//Uso: Esta funcion se encarga de guardar los datos de los usuarios en el fichero de cuentas
//      Este fichero se guarda en el formato nombre;id;saldo;transacciones
//      Ademas de esto se encarga de asegurarse de que no hayan problemas de carrera con semáforos

void guardarFichero()
{
    //Creamos las variables
    FILE *fichero,*tmp;
    int i,j;
    char mensaje[255],linea[255];
    char * nombre,*id_fichero,*saldo,*trans;
    bool saltar,encontrado;

    //Esperamos a que el semaforo de cuentas se libere para poder acceder al fichero
    sem_wait(semaforo_cuentas);
    fichero = fopen(PROPS.archivo_cuentas,"r");
    if (fichero == NULL)
    {
        snprintf(mensaje,sizeof(mensaje),"Error al abrir el fichero %s",PROPS.archivo_cuentas);
        escribirLog(mensaje);
        fprintf(stderr,"%s\n",mensaje);
        sem_post(semaforo_cuentas);
        return;
    }
    tmp = fopen("tmp.txt","w");
    if (tmp == NULL)
    {
        snprintf(mensaje,sizeof(mensaje),"Error al abrir el fichero tmp.txt");
        escribirLog(mensaje);
        fprintf(stderr,"%s\n",mensaje);
        fclose(fichero);
        sem_post(semaforo_cuentas);
    
        return;
    }
    saltar = true;  

    fprintf(tmp,"nombre;numerocuenta;saldo;transacciones\n");
    sem_wait(semaforo_memoria);

    //Primero escribimos los usuarios que ya tenemos en memoria
    for (j = 0; j < listaUsers->n_users; j++)
    {
        
            fprintf(tmp, "%s;%d;%.2f;%d\n",
                listaUsers->lista[j].nombre,
                listaUsers->lista[j].id,
                listaUsers->lista[j].saldo,
                listaUsers->lista[j].operaciones
            );
        
    }
    while (fgets(linea,255,fichero) != NULL)
    {
        encontrado = false;
        //saltamos la cabecera
        if (saltar)
        {
            saltar = false;
            continue;
        }

        //Quitamos el \n 
        linea[strcspn(linea, "\n")] = '\0';
        //Sacamos los datos de cada linea
        nombre = strtok(linea, ";");
        id_fichero = strtok(NULL, ";");
        saldo = strtok(NULL, ";");
        trans = strtok(NULL, ";");

        //Para los usuarios del fichero, vemos si ya existen en memoria
        for (i = 0; i < listaUsers->n_users; i++)
        {
            //Si el id coincide lo actualizamos
            if (atoi(id_fichero) == listaUsers->lista[i].id)
            {
                encontrado = true;
                break;
                
            }
        }
      
        if (!encontrado)
        {
            //Si no lo hemos encontrado lo escribimos tal cual
            fprintf(tmp, "%s;%s;%s;%s\n",
                nombre,
                id_fichero,
                saldo,trans
            );
        }
    }

    fclose(tmp);
    fclose(fichero);

    remove(PROPS.archivo_cuentas);
    rename("tmp.txt", PROPS.archivo_cuentas);

    sem_post(semaforo_memoria);
    sem_post(semaforo_cuentas);
   

    return;


    
}

