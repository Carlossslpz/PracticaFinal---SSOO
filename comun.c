#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <semaphore.h>
#include <signal.h>
#include <stdatomic.h>

#define FIFO_PROGRAMAS_BANCO "/tmp/fifo_user_banco"


#define SEMAFORO_CONTROL_HILOS "/semaforo_control_hilos"
#define SEMAFORO_MUTEX_LOG "/semaforo_mutex_log"
#define SEMAFORO_MUTEX_TRANSACCIONES "/semaforo_mutex_trans"
#define SEMAFORO_MUTEX_CUENTAS "/semaforo_mutex_cuentas"
#define SEMAFORO_MUTEX_CONFIGURACION "/semaforo_mutex_config"
#define SEMAFORO_MUTEX_FIFO "/semaforo_mutex_fifo"
