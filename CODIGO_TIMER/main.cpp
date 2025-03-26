#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <freertos/semphr.h>

#define BUTTON_PIN 0  /*Botón de BOOT del ESP32*/
#define LED_PIN 2     /*LED interno (DIODE2)*/

TaskHandle_t taskHandle = NULL;  // Manejador de la tarea
TimerHandle_t ledTimer;  // Timer de FreeRTOS para controlar el LED
volatile uint32_t pressDuration = 0;  // Almacena el tiempo de pulsación

// Variables para medir el tiempo de pulsación
volatile uint32_t pressStartTime = 0;
volatile bool buttonPressed = false;

// ISR para manejar la interrupción del botón
void IRAM_ATTR buttonISR() {
    if (digitalRead(BUTTON_PIN) == LOW) {
        pressStartTime = millis();  // Inicia el conteo
        buttonPressed = true;
    } else if (buttonPressed) {
        pressDuration = millis() - pressStartTime;  // Calcula duración
        buttonPressed = false;

        // Notificar a la tarea que se ha completado la pulsación del botón
        xTaskNotifyFromISR(taskHandle, pressDuration, eSetValueWithOverwrite, NULL);
    }
}

// Callback del Timer: Parpadea el LED por el mismo tiempo que se presionó el botón
void ledTimerCallback(TimerHandle_t xTimer) {
    static uint32_t elapsedTime = 0;

    if (elapsedTime < pressDuration) {
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));  // Alterna el LED
        elapsedTime += 200;  // Cada cambio de estado es 200 ms
        xTimerChangePeriod(ledTimer, pdMS_TO_TICKS(200), 0);
    } else {
        digitalWrite(LED_PIN, LOW);  // Apaga el LED al finalizar
        elapsedTime = 0;
        xTimerStop(ledTimer, 0);
    }
}

// Tarea que maneja la notificación del tiempo de pulsación
void taskHandler(void *pvParameters) {
    uint32_t duration = 0;
    while (true) {
        // Espera a la notificación desde la ISR
        if (xTaskNotifyWait(0, 0, &duration, portMAX_DELAY) == pdTRUE) {
            pressDuration = duration;  // Actualiza el tiempo de la pulsación

            // Inicia el Timer para controlar el LED
            xTimerStart(ledTimer, 0);
        }
    }
}

void setup() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);  // Configurar pull-up interno
    pinMode(LED_PIN, OUTPUT);

    // Crear el Timer para manejar el parpadeo del LED
    ledTimer = xTimerCreate("LedTimer", pdMS_TO_TICKS(200), pdFALSE, NULL, ledTimerCallback);

    // Crear la tarea que maneja la notificación
    xTaskCreate(taskHandler, "TaskHandler", 2048, NULL, 1, &taskHandle);

    // Adjuntar la interrupción al botón
    attachInterrupt(BUTTON_PIN, buttonISR, CHANGE);
}

void loop() {
    // No se necesita código aquí porque todo se maneja con ISR y FreeRTOS
}
