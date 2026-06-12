#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_system.h>
#include <esp_random.h>
#include <nvs_flash.h>

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
    printf("Error al conectarse con " SERVER_IP);
    close(sock);
    return;
  }
  printf("Conectado con el servidor en " SERVER_IP);

  uint32_t buf[10];
  uint32_t buf_len = (esp_random() % 8) + 2;
  buf[0] = htonl(buf_len);

  uint32_t expected = 0;
  for (uint32_t j = 1; j <= buf_len; j++) {
    uint32_t v = esp_random();
    buf[j] = htonl(v);
    expected += v;
  }

  send(sock, buf, (buf_len + 1) * sizeof(uint32_t), 0);
  printf("Array de %ld elementos enviado, suma esperada %lu, ", buf_len, expected);

  uint32_t result;
  recv(sock, &result, sizeof(result), MSG_WAITALL);
  printf("suma recibida: %lu\n", ntohl(result));

  close(sock);
  sleep(5);
  esp_restart();
}
