#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "led_matrix.h"
#include "hardware/watchdog.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "inc/ssd1306.h"
#include "hardware/i2c.h"
#include "inc/ssd1306_font.h"

// Definições de constantes e configurações
#define WIFI_SSID "WIFI_NAME"               // Nome da rede Wi-Fi
#define WIFI_PASSWORD "WIFI_PASSWORD"       // Senha da rede Wi-Fi
#define TEST_TCP_SERVER_IP "IP_SERVER"      // Endereço IP do servidor TCP
#define TCP_PORT 10                         // Porta TCP para conexão (e número da comanda)
#define DEBUG_printf printf                 // Macro para facilitar o debug
#define BUF_SIZE 2048                       // Tamanho do buffer para recebimento de dados
#define STOP_BUTTON 5                       // Pino do botão de parada
#define RECONNECT_BUTTON 6                  // Pino do botão de reconexão
#define BUZZER_PIN 21                       // Pino do buzzer
#define BUZZER_FREQUENCY 100                // Frequência do buzzer em Hz
#define I2C_SDA 14                          // Pino SDA para I2C (OLED)
#define I2C_SCL 15                          // Pino SCL para I2C (OLED)

// Estrutura para armazenar o estado do cliente TCP
typedef struct TCP_CLIENT_T_ {
    struct tcp_pcb *tcp_pcb;                // Estrutura de controle do protocolo TCP
    ip_addr_t remote_addr;                  // Endereço IP do servidor remoto
    uint8_t buffer[BUF_SIZE];               // Buffer para armazenar dados recebidos
    int buffer_len;                         // Tamanho dos dados no buffer
    bool complete;                          // Flag para indicar se a transmissão foi concluída
    bool connected;                         // Flag para indicar se está conectado ao servidor
} TCP_CLIENT_T;

// Variáveis globais para controle de estado
volatile bool reconnect_flag = false;       // Flag para reconexão ao servidor
volatile bool stop_flag = false;            // Flag para parar a notificação de pedido pronto

// Função para inicializar um botão
void init_button(uint pin) {
    gpio_init(pin);                         // Inicializa o pino do botão
    gpio_set_dir(pin, GPIO_IN);             // Configura o pino como entrada
    gpio_pull_up(pin);                      // Habilita o resistor de pull-up
}

// Função para inicializar o PWM no pino do buzzer
void pwm_init_buzzer(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);  // Configura o pino para função PWM
    uint slice_num = pwm_gpio_to_slice_num(pin); // Obtém o slice do PWM associado ao pino
    pwm_config config = pwm_get_default_config(); // Obtém a configuração padrão do PWM
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (BUZZER_FREQUENCY * 4096)); // Configura o divisor de clock
    pwm_init(slice_num, &config, true);     // Inicializa o PWM com a configuração
    pwm_set_gpio_level(pin, 0);             // Define o nível inicial do PWM como 0 (desligado)
}

// Função para inicializar o barramento I2C
void init_i2c(uint pin) {
    i2c_init(i2c1, ssd1306_i2c_clock * 1000); // Inicializa o I2C com a frequência especificada
    gpio_set_function(pin, GPIO_FUNC_I2C);  // Configura o pino para função I2C
    gpio_pull_up(pin);                      // Habilita o resistor de pull-up
}

// Função para exibir texto no display OLED
void display_text_on_oled(const char *text[], uint8_t num_lines) {
    // Define a área de renderização no display
    struct render_area frame_area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1,
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1
    };
    calculate_render_area_buffer_length(&frame_area); // Calcula o tamanho do buffer necessário

    // Limpa o display
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);

    // Escreve cada linha de texto no display
    int y = 0;
    for (uint8_t i = 0; i < num_lines; i++) {
        ssd1306_draw_string(ssd, 5, y, text[i]); // Desenha a string no buffer
        y += 8;  // Avança para a próxima linha
    }

    // Renderiza o conteúdo no display
    render_on_display(ssd, &frame_area);
}

// Função para emitir um beep com duração especificada
void beep(uint pin, uint duration_ms) {
    pwm_set_gpio_level(pin, 2048);          // Define o duty cycle para 50% (som ativo)
    displayColor(0, 2);                     // Altera a cor da matriz de LEDs
    busy_wait_ms(duration_ms);              // Mantém o som ativo pelo tempo especificado
    pwm_set_gpio_level(pin, 0);             // Desliga o som
    displayColor(0, 3);                     // Restaura a cor da matriz de LEDs
    busy_wait_ms(100);                      // Pausa entre os beeps
}

// Função de callback para interrupções dos botões
void button_callback(uint gpio, uint32_t events) {
    static absolute_time_t last_press_time_stop = 0;
    static absolute_time_t last_press_time_reconnect = 0;

    // Debounce de 200 ms para evitar leituras múltiplas
    if (gpio == STOP_BUTTON && absolute_time_diff_us(last_press_time_stop, get_absolute_time()) > 200000) {
        last_press_time_stop = get_absolute_time();
        stop_flag = true;                    // Ativa a flag de parada
        DEBUG_printf("Botão STOP pressionado.\n");
    } else if (gpio == RECONNECT_BUTTON && absolute_time_diff_us(last_press_time_reconnect, get_absolute_time()) > 200000) {
        last_press_time_reconnect = get_absolute_time();
        reconnect_flag = true;               // Ativa a flag de reconexão
        DEBUG_printf("Botão RECONNECT pressionado.\n");
    }
}

// Função para controlar notificações com base no comando recebido
void notification_control(const char *command) {
    if (strcmp(command, "FINALIZAR") == 0) {
        // Altera a cor da matriz de LEDs para amarelo
        displayColor(0, 1);
        const char *message[] = {
            "     ",
            "     ",
            "     ",
            "   Em preparo  "
        };
        display_text_on_oled(message, 4);    // Exibe mensagem no OLED
    } else if (strcmp(command, "PEDIDO_PRONTO") == 0) {
        const char *message[] = {
            "     ",
            "     ",
            "     ",
            " Pedido pronto!  "
        };
        display_text_on_oled(message, 4);    // Exibe mensagem no OLED
        stop_flag = false;
        while (!stop_flag) {
            beep(BUZZER_PIN, 500);          // Emite beeps até o botão de parada ser pressionado
        }
        pwm_set_gpio_level(BUZZER_PIN, 0);  // Desliga o buzzer
        stop_flag = false;                  // Reseta a flag de parada
        watchdog_reboot(0, 0, 0);          // Reinicia a placa
    } else if (strcmp(command, "FECHAR_COMANDA") == 0) {
        watchdog_reboot(0, 0, 0);          // Reinicia a placa
    } else {
        DEBUG_printf("Comando desconhecido: %s\n", command); // Log de comando desconhecido
    }
}

// Função para fechar a conexão TCP
static err_t tcp_client_close(void *arg) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    err_t err = ERR_OK;
    if (state->tcp_pcb != NULL) {
        tcp_arg(state->tcp_pcb, NULL);
        tcp_sent(state->tcp_pcb, NULL);
        tcp_recv(state->tcp_pcb, NULL);
        tcp_err(state->tcp_pcb, NULL);
        err = tcp_close(state->tcp_pcb);
        if (err != ERR_OK) {
            DEBUG_printf("close failed %d, calling abort\n", err);
            tcp_abort(state->tcp_pcb);
            err = ERR_ABRT;
        }
        state->tcp_pcb = NULL;
    }
    state->connected = false;
    return err;
}

// Callback para receber dados do servidor TCP
err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    TCP_CLIENT_T *state = (TCP_CLIENT_T*)arg;
    if (!p) {
        return tcp_client_close(arg);        // Fecha a conexão se não houver dados
    }
    cyw43_arch_lwip_check();
    if (p->tot_len > 0) {
        state->buffer_len = p->tot_len;
        memcpy(state->buffer, p->payload, p->tot_len); // Copia os dados recebidos para o buffer
        state->buffer[state->buffer_len] = '\0';       // Adiciona terminador de string
        notification_control((const char *)state->buffer); // Processa o comando recebido
        tcp_recved(tpcb, p->tot_len);       // Confirma o recebimento dos dados
    }
    pbuf_free(p);                           // Libera o buffer de recebimento
    return ERR_OK;
}

// Função para tentar conectar ao servidor TCP
static bool tcp_client_connect(TCP_CLIENT_T *state) {
    DEBUG_printf("Conectando ao servidor %s:%d...\n", ip4addr_ntoa(&state->remote_addr), TCP_PORT);
    cyw43_arch_lwip_begin();
    err_t err = tcp_connect(state->tcp_pcb, &state->remote_addr, TCP_PORT, NULL);
    cyw43_arch_lwip_end();
    if (err != ERR_OK) {
        DEBUG_printf("Falha na conexão. Aguardando novo comando...\n");
        return false;
    }
    absolute_time_t start_time = get_absolute_time();
    while (absolute_time_diff_us(start_time, get_absolute_time()) < 5000000) { // Espera até 5 segundos
        if (state->buffer_len > 0 && strcmp((char *)state->buffer, "CONNECTED") == 0) {
            state->connected = true;
            DEBUG_printf("Conectado ao servidor e confirmado.\n");
            return true;
        }
        sleep_ms(100);                      // Aguarda antes de verificar novamente
    }
    DEBUG_printf("Não recebeu confirmação. Tentando novamente...\n");
    return false;
}

// Função principal
int main() {
    stdio_init_all();                       // Inicializa a stdio para debug
    npInit(LED_PIN);                        // Inicializa a matriz de LEDs
    displayColor(0, 3);                     // Define a cor inicial da matriz de LEDs
    pwm_init_buzzer(BUZZER_PIN);            // Inicializa o buzzer
    init_button(STOP_BUTTON);               // Inicializa o botão de parada
    init_button(RECONNECT_BUTTON);          // Inicializa o botão de reconexão
    init_i2c(I2C_SDA);                      // Inicializa o I2C para o OLED
    init_i2c(I2C_SCL);
    ssd1306_init();                         // Inicializa o display OLED

    // Configura interrupções para os botões
    gpio_set_irq_enabled_with_callback(STOP_BUTTON, GPIO_IRQ_EDGE_FALL, true, &button_callback);
    gpio_set_irq_enabled_with_callback(RECONNECT_BUTTON, GPIO_IRQ_EDGE_FALL, true, &button_callback);

    // Aloca memória para o estado do cliente TCP
    TCP_CLIENT_T *state = calloc(1, sizeof(TCP_CLIENT_T));
    if (!state) {
        DEBUG_printf("failed to allocate state\n");
        return 1;
    }
    ip4addr_aton(TEST_TCP_SERVER_IP, &state->remote_addr); // Converte o endereço IP para o formato interno

    // Exibe uma mensagem inicial no OLED
    const char *message[] = {
        "     ",
        "     ",
        "     ",
        "      10  "
    };
    display_text_on_oled(message, 4);

    // Loop principal
    while (true) {
        sleep_ms(1000);                     // Aguarda 1 segundo
        if (reconnect_flag) {               // Verifica se a flag de reconexão foi ativada
            reconnect_flag = false;
            displayColor(0, 0);             // Altera a cor da matriz de LEDs para verde

            // Inicializa o Wi-Fi
            if (cyw43_arch_init()) {
                displayColor(0, 3); // Apaga a matriz de LEDs
                DEBUG_printf("failed to initialise\n");
                continue;
            }
            cyw43_arch_enable_sta_mode();   // Habilita o modo station (cliente Wi-Fi)

            // Conecta ao Wi-Fi
            if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
                displayColor(0, 3); 
                DEBUG_printf("failed to connect to Wi-Fi\n");
                cyw43_arch_deinit();
                continue;
            }

            // Tenta conectar ao servidor TCP
            state->tcp_pcb = tcp_new_ip_type(IP_GET_TYPE(&state->remote_addr));
            if (!state->tcp_pcb) {
                displayColor(0, 3); 
                DEBUG_printf("failed to create pcb\n");
                cyw43_arch_deinit();
                continue;
            }
            tcp_arg(state->tcp_pcb, state);
            tcp_recv(state->tcp_pcb, tcp_client_recv);

            if (tcp_client_connect(state)) { // Tenta conectar ao servidor
                while (state->connected) {   // Mantém a conexão ativa
                    sleep_ms(1000);
                }
            }
            tcp_client_close(state);        // Fecha a conexão
            displayColor(0, 3); 
            cyw43_arch_deinit();           // Desativa o Wi-Fi
        }
    }
    return 0;
}