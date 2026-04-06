#include <Arduino.h>
#include "lora_sm.h"

#define NODE_ID 0x03

#define CS_PIN 8
#define RST_PIN 4
#define DIO0_PIN 7

#define MAX_PAYLOAD 64

void setup() {
#ifdef LORA_SM_DEBUG
  Serial.begin(115200);
#endif

  lora_configuration_t config = {
    .node_id = NODE_ID,
    .cs_pin = CS_PIN,
    .rst_pin = RST_PIN,
    .dio0_pin = DIO0_PIN,
    .frequency = (uint32_t)915E6
  };
  lora_sm_set_configuration(&config);

  #if (NODE_ID == 0x01)
    pinMode(A1, INPUT_PULLUP);
  #else
    pinMode(A0, OUTPUT);
    digitalWrite(A0, LOW);
  #endif
}

void loop() {

  lora_sm_state_t lora_sm_state = lora_sm_get_state();
  if(lora_sm_state != lora_sm_run()) {
#ifdef LORA_SM_DEBUG
    char state_str[128];
    sprintf(state_str, "Transitioning from %s to %s", lora_sm_state_to_string(lora_sm_state), lora_sm_state_to_string(lora_sm_get_state()));
    Serial.println(state_str);
#endif
  }

#if (NODE_ID == 0x01)
  static bool last_button_state = HIGH;
  bool current_button_state = digitalRead(A1);
  // Only send message when button is pressed
  if(last_button_state == HIGH && current_button_state == LOW) {
    delay(50); // Debounce delay
    if(digitalRead(A1) == LOW) {
      // Button is still pressed after debounce delay, send a message
      char msg[MAX_PAYLOAD];
      sprintf(msg, "Hello from node 0x%02X!", NODE_ID);
    #ifdef LORA_SM_DEBUG
      Serial.print("Enviando: ");
      Serial.println(msg);
    #endif
      // Prepare message struct
      lora_sm_msg_t msg_struct = {
        .dst_id = 0x03,
        .src_id = NODE_ID,
        .ttl = 5,
        .type = LORA_SM_MSG_TYPE_DATA,
        .payload = {0},
        .payload_len = (uint8_t)strlen(msg)
      };
      memcpy(msg_struct.payload, msg, strlen(msg));
      if (!lora_sm_request_to_send(&msg_struct)) {
      #ifdef LORA_SM_DEBUG
        Serial.println("TX queue full!");
      #endif
      }
      delay(100);
    }
  }
  // Update last button state
  last_button_state = current_button_state;

#else
  // Check if there was a message received for this node
  lora_sm_msg_t received_msg;
  if(lora_sm_message_for_me(&received_msg)) {
  #ifdef LORA_SM_DEBUG
    Serial.println("Message received for me from 0x" + String(received_msg.src_id, HEX));
  #endif
    // Toggle LED to indicate message received
    digitalWrite(A0, !digitalRead(A0));
  }
#endif
}