#include "lora_sm.h"
#include <string.h>

// State machine state variable
static lora_sm_state_t lora_sm_current_state = LORA_SM_INITIALIZING;
// LoRa configuration variable
static lora_configuration_t lora_sm_config = {
  .node_id = 0x00,
  .cs_pin = LORA_DEFAULT_SS_PIN,
  .rst_pin = LORA_DEFAULT_RESET_PIN,
  .dio0_pin = LORA_DEFAULT_DIO0_PIN,
  .frequency = (uint32_t)915E6
};
// Pending packet variable
static volatile bool lora_sm_packet_pending = false;

// Pending data to be sent circular buffer
#define LORA_SM_TX_QUEUE_SIZE 4
static lora_sm_msg_t tx_queue[LORA_SM_TX_QUEUE_SIZE];
static uint8_t tx_head = 0;
static uint8_t tx_tail = 0;
static uint8_t lora_sm_pending_to_send_count = 0;

/** @brief LoRa receive callback function */
static void lora_sm_on_receive_callback(int packetSize) {
  // Trigger state transition to RECEIVING state when a packet is received
  if (packetSize > 0) { lora_sm_packet_pending = true; }
};

/** @brief Helper function to flush rx buffer */
static void lora_sm_flush_rx_buffer() {
  while (LoRa.available()) {
    LoRa.read();
  }
}

// State handler implementations

static lora_sm_state_t lora_sm_state_initializing(void) {

  LoRa.setPins(lora_sm_config.cs_pin, lora_sm_config.rst_pin, lora_sm_config.dio0_pin);
  if (!LoRa.begin(lora_sm_config.frequency)) {
      return LORA_SM_INITIALIZING;
  }
  LoRa.onReceive(lora_sm_on_receive_callback);
  LoRa.receive();
  return LORA_SM_IDLE;
}

static lora_sm_state_t lora_sm_state_idle(void) {
  // In idle state we just wait for a packet to be received or a send request to be made

  // Fairness to avoid starvation
  static uint8_t lora_sm_tx_priority = 0;
  if(lora_sm_packet_pending && lora_sm_tx_priority < 3) {
    lora_sm_packet_pending = false;
    lora_sm_tx_priority++;
    return LORA_SM_PROCESSING;
  }
  if(lora_sm_pending_to_send_count > 0) {
    lora_sm_tx_priority = 0;
    return LORA_SM_SENDING;
  }
  return LORA_SM_IDLE;
}

static lora_sm_state_t lora_sm_state_processing(void) {

  lora_sm_msg_t msg;
  // Validación mínima: al menos header (5 bytes)
  if (LoRa.available() < 5) {
#ifdef DEBUG
    Serial.println("Packet too short");
#endif
    lora_sm_flush_rx_buffer();
    return LORA_SM_IDLE;
  }

  // Parse header
  msg.dst_id = LoRa.read();
  msg.src_id = LoRa.read();
  msg.ttl    = LoRa.read();
  msg.type   = (lora_sm_msg_type_t)LoRa.read();
  msg.payload_len = LoRa.read();

  // Validación de longitud
  if (msg.payload_len > sizeof(msg.payload)) {
#ifdef DEBUG
    Serial.println("Payload too large");
#endif
    lora_sm_flush_rx_buffer();
    return LORA_SM_IDLE;
  }

  // Leer payload
  uint8_t i = 0;
  while (LoRa.available() && i < msg.payload_len) {
    msg.payload[i++] = LoRa.read();
  }

  // Si llegaron menos bytes de los esperados
  if (i != msg.payload_len) {
#ifdef DEBUG
    Serial.println("Warning: payload length mismatch");
#endif
    lora_sm_flush_rx_buffer();
    return LORA_SM_IDLE;
  }

#ifdef DEBUG
  Serial.println("----- LoRa Packet -----");
  Serial.print("DST: 0x");
  if (msg.dst_id < 16) Serial.print("0");
  Serial.println(msg.dst_id, HEX);

  Serial.print("SRC: 0x");
  if (msg.src_id < 16) Serial.print("0");
  Serial.println(msg.src_id, HEX);

  Serial.print("TTL: ");
  Serial.println(msg.ttl);

  Serial.print("TYPE: ");
  Serial.println((uint8_t)msg.type);

  Serial.print("LEN: ");
  Serial.println(msg.payload_len);

  Serial.print("PAYLOAD (HEX): ");
  for (uint8_t j = 0; j < msg.payload_len; j++) {
    if (msg.payload[j] < 16) Serial.print("0");
    Serial.print(msg.payload[j], HEX);
    Serial.print(" ");
  }
  Serial.println();

  Serial.print("PAYLOAD (ASCII): ");
  for (uint8_t j = 0; j < msg.payload_len; j++) {
    char c = msg.payload[j];
    if (c >= 32 && c <= 126) {
      Serial.print(c);
    } else {
      Serial.print(".");
    }
  }
  Serial.println();

  Serial.print("RSSI: ");
  Serial.println(LoRa.packetRssi());
  

  Serial.print("SNR: ");
  Serial.println(LoRa.packetSnr());
  Serial.println("-----------------------");
#endif

  if(msg.dst_id == lora_sm_config.node_id) {
#ifdef DEBUG
    Serial.println("Packet is for me!");
#endif
    /** @todo Implement message processing logic */
  } else {
#ifdef DEBUG
    Serial.println("Packet is not for me, ignoring...");
#endif
  }

  return LORA_SM_IDLE;
}

static lora_sm_state_t lora_sm_state_sending(void) {
  // Check for a pending message to be sent
  if (lora_sm_pending_to_send_count == 0) { return LORA_SM_IDLE; }
  // Copy message from the queue
  lora_sm_msg_t *msg = &tx_queue[tx_tail];

  // Send message
  LoRa.beginPacket();
  LoRa.write(msg->dst_id);
  LoRa.write(msg->src_id);
  LoRa.write(msg->ttl);
  LoRa.write((uint8_t)msg->type);
  LoRa.write(msg->payload_len);
  LoRa.write(msg->payload, msg->payload_len);
  LoRa.endPacket();

  // Remove message from the queue
  tx_tail = (tx_tail + 1) % LORA_SM_TX_QUEUE_SIZE;
  lora_sm_pending_to_send_count--;
  // After sending, go back to idle state and wait for the next packet or send request
  LoRa.receive();
  return LORA_SM_IDLE;
}

// State handler function pointer array
static lora_sm_state_t (*lora_sm_state_handlers[])(void) = {
  [LORA_SM_INITIALIZING] = lora_sm_state_initializing,
  [LORA_SM_IDLE] = lora_sm_state_idle,
  [LORA_SM_PROCESSING] = lora_sm_state_processing,
  [LORA_SM_SENDING] = lora_sm_state_sending
};

// Public function implementations

/** @brief Sets the LoRa configuration */
bool lora_sm_set_configuration(lora_configuration_t *config) {
  memcpy(&lora_sm_config, config, sizeof(lora_configuration_t));
  // A new configuration may require re-initialization, so we reset the state machine to the INITIALIZING state
  lora_sm_current_state = LORA_SM_INITIALIZING;
  LoRa.end();
  return true;
}

/** @brief Runs the LoRa state machine and returns current state */
lora_sm_state_t lora_sm_run(void) {
  lora_sm_current_state = lora_sm_state_handlers[lora_sm_current_state]();
  return lora_sm_current_state;
}

/** @brief Returns the current state of the LoRa state machine */
lora_sm_state_t lora_sm_get_state(void) {
  return lora_sm_current_state;
}

/** @brief Requests to send a message */
bool lora_sm_request_to_send(lora_sm_msg_t *msg) {
  // Validate size
  if (msg->payload_len > sizeof(msg->payload)) { return false; }
  // Check if queue is full
  if (lora_sm_pending_to_send_count >= LORA_SM_TX_QUEUE_SIZE) { return false; }
  // Copy message to the queue
  tx_queue[tx_head] = *msg;
  tx_head = (tx_head + 1) % LORA_SM_TX_QUEUE_SIZE;
  lora_sm_pending_to_send_count++;
  return true;
}

#ifdef DEBUG
/** @brief Converts a LoRa state machine state to a human-readable string */
const char *lora_sm_state_to_string(lora_sm_state_t state) {
  switch (state) {
    case LORA_SM_INITIALIZING:
      return "INITIALIZING";
    case LORA_SM_IDLE:
      return "IDLE";
    case LORA_SM_PROCESSING:
      return "PROCESSING";
    case LORA_SM_SENDING:
      return "SENDING";
    default:
      return "UNKNOWN";
  }
}
#endif