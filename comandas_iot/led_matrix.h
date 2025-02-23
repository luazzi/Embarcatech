#ifndef LED_MATRIX_H
#define LED_MATRIX_H

#include <stdint.h>
#include "pico/stdlib.h"

#define LED_COUNT 25
#define LED_PIN 7

// Definição de pixel GRB
typedef struct {
    uint8_t G, R, B; // Três valores de 8-bits compõem um pixel.
} pixel_t;

// Declaração do buffer de pixels que formam a matriz.
extern pixel_t leds[LED_COUNT];

// Funções para controle da matriz de LEDs
void npInit(uint pin);
void npSetLED(uint index, uint8_t color);
void npClear();
void npWrite();

// Funções para exibir números de 0 a 10
void displayColor(int number, uint8_t color);

#endif // LED_MATRIX_H