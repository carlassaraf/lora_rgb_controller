#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

#define NODE_ID 0x01

#define CS_PIN 8
#define RST_PIN 4
#define DIO0_PIN 7

#define MAX_PAYLOAD 64

void sendPacket(uint8_t dst, uint8_t src, uint8_t ttl, const uint8_t* data, uint8_t len) {
    LoRa.beginPacket();
    LoRa.write(dst);
    LoRa.write(src);
    LoRa.write(ttl);
    LoRa.write(len);
    LoRa.write(data, len);
    LoRa.endPacket();
}

bool receivePacket(uint8_t* dst, uint8_t* src, uint8_t* ttl, uint8_t* buf, uint8_t* len) {
    int packetSize = LoRa.parsePacket();
    if (!packetSize) return false;

    *dst = LoRa.read();
    *src = LoRa.read();
    *ttl = LoRa.read();
    *len = LoRa.read();

    for (int i = 0; i < *len && LoRa.available(); i++) {
        buf[i] = LoRa.read();
    }

    return true;
}

void setup() {
    Serial.begin(115200);
    LoRa.setPins(CS_PIN, RST_PIN, DIO0_PIN);
    LoRa.begin(915E6);
    Serial.println("MASTER listo");
}

void loop() {
    static uint32_t counter = 0;

    char msg[32];
    sprintf(msg, "Msg %lu", counter++);

    Serial.print("Enviando: ");
    Serial.println(msg);

    sendPacket(
        0x03,        // destino final
        NODE_ID,
        5,           // TTL (máx saltos)
        (uint8_t*)msg,
        strlen(msg)
    );

    delay(3000);
}