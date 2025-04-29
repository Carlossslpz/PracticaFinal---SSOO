#!/bin/bash
#Primero compilamos todos los ficheros
gcc banco.c -o banco -lpthread
gcc init_cuenta.c -o init_cuenta -lpthread
gcc monitor.c -o monitor -lpthread
gcc usuario.c -o usuario -lpthread

#Este usuario es el que va a ejecutar el programa
# y por lo tanto necesita permisos para acceder a los ficheros
usuario="BANCO"
# Comprobamos si el usuario existe
if id "$usuario" &>/dev/null; then
    echo "El usuario $usuario ya existe."
else
    # Si no existe, lo creamos
    sudo useradd -m -s /bin/bash -d /home/banco banco
fi


#Luego vemos si estan los ficheros que necesitamos sino los creamos 

directorio="ficheros"
ficheros=("cuentas.dat" "control.log" "transacciones.log")

# Creamos el directorio si no existe
if [ ! -d "$directorio" ]; then 
    mkdir "$directorio"
    # Cambiamos el propietario del directorio al usuario banco
    sudo chown -R "$usuario":"$usuario" "$directorio"
    # Cambiamos los permisos del directorio para que el usuario banco pueda acceder
    sudo chmod 700 "$directorio"
fi

# Creamos los archivos dentro del directorio si no existen
for x in "${ficheros[@]}"; do 
    archivo="$directorio/$x"
    if [ ! -f "$archivo" ]; then
        touch "$archivo"
        # Cambiamos el propietario del archivo al usuario banco
        sudo chown "$usuario":"$usuario" "$archivo"
        # Cambiamos los permisos del archivo para que el usuario banco pueda acceder
        sudo chmod 600 "$archivo"
        
    fi
done


echo "Se han compilado los ficheros y creado los ficheros necesarios"
echo "Para inciar el sistema ejecute ./banco <ruta_fichero_configuracion.conf>"
