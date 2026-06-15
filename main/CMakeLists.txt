# listar los archivos del runtime de nanopb
file(GLOB_RECURSE nanopb_runtime "nanopb/*.c")

idf_component_register(
  SRCS
    "main.c"
    "packet.pb.c"
    "${nanopb_runtime}" # compilar los archivos de nanopb

  INCLUDE_DIRS
    "."
    "nanopb" # incluir los headers de nanopb

  PRIV_REQUIRES
    esp_wifi
    nvs_flash
)
