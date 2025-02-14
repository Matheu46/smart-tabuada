#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <string.h>
#include "inc/ssd1306.h"
#include "hardware/i2c.h"

#include "pico/cyw43_arch.h"
#include "hardware/adc.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "lwip/init.h"

#include <time.h>

// Config para a conexão do wifi
#define WIFI_SSID "brisa-2253009" // Nome da rede Wi-Fi
#define WIFI_PASS "wr4cmofe"      // Senha da rede Wi-Fi

// Config para o Thingspeak
#define THINGSPEAK_HOST "api.thingspeak.com"
#define THINGSPEAK_PORT 80

#define API_KEY "74PC5A1Y0IQNNQEU" // API Key do ThingSpeak

struct tcp_pcb *tcp_client_pcb;
ip_addr_t server_ip;


// Callback quando recebe resposta do ThingSpeak
static err_t http_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        tcp_close(tpcb);
        return ERR_OK;
    }
    printf("Resposta do ThingSpeak: %.*s\n", p->len, (char *)p->payload);
    pbuf_free(p);
    return ERR_OK;
}

// Callback quando a conexão TCP é estabelecida
static err_t http_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err) {
    if (err != ERR_OK) {
        printf("Erro na conexão TCP\n");
        return err;
    }

    printf("Conectado ao ThingSpeak!\n");

    float temperature = 32.5;
    char request[256];
    snprintf(request, sizeof(request),
        "GET /update?api_key=%s&field1=%.2f HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "\r\n",
        API_KEY, temperature, THINGSPEAK_HOST);

    tcp_write(tpcb, request, strlen(request), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
    tcp_recv(tpcb, http_recv_callback);

    return ERR_OK;
}

// Resolver DNS e conectar ao servidor
static void dns_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    if (ipaddr) {
        printf("Endereço IP do ThingSpeak: %s\n", ipaddr_ntoa(ipaddr));
        tcp_client_pcb = tcp_new();
        tcp_connect(tcp_client_pcb, ipaddr, THINGSPEAK_PORT, http_connected_callback);
    } else {
        printf("Falha na resolução de DNS\n");
    }
}


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


void gerar_pergunta(int *num1, int *num2, int *opcao_correta, int opcoes[3]) {
    *num1 = (rand() % 10) + 1;
    *num2 = (rand() % 10) + 1;
    int resultado = (*num1) * (*num2);

    *opcao_correta = rand() % 3;

    for (int i = 0; i < 3; i++) {
        if (i == *opcao_correta) {
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

    // display_message("Tabuada", "Carregando...");

    // conexão WIFI
    if (cyw43_arch_init()) {
        printf("Falha ao iniciar Wi-Fi\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n");

    if (cyw43_arch_wifi_connect_blocking(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_MIXED_PSK)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        return 1;
    }

    printf("Wi-Fi conectado!\n");
    display_message("Wi-fi....", "conectado!");

    sleep_ms(2000);

    srand(time(NULL));

    while (true) {
        int num1, num2, opcao_correta, opcoes[3];
        gerar_pergunta(&num1, &num2, &opcao_correta, opcoes);

        char question[16], options[16];
        snprintf(question, sizeof(question), "%d x %d =", num1, num2);
        snprintf(options, sizeof(options), "%d %d %d", opcoes[0], opcoes[1], opcoes[2]);

        display_message(question, options);

        sleep_ms(3000);

        dns_gethostbyname(THINGSPEAK_HOST, &server_ip, dns_callback, NULL);
        display_message("olha no...", "ThingSpeak!!");
        sleep_ms(15000);

        // zera o display inteiro
        uint8_t ssd[ssd1306_buffer_length];
        memset(ssd, 0, ssd1306_buffer_length);
        render_on_display(ssd, &frame_area);
    }

    return 0;
}