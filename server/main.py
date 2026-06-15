import random
import socket
import random
import struct

import packet_pb2 as pb

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

        while True:
            len_raw = conn.recv(4, socket.MSG_WAITALL)
            if len(len_raw) != 4:
                break

            length = struct.unpack("!I", len_raw)[0]
            raw_data = conn.recv(length, socket.MSG_WAITALL)

            req = pb.Request()
            req.ParseFromString(raw_data)
            print(f"Mensaje de {req.device} con timestamp {req.timestamp} ms")

            resp = pb.Response()

            if req.WhichOneof("body") == "ping":
                print(f"ping recibido con num seq {req.ping}")
                resp.pong = req.ping

            elif req.WhichOneof("body") == "data":
                values = list(req.data.values)
                print(f"  datos: {[round(v, 1) for v in values]}")
                resp.average = sum(values) / len(values)

            elif req.WhichOneof("body") == "state":
                print(f"estado {req.state} recibido")
                resp.new_state = random.randrange(4)

            payload = resp.SerializeToString()
            conn.sendall(struct.pack("!I", len(payload)) + payload)

        conn.close()
        print("Conexión cerrada \n")
