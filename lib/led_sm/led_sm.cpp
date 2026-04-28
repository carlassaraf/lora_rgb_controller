#include "led_sm.h"
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <string.h>

static const uint8_t PINS[]  = {18, 19, 20};
static const uint8_t N_PINS  = sizeof(PINS);

/* Buffer en .bss — sin malloc, sin heap, layout determinístico */
static uint8_t s_pix_buf[LED_BUF_SIZE];

/* Subclase que reemplaza el malloc interno de NeoPixel por s_pix_buf */
class StaticNeoPixel : public Adafruit_NeoPixel {
public:
    StaticNeoPixel(uint16_t n, uint8_t p, neoPixelType t)
        : Adafruit_NeoPixel(0, p, t) {  /* 0 LEDs → no malloc en la base */
        free(pixels);                   /* libera el malloc(0) de la base  */
        numLEDs  = n;
        numBytes = n * 3;
        pixels   = s_pix_buf;
    }
    void begin() {
        memset(pixels, 0, numBytes);
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
        begun = true;
    }
};

static StaticNeoPixel s_strip(LED_COUNT, PINS[0], NEO_BRG + NEO_KHZ800);
static led_sm_state_t s_state          = LED_IDLE;
static uint16_t       s_offset         = 0;
static uint16_t       s_blink_interval = 0;
static uint32_t       s_blink_ts       = 0;

static void show_all(void) {
    for (uint8_t p = 0; p < N_PINS; p++) {
        s_strip.setPin(PINS[p]);
        s_strip.show();
    }
}

void led_sm_init(void) {
    s_strip.begin();
    show_all();
    s_state = LED_IDLE;
}

void led_sm_start(void) {
    memset(s_pix_buf, 0, LED_BUF_SIZE);
    s_offset = 0;
    s_state  = LED_LOADING;
}

/* Escribe chunk directo en el buffer estático — sin copia extra */
void led_sm_feed(uint8_t *data, uint16_t len) {
    if (s_state != LED_LOADING) return;
    uint16_t space = LED_BUF_SIZE - s_offset;
    uint16_t copy  = (len < space) ? len : space;
    memcpy(s_pix_buf + s_offset, data, copy);
    s_offset += copy;
}

void led_sm_finish(void) {
    if (s_state == LED_LOADING) s_state = LED_SENDING;
}

void led_sm_run(void) {
    if (s_state == LED_SENDING) {
        show_all();
        if (s_blink_interval > 0) {
            s_blink_ts = millis();
            s_state = LED_BLINK_ON;
        } else {
            s_state = LED_DONE;
        }
        return;
    }
    if (s_state == LED_BLINK_ON) {
        if ((uint32_t)(millis() - s_blink_ts) >= s_blink_interval) {
            memset(s_pix_buf, 0, LED_BUF_SIZE);
            show_all();
            s_blink_ts = millis();
            s_state = LED_BLINK_OFF;
        }
        return;
    }
    if (s_state == LED_BLINK_OFF) {
        if ((uint32_t)(millis() - s_blink_ts) >= s_blink_interval) {
            s_state = LED_BLINK_RELOAD;
        }
        return;
    }
    /* LED_BLINK_RELOAD: main.cpp re-feeds the frame via SD; after finish() loops back to SENDING */
}

bool led_sm_done(void)  { return s_state == LED_DONE; }
void led_sm_reset(void) { s_state = LED_IDLE; }

void led_sm_set_blink(uint16_t interval_ms) {
    s_blink_interval = interval_ms;
    if (interval_ms > 0 && (s_state == LED_IDLE || s_state == LED_DONE)) {
        /* Frame already shown but blink just enabled — reload it to start the cycle */
        s_state = LED_BLINK_RELOAD;
    }
    if (interval_ms == 0 &&
        (s_state == LED_BLINK_ON || s_state == LED_BLINK_OFF || s_state == LED_BLINK_RELOAD)) {
        /* Trigger one final reload → shows frame → stays on (LED_DONE path) */
        s_state = LED_BLINK_RELOAD;
    }
}

bool led_sm_blink_reload_needed(void) { return s_state == LED_BLINK_RELOAD; }
