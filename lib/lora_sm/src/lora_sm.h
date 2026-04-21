#ifndef LORA_SM_H
#define LORA_SM_H

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

typedef enum {
  LORA_SM_INITIALIZING,
  LORA_SM_IDLE,
  LORA_SM_PROCESSING,
  LORA_SM_SENDING,
} lora_sm_state_t;

typedef enum {
  LORA_SM_MSG_TYPE_DATA,
  LORA_SM_MSG_TYPE_ACK,
  LORA_SM_MSG_TYPE_COMMAND
} lora_sm_msg_type_t;

typedef struct lora_sm_msg {
  uint8_t dst_id;
  uint8_t src_id;
  uint8_t ttl;
  lora_sm_msg_type_t type;
  uint8_t payload[32];
  uint8_t payload_len;
} lora_sm_msg_t;

typedef struct lora_configuration {
  uint8_t node_id;
  int8_t cs_pin;
  int8_t rst_pin;
  int8_t dio0_pin;
  uint32_t frequency;
} lora_configuration_t;

// Public fuinctions
bool lora_sm_set_configuration(lora_configuration_t *config);
lora_sm_state_t lora_sm_run(void);
lora_sm_state_t lora_sm_get_state(void);
bool lora_sm_request_to_send(lora_sm_msg_t *msg);
lora_sm_msg_t lora_sm_build_message(uint8_t dst_id, uint8_t src_id, uint8_t ttl, lora_sm_msg_type_t type, uint8_t *payload, uint8_t payload_len);

#ifdef DEBUG
const char *lora_sm_state_to_string(lora_sm_state_t state);
#endif

#endif // LORA_SM_H