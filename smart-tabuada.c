// diminuir range da resposta errada (5 para mais e 5 para menos)
// Melhorar a função de display oled (ta feio)
// Acender algum led para ficar mais legal

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

// Config para a conexão do wifi
#define WIFI_SSID "brisa-2253009" // Nome
#define WIFI_PASS "wr4cmofe"      // Senha

// Config para o Thingspeak
#define THINGSPEAK_HOST "api.thingspeak.com"
#define THINGSPEAK_PORT 80
#define API_KEY "74PC5A1Y0IQNNQEU" // API Key do ThingSpeak

// Configuração do pino do buzzer
#define BUZZER_PIN 21
#define BUZZER_FREQUENCY 100

// GPIO's do botão A e B
#define BUTTON_A 5 
#define BUTTON_B 6

// I2C defines - display oled
#define I2C_PORT i2c0
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

struct tcp_pcb *tcp_client_pcb;
ip_addr_t server_ip;

// Estrutura para armazenar os dados que vão para o thingspeak
typedef struct {
    int acertos;
    int tempo_resposta;
} request_data_t;

// Variável para controle do envio ao ThingSpeak
static bool enviando_thingspeak = false;

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

static void init_buttons() {
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);
}

// Função para inicializar o OLED
void init_oled() {
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_init();
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

    ssd1306_draw_string(ssd, 5, 20, line1);
    ssd1306_draw_string(ssd, 5, 36, line2);

    render_on_display(ssd, &frame_area);
}

void LimparDisplay(uint8_t *ssd, struct render_area *frame_area) {
  memset(ssd, 0, ssd1306_buffer_length);
  render_on_display(ssd, frame_area);
}


// Callback quando recebe resposta do ThingSpeak
static err_t http_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        // request_data_t *dados = (request_data_t *)arg;
        // if (dados) free(dados);
        tcp_close(tpcb);
        enviando_thingspeak = false; // Libera o envio
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
        enviando_thingspeak = false; // Libera o envio
        return err;
    }

    printf("Conectado ao ThingSpeak!\n");

    // Recupera os dados passados como argumento
    request_data_t *dados = (request_data_t *)arg;
    int acertos = dados->acertos;
    int tempo_resposta = dados->tempo_resposta;

    char request[256];
    snprintf(request, sizeof(request),
        "GET /update?api_key=%s&field1=%d&field2=%d HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "\r\n",
        API_KEY, acertos, tempo_resposta, THINGSPEAK_HOST);

    tcp_write(tpcb, request, strlen(request), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
    tcp_recv(tpcb, http_recv_callback);

    return ERR_OK;
}

// Resolver DNS e conectar ao servidor
static void dns_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    if (ipaddr) {
        struct tcp_pcb *tcp_client_pcb = tcp_new();
        request_data_t *dados = (request_data_t *)callback_arg;
        printf("Endereço IP do ThingSpeak: %s\n", ipaddr_ntoa(ipaddr));

        tcp_arg(tcp_client_pcb, dados);
        tcp_connect(tcp_client_pcb, ipaddr, THINGSPEAK_PORT, http_connected_callback);
    } else {
        request_data_t *dados = (request_data_t *)callback_arg;
        free(dados);
        printf("Falha na resolução de DNS\n");
        enviando_thingspeak = false; // Libera o envio
    }
}


// Função para enviar dados ao ThingSpeak
void send_to_thingspeak(int acertos, int tempo_resposta) {
    if (enviando_thingspeak) return; // Evita múltiplos envios simultâneos

    // Prepara os dados para enviar como argumento
    static int dados[2];
    dados[0] = acertos;
    dados[1] = tempo_resposta; 

    // Resolve o DNS e inicia a conexão
    enviando_thingspeak = true;
    dns_gethostbyname(THINGSPEAK_HOST, &server_ip, dns_callback, &dados);
}


void gerar_pergunta(int *num1, int *num2, int *opcao_correta, int opcoes[2]) {
    // Gera número de 2 a 10
    *num1 = (rand() % 9) + 2; 
    *num2 = (rand() % 9) + 2;
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
    LimparDisplay(ssd, &frame_area);

    init_buttons();

    // conexão WIFI
    display_message("Wi-fi....", "conectando!");

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


    srand(time(NULL));

    while (true) {
        
        bool iniciar = false;
        while(!iniciar) {
            display_message("Aperte A", "Para iniciar");
            if (!gpio_get(BUTTON_A)) { // A for pressionado
                iniciar = true;
            }
        }

        LimparDisplay(ssd, &frame_area);
        sleep_ms(300); // não dar conflito com a pergunta

        int acertos = 0;
        uint32_t inicio_tempo = time_us_32();  // Tempo inicial p/ calcular tempo de resposta
        for(int i = 0; i < 5; i++) { // uma sequência de 5 questões
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
                if (!gpio_get(BUTTON_A)) { // A for pressionado
                    escolha = 0;
                    resposta = true;
                }
                if (!gpio_get(BUTTON_B)) { // B for pressionado
                    escolha = 1;
                    resposta = true;
                }
                sleep_ms(100);
            }

            // Verifica se a resposta está correta
            if (escolha == opcao_correta) {
                display_message("Correto!", "");
                beep(BUZZER_PIN, 300); // Bipe curto para resposta certa
                acertos++;
            } else {
                display_message("Errado!", "");
                beep(BUZZER_PIN, 600); // Bipe longo para resposta errada
            }

            sleep_ms(1000);
        }
        uint32_t fim_tempo = time_us_32();  // Marca o tempo final
        int tempo_resposta = (fim_tempo - inicio_tempo) / 1000000;

        send_to_thingspeak(acertos, tempo_resposta);
        sleep_ms(1000);  

        // Verifica se o envio foi concluído
        if (!enviando_thingspeak) {
            printf("Envio concluído. Pronto para o próximo ciclo.\n");
        } else {
            printf("Envio em andamento...\n");
        }

        char valor1[16], valor2[16];
        snprintf(valor1, sizeof(valor1), "%d acertos", acertos);
        snprintf(valor2, sizeof(valor2), "%d segs", tempo_resposta);

        display_message(valor1, valor2);
        sleep_ms(8000);
    }

    return 0;
}