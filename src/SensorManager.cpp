/**
 * @file SensorManager.cpp
 * @brief Implementación de la clase SensorManager para la gestión de sensores.
 * @details Este archivo contiene la lógica para inicializar, leer y gestionar
 * los sensores de CO2 (MH-Z19C), temperatura/humedad (DHT22) y
 * presión (BMP280). También incluye el control del ventilador.
 * @author Francisco Aguirre
 * @date 2023-10-27
 */

#include "SensorManager.h"
#include <Arduino.h>
#include <Wire.h>

// --- Prototipos de Funciones Estáticas ---
static byte calculateChecksum(byte *packet);

/**
 * @brief Constructor de la clase SensorManager.
 * @details Inicializa los objetos de los sensores y establece los valores
 * por defecto para las variables de estado.
 */
SensorManager::SensorManager() : dht(DHT_PIN, DHT22) {
    bmp_initialized = false;
    last_bmp_reconnect_attempt = 0;
    preheat_start_time = 0;
    state = PREHEATING;
    fan_state = false;
}

/**
 * @brief Inicializa todos los sensores y componentes gestionados por esta clase.
 * @details Configura la comunicación UART para el MH-Z19C, desactiva su
 * autocalibración, inicia el DHT22, y intenta conectar con el BMP280.
 * También configura el pin del ventilador y lo deja apagado.
 */
void SensorManager::init() {
    Serial.println("Inicializando SensorManager...");

    // --- Inicialización del sensor de CO2 (MH-Z19C) ---
    // Inicia comunicación UART en el puerto Serial2.
    Serial2.begin(9600, SERIAL_8N1, RXD2_PIN, TXD2_PIN);
    
    // Configura el pin HD para la calibración manual y lo pone en ALTO (inactivo).
    pinMode(HD_PIN, OUTPUT);
    digitalWrite(HD_PIN, HIGH);
    
    // Envía el comando para desactivar la calibración automática del sensor.
    Serial.println("Desactivando autocalibración del sensor de CO2...");
    byte cmd_disable_autocal[9] = {0xFF, 0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    cmd_disable_autocal[8] = calculateChecksum(cmd_disable_autocal);
    Serial2.write(cmd_disable_autocal, 9);
    
    // Inicia el temporizador de precalentamiento.
    preheat_start_time = millis();
    Serial.println("Iniciado precalentamiento de 1 minuto para el sensor de CO2.");
    
    // Configura el pin del ventilador y lo mantiene apagado al inicio.
    pinMode(FAN_PIN, OUTPUT);
    digitalWrite(FAN_PIN, LOW);
    fan_state = false;
    
    // --- Inicialización del sensor de Temp/Hum (DHT22) ---
    dht.begin();

    // --- Inicialización del sensor de Presión (BMP280) ---
    if (bmp.begin(0x76)) {
        Serial.println(F("Sensor BMP280 encontrado e inicializado."));
        // Configura los parámetros de muestreo y filtrado para el BMP280.
        bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                        Adafruit_BMP280::SAMPLING_X2,
                        Adafruit_BMP280::SAMPLING_X16,
                        Adafruit_BMP280::FILTER_X16,
                        Adafruit_BMP280::STANDBY_MS_500);
        bmp_initialized = true;
    } else {
        Serial.println(F("ADVERTENCIA: No se pudo encontrar un sensor BMP280 válido. Se reintentará periódicamente."));
        Serial.print("Sensor ID: 0x");
        Serial.println(bmp.sensorID(), HEX);
        bmp_initialized = false;
    }
    
    Serial.println("Sensores inicializados.");
}

/**
 * @brief Lee los valores de todos los sensores.
 * @details Realiza la lectura de temperatura, humedad, presión y CO2.
 * Incluye manejo de errores para el DHT22 y una lógica de
 * reconexión para el BMP280 si la comunicación falla.
 * @return SensorData Una estructura con los últimos valores leídos de los sensores.
 */
SensorData SensorManager::readAllSensors() {
    SensorData currentData;

    // --- Lectura de Temperatura y Humedad (DHT22) ---
    currentData.humidity = dht.readHumidity();
    currentData.temperature = dht.readTemperature();
    // Comprueba si la lectura del DHT22 fue exitosa.
    if (isnan(currentData.humidity) || isnan(currentData.temperature)) {
        Serial.println(F("Error al leer del sensor DHT!"));
        currentData.humidity = -1.0;
        currentData.temperature = -1.0;
    }

    // --- Lectura de Presión (BMP280) con lógica de reconexión ---
    if (bmp_initialized) {
        // Si el sensor está conectado, lee la presión.
        currentData.pressure = bmp.readPressure() / 100.0F; // Convierte de Pa a hPa
    } else {
        // Si no está conectado, asigna un valor de error.
        currentData.pressure = -1.0; 
        
        // Intenta reconectar cada 5 segundos para no bloquear el sistema.
        if (millis() - last_bmp_reconnect_attempt > 5000) {
            last_bmp_reconnect_attempt = millis();
            Serial.println("Intentando reconectar con el sensor BMP280...");
            if (bmp.begin(0x76)) {
                Serial.println("¡BMP280 reconectado exitosamente!");
                // Si la reconexión es exitosa, se reaplica la configuración.
                bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                                Adafruit_BMP280::SAMPLING_X2,
                                Adafruit_BMP280::SAMPLING_X16,
                                Adafruit_BMP280::FILTER_X16,
                                Adafruit_BMP280::STANDBY_MS_500);
                bmp_initialized = true;
            }
        }
    }

    // --- Lectura de CO2 (MH-Z19C) ---
    currentData.co2 = readCO2();

    return currentData;
}

/**
 * @brief Lee la concentración de CO2 del sensor MH-Z19C.
 * @details Envía el comando de lectura por UART y procesa la respuesta.
 * También gestiona la máquina de estados de precalentamiento del sensor.
 * @return int La concentración de CO2 en ppm, o -1 si ocurre un error.
 * @note Esta es una función privada de ayuda.
 */
int SensorManager::readCO2() {
    const unsigned long PREHEAT_TIME_MS = 60 * 1000UL; // 1 minuto

    // Comprueba si el tiempo de precalentamiento ha finalizado.
    if (state == PREHEATING && millis() - preheat_start_time > PREHEAT_TIME_MS) {
        setFanState(true); // Enciende el ventilador
        Serial.println("Precalentamiento del sensor de CO2 completado. El sensor está listo (READY).");
        state = READY;
    }
    
    // Comando para solicitar la lectura de CO2.
    byte cmd[9] = {0xFF, 0x01, 0x86, 0, 0, 0, 0, 0, 0x79};
    Serial2.write(cmd, 9);
    
    // Espera la respuesta del sensor con un timeout.
    unsigned long startTime = millis();
    while (Serial2.available() < 9) {
        if (millis() - startTime > 150) { // Timeout de 150ms
            Serial.println("Timeout esperando respuesta del sensor de CO2.");
            return -1;
        }
    }

    byte response[9];
    Serial2.readBytes(response, 9);

    // Valida y procesa la respuesta.
    if (response[0] == 0xFF && response[1] == 0x86) {
        // El valor de CO2 se forma con 2 bytes (High y Low).
        return (response[2] << 8) | response[3];
    } else {
        Serial.println("Respuesta inválida del sensor de CO2.");
        return -1;
    }
}

/**
 * @brief Controla el estado del ventilador.
 * @details Activa o desactiva el pin del ventilador y actualiza la variable de estado.
 * @param on `true` para encender el ventilador, `false` para apagarlo.
 */
void SensorManager::setFanState(bool on) {
    if (on) {
        digitalWrite(FAN_PIN, HIGH);
        fan_state = true;
    } else {
        digitalWrite(FAN_PIN, LOW);
        fan_state = false;
    }
    Serial.printf("Ventilador %s.\n", on ? "activado" : "desactivado");
}

/**
 * @brief Obtiene el estado actual del ventilador.
 * @return bool `true` si el ventilador está encendido, `false` si está apagado.
 */
bool SensorManager::getFanState() {
    return fan_state;
}

/**
 * @brief Obtiene el estado actual del sensor de CO2.
 * @return SensorState El estado actual (`PREHEATING`, `READY`, `CALIBRATING`).
 */
SensorState SensorManager::getState() {
    return state;
}

/**
 * @brief Calcula el checksum para un comando del sensor MH-Z19C.
 * @details Esta función es específica del protocolo del sensor MH-Z19C.
 * Suma los bytes del 1 al 7, resta el resultado de 0xFF y suma 1.
 * @param packet Puntero a un array de 8 bytes (los primeros 8 bytes del comando).
 * @return byte El byte de checksum calculado.
 * @note Esta es una función estática, privada al archivo.
 */
static byte calculateChecksum(byte *packet) {
    byte checksum = 0;
    for (int i = 1; i < 8; i++) {
        checksum += packet[i];
    }
    checksum = 0xFF - checksum;
    checksum += 1;
    return checksum;
}
