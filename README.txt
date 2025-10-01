README
======

Instrucciones para la compilación y despliegue de la aplicación
---------------------------------------------------------------

1. COMPILACIÓN
--------------

Para compilar la aplicación hemos desarrollado un Makefile que se encarga de generar los ejecutables necesarios.

Para compilar simplemente ejecutamos:

    make

NOTA IMPORTANTE: Solo se compilan los archivos escritos en C. Los scripts en Python no requieren compilación.

Durante la compilación se generan dos ejecutables principales:

- server
- server_rpc

LIMPIEZA DE ARCHIVOS GENERADOS

Para eliminar los archivos generados en la compilación, se define una regla clean:

    make clean

2. DESPLIEGUE Y EJECUCIÓN
--------------------------

Para ejecutar correctamente la aplicación, se recomienda abrir 4 terminales diferentes (una adicional por cada cliente extra).

PASO 1: Ejecutar el servidor principal

En la primera terminal:

1. Definimos la variable de entorno LOG_RPC_IP:

       export LOG_RPC_IP=localhost

2. Ejecutamos el servidor principal:

       ./server -p <puerto>

   Ejemplo:

       ./server -p 4500

   IMPORTANTE: El valor de <puerto> debe coincidir con el puerto indicado al ejecutar el cliente.

PASO 2: Ejecutar el servidor RPC

En la segunda terminal, ejecutamos:

    ./server_rpc

PASO 3: Ejecutar el web service

En la tercera terminal, ejecutamos:

    python3 ./ws_server.py

PASO 4: Ejecutar el cliente

En la cuarta terminal (y siguientes si hay más de un cliente), ejecutamos:

    python3 ./client.py -s localhost -p <puerto>

Ejemplo:

    python3 ./client.py -s localhost -p 4500

El valor de <puerto> debe ser el mismo que el usado al ejecutar ./server.

3. VERIFICACIÓN
---------------

Si todo ha sido ejecutado correctamente, en la terminal del cliente deberías ver:

    c>

Esto indica que la aplicación está lista para ser utilizada.
