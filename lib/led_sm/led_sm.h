#ifndef LED_SM_H
#define LED_SM_H

#include <stdint.h>
#include <stdbool.h>

#define LED_COUNT    100
#define LED_BUF_SIZE (LED_COUNT * 3)   /* 300 bytes */

typedef enum {
    LED_IDLE,
    LED_SENDING,
    LED_DONE
} led_sm_state_t;

void led_sm_init(void);
void led_sm_play(uint8_t frame_id);   /* 0 = FRAME001, 1 = FRAME002 */
void led_sm_run(void);
bool led_sm_done(void);
void led_sm_reset(void);

#endif
