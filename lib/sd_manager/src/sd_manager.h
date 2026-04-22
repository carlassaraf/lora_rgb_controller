/**
 * @file sd_manager.h
 * @brief Non-blocking SD card file reader, streaming in fixed-size chunks via SdFat.
 *
 * Typical usage:
 *   sd_manager_init(cs_pin);
 *   sd_manager_request_file("FILE.BIN", buf, sizeof(buf));
 *   // In loop():
 *   sd_manager_run();
 *   if (sd_manager_data_ready()) {
 *     process(buf, sd_manager_get_bytes_read());
 *     sd_manager_consume();   // triggers next chunk or closes file at EOF
 *   }
 */

#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "SdFat.h"

typedef enum {
  SD_SM_INIT = 0,   /**< Mount the SD card over SPI. */
  SD_SM_IDLE,       /**< Waiting for a file request. */
  SD_SM_OPEN,       /**< Opening the requested file. */
  SD_SM_READ,       /**< Reading the next chunk into the caller's buffer. */
  SD_SM_CLOSE,      /**< Reserved — not reached in the normal read path. */
  SD_SM_DATA_READY, /**< Chunk ready; caller must call consume() before the next read. */
  SD_SM_ERROR       /**< Mount or read failure; resets to INIT on the next request. */
} sd_sm_state_t;

/**
 * @brief Initialise the SD manager and begin mounting the card.
 * @param cs_pin SPI chip-select pin connected to the SD module.
 */
void sd_manager_init(uint8_t cs_pin);

/**
 * @brief Request a file to be read in chunks into a caller-owned buffer.
 * @param filename Null-terminated 8.3 filename in the SD root directory.
 * @param buf      Buffer to write into; must remain valid until the last consume().
 * @param buf_size Bytes per chunk (must equal sizeof(buf)).
 * @return true if the request was accepted; false if the SM is busy or not idle.
 * @note If the SM is in ERROR, calling this resets it to INIT automatically.
 */
bool sd_manager_request_file(const char *filename, uint8_t *buf, uint16_t buf_size);

/**
 * @brief Advance the state machine by one step; call every loop iteration.
 * @return Current state after the transition.
 */
sd_sm_state_t sd_manager_run(void);

/** @brief Returns the current state without advancing the SM. */
sd_sm_state_t sd_manager_get_state(void);

/**
 * @brief Returns true when a chunk is ready in the caller's buffer.
 * @note Inspect the data with sd_manager_get_bytes_read(), then call consume().
 */
bool sd_manager_data_ready(void);

/**
 * @brief Number of bytes written into the buffer by the last read.
 * @note Valid only while sd_manager_data_ready() returns true.
 */
uint16_t sd_manager_get_bytes_read(void);

/**
 * @brief Consume the current chunk and schedule the next read or close the file.
 * @note Must be called after processing each DATA_READY chunk.
 *       Closes the file automatically when EOF is reached.
 */
void sd_manager_consume(void);

#ifdef DEBUG
/** @brief Returns a human-readable name for the given state (DEBUG builds only). */
const char *sd_sm_state_to_string(sd_sm_state_t state);
#endif

#endif
