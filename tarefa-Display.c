#include <stdlib.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "inc/font.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"
#include "hardware/timer.h"

// Declaração de constantes
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define NUM_NUMBERS 11
#define NUM_PIXELS 25
#define WS2812_PIN 7
#define IS_RGBW 0
#define LED_PIN_BLUE 12
#define LED_PIN_GREEN 11
#define PIN_BUTTON_A 5
#define PIN_BUTTON_B 6

// Delcaração de variaveis
static volatile uint32_t LAST_TIME_A = 0;
static volatile uint32_t LAST_TIME_B = 0;
static volatile bool button_a_pressed = false;
static volatile bool button_b_pressed = false;
int current_number = 0;
int count_a = 0;
int count_b = 0;

// Declaração das funções
static inline void put_pixel(uint32_t pixel_grb);
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b);
void show_number_leds(int number);
void gpio_irq_handler(uint gpio, uint32_t events);
void initialize_pin_led();

// Matriz para matriz de leds
const bool led_buffer[NUM_NUMBERS][NUM_PIXELS] = {
    {0,1,1,1,0, 1,0,0,0,1, 1,0,0,0,1, 1,0,0,0,1, 0,1,1,1,0}, // 0
    {0,0,1,0,0, 0,0,1,0,0, 0,0,1,0,1, 0,1,1,0,0, 0,0,1,0,0}, // 1
    {1,1,1,1,1, 1,0,0,0,0, 1,1,1,1,1, 0,0,0,0,1, 1,1,1,1,1}, // 2
    {1,1,1,1,1, 0,0,0,0,1, 0,1,1,1,1, 0,0,0,0,1, 1,1,1,1,1}, // 3
    {1,0,0,0,0, 0,0,0,0,1, 1,1,1,1,1, 1,0,0,0,1, 1,0,0,0,1}, // 4
    {1,1,1,1,1, 0,0,0,0,1, 1,1,1,1,1, 1,0,0,0,0, 1,1,1,1,1}, // 5
    {1,1,1,1,1, 1,0,0,0,1, 1,1,1,1,1, 1,0,0,0,0, 1,1,1,1,1}, // 6
    {0,0,0,0,1, 0,1,0,0,0, 0,0,1,0,0, 0,0,0,1,0, 1,1,1,1,1}, // 7
    {1,1,1,1,1, 1,0,0,0,1, 0,1,1,1,0, 1,0,0,0,1, 1,1,1,1,1}, // 8
    {1,1,1,1,1, 0,0,0,0,1, 1,1,1,1,1, 1,0,0,0,1, 1,1,1,1,1}  // 9
};

int main() {
    // inicialização da matriz de leds, display e led RGB
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);
    stdio_init_all();
    i2c_init(I2C_PORT, 400 * 1000);
    initialize_pin_led();
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    // apaga todo o display
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Configuração dos botões A e B
    gpio_init(PIN_BUTTON_A);
    gpio_set_dir(PIN_BUTTON_A, GPIO_IN);
    gpio_pull_up(PIN_BUTTON_A);
    gpio_set_irq_enabled_with_callback(PIN_BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler); // Interrupção para o botão A

    gpio_init(PIN_BUTTON_B);
    gpio_set_dir(PIN_BUTTON_B, GPIO_IN);
    gpio_pull_up(PIN_BUTTON_B);
    gpio_set_irq_enabled_with_callback(PIN_BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler); // Interrupção para o botão B

    bool cor = true;

    show_number_leds(NUM_NUMBERS - 1); // Apaga a matriz de leds

    while (true) {
        if(stdio_usb_connected()) {
            char letter;
            int c = getchar_timeout_us(1000);

            // Lê entrada do monitor serial
            if(c != PICO_ERROR_TIMEOUT) { 
                letter = (char)c;

                if(!(letter >= '0' && letter <= '9') && !(letter >= 'A' && letter <= 'Z') && !(letter >= 'a' && letter <= 'z')) {
                    continue;
                }

                printf("Caractere recebido: %c\n", letter);

                cor = !cor;

                // Em caso de numero, o mostra na matriz de leds
                if(letter >= '0' && letter <= '9') {
                    printf("%d\n", letter - '0');
                    show_number_leds(letter - '0');
                } else {
                    show_number_leds(NUM_NUMBERS - 1); // Apaga a matriz de leds
                }

                // Imprime a entrada do monitor serial no display
                ssd1306_fill(&ssd, !cor);
                ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor);
                ssd1306_draw_char(&ssd, letter, 40, 30);
                ssd1306_send_data(&ssd);

                sleep_ms(1000);
            } else if(button_a_pressed) { // Verifica se o botão A foi apertado
                button_a_pressed = false;
                cor = !cor;

                show_number_leds(NUM_NUMBERS - 1); // Apaga a matriz de leds

                if(count_a % 2 != 0) { // Se o botão A for apertado acende o led
                    ssd1306_fill(&ssd, !cor);
                    ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor);
                    ssd1306_draw_string(&ssd, "Led verde", 40, 30);
                    ssd1306_draw_string(&ssd, "On", 40, 40);
                    ssd1306_send_data(&ssd);
                    gpio_put(LED_PIN_GREEN, true);
                    printf("Led verde On.\n");
                } else { // Se o botão A foi apertado e o led já estava ligado ele sera desligado
                    ssd1306_fill(&ssd, !cor);
                    ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor);
                    ssd1306_draw_string(&ssd, "Led verde", 40, 30);
                    ssd1306_draw_string(&ssd, "Off", 40, 40);
                    ssd1306_send_data(&ssd);
                    gpio_put(LED_PIN_GREEN, false);
                    printf("Led verde Off.\n");
                }

                sleep_ms(1000);
            } else if(button_b_pressed) { // Verifica se o botão A foi apertado
                button_b_pressed = false;
                cor = !cor;

                show_number_leds(NUM_NUMBERS - 1); // Apaga a matriz de leds

                if(count_b % 2 != 0) { // Se o botão B for apertado acende o led
                    ssd1306_fill(&ssd, !cor);
                    ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor);
                    ssd1306_draw_string(&ssd, "Led azul", 40, 30);
                    ssd1306_draw_string(&ssd, "On", 40, 40);
                    ssd1306_send_data(&ssd);
                    gpio_put(LED_PIN_BLUE, true);
                    printf("Led azul On.\n");
                } else { // Se o botão B foi apertado e o led já estava ligado ele sera desligado
                    ssd1306_fill(&ssd, !cor);
                    ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor);
                    ssd1306_draw_string(&ssd, "Led azul", 40, 30);
                    ssd1306_draw_string(&ssd, "Off", 40, 40);
                    ssd1306_send_data(&ssd);
                    gpio_put(LED_PIN_BLUE, false);
                    printf("Led azul Off.\n");
                }

                sleep_ms(1000);
            }
        }

        sleep_ms(10);
    }

    return 0;
} 

// Função para inicializar os pinos dos leds verde e azul
void initialize_pin_led() {
    gpio_init(LED_PIN_GREEN);
    gpio_set_dir(LED_PIN_GREEN, GPIO_OUT);

    gpio_init(LED_PIN_BLUE);
    gpio_set_dir(LED_PIN_BLUE, GPIO_OUT);
} 

// Funções para configurar os leds da matriz de leds
static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

// Função para desenhar um numero na matriz de leds
void show_number_leds(int number) {
    int i;

    for (i = 0; i < NUM_PIXELS; i++) {
        put_pixel(led_buffer[number][i] ? urgb_u32(0, 0, 10) : 0);
    }
    sleep_ms(10);
}

// Função para trabalhar a interrupção dos botões A e B
void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_us_since_boot(get_absolute_time());

    if (gpio == PIN_BUTTON_A && current_time - LAST_TIME_A > 200000) {
        LAST_TIME_A = current_time;
        button_a_pressed = true;
        count_a++;
    } else if(gpio == PIN_BUTTON_B && current_time - LAST_TIME_B > 200000) {
        LAST_TIME_B = current_time;
        button_b_pressed = true;
        count_b++;
    }
}
