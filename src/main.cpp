#include <Arduino.h>
#include "network_sm.h"
#include "led_sm.h"

void setup() {
  Serial.begin(115200);
  while (!Serial);
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
    Serial.print(network_sm_state_to_string(net_prev));
    Serial.print(F(" -> "));
    Serial.println(network_sm_state_to_string(net_curr));
  }
#endif

  if (network_sm_mqtt_message_available()) {
    char topic[32], payload[32];
    network_sm_mqtt_get_message(topic, payload);
#ifdef DEBUG
    Serial.print(F("MQTT rx: ")); Serial.println(payload);
#endif
    if      (strncmp(payload, "FRAME001", 8) == 0) led_sm_play(0);
    else if (strncmp(payload, "FRAME002", 8) == 0) led_sm_play(1);
  }

  led_sm_run();
  if (led_sm_done()) led_sm_reset();
}
