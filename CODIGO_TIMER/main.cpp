#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

#define BUTTON_PIN 0  // Botón BOOT del ESP32
#define LED_PIN 2     // LED interno

TaskHandle_t taskHandle = NULL;
TimerHandle_t ledTimer;
volatile uint32_t pressDuration = 0;  // Se inicia en 0 (inactivo)
volatile uint32_t pressStartTime = 0;
volatile bool buttonPressed = false;
volatile bool ledActive = false;  // Indica si el sistema está activo

// ISR para medir la duración del botón
void IRAM_ATTR buttonISR() {
    if (digitalRead(BUTTON_PIN) == LOW) {
        pressStartTime = millis();
        buttonPressed = true;
    } else if (buttonPressed) {
        uint32_t duration = millis() - pressStartTime;
        if (duration < 100) duration = 100;  // Valor mínimo razonable
        buttonPressed = false;

        xTaskNotifyFromISR(taskHandle, duration, eSetValueWithOverwrite, NULL);
    }
}

// Callback del timer: alterna el LED
void ledTimerCallback(TimerHandle_t xTimer) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
}

// Tarea que maneja la activación y el cambio de frecuencia
void taskHandler(void *pvParameters) {
    uint32_t duration = 0;

    while (true) {
        if (xTaskNotifyWait(0, 0, &duration, portMAX_DELAY) == pdTRUE) {
            pressDuration = duration;
            ledActive = true;

            // Si el timer ya está corriendo, cambiar el periodo
            if (xTimerIsTimerActive(ledTimer)) {
                xTimerChangePeriod(ledTimer, pdMS_TO_TICKS(pressDuration), 0);
            } else {
                xTimerChangePeriod(ledTimer, pdMS_TO_TICKS(pressDuration), 0);
                xTimerStart(ledTimer, 0);
            }
        }
    }
}

void setup() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);  // LED apagado al inicio

    // Crear el timer pero no iniciarlo aún
    ledTimer = xTimerCreate("LedTimer", pdMS_TO_TICKS(1000), pdTRUE, NULL, ledTimerCallback);

    // Crear la tarea que maneja la lógica
    xTaskCreate(taskHandler, "TaskHandler", 2048, NULL, 1, &taskHandle);

    // Configurar la interrupción del botón
    attachInterrupt(BUTTON_PIN, buttonISR, CHANGE);
}

void loop() {
    
}
