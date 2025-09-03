/**
 * @file CalibrationManager.cpp
 * @brief Implementación de la clase CalibrationManager para la calibración del sensor de CO2.
 * @details Este archivo contiene la lógica de la máquina de estados que gestiona
 * el proceso de calibración manual a 400 ppm del sensor MH-Z19C.
 * @author Francisco Aguirre
 * @date 2025-08-28
 */

#include "CalibrationManager.h"
#include <Arduino.h> // Necesario para millis(), Serial, etc.

/**
 * @brief Constructor de la clase CalibrationManager.
 * @details Inicializa la máquina de estados en IDLE y resetea los
 * temporizadores de estado y de registro de mensajes.
 */
CalibrationManager::CalibrationManager()
{
    currentState = IDLE;
    stateStartTime = 0;
}

/**
 * @brief Inicializa el gestor de calibración.
 * @details Configura el pin HD (utilizado para la calibración) como una salida
 * y lo establece en un nivel ALTO para asegurar que el proceso de
 * calibración no se active accidentalmente al iniciar el sistema.
 */
void CalibrationManager::init()
{
    pinMode(HD_PIN, OUTPUT);
    digitalWrite(HD_PIN, HIGH);
    Serial.println("Calibration Manager inicializado.");
}

/**
 * @brief Inicia el proceso de calibración del sensor.
 * @details Si el sistema no está ya en un proceso de calibración (es decir,
 * está en estado IDLE), cambia al estado STABILIZING y comienza
 * la cuenta regresiva de 20 minutos para la estabilización del sensor.
 */
void CalibrationManager::startCalibration()
{
    if (currentState == IDLE)
    {
        Serial.println("Comando de calibración recibido. Iniciando fase de estabilización (20 min)...");
        currentState = PULSING;
        stateStartTime = millis();
        digitalWrite(HD_PIN, LOW);
    }
}

/**
 * @brief Verifica si un proceso de calibración está actualmente en curso.
 * @return bool `true` si el estado actual no es IDLE, `false` en caso contrario.
 */
bool CalibrationManager::isCalibrating()
{
    return currentState != IDLE;
}

/**
 * @brief Ejecuta la lógica de la máquina de estados de calibración.
 * @details Esta función debe ser llamada repetidamente en el bucle principal del programa.
 * Gestiona las transiciones entre los estados IDLE, STABILIZING y PULSING.
 */
void CalibrationManager::run()
{
    switch (currentState)
    {
    /**
     * @brief Estado de reposo. El sistema no está realizando ninguna acción de calibración.
     */
    case IDLE:
        // No se realiza ninguna acción.
        break;

    /**
     * @brief Estado de pulso de calibración. El pin HD se mantiene en BAJO
     * durante 7 segundos para indicarle al sensor que calibre su punto cero.
     */
    case PULSING:
    {
        // Comprueba si han transcurrido los 7 segundos del pulso.
        if (millis() - stateStartTime >= PULSE_TIME_MS)
        {
            // Termina el pulso devolviendo el pin HD a un estado ALTO.
            digitalWrite(HD_PIN, HIGH);
            Serial.println("Sensor calibrado manualmente a 400 ppm.");
            // Vuelve al estado de reposo.
            currentState = IDLE;
        }
        break;
    }
    }
}
