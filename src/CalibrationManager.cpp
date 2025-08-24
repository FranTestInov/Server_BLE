#include "CalibrationManager.h"
#include <Arduino.h> // Necesario para millis(), Serial, etc.

CalibrationManager::CalibrationManager() {
    // Inicializamos las variables de estado en el constructor
    currentState = IDLE;
    stateStartTime = 0;
    lastLogTime = 0;
}

void CalibrationManager::init() {
    // Configuramos el pin de calibración como salida
    pinMode(HD_PIN, OUTPUT);
    // Nos aseguramos de que el pin esté en ALTO (estado inactivo) al iniciar
    digitalWrite(HD_PIN, HIGH);
    Serial.println("Calibration Manager inicializado.");
}

void CalibrationManager::startCalibration() {
    // Solo podemos iniciar una calibración si no hay otra en curso
    if (currentState == IDLE) {
        Serial.println("Comando de calibración recibido. Iniciando fase de estabilización (20 min)...");
        // Cambiamos al estado de estabilización y reiniciamos el temporizador
        currentState = STABILIZING;
        stateStartTime = millis();
        lastLogTime = stateStartTime;
    }
}

bool CalibrationManager::isCalibrating() {
    // El sistema está ocupado si no está en IDLE
    return currentState != IDLE;
}

void CalibrationManager::run() {
    // La máquina de estados se ejecuta aquí, llamada desde el loop() principal
    switch (currentState) {
        case IDLE:
            // No hacer nada
            break;

        case STABILIZING: {
            unsigned long now = millis();
            unsigned long elapsed = now - stateStartTime;

            // Informar el tiempo restante cada 10 segundos
            if (now - lastLogTime >= 10000) {
                Serial.print("Estabilizando... quedan ");
                Serial.print((STABILIZATION_TIME_MS - elapsed) / 1000);
                Serial.println(" segundos.");
                lastLogTime = now;
            }
            
            // Comprobar si el tiempo de estabilización ha terminado
            if (elapsed >= STABILIZATION_TIME_MS) {
                Serial.println("Estabilización completada. Iniciando pulso de calibración (7 seg)...");
                // Cambiamos al siguiente estado y reiniciamos el temporizador
                currentState = PULSING;
                stateStartTime = millis();
                // Ponemos el pin en BAJO para iniciar el pulso
                digitalWrite(HD_PIN, LOW);
            }
            break;
        }

        case PULSING: {
            // Comprobar si el pulso de 7 segundos ha terminado
            if (millis() - stateStartTime >= PULSE_TIME_MS) {
                // Terminamos el pulso poniendo el pin en ALTO
                digitalWrite(HD_PIN, HIGH);
                Serial.println("Sensor calibrado manualmente a 400 ppm.");
                // Volvemos al estado de reposo
                currentState = IDLE;
            }
            break;
        }
    }
}