#!/bin/bash

#Como vamos a crear usuaruios y grupos necesitamos permisos de superusuario

if [ "$EUID" -ne 0 ]; then
    echo "Por favor, ejecute este script como superusuario (root)."
    exit 1
fi

grupo2="$(ls -ld . | awk '{print $4}')"
#Definimos las variables necesarias
password="banco"
usuario="banco"
grupo="banco"
directorio="ficheros"
ficheros=("cuentas.dat" "control.log")


#Primero vemos si el grupo existe
#Quiero que si esite no haga nada y sino lo cree
if !  getent group "$grupo" > /dev/null; then

    groupadd "$grupo"
fi

#Luego vemos si el usuario existe
if ! id "$usuario" &>/dev/null; then

    # Si no existe, lo creamos con el grupo y la contraseña
    # -m: Crea el directorio home del usuario
    # -s: Especifica el shell del usuario
    # -d: Especifica el directorio home del usuario
    # -g: Especifica el grupo del usuario
    # -p: Especifica la contraseña del usuario
    useradd -m -s /bin/bash -d /home/banco "$usuario" -g "$grupo" -p "$(openssl passwd -1 "$password")" -G "$grupo2","sudo"

fi

#Ahora creamos una mascara de permiso para el usuario para que todo lo que cree 
#solo tenga permisos de lectura y escritura para el grupo y el propietario y nadie mas
#Grupo esta para poder hacer modificaciones de debug con ootros usuarios
echo "umask 007" >> /home/banco/.bashrc
# Primero vemos si el directorio existe
if [ ! -d "$directorio" ]; then 
    mkdir "$directorio"
    #Luego le cambiamos el propietario y el grupo al directorio
    chown "$usuario:$grupo" "$directorio"
    #Y le cambiamos los permisos para que el usuario y el grupo puedan leer y escribir
    chmod 770 "$directorio"
    
  
fi


# Creamos los archivos dentro del directorio si no existen
for x in "${ficheros[@]}"; do 
    archivo="$directorio/$x"
    if [ ! -f "$archivo" ]; then
        touch "$archivo"
        # Cambiamos el propietario y el grupo del archivo
        chown "$usuario:$grupo" "$archivo"
        # Cambiamos los permisos del archivo para que el usuario y el grupo puedan leer y escribir
        chmod 660 "$archivo"   
    fi
done

#Compilamos el programa
#Iteramos sobre todos losa archivos y les cambiamos los permisos
for x in ./*; do

    #Primero obtenemos el nombre del fichero
    fichero=$(basename "$x")

    #Vemos si es un fichero o un directorio ya que los permisos varian 
    if [ -d "$fichero" ]; then
        #El directorio necesita permisos de lectura escritura y ejecucion 
        chmod 770 "$x"
        chown "$usuario:$grupo" "$fichero"
    else
       
       #Para los ficheros saco la extension y el nombre, ya que en funcion de la 
        #extension le doy unos permisos u otros
        extension="${fichero##*.}"
        nombre="${fichero%.*}"

        #Si el fichero es un .c , hacemos dos cosas
        #1.Compilamos el fichero y le cambiamos los permisos y el propietario, dandole permisos de ejecucion para que 
        #pueda funcionar
        #2.Dejamos el fichero .c solo con permisos de lectura y escritura, ya que no necesita mas
        if [ "$extension" == "c" ]; then
            if [ "$nombre" == "comun" ]; then
                continue
            fi
            # Compilamos el fichero
            gcc "$fichero" -o "$nombre" -lpthread
            chmod 660 "$x"
            chown "$usuario:$grupo" "$fichero"

            chmod 770 "$nombre"
            chown "$usuario:$grupo" "$nombre"

        #Si el fichero tiene el mismo nombre que la extension es que es un archivo compilado
        #Por lo que le cambiamos los permisos y el propietario y le dejamos los permisos de ejecucion
        elif [ "$extension" == "$fichero" ]; then
            chmod 770 "$x"
            chown "$usuario:$grupo" "$fichero"
        #Este .sh es este script por lo que lo dejmos como esta, y si por si acaso se han cambiado los permisos
        #le cambiamos solo los permisos ya que el propietario no tiene porque ser banco
        elif [ "$extension" == "sh" ]; then
            chmod 770 "$x"
        #El resto son ficheros de datos y demas asi que se quedan como estan con permisos de lectura y escritura
        else 
            chmod 660 "$x"
            chown "$usuario:$grupo" "$fichero"
        fi
    fi
done

echo "Se han compilado los ficheros y creado los ficheros necesarios"
echo "Para inciar el sistema ejecute cambie el ususario a $usuario" 
echo "1º su $usuario"
echo "2º contraseña: $password"
echo "Y ejecute ./banco <ruta_fichero_configuracion.conf>"


