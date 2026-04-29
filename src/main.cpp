#include <Arduino.h>
#include "network_sm.h"
#include "sd_manager.h"
#include "led_sm.h"

#define SD_CS_PIN     3
#define SD_CHUNK_SIZE 32

static uint8_t s_chunk[SD_CHUNK_SIZE];
static bool    s_loading       = false;
static char    s_last_file[16] = {0};  /* filename of the last loaded frame, for blink reload */

/* Request a file from SD and arm the LED SM for loading. */
static void request_file(const char *filename) {
  if (sd_manager_request_file(filename, s_chunk, SD_CHUNK_SIZE)) {
    strncpy(s_last_file, filename, sizeof(s_last_file) - 1);
    s_last_file[sizeof(s_last_file) - 1] = '\0';
    led_sm_start();
    s_loading = true;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial);
  sd_manager_init(SD_CS_PIN);
  network_configuration_t network_config = {0};
  strcpy(network_config.id,     "lora32u4");
  strcpy(network_config.apn,    "datos.personal.com");
  strcpy(network_config.broker, "broker.hivemq.com");
  strcpy(network_config.topic,  "test/lora32u4");
  if (!network_sm_set_configuration(&network_config)) {
#ifdef DEBUG
    Serial.println(F("Failed to configure Network SM"));
#endif
    while(1);
  }
  led_sm_init();
}

void loop() {
#ifdef DEBUG
  network_sm_state_t net_prev = network_sm_get_state();
#endif
  network_sm_run();
#ifdef DEBUG
  network_sm_state_t net_curr = network_sm_get_state();
  if (net_prev != net_curr) {
    Serial.print(F("network_sm: "));
    Serial.print(net_prev);
    Serial.print(F(" -> "));
    Serial.println(net_curr);
  }
#endif

  if (network_sm_mqtt_message_available()) {
    char topic[16], payload[16];
    network_sm_mqtt_get_message(topic, payload);
#ifdef DEBUG
    Serial.print(F("MQTT rx: ")); Serial.println(payload);
#endif
    if (strncmp(payload, "BLK:", 4) == 0) {
      /* BLK:<ms> — set blink interval; BLK:0 disables */
      led_sm_set_blink((uint16_t)atoi(payload + 4));
    } else if (strncmp(payload, "ROT:", 4) == 0) {
      /* ROT:<ms> — rotate 1 LED every <ms>; ROT:0 freezes */
      led_sm_set_rot((uint16_t)atoi(payload + 4));
    } else if (!s_loading) {
      /* Numeric payload: "1" → "FRAME001.BIN", "23" → "FRAME023.BIN" */
      char filename[] = "FRAME000.BIN";
      uint16_t n = (uint16_t)atoi(payload);
      filename[7] = '0' + (n % 10);
      filename[6] = '0' + ((n / 10) % 10);
      filename[5] = '0' + ((n / 100) % 10);
      request_file(filename);
    }
#ifdef DEBUG
    else { Serial.println(F("SD busy")); }
#endif
  }

  /* Blink reload: re-feed the last frame from SD so the strip turns on again */
  if (!s_loading && led_sm_blink_reload_needed() && s_last_file[0] != '\0') {
    request_file(s_last_file);
  }

  sd_manager_run();
  if (s_loading && sd_manager_data_ready()) {
    led_sm_feed(s_chunk, sd_manager_get_bytes_read());
    sd_manager_consume();
    if (sd_manager_get_state() == SD_SM_IDLE) {
      led_sm_finish();
      s_loading = false;
    }
  } else if (s_loading && sd_manager_get_state() == SD_SM_IDLE) {
    /* File not found or other SD error — abort gracefully */
    led_sm_reset();
    s_loading = false;
  }

  if (!network_sm_rx_in_progress()) led_sm_run();
  if (led_sm_done()) led_sm_reset();
}
