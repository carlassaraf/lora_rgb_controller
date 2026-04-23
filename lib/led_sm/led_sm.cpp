#include "led_sm.h"
#include "light_ws2812.h"
#include <string.h>

static uint8_t        s_frame[LED_BUF_SIZE];
static uint16_t       s_offset;
static led_sm_state_t s_state = LED_IDLE;

void led_sm_init(void) { s_state = LED_IDLE; }

void led_sm_start(void) {
    s_offset = 0;
    s_state  = LED_LOADING;
}

void led_sm_feed(uint8_t *data, uint16_t len) {
    if (s_state != LED_LOADING) return;
    uint16_t space = LED_BUF_SIZE - s_offset;
    uint16_t copy  = (len < space) ? len : space;
    memcpy(s_frame + s_offset, data, copy);
    s_offset += copy;
}

void led_sm_finish(void) {
    if (s_state == LED_LOADING) s_state = LED_SENDING;
}

void led_sm_run(void) {
    if (s_state == LED_SENDING) {
        ws2812_sendarray(s_frame, s_offset);
        s_state = LED_DONE;
    }
}

bool           led_sm_done(void)  { return s_state == LED_DONE; }
led_sm_state_t led_sm_state(void) { return s_state; }
void           led_sm_reset(void) { s_state = LED_IDLE; }