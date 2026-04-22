#include <Arduino.h>
#include "network_sm.h"
#include "sd_manager.h"

#define SD_CS_PIN 3
#define SD_CHUNK_SIZE 32

static uint8_t s_frame_buf[SD_CHUNK_SIZE];

void setup() {
  
  Serial.begin(115200);
  while (!Serial);
  // Start SD manager
  sd_manager_init(SD_CS_PIN);
  // Network configuration
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
}

void loop() {

  // --- Network SM ---

#ifdef DEBUG
  network_sm_state_t net_prev = network_sm_get_state();
#endif
  network_sm_state_t net_curr = network_sm_run();
#ifdef DEBUG
  if (net_prev != net_curr) {
    char buf[64];
    sprintf(buf, "network_sm: %s -> %s",
            network_sm_state_to_string(net_prev),
            network_sm_state_to_string(net_curr));
    Serial.println(buf);
  }
#endif
  // Check if there's any message available from MQTT
  if (network_sm_mqtt_message_available()) {
    char topic[32];
    char payload[32];
    network_sm_mqtt_get_message(topic, payload);
#ifdef DEBUG
    Serial.print(F("MQTT rx: ")); Serial.println(payload);
#endif
    // Try to find file in SD
  if (!sd_manager_request_file(payload, s_frame_buf, SD_CHUNK_SIZE)) {
#ifdef DEBUG
      Serial.println(F("SD busy"));
#endif
    }
  }

  // --- SD Manager ---

  // Run SD state machine
  sd_manager_run();
  // Once requested, read data from SD
  if (sd_manager_data_ready()) {
    uint16_t n = sd_manager_get_bytes_read();
    for (uint16_t i = 0; i < n; i++) {
      Serial.write(s_frame_buf[i]);
    }
    sd_manager_consume();
  }
}
