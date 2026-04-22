#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "SdFat.h"

typedef enum {
  SD_SM_INIT = 0,
  SD_SM_IDLE,
  SD_SM_OPEN,
  SD_SM_READ,
  SD_SM_CLOSE,
  SD_SM_DATA_READY,
  SD_SM_ERROR
} sd_sm_state_t;

void sd_manager_init(uint8_t cs_pin);
bool sd_manager_request_file(const char *filename, uint8_t *buf, uint16_t buf_size);
sd_sm_state_t sd_manager_run(void);
sd_sm_state_t sd_manager_get_state(void);
bool sd_manager_data_ready(void);
uint16_t sd_manager_get_bytes_read(void);
void sd_manager_consume(void);

#ifdef DEBUG
const char *sd_sm_state_to_string(sd_sm_state_t state);
#endif

#endif
