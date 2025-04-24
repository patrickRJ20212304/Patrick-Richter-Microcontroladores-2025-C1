#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "driver/gpio.h"

#define WIFI_SSID "ESP32_HP"
#define WIFI_PASS "12345678"
#define LED_ASTABLE_PIN GPIO_NUM_13    // Pin del LED para modo astable
#define LED_MONOESTABLE_PIN GPIO_NUM_2 // Pin del LED para modo monoastable
#define BUTTON_PIN GPIO_NUM_0          // Pin del botón para activar el modo monoestable

static const char *TAG = "webserver";

// Estados globales
static bool astable_activado = false;
static bool monoestable_activado = false;
static float frecuencia_astable = 0;
static float tiempo_monoestable = 0;
static int resistencia_pwm = 1000; // Resistencia inicial en Ω

QueueHandle_t monoestable_queue;

// Página HTML
const char* html_page = "<!DOCTYPE html><html lang=\"es\">"
"<head><meta charset=\"UTF-8\" /><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"/>"
"<title>Simulador NE555</title><style>"
"body { font-family: Arial; background-color:rgb(0, 26, 255); text-align: center; padding: 30px; }"
".container { background-color: white; padding: 30px; border-radius: 12px; display: inline-block; box-shadow: 0 0 10px rgba(0,0,0,0.1); }"
"input, button { padding: 10px; margin: 5px; font-size: 16px; border-radius: 5px; }"
"button { background-color:rgb(9, 119, 20); color: white; border: none; }"
"button:hover { background-color: #218838; cursor: pointer; }"
".cancel-button { background-color: red; color: white; }"
".cancel-button:hover { background-color: darkred; cursor: pointer; }"
"</style></head><body><div class=\"container\">"
"<h1>Simulador NE555</h1>"

"<h2>Modo Astable</h2>"
"<label>R1 (Ω): <input type=\"number\" id=\"R1\"></label><br>"
"<label>R2 (Ω): <input type=\"number\" id=\"R2\"></label><br>"
"<label>C1 (F): <input type=\"number\" id=\"C1\" step=\"any\"></label><br>"
"<button onclick=\"calcularAstable()\">Calcular Astable</button>"
"<div id=\"resultadoAstable\"></div>"
"<div id=\"tiemposAstable\"></div>"

"<h2>Modo Monoestable</h2>"
"<label>R (Ω): <input type=\"number\" id=\"R_mono\"></label><br>"
"<label>C (F): <input type=\"number\" id=\"C_mono\" step=\"any\"></label><br>"
"<button onclick=\"calcularMonoestable()\">Calcular Monoestable</button>"
"<div id=\"resultadoMono\"></div>"

"<h2>Resistencia Variable (PWM)</h2>"
"<label>Resistencia (Ω): <input type=\"range\" id=\"resistencia\" min=\"1\" max=\"10000\" step=\"1\" value=\"1000\" oninput=\"mostrarResistencia(this.value)\">"
"<span id=\"valorResistencia\">1000</span> Ω</label><br>"
"<button onclick=\"enviarResistencia()\">Aplicar Resistencia</button>"

"<hr><button class=\"cancel-button\" onclick=\"cancelarOperaciones()\">Cancelar</button>"

"<script>"
"function calcularAstable() {"
"  const R1 = parseFloat(document.getElementById('R1').value);"
"  const R2 = parseFloat(document.getElementById('R2').value);"
"  const C1 = parseFloat(document.getElementById('C1').value);"
"  if (R1 > 0 && R2 > 0 && C1 > 0) {"
"    const T_high = 0.693 * (R1 + R2) * C1;"
"    const T_low = 0.693 * R2 * C1;"
"    const T_total = T_high + T_low;"
"    const f = 1 / T_total;"
"    document.getElementById('resultadoAstable').textContent = `Frecuencia: ${f.toFixed(2)} Hz`;"
"    document.getElementById('tiemposAstable').innerHTML = `T_high: ${T_high.toFixed(4)} s, T_low: ${T_low.toFixed(4)} s`;"
"    fetch('/setfreq', { method: 'POST', headers: { 'Content-Type': 'text/plain' }, body: f.toFixed(2) });"
"  } else {"
"    document.getElementById('resultadoAstable').textContent = 'Por favor, ingrese valores válidos para R1, R2 y C1';"
"    document.getElementById('tiemposAstable').textContent = '';"
"  }"
"}"
"function calcularMonoestable() {"
"  const R = parseFloat(document.getElementById('R_mono').value);"
"  const C = parseFloat(document.getElementById('C_mono').value);"
"  if (R > 0 && C > 0) {"
"    const T = 1.1 * R * C;"
"    document.getElementById('resultadoMono').textContent = `Pulso: ${T.toFixed(2)} s`;"
"    fetch('/setmono', { method: 'POST', headers: { 'Content-Type': 'text/plain' }, body: T.toFixed(2) });"
"  }"
"}"
"function mostrarResistencia(valor) {"
"  document.getElementById('valorResistencia').textContent = valor;"
"}"
"function enviarResistencia() {"
"  const resistencia = document.getElementById('resistencia').value;"
"  fetch('/setresistencia', { method: 'POST', headers: { 'Content-Type': 'text/plain' }, body: resistencia });"
"}"
"function cancelarOperaciones() {"
"  fetch('/cancel', { method: 'POST' });"
"}"
"</script></div></body></html>";

// Handlers HTTP
esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t set_freq_handler(httpd_req_t *req) {
    char buf[32];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = '\0';
    frecuencia_astable = atof(buf);
    astable_activado = true;
    monoestable_activado = false;
    ESP_LOGI(TAG, "Modo Astable activado - %.2f Hz", frecuencia_astable);
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t set_mono_handler(httpd_req_t *req) {
    char buf[32];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = '\0';
    tiempo_monoestable = atof(buf);
    monoestable_activado = true;
    astable_activado = false;
    ESP_LOGI(TAG, "Modo Monoestable activado - %.2f s", tiempo_monoestable);
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t set_resistencia_handler(httpd_req_t *req) {
    char buf[32];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = '\0';
    resistencia_pwm = atoi(buf); // Convertir el valor recibido a entero
    ESP_LOGI(TAG, "Resistencia PWM configurada: %d Ω", resistencia_pwm);
    httpd_resp_send(req, "Resistencia configurada", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t cancel_handler(httpd_req_t *req) {
    astable_activado = false;
    monoestable_activado = false; // Detener el modo monoestable
    ESP_LOGI(TAG, "Operaciones canceladas");
    httpd_resp_send(req, "Operaciones canceladas", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Registrar rutas
void register_endpoints(httpd_handle_t server) {
    httpd_register_uri_handler(server, &(httpd_uri_t){
        .uri = "/", .method = HTTP_GET, .handler = root_get_handler });
    httpd_register_uri_handler(server, &(httpd_uri_t){
        .uri = "/setfreq", .method = HTTP_POST, .handler = set_freq_handler });
    httpd_register_uri_handler(server, &(httpd_uri_t){
        .uri = "/setmono", .method = HTTP_POST, .handler = set_mono_handler });
    httpd_register_uri_handler(server, &(httpd_uri_t){
        .uri = "/setresistencia", .method = HTTP_POST, .handler = set_resistencia_handler });
    httpd_register_uri_handler(server, &(httpd_uri_t){
        .uri = "/cancel", .method = HTTP_POST, .handler = cancel_handler });
}

httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) register_endpoints(server);
    return server;
}

// Tarea ASTABLE
void led_astable_task(void *param) {
    gpio_reset_pin(LED_ASTABLE_PIN);
    gpio_set_direction(LED_ASTABLE_PIN, GPIO_MODE_OUTPUT);

    while (1) {
        if (astable_activado && frecuencia_astable > 0) {
            int delay = (int)(1000 / (2 * frecuencia_astable)) * resistencia_pwm / 1000; // Ajustar con resistencia
            gpio_set_level(LED_ASTABLE_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(delay));
            gpio_set_level(LED_ASTABLE_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(delay));
        } else {
            gpio_set_level(LED_ASTABLE_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// Tarea MONOESTABLE
void led_monoestable_task(void *param) {
    gpio_reset_pin(LED_MONOESTABLE_PIN);
    gpio_set_direction(LED_MONOESTABLE_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_MONOESTABLE_PIN, 0);

    int signal = 0;
    while (1) {
        if (xQueueReceive(monoestable_queue, &signal, portMAX_DELAY) == pdTRUE) {
            if (monoestable_activado && tiempo_monoestable > 0) {
                ESP_LOGI(TAG, "Generando pulso monoestable de %.2f segundos", tiempo_monoestable);
                gpio_set_level(LED_MONOESTABLE_PIN, 1);

                // Ajustar duración del pulso con resistencia
                int adjusted_duration = (int)(tiempo_monoestable * 1000) * resistencia_pwm / 1000;
                vTaskDelay(pdMS_TO_TICKS(adjusted_duration));

                gpio_set_level(LED_MONOESTABLE_PIN, 0); // Apagar el LED
                monoestable_activado = false; // Desactivar el modo monoestable
            }
        }
    }
}

// ISR para botón físico (modo monoestable)
static void IRAM_ATTR button_isr_handler(void* arg) {
    int signal = 1;
    xQueueSendFromISR(monoestable_queue, &signal, NULL);
}

void setup_button_interrupt() {
    gpio_config_t btn_cfg = {
        .pin_bit_mask = 1ULL << BUTTON_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&btn_cfg);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL);
}

// Iniciar WiFi
void wifi_init(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_cfg);
    esp_wifi_set_mode(WIFI_MODE_AP);

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .password = WIFI_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };

    if (strlen(WIFI_PASS) == 0) ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();
    ESP_LOGI(TAG, "Hotspot iniciado - SSID: %s", WIFI_SSID);
}

// app_main
void app_main(void) {
    nvs_flash_init();
    wifi_init();
    start_webserver();

    monoestable_queue = xQueueCreate(5, sizeof(int));
    setup_button_interrupt();

    xTaskCreate(led_astable_task, "LED Astable", 2048, NULL, 5, NULL);
    xTaskCreate(led_monoestable_task, "LED Monoestable", 2048, NULL, 5, NULL);
}
