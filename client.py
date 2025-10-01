from enum import Enum
import argparse
import socket
import threading
import os
import zeep

class client :

    # ******************** TYPES *********************
    # *
    # * @brief Return codes for the protocol methods
    class RC(Enum) :
        OK = 0
        ERROR = 1
        USER_ERROR = 2

    # ****************** ATTRIBUTES ******************
    _server = None
    _port = -1
    _username = None
    _get_file_socket = None
    _get_file_port = None
    _stop_get_file_thread = False
    _get_file_thread = None
    _lista_respuesta = None

    # ******************** METHODS *******************
    @staticmethod
    def solicitar_datetime():
        wsdl_url = "http://localhost:8080/?wsdl"
        soap = zeep.Client(wsdl=wsdl_url) 
        return  soap.service.get_datetime()
    
    @staticmethod
    def esperar_getfile():

        while not client._stop_get_file_thread:
            try:
                client._get_file_socket.settimeout(0.1)
                sock, addr = client._get_file_socket.accept()
                try:
                    op = sock.recv(8).decode().strip().replace('\x00', '')
                    if op != "GET_FILE":
                        sock.sendall(b'2')  # Error de operación
                        sock.close()
                        return
                    sock.recv(1)  # Recibe la coma
                    ruta = client.recv_str(sock)
                    ruta_abs = os.path.abspath(ruta)
                    
                    
                    if not os.path.isfile(ruta_abs):
                        sock.sendall(b'1')  # Archivo no existe
                    else:
                        with open(ruta, 'rb') as f:
                            contenido = f.read()
                        header = f"0,{os.path.getsize(ruta)},".encode()  # convierte string a bytes
                        sock.sendall(header + contenido) # Archivo existe, enviar contenido con prefijo '0'
                except Exception as e:
                    print(f"Error handling file request: {e}")
                    sock.sendall(b'2')
                sock.close()
            except socket.timeout:
                continue
            except Exception as e:
                print(f"Error in getfile listener: {e}")
                break

    @staticmethod
    def recv_str(sock):
        """Devuelve una cadena de bytes leidos uno a uno"""
        data = b''
        while True:
            byte = sock.recv(1)
            if not byte or byte == b'\0':
                break
            data += byte
        return data.decode("utf-8")

    @staticmethod
    def _send_request(message):
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.connect((client._server, client._port))
                s.sendall(message.encode())  # envía la cadena como bytes

                # Recibe 4 bytes como respuesta (int)
                response = s.recv(4)
                if not response:
                    return -1
                int_response = int.from_bytes(response, byteorder='little')
                s.close()
                return int_response
        except Exception as e:
            print("Error de conexión:", e)
            return -1
            
    @staticmethod
    def _send_request_rcv_str(message):
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.connect((client._server, client._port))
                s.sendall(message.encode())  # envía la cadena como bytes

                # Recibe 4 bytes como respuesta (int)
                response = s.recv(4)
                if not response:
                    return -1
                int_response = int.from_bytes(response, byteorder='little')
                if int_response == 0:
                    client._lista_respuesta = client.recv_str(s)
                s.close()
                return int_response
        except Exception as e:
            print("Error de conexión:", e)
            return -1

    @staticmethod
    def register(user):
        timestamp = client.solicitar_datetime() # Obtener la fecha y hora actual
        mensaje = f"REGISTER,{timestamp},{user}\0" # Crear el mensaje
        rc = client._send_request(mensaje) # Enviar el mensaje al servidor
        if rc == 0:
            print("REGISTER OK")
            return client.RC.OK
        elif rc == 1:
            print("USERNAME IN USE")
            return client.RC.USER_ERROR
        
        print("REGISTER FAIL")
        return client.RC.ERROR

    @staticmethod
    def  unregister(user) :
        if client._username == user: # Desconectar el usuario antes de desregistrarlo
            client.disconnect(user)
        timestamp = client.solicitar_datetime() # Obtener la fecha y hora actual
        mensaje = f"UNREGISTER,{timestamp},{user}\0" # Crear el mensaje
        rc = client._send_request(mensaje) # Enviar el mensaje al servidor
        if rc == 0:
            print("UNREGISTER OK")
            return client.RC.OK
        elif rc == 1:
            print("USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        
        print("UNREGISTER FAIL")
        return client.RC.ERROR

    @staticmethod
    def connect(user):
        if client._username is not None: # Si ya hay un usuario conectado, se desconecta
            client.disconnect(client._username)

        # Crea el socket para recibir archivos
        try:
            client._get_file_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            client._get_file_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            client._get_file_socket.bind(('', 0))
            client._get_file_port = client._get_file_socket.getsockname()[1]
            client._get_file_socket.listen()
        except Exception as e:
            print(f"Error creating listener socket: {e}")
            return client.RC.ERROR

        # Obtener la IP local
        hostname = socket.gethostname()
        ip = socket.gethostbyname(hostname)
        
        # Enviar el mensaje de conexión al servidor
        timestamp = client.solicitar_datetime()
        mensaje = f"CONNECT,{timestamp},{user},{ip},{client._get_file_port}\0"
        rc = client._send_request(mensaje)

        if rc == 0:
            print("CONNECT OK")
            client._username = user
            client._stop_get_file_thread = False
            client._get_file_thread = threading.Thread(target=client.esperar_getfile, daemon=True)
            client._get_file_thread.start()
            return client.RC.OK
        # Ha habido un error, se cierra el socket
        client._get_file_socket.close()
        client._get_file_socket = None
        client._get_file_port = None
        if rc == 1:
            print("CONNECT FAIL , USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        elif rc == 2:
            print("USER ALREADY CONNECTED")
            return client.RC.USER_ERROR
        
        print("CONNECT FAIL")
        return client.RC.ERROR




    @staticmethod
    def  disconnect(user):
        timestamp = client.solicitar_datetime() # Obtener la fecha y hora actual
        mensaje = f"DISCONNECT,{timestamp},{user}\0" # Crear el mensaje
        rc = client._send_request(mensaje) # Enviar el mensaje al servidor
        if rc == 0:
            print("DISCONNECT OK")
            client._username = None
            if client._get_file_socket: # Si el socket de recepción de archivos está activo
                # Cerrar el socket y detener el hilo
                client._stop_get_file_thread = True
                client._get_file_thread.join()
                client._get_file_socket.close()
                client._get_file_socket = None
            return client.RC.OK
        elif rc == 1:
            print("DISCONNECT FAIL , USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        elif rc == 2:
            print("DISCONNECT FAIL , USER NOT CONNECTED")
            return client.RC.USER_ERROR
        
        print("DISCONNECT FAIL")
        return client.RC.ERROR

    @staticmethod
    def  publish(fileName,  description) :
        if len(fileName) > 256: # Limitar el tamaño del nombre del archivo
            print("File name too long")
            return client.RC.USER_ERROR
        timestamp = client.solicitar_datetime() # Obtener la fecha y hora actual
        mensaje = f"PUBLISH,{timestamp},{client._username},{fileName},{description}\0" # Crear el mensaje
        rc = client._send_request(mensaje) # Enviar el mensaje al servidor
        if rc == 0:
            print("PUBLISH OK")
            return client.RC.OK
        elif rc == 1:
            print("PUBLISH FAIL , USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        elif rc == 2:
            print("PUBLISH FAIL , USER NOT CONNECTED")
            return client.RC.USER_ERROR
        elif rc == 3:
            print("PUBLISH FAIL , CONTENT ALREADY PUBLISHED")
            return client.RC.USER_ERROR
        
        print("PUBLISH FAIL")
        return client.RC.ERROR

    @staticmethod
    def  delete(fileName) :
        timestamp = client.solicitar_datetime() # Obtener la fecha y hora actual
        mensaje = f"DELETE,{timestamp},{client._username},{fileName}\0" # Crear el mensaje
        rc = client._send_request(mensaje) # Enviar el mensaje al servidor
        if rc == 0:
            print("DELETE OK")
            return client.RC.OK
        elif rc == 1:
            print("DELETE FAIL , USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        elif rc == 2:
            print("DELETE FAIL , USER NOT CONNECTED")
            return client.RC.USER_ERROR
        elif rc == 3:
            print("DELETE FAIL , CONTENT NOT PUBLISHED")
            return client.RC.USER_ERROR
        
        print("DELETE FAIL")
        return client.RC.ERROR

    @staticmethod
    def  listusers() :
        timestamp = client.solicitar_datetime() # Obtener la fecha y hora actual
        mensaje = f"LIST_USERS,{timestamp},{client._username}\0" # Crear el mensaje
        rc = client._send_request_rcv_str(mensaje) # Enviar el mensaje al servidor
        if rc == 0:
            print("LIST_USERS OK")
            parts = client._lista_respuesta.split(',')

            if parts:
                try:
                    num_users = int(parts[0])
                except ValueError:
                    print("Error: respuesta inválida")
                    return client.RC.ERROR
                # Separando los datos de los usuarios
                for i in range(num_users):
                    offset = 1 + i * 3
                    try:
                        user = parts[offset]
                        ip = parts[offset + 1]
                        port = parts[offset + 2]
                        print(f"{user} {ip} {port}")
                    except IndexError:
                        print("Error: datos incompletos para un usuario")
            return client.RC.OK
        elif rc == 1:
            print("LIST_USERS FAIL , USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        elif rc == 2:
            print("LIST_USERS FAIL , USER NOT CONNECTED")
            return client.RC.USER_ERROR
        
        print("LIST_USERS FAIL")
        return client.RC.ERROR

    @staticmethod
    def  listcontent(user) :
        timestamp = client.solicitar_datetime() # Obtener la fecha y hora actual
        mensaje = f"LIST_CONTENT,{timestamp},{client._username},{user}\0" # Crear el mensaje
        rc = client._send_request_rcv_str(mensaje) # Enviar el mensaje al servidor
        if rc == 0:
            print("LIST_CONTENT OK")
            parts = client._lista_respuesta.split(',')
            if parts:
                try:
                    num_files = int(parts[0])
                except ValueError:
                    print("Error: respuesta inválida")
                    return client.RC.ERROR
                offset = 0
                # Separando los datos de los archivos
                for i in range(num_files):
                    offset += 1
                    try:
                        file_name = parts[offset]
                        
                        print(f"{file_name}")
                    except IndexError:
                        print("Error: datos incompletos para un archivo")
            return client.RC.OK
        elif rc == 1:
            print("LIST_CONTENT FAIL , USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        elif rc == 2:
            print("LIST_CONTENT FAIL , USER NOT CONNECTED")
            return client.RC.USER_ERROR
        elif rc == 3:
            print("LIST_CONTENT FAIL , REMOTE USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        print("LIST_CONTENT FAIL")
        return client.RC.ERROR
    
    @staticmethod
    def get_file_comms(ip, port, local_FileName, remote_FileName):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM) # Crear el socket
        sock.connect((ip, port)) # Conectar al servidor
        sock.sendall(f"GET_FILE,{remote_FileName}\0".encode()) # Enviar el mensaje al servidor
        cod_error = int(sock.recv(1).decode())# Recibe 4 bytes como respuesta (int)
        if cod_error == 0:
            sock.recv(1)  # Recibe la primera coma
            n_bytes_str = ''
            while True:
                byte = sock.recv(1)
                if byte == b',':
                    break
                n_bytes_str += byte.decode()
            n_bytes = int(n_bytes_str)

            with open(local_FileName, 'wb') as archivo:
                    pass # Vacia el archivo
            
            for i in range(n_bytes):
                byte = sock.recv(1)
                if not byte:
                    break
                
                with open(local_FileName, 'ab') as f:
                    f.write(byte)
            print("GET_FILE FAIL OK")
            return client.RC.OK
        elif cod_error == 1:
            print("GET_FILE FAIL , FILE DOES NOT EXIST")
            return client.RC.USER_ERROR
        
        print("GET_FILE FAIL")
        return client.RC.ERROR



    @staticmethod
    def getfile(user,  remote_FileName,  local_FileName):
        mensaje = f"GET_FILE,{client._username},{user}\0" # Crear el mensaje
        rc = client._send_request_rcv_str(mensaje) # Enviar el mensaje al servidor

        if rc == 0:
            parts = client._lista_respuesta.split(',')
            ip = parts[0]
            port = int(parts[1])
            client.get_file_comms(ip, port, local_FileName, remote_FileName) # Solicitar el archivo al cliente

            return client.RC.OK
        elif rc == 1:
            print("GET_FILE FAIL , USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        elif rc == 2:
            print("GET_FILE FAIL , USER NOT CONNECTED")
            return client.RC.USER_ERROR
        elif rc == 3:
            print("GET_FILE FAIL , REMOTE USER DOES NOT EXIST")
            return client.RC.USER_ERROR
        print("GET_FILE FAIL")
        return client.RC.ERROR

    
        
    
    
    # *
    # **
    # * @brief Command interpreter for the client. It calls the protocol functions.
    @staticmethod
    def shell():

        while (True) :
            try :
                command = input("c> ")
                line = command.split(" ")
                if (len(line) > 0):

                    line[0] = line[0].upper()

                    if (line[0]=="REGISTER") :
                        if (len(line) == 2) :
                            client.register(line[1])
                        else :
                            print("Syntax error. Usage: REGISTER <userName>")

                    elif(line[0]=="UNREGISTER") :
                        if (len(line) == 2) :
                            client.unregister(line[1])
                        else :
                            print("Syntax error. Usage: UNREGISTER <userName>")

                    elif(line[0]=="CONNECT") :
                        if (len(line) == 2) :
                            client.connect(line[1])
                        else :
                            print("Syntax error. Usage: CONNECT <userName>")
                    
                    elif(line[0]=="PUBLISH") :
                        if (len(line) >= 3) :
                            #  Remove first two words
                            description = ' '.join(line[2:])
                            client.publish(line[1], description)
                        else :
                            print("Syntax error. Usage: PUBLISH <fileName> <description>")

                    elif(line[0]=="DELETE") :
                        if (len(line) == 2) :
                            client.delete(line[1])
                        else :
                            print("Syntax error. Usage: DELETE <fileName>")

                    elif(line[0]=="LIST_USERS") :
                        if (len(line) == 1) :
                            client.listusers()
                        else :
                            print("Syntax error. Use: LIST_USERS")

                    elif(line[0]=="LIST_CONTENT") :
                        if (len(line) == 2) :
                            client.listcontent(line[1])
                        else :
                            print("Syntax error. Usage: LIST_CONTENT <userName>")

                    elif(line[0]=="DISCONNECT") :
                        if (len(line) == 2) :
                            client.disconnect(line[1])
                        else :
                            print("Syntax error. Usage: DISCONNECT <userName>")

                    elif(line[0]=="GET_FILE") :
                        if (len(line) == 4) :
                            client.getfile(line[1], line[2], line[3])
                        else :
                            print("Syntax error. Usage: GET_FILE <userName> <remote_fileName> <local_fileName>")

                    elif(line[0]=="QUIT"):
                        if (len(line) == 1):
                            if client._username is not None:
                                client.disconnect(client._username)
                            break
                        else :
                            print("Syntax error. Use: QUIT")
                    else :
                        print("Error: command " + line[0] + " not valid.")
            except Exception as e:
                print("Exception: " + str(e))

    # *
    # * @brief Prints program usage
    @staticmethod
    def usage() :
        print("Usage: python3 client.py -s <server> -p <port>")


    # *
    # * @brief Parses program execution arguments
    @staticmethod
    def  parseArguments(argv) :
        parser = argparse.ArgumentParser()
        parser.add_argument('-s', type=str, required=True, help='Server IP')
        parser.add_argument('-p', type=int, required=True, help='Server Port')
        args = parser.parse_args()

        if (args.s is None):
            parser.error("Usage: python3 client.py -s <server> -p <port>")
            return False

        if ((args.p < 1024) or (args.p > 65535)):
            parser.error("Error: Port must be in the range 1024 <= port <= 65535")
            return False
        
        client._server = args.s
        client._port = args.p

        return True


    # ******************** MAIN *********************
    @staticmethod
    def main(argv) :
        if (not client.parseArguments(argv)) :
            client.usage()
            return

        #  Write code here
        client.shell()
        print("+++ FINISHED +++")
    

if __name__=="__main__":
    client.main([])