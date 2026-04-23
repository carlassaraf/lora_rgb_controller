#include "led_sm.h"
#include <Adafruit_NeoPixel.h>

static const uint8_t PINS[]  = {18, 19, 20};
static const uint8_t N_PINS  = sizeof(PINS);

// Un solo buffer (300 bytes) reutilizado en los 3 pines via setPin()
static Adafruit_NeoPixel s_strip(LED_COUNT, PINS[0], NEO_BRG + NEO_KHZ800);
static led_sm_state_t    s_state    = LED_IDLE;
static uint8_t           s_frame_id = 0;

// FRAME001: cian (R=0 G=200 B=200) -> verde (R=0 G=200 B=0), sin rojo
static void fill_cyan_green(void) {
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        uint8_t b = (uint16_t)(LED_COUNT - 1 - i) * 200 / (LED_COUNT - 1);
        s_strip.setPixelColor(i, 0, 200, b);
    }
}

// FRAME002: gradiente de rojo oscuro a intenso
static void fill_red_gradient(void) {
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        uint8_t r = 10 + (uint16_t)i * 190 / (LED_COUNT - 1);
        s_strip.setPixelColor(i, r, 0, 0);
    }
}

void led_sm_init(void) {
    s_strip.begin();
    s_strip.clear();
    s_strip.show();
    s_state = LED_IDLE;
}

void led_sm_play(uint8_t frame_id) {
    if (s_state != LED_IDLE && s_state != LED_DONE) return;
    s_frame_id = frame_id;
    s_state    = LED_SENDING;
}

void led_sm_run(void) {
    if (s_state != LED_SENDING) return;
    if (s_frame_id == 0) fill_cyan_green();
    else                 fill_red_gradient();
    for (uint8_t p = 0; p < N_PINS; p++) {
        s_strip.setPin(PINS[p]);
        s_strip.show();
    }
    s_state = LED_DONE;
}

bool led_sm_done(void)  { return s_state == LED_DONE; }
void led_sm_reset(void) { s_state = LED_IDLE; }
