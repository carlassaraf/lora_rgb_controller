/**
 * @file network_sm.h
 * @brief Non-blocking network state machine: A76XX modem bring-up, GPRS and MQTT.
 *
 * The SM drives the full connection sequence autonomously:
 *   baud-rate init → network registration → PDP context → MQTT broker → subscribe.
 * Once in READY, it receives MQTT messages and exposes them via the message API.
 * On connection loss it reconnects automatically from the GPRS stage.
 */

#ifndef NETWORK_SM_H
#define NETWORK_SM_H

#include <stdint.h>
#include <stdbool.h>

/** @brief Runtime configuration passed once before the first network_sm_run() call. */
typedef struct {
  char id[32];      /**< MQTT client identifier string. */
  char apn[32];     /**< Mobile network APN (e.g. "datos.personal.com"). */
  char broker[32];  /**< MQTT broker hostname or IP address. */
  char topic[32];   /**< MQTT topic to subscribe to. */
  char user[32];    /**< MQTT username; leave empty if the broker has no auth. */
  char passwd[32];  /**< MQTT password; leave empty if the broker has no auth. */
} network_configuration_t;

typedef enum {
  NETWORK_SM_INIT = 0,       /**< Set baud rate (115200 → 9600) and disable echo (ATE0). */
  NETWORK_SM_MODEM_START,    /**< Record the start time for modem stabilization. */
  NETWORK_SM_MODEM_WAIT,     /**< Hold 5 s to let the A76XX firmware finish booting. */
  NETWORK_SM_REGISTER_WAIT,  /**< Poll AT+CEREG? until registered on the network (stat 1 or 5). */
  NETWORK_SM_GPRS_CONNECT,   /**< Set the PDP context APN via AT+CGDCONT. */
  NETWORK_SM_GPRS_WAIT,      /**< Poll AT+CGPADDR until a non-zero IP address is assigned. */
  NETWORK_SM_NETWORK_WAIT,   /**< Pass-through; reserved for future pre-MQTT logic. */
  NETWORK_SM_MQTT_CONNECT,   /**< Start MQTT service, acquire client, connect to broker. */
  NETWORK_SM_MQTT_SUBSCRIBE, /**< Subscribe to the configured topic via AT+CMQTTSUB. */
  NETWORK_SM_READY,          /**< MQTT session active; processes incoming message URCs. */
  NETWORK_SM_RECONNECT,      /**< Gracefully disconnect and restart from GPRS_CONNECT. */
  NETWORK_SM_MQTT_WAIT,      /**< Unused pass-through; transitions immediately to MQTT_SUBSCRIBE. */
  NETWORK_SM_ERROR           /**< Fatal error; SM halts here indefinitely. */
} network_sm_state_t;

/**
 * @brief Store configuration and reset the SM to INIT.
 * @param config Pointer to a fully populated network_configuration_t; must not be NULL.
 * @return false if config is NULL.
 */
bool network_sm_set_configuration(network_configuration_t *config);

/** @brief Returns the current state without advancing the SM. */
network_sm_state_t network_sm_get_state(void);

/**
 * @brief Advance the state machine by one step; call every loop iteration.
 * @note On every state transition, Serial1 RX is flushed and the internal
 *       step counter and cmd_sent flag are reset to avoid stale responses
 *       bleeding into the next state.
 * @return The state after the transition.
 */
network_sm_state_t network_sm_run(void);

/**
 * @brief Returns true when an MQTT message has been received and not yet consumed.
 * @note The message is cleared by the next call to network_sm_mqtt_get_message().
 */
bool network_sm_mqtt_message_available(void);

/**
 * @brief Copy the last received MQTT message into caller-owned buffers.
 * @param topic   Destination buffer for the topic string; must be at least 32 bytes.
 * @param payload Destination buffer for the payload string; must be at least 32 bytes.
 * @note Clears the available flag. No-op if no message is pending.
 */
void network_sm_mqtt_get_message(char *topic, char *payload);

// #ifdef DEBUG
// /** @brief Returns a human-readable name for the given state (DEBUG builds only). */
// const char *network_sm_state_to_string(network_sm_state_t state);
// #endif

#endif
