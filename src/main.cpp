#include <Arduino.h>
#include "network_sm.h"
#include "sd_manager.h"
#include "led_sm.h"

#define SD_CS_PIN     3
#define SD_CHUNK_SIZE 32

static uint8_t s_chunk[SD_CHUNK_SIZE];
static bool    s_loading = false;

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
    if (sd_manager_request_file(payload, s_chunk, SD_CHUNK_SIZE)) {
      led_sm_start();
      s_loading = true;
    }
#ifdef DEBUG
    else { Serial.println(F("SD busy")); }
#endif
  }

  sd_manager_run();
  if (s_loading && sd_manager_data_ready()) {
    led_sm_feed(s_chunk, sd_manager_get_bytes_read());
    sd_manager_consume();
    if (sd_manager_get_state() == SD_SM_IDLE) {
      led_sm_finish();
      s_loading = false;
    }
  }

  led_sm_run();
  if (led_sm_done()) led_sm_reset();
}
