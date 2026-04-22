#ifndef NETWORK_SM_H
#define NETWORK_SM_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
  char id[32];
  char apn[64];
  char broker[64];
  char topic[64];
  char user[32];
  char passwd[32];
} network_configuration_t;

/**
 * @brief Network state machine states
 */
typedef enum {
  NETWORK_SM_INIT = 0,        /*< Initialize serial and configuration */
  NETWORK_SM_MODEM_START,     /*< Start/restart modem */
  NETWORK_SM_MODEM_WAIT,      /*< Wait modem stabilization */
  NETWORK_SM_REGISTER_WAIT,   /*< Wait for network registration */
  NETWORK_SM_GPRS_CONNECT,    /*< Attempt GPRS connection */
  NETWORK_SM_GPRS_WAIT,       /*< Wait for GPRS connection */
  NETWORK_SM_NETWORK_WAIT,    /*< Intermediate state before MQTT */
  NETWORK_SM_MQTT_CONNECT,    /*< Connect to MQTT broker */
  NETWORK_SM_MQTT_SUBSCRIBE,  /*< Subscribe to topic */
  NETWORK_SM_READY,           /*< Fully operational (MQTT loop active) */
  NETWORK_SM_RECONNECT,       /*< Recover from connection loss */
  NETWORK_SM_MQTT_WAIT,       /*< Run MQTT client briefly */
  NETWORK_SM_ERROR            /*< Fatal error state */
} network_sm_state_t;

// API

bool network_sm_set_configuration(network_configuration_t *config);
network_sm_state_t network_sm_get_state(void);
network_sm_state_t network_sm_run(void);

// MQTT wrapper API

bool network_sm_mqtt_message_available(void);
void network_sm_mqtt_get_message(char *topic, char *payload);

#ifdef DEBUG
const char *network_sm_state_to_string(network_sm_state_t state);
#endif

#endif