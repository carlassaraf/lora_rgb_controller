#ifndef LED_SM_H
#define LED_SM_H

#include <stdint.h>
#include <stdbool.h>

#define LED_COUNT    100
#define LED_BUF_SIZE (LED_COUNT * 3)   /* 300 bytes BRG */

typedef enum {
    LED_IDLE,
    LED_LOADING,
    LED_SENDING,
    LED_DONE,
    LED_BLINK_ON,      /* frame is shown; waiting interval before blanking */
    LED_BLINK_OFF,     /* strip is blank; waiting interval before reload   */
    LED_BLINK_RELOAD   /* main.cpp must re-feed the frame from SD          */
} led_sm_state_t;

void led_sm_init(void);
void led_sm_start(void);
void led_sm_feed(uint8_t *data, uint16_t len);
void led_sm_finish(void);
void led_sm_run(void);
bool led_sm_done(void);
void led_sm_reset(void);

/* Blink API — interval_ms=0 disables blink */
void led_sm_set_blink(uint16_t interval_ms);
bool led_sm_blink_reload_needed(void);

#endif
