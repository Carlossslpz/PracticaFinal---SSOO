#!/bin/bash
#Primero compilamos todos los ficheros
gcc banco.c -o banco -lpthread
gcc init_cuenta.c -o init_cuenta -lpthread
gcc monitor.c -o monitor -lpthread
gcc usuario.c -o usuario -lpthread

#Luego vemos si estan los ficheros que necesitamos sino los creamos 

directorio="ficheros"
ficheros=("cuentas.dat" "control.log" "transacciones.log")

# Creamos el directorio si no existe
if [ ! -d "$directorio" ]; then 
    mkdir "$directorio"
fi

# Creamos los archivos dentro del directorio si no existen
for x in "${ficheros[@]}"; do 
    archivo="$directorio/$x"
    if [ ! -f "$archivo" ]; then
        touch "$archivo"
    fi
done

echo "Se han compilado los ficheros y creado los ficheros necesarios"
echo "Para inciar el sistema ejecute ./banco <ruta_fichero_configuracion.conf>"
