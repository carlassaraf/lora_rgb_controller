#include "network_sm.h"
#include <Arduino.h>
#include <string.h>

static network_sm_state_t   s_state    = NETWORK_SM_INIT;
static network_configuration_t s_config = {0};
static uint32_t s_ts       = 0;    // timestamp used for timeouts within each state
static uint8_t  s_step     = 0;    // sub-step counter for states that issue multiple AT commands
static bool     s_cmd_sent = false; // prevents re-sending a command before its response arrives

// AT line reader state
static char    s_line[64];   // last complete line received from Serial1
static uint8_t s_rx_pos = 0; // write position in s_line

// MQTT receive reassembly counters (populated from +CMQTTRXSTART URC)
static uint8_t s_recv_topic_len   = 0;
static uint8_t s_recv_payload_len = 0;

// Last received MQTT message (valid while s_mqtt_available is true)
static char s_mqtt_topic[32];
static char s_mqtt_payload[32];
static bool s_mqtt_available = false;

// ---------------------------------------------------------------------------
// AT helpers
// ---------------------------------------------------------------------------

/**
 * @brief Read from Serial1 into s_line until a complete non-empty line arrives.
 * @return true when a line is ready in s_line (CR stripped), false otherwise.
 */
static bool at_readline(void) {
  while (Serial1.available()) {
    char c = (char)Serial1.read();
    if (c == '\n') {
      while (s_rx_pos > 0 && s_line[s_rx_pos - 1] == '\r') s_rx_pos--;
      s_line[s_rx_pos] = '\0';
      uint8_t len = s_rx_pos;
      s_rx_pos = 0;
      if (len > 0) return true;
    } else if (c != '\r' && s_rx_pos < (uint8_t)(sizeof(s_line) - 1)) {
      s_line[s_rx_pos++] = c;
    }
  }
  return false;
}

/** @brief Returns true if s_line exactly matches s. */
static bool line_is(const char *s)     { return strcmp(s_line, s) == 0; }

/** @brief Returns true if s_line starts with the prefix s. */
static bool line_starts(const char *s) { return strncmp(s_line, s, strlen(s)) == 0; }

/** @brief Discard all pending Serial1 bytes and reset the line buffer. */
static void serial1_flush_rx(void) {
  while (Serial1.available()) Serial1.read();
  s_rx_pos = 0;
}

#ifdef DEBUG
static bool at_readline_dbg(void) {
  bool got = at_readline();
  if (got) { Serial.print(F("< ")); Serial.println(s_line); }
  return got;
}
#define AT_READLINE() at_readline_dbg()
#else
#define AT_READLINE() at_readline()
#endif

// ---------------------------------------------------------------------------
// State handlers
// ---------------------------------------------------------------------------

/**
 * @brief Start Serial1 at 115200, lock it to 9600 via AT+IPR, then disable echo.
 * @note Retries every 3 s until the modem acknowledges ATE0 with OK.
 */
static network_sm_state_t network_sm_init(void) {
  if (!s_cmd_sent) {
    Serial1.begin(115200);
    Serial1.println(F("AT+IPR=9600"));
    delay(200);
    Serial1.end();
    Serial1.begin(9600);
    Serial1.println(F("ATE0"));
    s_ts = millis();
    s_cmd_sent = true;
  }
  if (AT_READLINE() && line_is("OK")) return NETWORK_SM_MODEM_START;
  if (millis() - s_ts > 3000) { s_cmd_sent = false; }  // retry
  return NETWORK_SM_INIT;
}

/** @brief Record the current time so MODEM_WAIT can measure the stabilization delay. */
static network_sm_state_t network_sm_modem_start(void) {
  s_ts = millis();
  return NETWORK_SM_MODEM_WAIT;
}

/** @brief Block for 5 s to allow the A76XX firmware to finish its internal start-up. */
static network_sm_state_t network_sm_modem_wait(void) {
  if (millis() - s_ts < 5000) return NETWORK_SM_MODEM_WAIT;
  s_ts = millis();
  return NETWORK_SM_REGISTER_WAIT;
}

/**
 * @brief Poll AT+CEREG? every 5 s until the modem reports registered (stat 1 or 5).
 * @note Handles both +CEREG: <stat> and +CEREG: <n>,<stat> response formats.
 */
static network_sm_state_t network_sm_register_wait(void) {
  if (!s_cmd_sent || millis() - s_ts > 5000) {
    Serial1.println(F("AT+CEREG?"));
    s_ts = millis();
    s_cmd_sent = true;
  }
  if (AT_READLINE() && line_starts("+CEREG:")) {
    char *p = strrchr(s_line, ',');
    uint8_t stat;
    if (p) {
      stat = (uint8_t)atoi(p + 1);
    } else {
      p = strchr(s_line, ':');
      stat = p ? (uint8_t)atoi(p + 2) : 0;
    }
    if (stat == 1 || stat == 5) return NETWORK_SM_GPRS_CONNECT;
    s_cmd_sent = false;
  }
  return NETWORK_SM_REGISTER_WAIT;
}

/**
 * @brief Send AT+CGDCONT to define the PDP context with the configured APN.
 * @note Both OK and ERROR advance to GPRS_WAIT — ERROR is acceptable because
 *       the context may already be defined from a previous session.
 */
static network_sm_state_t network_sm_gprs_connect(void) {
  if (!s_cmd_sent) {
    Serial1.print(F("AT+CGDCONT=1,\"IP\",\""));
    Serial1.print(s_config.apn);
    Serial1.println('"');
    s_ts = millis();
    s_cmd_sent = true;
  }
  if (AT_READLINE()) {
    if (line_is("OK") || line_is("ERROR")) return NETWORK_SM_GPRS_WAIT;
  }
  if (millis() - s_ts > 10000) return NETWORK_SM_RECONNECT;
  return NETWORK_SM_GPRS_CONNECT;
}

/**
 * @brief Poll AT+CGPADDR every 5 s until the modem reports a non-zero IP address.
 * @note Rejects 0.0.0.0 which the modem reports while the PDP context is still activating.
 */
static network_sm_state_t network_sm_gprs_wait(void) {
  if (!s_cmd_sent || millis() - s_ts > 5000) {
    Serial1.println(F("AT+CGPADDR=1"));
    s_ts = millis();
    s_cmd_sent = true;
  }
  if (AT_READLINE() && line_starts("+CGPADDR: 1,") && strlen(s_line) > 12 && !strstr(s_line, "0.0.0.0")) {
    return NETWORK_SM_NETWORK_WAIT;
  }
  return NETWORK_SM_GPRS_WAIT;
}

/** @brief Pass-through; reserved for future pre-MQTT logic. */
static network_sm_state_t network_sm_network_wait(void) {
  return NETWORK_SM_MQTT_CONNECT;
}

/**
 * @brief Run the three-step MQTT connection sequence.
 *
 * Step 0: AT+CMQTTSTART  — start the MQTT service (code 23 = already running).
 * Step 1: AT+CMQTTACCQ   — acquire client slot 0 with the configured client ID.
 * Step 2: AT+CMQTTCONNECT — connect to the broker on port 1883, keepalive 60 s.
 *
 * Any ERROR or a 15 s overall timeout triggers RECONNECT.
 */
static network_sm_state_t network_sm_mqtt_connect(void) {
  if (!s_cmd_sent) {
    switch (s_step) {
      case 0: Serial1.println(F("AT+CMQTTSTART")); break;
      case 1:
        Serial1.print(F("AT+CMQTTACCQ=0,\""));
        Serial1.print(s_config.id);
        Serial1.println(F("\",0"));
        break;
      case 2:
        Serial1.print(F("AT+CMQTTCONNECT=0,\"tcp://"));
        Serial1.print(s_config.broker);
        Serial1.println(F(":1883\",60,1"));
        break;
    }
    s_ts = millis();
    s_cmd_sent = true;
  }

  if (AT_READLINE()) {
    if (s_step == 0) {
      if (line_starts("+CMQTTSTART:")) {
        char *p = strchr(s_line, ':');
        int code = p ? atoi(p + 2) : -1;
        if (code == 0 || code == 23) { s_step++; s_cmd_sent = false; }
        else return NETWORK_SM_RECONNECT;
      } else if (line_is("OK")) {
        s_step++; s_cmd_sent = false;
      } else if (line_is("ERROR")) return NETWORK_SM_RECONNECT;
    } else if (s_step == 1) {
      if (line_is("OK")) { s_step++; s_cmd_sent = false; }
      else if (line_is("ERROR")) return NETWORK_SM_RECONNECT;
    } else if (s_step == 2) {
      if (line_starts("+CMQTTCONNECT:")) {
        char *p = strrchr(s_line, ',');
        if (p && atoi(p + 1) == 0) return NETWORK_SM_MQTT_SUBSCRIBE;
        return NETWORK_SM_RECONNECT;
      } else if (line_is("ERROR")) return NETWORK_SM_RECONNECT;
    }
  }
  if (millis() - s_ts > 15000) return NETWORK_SM_RECONNECT;
  return NETWORK_SM_MQTT_CONNECT;
}

/** @brief Unused pass-through; transitions immediately to MQTT_SUBSCRIBE. */
static network_sm_state_t network_sm_mqtt_wait(void) {
  return NETWORK_SM_MQTT_SUBSCRIBE;
}

/**
 * @brief Subscribe to the configured topic via AT+CMQTTSUB.
 *
 * The A76XX CMQTTSUB command sends the topic length first, then waits for a
 * raw '>' prompt (no newline) before accepting the topic string.
 *
 * Step 0: send AT+CMQTTSUB=0,<len>,0 and scan for '>' in the raw serial stream.
 * Step 1: send the topic string and wait for +CMQTTSUB: 0,0 (success).
 */
static network_sm_state_t network_sm_mqtt_subscribe(void) {
  if (s_step == 0) {
    if (!s_cmd_sent) {
      uint8_t tlen = (uint8_t)strlen(s_config.topic);
      Serial1.print(F("AT+CMQTTSUB=0,"));
      Serial1.print(tlen);
      Serial1.println(F(",0"));
      s_ts = millis();
      s_cmd_sent = true;
    }
    // '>' prompt has no newline — scan raw serial for it
    while (Serial1.available()) {
      if ((char)Serial1.read() == '>') { s_step = 1; s_cmd_sent = false; break; }
    }
  } else {
    if (!s_cmd_sent) {
      Serial1.println(s_config.topic);
      s_ts = millis();
      s_cmd_sent = true;
    }
    if (AT_READLINE()) {
      if (line_starts("+CMQTTSUB:")) {
        char *p = strrchr(s_line, ',');
        if (p && atoi(p + 1) == 0) return NETWORK_SM_READY;
        return NETWORK_SM_RECONNECT;
      }
      if (line_is("ERROR")) return NETWORK_SM_RECONNECT;
    }
  }
  if (millis() - s_ts > 10000) return NETWORK_SM_RECONNECT;
  return NETWORK_SM_MQTT_SUBSCRIBE;
}

/**
 * @brief MQTT receive loop — parses A76XX message URCs and detects disconnection.
 *
 * The A76XX delivers each incoming message as five consecutive URCs:
 *   +CMQTTRXSTART: 0,<topic_len>,<payload_len>
 *   +CMQTTRXTOPIC: 0,<topic_len>
 *   <topic string>
 *   +CMQTTRXPAYLOAD: 0,<payload_len>
 *   <payload string>
 *
 * s_step tracks which URC is expected next (0 = idle, 1–4 = mid-receive).
 * +CMQTTCONNLOST / +CMQTTNONET trigger an immediate RECONNECT.
 */
static network_sm_state_t network_sm_ready(void) {
  if (!at_readline()) return NETWORK_SM_READY;

  if (line_starts("+CMQTTCONNLOST:") || line_starts("+CMQTTNONET:")) {
    return NETWORK_SM_RECONNECT;
  }

  if (s_step == 0 && line_starts("+CMQTTRXSTART: 0,")) {
    const char *p = s_line + sizeof("+CMQTTRXSTART: 0,") - 1;
    s_recv_topic_len   = (uint8_t)atoi(p);
    p = strchr(p, ',');
    s_recv_payload_len = p ? (uint8_t)atoi(p + 1) : 0;
    s_step = 1;
  } else if (s_step == 1 && line_starts("+CMQTTRXTOPIC:")) {
    s_step = 2;
  } else if (s_step == 2) {
    uint8_t tc = (s_recv_topic_len < sizeof(s_mqtt_topic) - 1) ? s_recv_topic_len : sizeof(s_mqtt_topic) - 1;
    memcpy(s_mqtt_topic, s_line, tc);
    s_mqtt_topic[tc] = '\0';
    s_step = 3;
  } else if (s_step == 3 && line_starts("+CMQTTRXPAYLOAD:")) {
    s_step = 4;
  } else if (s_step == 4) {
    uint8_t pc = (s_recv_payload_len < sizeof(s_mqtt_payload) - 1) ? s_recv_payload_len : sizeof(s_mqtt_payload) - 1;
    memcpy(s_mqtt_payload, s_line, pc);
    s_mqtt_payload[pc] = '\0';
    s_mqtt_available = true;
    s_step = 0;
  }
  return NETWORK_SM_READY;
}

/**
 * @brief Gracefully disconnect from MQTT and restart from GPRS_CONNECT.
 * @note Uses blocking delays; acceptable here since this is an error-recovery path.
 */
static network_sm_state_t network_sm_reconnect(void) {
  Serial1.println(F("AT+CMQTTDISC=0,120"));
  delay(500);
  Serial1.println(F("AT+CMQTTSTOP"));
  delay(1000);
  return NETWORK_SM_GPRS_CONNECT;
}

/** @brief Fatal error sink; the SM halts here until reset. */
static network_sm_state_t network_sm_error(void) {
  return NETWORK_SM_ERROR;
}

static network_sm_state_t (*s_handlers[])(void) = {
  [NETWORK_SM_INIT]           = network_sm_init,
  [NETWORK_SM_MODEM_START]    = network_sm_modem_start,
  [NETWORK_SM_MODEM_WAIT]     = network_sm_modem_wait,
  [NETWORK_SM_REGISTER_WAIT]  = network_sm_register_wait,
  [NETWORK_SM_GPRS_CONNECT]   = network_sm_gprs_connect,
  [NETWORK_SM_GPRS_WAIT]      = network_sm_gprs_wait,
  [NETWORK_SM_NETWORK_WAIT]   = network_sm_network_wait,
  [NETWORK_SM_MQTT_CONNECT]   = network_sm_mqtt_connect,
  [NETWORK_SM_MQTT_SUBSCRIBE] = network_sm_mqtt_subscribe,
  [NETWORK_SM_READY]          = network_sm_ready,
  [NETWORK_SM_RECONNECT]      = network_sm_reconnect,
  [NETWORK_SM_MQTT_WAIT]      = network_sm_mqtt_wait,
  [NETWORK_SM_ERROR]          = network_sm_error,
};

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool network_sm_set_configuration(network_configuration_t *config) {
  if (!config) return false;
  memcpy(&s_config, config, sizeof(network_configuration_t));
  s_state = NETWORK_SM_INIT;
  return true;
}

network_sm_state_t network_sm_get_state(void) { return s_state; }

network_sm_state_t network_sm_run(void) {
  network_sm_state_t next = s_handlers[s_state]();
  if (next != s_state) {
    s_cmd_sent = false;
    s_step     = 0;
    serial1_flush_rx();
  }
  s_state = next;
  return s_state;
}

bool network_sm_mqtt_message_available(void) { return s_mqtt_available; }

void network_sm_mqtt_get_message(char *topic, char *payload) {
  if (!s_mqtt_available) return;
  strcpy(topic,   s_mqtt_topic);
  strcpy(payload, s_mqtt_payload);
  s_mqtt_available = false;
}

#ifdef DEBUG
const char *network_sm_state_to_string(network_sm_state_t state) {
  switch (state) {
    case NETWORK_SM_INIT:           return "INIT";
    case NETWORK_SM_MODEM_START:    return "MODEM_START";
    case NETWORK_SM_MODEM_WAIT:     return "MODEM_WAIT";
    case NETWORK_SM_REGISTER_WAIT:  return "REGISTER_WAIT";
    case NETWORK_SM_GPRS_CONNECT:   return "GPRS_CONNECT";
    case NETWORK_SM_GPRS_WAIT:      return "GPRS_WAIT";
    case NETWORK_SM_NETWORK_WAIT:   return "NETWORK_WAIT";
    case NETWORK_SM_MQTT_CONNECT:   return "MQTT_CONNECT";
    case NETWORK_SM_MQTT_SUBSCRIBE: return "MQTT_SUBSCRIBE";
    case NETWORK_SM_READY:          return "READY";
    case NETWORK_SM_RECONNECT:      return "RECONNECT";
    case NETWORK_SM_MQTT_WAIT:      return "MQTT_WAIT";
    case NETWORK_SM_ERROR:          return "ERROR";
    default:                        return "UNKNOWN";
  }
}
#endif
