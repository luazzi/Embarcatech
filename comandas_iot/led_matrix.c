#include "led_matrix.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2818b.pio.h"

// Variáveis para uso da máquina PIO.
PIO np_pio;
uint sm;

// Buffer de pixels
pixel_t leds[LED_COUNT];

/**
* Inicializa a máquina PIO para controle da matriz de LEDs.
*/
void npInit(uint pin) {
    // Cria programa PIO.
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;

    // Toma posse de uma máquina PIO.
    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0) {
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, panic!
    }

    // Inicia programa na máquina PIO obtida.
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);

    // Limpa buffer de pixels.
    npClear();
}

/**
* Atribui uma cor RGB a um LED.
*/
void npSetLED(uint index, uint8_t color) {
    switch (color) {
        case 0: // Verde
            leds[index].R = 0;
            leds[index].G = 120;
            leds[index].B = 0;
            break;
        case 1: // Amarelo
            leds[index].R = 120;
            leds[index].G = 120;
            leds[index].B = 0;
            break;
        case 2: // Vermelho
            leds[index].R = 120;
            leds[index].G = 0;
            leds[index].B = 0;
            break;
        case 3:
            leds[index].R = 0;
            leds[index].G = 0;
            leds[index].B = 0;
            break;
    }
}

/**
* Limpa o buffer de pixels.
*/
void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i) {
        leds[i].R = 0;
        leds[i].G = 0;
        leds[i].B = 0;
    }
}

/**
* Escreve os dados do buffer nos LEDs.
*/
void npWrite() {
    // Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO.
    for (uint i = 0; i < LED_COUNT; ++i) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
}

/**
* Exibe um número de 0 a 10 na matriz de LEDs.
*/
void displayColor(int number, uint8_t color) {
    npClear(); // Limpa a matriz antes de exibir o número

    // Definição dos padrões para cada número
    const uint8_t patterns[1][25] = {

        // Preenche a matriz
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    };

    // Aplica o padrão correspondente ao número
    for (uint i = 0; i < LED_COUNT; ++i) {
        if (patterns[number][i]) {
            npSetLED(i, color); // Define a cor escolhida
        }
    }

    npWrite(); // Escreve os dados nos LEDs
}
