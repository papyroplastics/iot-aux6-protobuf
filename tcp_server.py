import socket
import struct

HOST = "0.0.0.0"
PORT = 5000

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as svr:
    svr.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    svr.bind((HOST, PORT))
    svr.listen(1)
    print(f"Servidor escuchando en {HOST}:{PORT}")

    while True:
        conn, addr = svr.accept()
        print(f"Conexión recibida de {addr}")

        len_raw = conn.recv(4, socket.MSG_WAITALL)
        len_struct = struct.unpack("!I", len_raw)
        len_arr = len_struct[0]
        print(f"largo del arreglo: {len_arr}")

        raw_data = conn.recv(len_arr * 4, socket.MSG_WAITALL)
        values = struct.unpack(f"!{len_arr}I", raw_data)
        print(f"valores: {values}")

        total = sum(values) % (2**32)
        print(f"suma: {total}")

        conn.sendall(struct.pack("!I", total))
        conn.close()
        print("Conexión cerrada \n")
