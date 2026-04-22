#include <Arduino.h>
#include "lora_sm.h"
#include "network_sm.h"

#define NODE_ID 0x01
#define CS_PIN 8
#define RST_PIN 4
#define DIO0_PIN 7


void setup() {
#ifdef DEBUG
  Serial.begin(115200);
  while(!Serial);
#endif

  lora_configuration_t lora_config = {
    .node_id = NODE_ID,
    .cs_pin = CS_PIN,
    .rst_pin = RST_PIN,
    .dio0_pin = DIO0_PIN,
    .frequency = (uint32_t)915E6
  };
  lora_sm_set_configuration(&lora_config);

  network_configuration_t network_config = {0};
  strcpy(network_config.id, "lora32u4");
  strcpy(network_config.apn, "datos.personal.com");
  strcpy(network_config.broker, "broker.hivemq.com");
  strcpy(network_config.topic, "test/lora32u4");
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
  network_sm_state_t network_sm_prev = network_sm_get_state();
#endif
  network_sm_state_t network_sm_curr = network_sm_run();
#ifdef DEBUG
  if (network_sm_prev != network_sm_curr) {
    char state_str[64];
    sprintf(state_str, "network_sm: %s -> %s",
            network_sm_state_to_string(network_sm_prev),
            network_sm_state_to_string(network_sm_curr));
    Serial.println(state_str);
  }
#endif

  if (network_sm_mqtt_message_available()) {
    char topic[64];
    char payload[64];

    network_sm_mqtt_get_message(topic, payload);

    Serial.print("TOPIC: ");
    Serial.println(topic);

    Serial.print("PAYLOAD: ");
    Serial.println(payload);
  }

  // --- LoRa SM ---

#ifdef DEBUG
  lora_sm_state_t lora_sm_prev = lora_sm_get_state();
#endif
  lora_sm_state_t lora_sm_curr = lora_sm_run();
#ifdef DEBUG
  if (lora_sm_prev != lora_sm_curr) {
    char state_str[64];
    sprintf(state_str, "lora_sm: %s -> %s",
            lora_sm_state_to_string(lora_sm_prev),
            lora_sm_state_to_string(lora_sm_curr));
    Serial.println(state_str);
  }
#endif
}
