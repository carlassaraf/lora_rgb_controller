#include <Arduino.h>
#include "lora_sm.h"

#define NODE_ID 0x01

#define CS_PIN 8
#define RST_PIN 4
#define DIO0_PIN 7

#define MAX_PAYLOAD 64

void setup() {
  Serial.begin(115200);

  lora_configuration_t config = {
    .node_id = NODE_ID,
    .cs_pin = CS_PIN,
    .rst_pin = RST_PIN,
    .dio0_pin = DIO0_PIN,
    .frequency = (uint32_t)915E6
  };
  lora_sm_set_configuration(&config);
#if (NODE_ID == 0x01)
  Serial.println("MASTER listo");
#else
  Serial.println("END NODE listo");
#endif
}

void loop() {

  lora_sm_state_t lora_sm_state = lora_sm_get_state();
  if(lora_sm_state != lora_sm_run()) {
    char state_str[128];
    sprintf(state_str, "Transitioning from %s to %s", lora_sm_state_to_string(lora_sm_state), lora_sm_state_to_string(lora_sm_get_state()));
    Serial.println(state_str);
  }

#if (NODE_ID == 0x01)
  static uint32_t counter = 0;
  char msg[32];
  sprintf(msg, "Msg %lu", counter++);
  Serial.print("Enviando: ");
  Serial.println(msg);

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
  delay(3000);
#endif
}