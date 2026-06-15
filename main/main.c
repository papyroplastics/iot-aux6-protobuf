#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <freertos/idf_additions.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_system.h>
#include <esp_random.h>
#include <esp_task.h>
#include <nvs_flash.h>

#include "pb.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "packet.pb.h"

#include "credentials.h"

SemaphoreHandle_t sem;

const uint8_t max_retries = 5;
uint8_t retries = 0;

void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  if (event_base == WIFI_EVENT) {
    if (event_id == WIFI_EVENT_STA_START) {
      esp_wifi_connect();

    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
      printf("Error al conectar, intento %d/%d\n", retries, max_retries);

      if (retries < max_retries) {
        esp_wifi_connect();
        retries++;

      } else {
        xSemaphoreGive(sem);
      }
    }

  } else if (event_base == IP_EVENT) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
      printf("IP obtenida: " IPSTR "\n", IP2STR(&event->ip_info.ip));

      xSemaphoreGive(sem);
    }
  }
}

char* device_name = "esp-room-4";
 
iot_Request get_request() {
  iot_Request req = iot_Request_init_default;
  strcpy(req.device, device_name);
  req.timestamp = xTaskGetTickCount() * (1000 / configTICK_RATE_HZ);
  return req;
}

bool send_request(int sock, const iot_Request* req, iot_Response* resp) {
  uint8_t buf[iot_Request_size];
  pb_ostream_t ostream = pb_ostream_from_buffer(buf, sizeof(buf));

  if (!pb_encode(&ostream, &iot_Request_msg, req)) {
    printf("error al codificar paquete\n");
    return false;
  }

  uint32_t len = htonl(ostream.bytes_written);
  send(sock, &len, sizeof(len), 0);
  send(sock, &buf, ostream.bytes_written, 0);

  recv(sock, &len, sizeof(len), MSG_WAITALL);
  len = ntohl(len);
  recv(sock, buf, len, MSG_WAITALL);

  pb_istream_t istream = pb_istream_from_buffer(buf, len);
  if (!pb_decode(&istream, &iot_Response_msg, resp)) {
    printf("error al decodificar respuesta\n");
    return false;
  }

  return true;
}

void app_main() {
  sem = xSemaphoreCreateBinary();

  nvs_flash_init();
  esp_netif_init();

  esp_event_loop_create_default();
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);

  esp_event_handler_instance_t wifi_any_evh;

  esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &wifi_any_evh);

  esp_event_handler_instance_t got_ip_evh;
  esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &got_ip_evh);

  esp_wifi_set_mode(WIFI_MODE_STA);
  wifi_config_t wifi_config = {
    .sta = {
      .ssid = WIFI_AP_SSID,
      .password = WIFI_AP_PASS,
      .threshold.authmode = WIFI_AUTH_WPA2_PSK,
      .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
      .sae_h2e_identifier = "",
    },
  };
  esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

  esp_wifi_start();

  xSemaphoreTake(sem, portMAX_DELAY);

  if (retries >= max_retries) {
    printf("Error al conectarse al AP %s\n, reiniciando...\n", WIFI_AP_SSID);
    sleep(5);
    esp_restart();
  }
  printf("Conectado con exito al AP %s\n", WIFI_AP_SSID);

  // Cliente TCP
  int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  struct sockaddr_in server_addr = {
    .sin_family = AF_INET,
    .sin_port   = htons(SERVER_PORT),
  };
  inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

  int err = connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
  if (err != 0) {
    printf("Error al conectarse a " SERVER_IP "\n");
    close(sock);
    sleep(5);
    esp_restart();
  }
  printf("Conectado con el servidor en " SERVER_IP "\n");

  iot_Request req = get_request();
  req.which_body = iot_Request_ping_tag;
  req.body.ping = esp_random() % 100;

  printf("Enviando ping seq num: %ld\n", req.body.ping);

  iot_Response resp = iot_Response_init_default;
  if (send_request(sock, &req, &resp)) {
    printf("Pong recibido num seq %ld\n", resp.body.pong);
  }

  // Datos
  req = get_request();
  req.which_body = iot_Request_data_tag;

  iot_Data* data = &req.body.data;
  data->values_count = (esp_random() % 5) + 3;

  printf("Enviando datos:");
  for (int i = 0; i < data->values_count; i++) {
    data->values[i] = (esp_random() % 1000) / 10.0f;
    printf(" %.1f", data->values[i]);
  }
  printf("\n");

  if (send_request(sock, &req, &resp)) {
    printf("Promedio recibido: %.1f\n", resp.body.average);
  }

  // Estado
  req = get_request();
  req.which_body = iot_Request_state_tag;
  req.body.state = esp_random() % 4;

  printf("Enviando estado %d\n", req.body.state);

  if (send_request(sock, &req, &resp)) {
    printf("Nuevo estado recibido num seq %ld\n", resp.body.pong);
  }

  close(sock);
  sleep(5);
  esp_restart();
}
