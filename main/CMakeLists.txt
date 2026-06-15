file(GLOB_RECURSE nanopb_runtime "nanopb/*.c")

idf_component_register(
  SRCS
    "main.c"
    "packet.pb.c"
    "${nanopb_runtime}"

  INCLUDE_DIRS
    "."
    "nanopb"

  PRIV_REQUIRES
    esp_wifi
    nvs_flash
)
