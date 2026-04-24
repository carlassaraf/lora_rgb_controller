#include "led_sm.h"
#include <Adafruit_NeoPixel.h>
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
static led_sm_state_t s_state  = LED_IDLE;
static uint16_t       s_offset = 0;

void led_sm_init(void) {
    s_strip.begin();
    s_strip.show();
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
    if (s_state != LED_SENDING) return;
    for (uint8_t p = 0; p < N_PINS; p++) {
        s_strip.setPin(PINS[p]);
        s_strip.show();
    }
    s_state = LED_DONE;
}

bool led_sm_done(void)  { return s_state == LED_DONE; }
void led_sm_reset(void) { s_state = LED_IDLE; }
