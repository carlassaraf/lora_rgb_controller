#include "sd_manager.h"
#include <Arduino.h>

#define SPI_CLOCK SD_SCK_MHZ(2)

static sd_sm_state_t s_state = SD_SM_INIT;
static SdFat32 sd;
static File32 file;
static int8_t s_cs_pin = -1;

static char s_filename[32];
static uint8_t *s_buf = NULL;
static uint16_t s_buf_size = 0;
static uint16_t s_bytes_read = 0;
static bool s_eof = false;

static sd_sm_state_t sd_sm_init(void) {
  if (s_cs_pin < 0) return SD_SM_ERROR;
  if (!sd.begin(SdSpiConfig(s_cs_pin, SHARED_SPI, SPI_CLOCK))) {
#ifdef DEBUG
    Serial.println(F("SD init failed"));
#endif
    return SD_SM_ERROR;
  }
#ifdef DEBUG
  Serial.println(F("SD init OK"));
#endif
  return SD_SM_IDLE;
}

static sd_sm_state_t sd_sm_idle(void) {
  if (s_filename[0] != '\0') return SD_SM_OPEN;
  return SD_SM_IDLE;
}

static sd_sm_state_t sd_sm_open(void) {
  if (!file.open(s_filename, O_RDONLY)) {
#ifdef DEBUG
    Serial.print(F("File not found: "));
    Serial.println(s_filename);
#endif
    s_filename[0] = '\0';
    return SD_SM_IDLE;
  }
#ifdef DEBUG
  Serial.print(F("Opened: "));
  Serial.println(s_filename);
#endif
  s_bytes_read = 0;
  return SD_SM_READ;
}

static sd_sm_state_t sd_sm_read(void) {
  int32_t n = file.read(s_buf, s_buf_size);
  if (n < 0) {
#ifdef DEBUG
    Serial.println(F("SD read error"));
#endif
    file.close();
    s_filename[0] = '\0';
    s_eof = false;
    return SD_SM_ERROR;
  }
  s_bytes_read = (uint16_t)n;
  s_eof = !file.available();
  return SD_SM_DATA_READY;
}

static sd_sm_state_t sd_sm_close(void) {
  file.close();
  s_filename[0] = '\0';
  return SD_SM_DATA_READY;
}

static sd_sm_state_t sd_sm_data_ready(void) {
  return SD_SM_DATA_READY;
}

static sd_sm_state_t sd_sm_error(void) {
  return SD_SM_ERROR;
}

static sd_sm_state_t (*s_handlers[])(void) = {
  [SD_SM_INIT]       = sd_sm_init,
  [SD_SM_IDLE]       = sd_sm_idle,
  [SD_SM_OPEN]       = sd_sm_open,
  [SD_SM_READ]       = sd_sm_read,
  [SD_SM_CLOSE]      = sd_sm_close,
  [SD_SM_DATA_READY] = sd_sm_data_ready,
  [SD_SM_ERROR]      = sd_sm_error,
};

void sd_manager_init(uint8_t cs_pin) {
  s_cs_pin = (int8_t)cs_pin;
  s_filename[0] = '\0';
  s_state = SD_SM_INIT;
}

bool sd_manager_request_file(const char *filename, uint8_t *buf, uint16_t buf_size) {
  if (s_state == SD_SM_ERROR) s_state = SD_SM_INIT;  // retry init on next run()
  if (s_state != SD_SM_IDLE) return false;
  strncpy(s_filename, filename, sizeof(s_filename) - 1);
  s_filename[sizeof(s_filename) - 1] = '\0';
  s_buf = buf;
  s_buf_size = buf_size;
  return true;
}

sd_sm_state_t sd_manager_run(void) {
  s_state = s_handlers[s_state]();
  return s_state;
}

sd_sm_state_t sd_manager_get_state(void) {
  return s_state;
}

bool sd_manager_data_ready(void) {
  return s_state == SD_SM_DATA_READY;
}

uint16_t sd_manager_get_bytes_read(void) {
  return s_bytes_read;
}

void sd_manager_consume(void) {
  if (s_state != SD_SM_DATA_READY) return;
  s_bytes_read = 0;
  if (s_eof) {
    file.close();
    s_filename[0] = '\0';
    s_eof = false;
    s_state = SD_SM_IDLE;
  } else {
    s_state = SD_SM_READ;
  }
}

#ifdef DEBUG
const char *sd_sm_state_to_string(sd_sm_state_t state) {
  switch (state) {
    case SD_SM_INIT:       return "INIT";
    case SD_SM_IDLE:       return "IDLE";
    case SD_SM_OPEN:       return "OPEN";
    case SD_SM_READ:       return "READ";
    case SD_SM_CLOSE:      return "CLOSE";
    case SD_SM_DATA_READY: return "DATA_READY";
    case SD_SM_ERROR:      return "ERROR";
    default:               return "UNKNOWN";
  }
}
#endif
