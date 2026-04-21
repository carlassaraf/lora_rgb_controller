#include <Arduino.h>
#include "lora_sm.h"
#include "network_sm.h"

#define NODE_ID 0x01

#define CS_PIN 8
#define RST_PIN 4
#define DIO0_PIN 7

#define MAX_PAYLOAD 64

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
  while(!Serial);
#endif

  lora_configuration_t config = {
    .node_id = NODE_ID,
    .cs_pin = CS_PIN,
    .rst_pin = RST_PIN,
    .dio0_pin = DIO0_PIN,
    .frequency = (uint32_t)915E6
  };
  lora_sm_set_configuration(&config);

  network_configuration_t network_config = {0};
  strcpy(network_config.id, "lora32u4");
  strcpy(network_config.apn, "datos.personal.com");
  strcpy(network_config.broker, "broker.hivemq.com");
  strcpy(network_config.topic, "test/lora32u4");
  if(!network_sm_set_configuration(&network_config)) {
  #ifdef DEBUG
    Serial.println("Failed to configure Network State Machine");
  #endif
    while(1);
  }
#ifdef DEBUG
  Serial.println("Successfully configured Network State Machine");
#endif
}

void loop() {
  // Network handling
  network_sm_state_t network_sm_prev = network_sm_get_state();
  network_sm_state_t network_sm_curr = network_sm_run();

#ifdef DEBUG
  if(network_sm_prev != network_sm_curr) {
    char state_str[64];
    sprintf(state_str, "network_sm: Transitioning from %s to %s",
            network_sm_state_to_string(network_sm_prev),
            network_sm_state_to_string(network_sm_curr));
    Serial.println(state_str);
  }
#endif

  if(network_sm_curr == NETWORK_SM_READY && network_sm_mqtt_message_available()) {
    static char topic[64];
    static char payload[128];
    network_sm_mqtt_get_message(topic, payload);
    Serial.print("RX [");
    Serial.print(topic);
    Serial.print("]: ");
    Serial.println(payload);
  }

  // LoRa handling

  lora_sm_state_t lora_sm_state = lora_sm_get_state();
  if(lora_sm_state != lora_sm_run()) {
#ifdef DEBUG
    char state_str[128];
    sprintf(state_str, "Transitioning from %s to %s", lora_sm_state_to_string(lora_sm_state), lora_sm_state_to_string(lora_sm_get_state()));
    Serial.println(state_str);
#endif
  }

#if (NODE_ID == 0x01)
  static uint32_t counter = 0;
  char msg[32];
  sprintf(msg, "Msg %lu", counter++);
#ifdef DEBUG
  Serial.print("Enviando: ");
  Serial.println(msg);
#endif

  lora_sm_msg_t msg_struct = {
    .dst_id = 0x03,
    .src_id = NODE_ID,
    .ttl = 5,
    .type = LORA_SM_MSG_TYPE_DATA,
    .payload = {0},
    .payload_len = (uint8_t)strlen(msg)
  };
  memcpy(msg_struct.payload, msg, strlen(msg));
  lora_sm_request_to_send(&msg_struct);
#endif
}