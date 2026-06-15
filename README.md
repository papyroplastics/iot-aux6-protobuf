
# Auxiliar 6: Protocol Buffers

Ejemplo de uso de protobuf para comunicar un ESP con un servidor hecho en python usando Wi-Fi,

## Como ejecutar

Lo primero es instalar el compilador de protobug y nanopb, en el raspberry se puede usar el comando `apt install protobuf-compiler nanopb`. Para otros sistemas operativos ver [esta guía](https://protobuf.dev/installation/).

Luego se tiene que ejecutar los siguientes comandos para generar el código de C y python:
```bash
# Compilar packet con nanopb para ESP
protoc packet.proto -omain/packet.pb
nanopb_generator main/packet.pb --output-dir=main

# Compilar packet para python
protoc packet.proto --python_out=server
```

Después se debe instalar el runtime de protobuf en su entorno vierual de python con `pip install protobuf`, el runtime de C está incluído en la carpeta [main/nanopb/](./main/nanopb/).

Finalmente se deben rellenar las credenciales en el archivo [main/credentials.h](./main/credentials.h) y ejecutar, para esto se deben correr estos dos comandos en terminales distintas:
```bash
python server/main.py
idf.py flash monitor
```

## Usar protobuf en sus tareas

Tienen que copiar el directorio [main/nanopb/](./main/nanopb) a sus proyectos e incluir los archivos en el proceso de compilación, ver cmain/CMakeLists.txtc como ejemplo de como hacerlo, estos archivos son el runtime de nanopb y son necesarios para poder usar sus archivos compilados de protobuf.

