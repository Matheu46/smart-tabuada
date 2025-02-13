#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <string.h>
#include "inc/ssd1306.h"
#include "hardware/i2c.h"

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

// Função para inicializar o OLED
void init_oled() {
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_init();

    // Preparar área de renderização para o display (ssd1306_width pixels por ssd1306_n_pages páginas)
    struct render_area frame_area = {
        start_column : 0,
        end_column : ssd1306_width - 1,
        start_page : 0,
        end_page : ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&frame_area);

    // zera o display inteiro
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);
}

// Função para exibir mensagem no OLED
void display_message(char *line1, char *line2) {
    struct render_area frame_area = {
        start_column : 0,
        end_column : ssd1306_width - 1,
        start_page : 0,
        end_page : ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&frame_area);

    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);

    ssd1306_draw_string(ssd, 5, 0, line1);
    ssd1306_draw_string(ssd, 5, 8, line2);

    render_on_display(ssd, &frame_area);
}

void gerar_pergunta() {
    int opcoes[3];
    int num1 = (rand() % 10) + 1;
    int num2 = (rand() % 10) + 1;
    int resultado = (num1) * (num2);

    int opcao_correta = rand() % 3;

    for (int i = 0; i < 3; i++) {
        if (i == opcao_correta) {
            opcoes[i] = resultado;
        } else {
            int opcao_errada;
            do {
                opcao_errada = (rand() % 100) + 1;
            } while (opcao_errada == resultado); //Para opção errada não ser igual a certa 
            opcoes[i] = opcao_errada;
        }
    }

}

int main()
{
    stdio_init_all();

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    // For more examples of I2C use see https://github.com/raspberrypi/pico-examples/tree/master/i2c

    init_oled();

    display_message("Iniciando", "Wi-Fi...");

    while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}