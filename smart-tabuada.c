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

#include "hardware/pwm.h"
#include "hardware/clocks.h"

// Configuração do pino do buzzer
#define BUZZER_PIN 21

// Configuração da frequência do buzzer (em Hz)
#define BUZZER_FREQUENCY 100


// Config para a conexão do wifi
#define WIFI_SSID "d" // Nome da rede Wi-Fi
#define WIFI_PASS "10102020"      // Senha da rede Wi-Fi

// Config para o Thingspeak
#define THINGSPEAK_HOST "api.thingspeak.com"
#define THINGSPEAK_PORT 80

#define API_KEY "74PC5A1Y0IQNNQEU" // API Key do ThingSpeak

struct tcp_pcb *tcp_client_pcb;
ip_addr_t server_ip;


// Definição de uma função para inicializar o PWM no pino do buzzer
void pwm_init_buzzer(uint pin) {
    // Configurar o pino como saída de PWM
    gpio_set_function(pin, GPIO_FUNC_PWM);

    // Obter o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configurar o PWM com frequência desejada
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (BUZZER_FREQUENCY * 4096)); // Divisor de clock
    pwm_init(slice_num, &config, true);

    // Iniciar o PWM no nível baixo
    pwm_set_gpio_level(pin, 0);
}

// Definição de uma função para emitir um beep com duração especificada
void beep(uint pin, uint duration_ms) {
    // Obter o slice do PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configurar o duty cycle para 50% (ativo)
    pwm_set_gpio_level(pin, 2048);

    // Temporização
    sleep_ms(duration_ms);

    // Desativar o sinal PWM (duty cycle 0)
    pwm_set_gpio_level(pin, 0);

    // Pausa entre os beeps
    sleep_ms(100); // Pausa de 100ms
}




#define BUTTON_A 5    // GPIO conectado ao Botão A
#define BUTTON_B 6    // GPIO conectado ao Botão B

static void init_buttons() {
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);
}

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


// I2C defines - display oled
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


void gerar_pergunta(int *num1, int *num2, int *opcao_correta, int opcoes[2]) {
    *num1 = (rand() % 10) + 1;
    *num2 = (rand() % 10) + 1;
    int resultado = (*num1) * (*num2);

    *opcao_correta = rand() % 2;

    for (int i = 0; i < 2; i++) {
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

    pwm_init_buzzer(BUZZER_PIN);

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

    init_buttons();

    // conexão WIFI
    // if (cyw43_arch_init()) {
    //     printf("Falha ao iniciar Wi-Fi\n");
    //     return 1;
    // }

    // cyw43_arch_enable_sta_mode();
    // printf("Conectando ao Wi-Fi...\n");

    // if (cyw43_arch_wifi_connect_blocking(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_MIXED_PSK)) {
    //     printf("Falha ao conectar ao Wi-Fi\n");
    //     return 1;
    // }

    // printf("Wi-Fi conectado!\n");
    // display_message("Wi-fi....", "conectado!");

    // sleep_ms(2000);

    srand(time(NULL));

    while (true) {
        // checar botões para para a escolha da resposta
        bool button_a_state = gpio_get(BUTTON_A);
        bool button_b_state = gpio_get(BUTTON_B);

        int num1, num2, opcao_correta, opcoes[2];
        gerar_pergunta(&num1, &num2, &opcao_correta, opcoes);

        char question[16], options[16];
        snprintf(question, sizeof(question), "%d x %d =", num1, num2);
        snprintf(options, sizeof(options), "A %d  |  B %d", opcoes[0], opcoes[1]);

        display_message(question, options);

        // Aguarda escolha do usuário
        bool resposta = false;
        int escolha = -1;

        while(!resposta) {
            if (!gpio_get(BUTTON_A)) { // Se o botão A for pressionado
                escolha = 0;
                resposta = true;
            }
            if (!gpio_get(BUTTON_B)) { // Se o botão B for pressionado
                escolha = 1;
                resposta = true;
            }
            sleep_ms(100);
        }

        // Verifica se a resposta está correta
        if (escolha == opcao_correta) {
            display_message("Correto!", "");
            beep(BUZZER_PIN, 300); // Bipe curto para resposta certa
        } else {
            display_message("Errado!", "");
            beep(BUZZER_PIN, 1000); // Bipe longo para resposta errada
        }

        // beep(BUZZER_PIN, 500); // Bipe de 500ms

        sleep_ms(2000);

        // dns_gethostbyname(THINGSPEAK_HOST, &server_ip, dns_callback, NULL);
        // display_message("olha no...", "ThingSpeak!!");
        // sleep_ms(15000);

        // zera o display inteiro
        uint8_t ssd[ssd1306_buffer_length];
        memset(ssd, 0, ssd1306_buffer_length);
        render_on_display(ssd, &frame_area);
    }

    return 0;
}