#include "network_sm.h"

#define TINY_GSM_MODEM_SIM7600
#define MQTT_MAX_PACKET_SIZE 64

#include <Arduino.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>

// Current state machine status
static network_sm_state_t network_sm_current_state = NETWORK_SM_INIT;
// Current network configuration
static network_configuration_t network_configuration = {0};

TinyGsm modem(Serial1);
TinyGsmClient client(modem);

// MQTT objects and variables
static PubSubClient mqtt(client);

static char mqtt_rx_topic[32];
static char mqtt_rx_payload[32];
static bool mqtt_msg_available = false;

static uint32_t state_ts = 0;

// Helpers

static inline bool config_is_valid(void) {
  return (strlen(network_configuration.apn) > 0 &&
          strlen(network_configuration.broker) > 0);
}

// MQTT callbacks

/** @brief Handles sub message */
static void mqtt_callback(char* topic, byte* payload, unsigned int len) {
  memset(mqtt_rx_topic, 0, sizeof(mqtt_rx_topic));
  memset(mqtt_rx_payload, 0, sizeof(mqtt_rx_payload));
  // Copy data from sub message
  strncpy(mqtt_rx_topic, topic, sizeof(mqtt_rx_topic) - 1);
  for (unsigned int i = 0; i < len && i < sizeof(mqtt_rx_payload) - 1; i++) {
    mqtt_rx_payload[i] = (char)payload[i];
  }
  mqtt_msg_available = true;
}

// State handlers

/** @brief Initialize Serial interface and modem configuration */ 
static network_sm_state_t network_sm_init(void) { 
  // Validate configuration
  if (!config_is_valid()) {
  #ifdef DEBUG
    Serial.println(F("Invalid network configuration"));
  #endif
    return NETWORK_SM_INIT;
  }
  // Initialize Serial communication and switch modem to 9600
  Serial1.begin(115200);
  Serial1.println(F("AT+IPR=9600"));
  delay(200);
#ifdef DEBUG
  while(Serial1.available()) { Serial.print(Serial1.read()); }
  Serial.println(); Serial.println(F("Changing baudrate to 9600..."));
#endif
  Serial1.end();
  Serial1.begin(9600);
#ifdef DEBUG
  Serial.println(F("Network SM init done"));
#endif 
  return NETWORK_SM_MODEM_START;
}

/** @brief Start/restart modem */
static network_sm_state_t network_sm_modem_start(void) {
#ifdef DEBUG
  Serial.println(F("Initializing modem..."));
#endif
  modem.init();
  state_ts = millis();
  return NETWORK_SM_MODEM_WAIT;
}

/** @brief Wait for modem to stabilize */
static network_sm_state_t network_sm_modem_wait(void) {
  if (millis() - state_ts < 8000) {
    return NETWORK_SM_MODEM_WAIT;
  }
#ifdef DEBUG
  Serial.println(F("Modem ready"));
#endif
  state_ts = millis();
  return NETWORK_SM_REGISTER_WAIT;
}

/** @brief Verify if module is connected to GPRS network */
static network_sm_state_t network_sm_register_wait(void) {
  if (modem.isNetworkConnected()) {
#ifdef DEBUG
    Serial.println(F("Network registered!"));
#endif
    return NETWORK_SM_GPRS_CONNECT;
  }
  if (millis() - state_ts > 60000) {
#ifdef DEBUG
    Serial.println(F("Still not registered... waiting more"));
#endif
    state_ts = millis();
  }
  return NETWORK_SM_REGISTER_WAIT;
}

/** @brief Handles GPRS connection */
static network_sm_state_t network_sm_gprs_connect(void) {
#ifdef DEBUG
  Serial.println(F("Connecting GPRS..."));
  Serial.print(F("APN: "));
  Serial.println(network_configuration.apn);
#endif
  modem.gprsDisconnect();
  delay(1000);
  modem.gprsConnect(network_configuration.apn, "", network_configuration.passwd);
  state_ts = millis();
  return NETWORK_SM_GPRS_WAIT;
}

/** @brief  */
static network_sm_state_t network_sm_gprs_wait(void) {
  if (modem.isGprsConnected()) {
    client.setTimeout(10000);
#ifdef DEBUG
    Serial.println(F("GPRS connected!"));
#endif
    return NETWORK_SM_NETWORK_WAIT;
  }
  if (millis() - state_ts > 30000) {
#ifdef DEBUG
    Serial.println(F("GPRS timeout -> reconnect"));
#endif
    return NETWORK_SM_RECONNECT;
  }
  return NETWORK_SM_GPRS_WAIT;
}

/** @brief  */
static network_sm_state_t network_sm_network_wait(void) {
  return NETWORK_SM_MQTT_CONNECT;
}

/** @brief Handles connection to MQTT broker */
static network_sm_state_t network_sm_mqtt_connect(void) {

  if (state_ts != 0 && millis() - state_ts < 5000) {
    return NETWORK_SM_MQTT_CONNECT;
  }

#ifdef DEBUG
  Serial.println(F("Connecting to MQTT broker..."));
  Serial.print(F("Broker: "));
  Serial.println(network_configuration.broker);
#endif

  mqtt.setServer(network_configuration.broker, 1883);
  mqtt.setCallback(mqtt_callback);

  if (mqtt.connect(network_configuration.id, NULL, NULL)) {
#ifdef DEBUG
    Serial.println(F("MQTT connected!"));
#endif
    state_ts = millis();
    return NETWORK_SM_MQTT_WAIT;
  }

#ifdef DEBUG
  Serial.print(F("MQTT state: "));
  Serial.println(mqtt.state());
  Serial.println(F("MQTT connect failed -> retry"));
#endif

  state_ts = millis();
  return NETWORK_SM_MQTT_CONNECT;
}

/** @brief Runs MQTT client before subscribing */
static network_sm_state_t network_sm_mqtt_wait(void) {

  mqtt.loop();
  if (millis() - state_ts < 500) {
    return NETWORK_SM_MQTT_WAIT;
  }
  return NETWORK_SM_MQTT_SUBSCRIBE;
}

/** @brief Handles subscription to topic */
static network_sm_state_t network_sm_mqtt_subscribe(void) {

#ifdef DEBUG
  Serial.print(F("Subscribing to topic: "));
  Serial.println(network_configuration.topic);
#endif

  mqtt.loop();

  if (mqtt.subscribe(network_configuration.topic, 0)) {
#ifdef DEBUG
    Serial.println(F("MQTT subscribed!"));
#endif
    return NETWORK_SM_READY;
  }

#ifdef DEBUG
  Serial.print(F("Subscribe failed, state: "));
  Serial.println(mqtt.state());
#endif
  return NETWORK_SM_RECONNECT;
}

/** @brief Network state machine ready */
static network_sm_state_t network_sm_ready(void) {

  if (!modem.isGprsConnected()) {
#ifdef DEBUG
    Serial.println(F("Lost GPRS -> reconnect"));
#endif
    return NETWORK_SM_RECONNECT;
  }

  if (!mqtt.connected()) {
#ifdef DEBUG
    Serial.println(F("Lost MQTT -> reconnect"));
#endif
    return NETWORK_SM_MQTT_CONNECT;
  }
  mqtt.loop();
  return NETWORK_SM_READY;
}

/** @brief Handles Network reconnection */
static network_sm_state_t network_sm_reconnect(void) {
#ifdef DEBUG
  Serial.println(F("Reconnecting..."));
#endif
  state_ts = millis();
  return NETWORK_SM_GPRS_CONNECT;
}

/** @brief Handles Network error */
static network_sm_state_t network_sm_error(void) {
#ifdef DEBUG
  Serial.println(F("Network SM error"));
#endif
  return NETWORK_SM_ERROR;
}

/** @brief State handlers */
static network_sm_state_t (*sm_handlers[])(void) = {
  [NETWORK_SM_INIT]            = network_sm_init,
  [NETWORK_SM_MODEM_START]     = network_sm_modem_start,
  [NETWORK_SM_MODEM_WAIT]      = network_sm_modem_wait,
  [NETWORK_SM_REGISTER_WAIT]   = network_sm_register_wait,
  [NETWORK_SM_GPRS_CONNECT]    = network_sm_gprs_connect,
  [NETWORK_SM_GPRS_WAIT]       = network_sm_gprs_wait,
  [NETWORK_SM_NETWORK_WAIT]    = network_sm_network_wait,
  [NETWORK_SM_MQTT_CONNECT]    = network_sm_mqtt_connect,
  [NETWORK_SM_MQTT_SUBSCRIBE]  = network_sm_mqtt_subscribe,
  [NETWORK_SM_READY]           = network_sm_ready,
  [NETWORK_SM_RECONNECT]       = network_sm_reconnect,
  [NETWORK_SM_MQTT_WAIT]       = network_sm_mqtt_wait,
  [NETWORK_SM_ERROR]           = network_sm_error,
};

// Public API

bool network_sm_set_configuration(network_configuration_t *config) {
  if (!config) return false;
  memcpy(&network_configuration, config, sizeof(network_configuration_t));
  network_sm_current_state = NETWORK_SM_INIT;
  return true;
}

network_sm_state_t network_sm_get_state(void) {
  return network_sm_current_state;
}

network_sm_state_t network_sm_run(void) {
  network_sm_current_state = sm_handlers[network_sm_current_state]();
  return network_sm_current_state;
}

bool network_sm_mqtt_message_available(void) {
  return mqtt_msg_available;
}

void network_sm_mqtt_get_message(char *topic, char *payload) {
  if (!mqtt_msg_available) return;

  strcpy(topic, mqtt_rx_topic);
  strcpy(payload, mqtt_rx_payload);

  mqtt_msg_available = false;
}

// Debug functions

#ifdef DEBUG
const char *network_sm_state_to_string(network_sm_state_t state) {
  switch (state) {
    case NETWORK_SM_INIT: return "INIT";
    case NETWORK_SM_MODEM_START: return "MODEM_START";
    case NETWORK_SM_MODEM_WAIT: return "MODEM_WAIT";
    case NETWORK_SM_REGISTER_WAIT: return "REGISTER_WAIT";
    case NETWORK_SM_GPRS_CONNECT: return "GPRS_CONNECT";
    case NETWORK_SM_GPRS_WAIT: return "GPRS_WAIT";
    case NETWORK_SM_NETWORK_WAIT: return "NETWORK_WAIT";
    case NETWORK_SM_MQTT_CONNECT: return "MQTT_CONNECT";
    case NETWORK_SM_MQTT_SUBSCRIBE: return "MQTT_SUBSCRIBE";
    case NETWORK_SM_READY: return "READY";
    case NETWORK_SM_RECONNECT: return "RECONNECT";
    case NETWORK_SM_ERROR: return "ERROR";
    default: return "UNKNOWN";
  }
}
#endif