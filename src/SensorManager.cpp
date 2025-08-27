#include "SensorManager.h"
#include <Arduino.h>
#include <Wire.h>

// Prototipo de la función
static byte calculateChecksum(byte *packet);

// Constructor: Inicializa los objetos de los sensores con sus pines
SensorManager::SensorManager() : dht(DHT_PIN, DHT22) {
    bmp_initialized = false;
    last_bmp_reconnect_attempt = 0;
    preheat_start_time = 0;
}

void SensorManager::init() {
    Serial.println("Inicializando SensorManager...");

    // Inicializar sensor de CO2 (MH-Z19C)
    Serial2.begin(9600, SERIAL_8N1, RXD2_PIN, TXD2_PIN);
    pinMode(HD_PIN, OUTPUT);
    digitalWrite(HD_PIN, HIGH); // Asegurarse de que la calibración manual no esté activa
    Serial.println("Desactivando autocalibración del sensor de CO2...");
    byte cmd_disable_autocal[9] = {0xFF, 0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    cmd_disable_autocal[8] = calculateChecksum(cmd_disable_autocal);
    Serial2.write(cmd_disable_autocal, 9);
    preheat_start_time = millis();
    Serial.println("Iniciado precalentamiento de 1 minuto para el sensor de CO2.");
    pinMode(FAN_PIN, OUTPUT);
    digitalWrite(FAN_PIN, LOW); // Asegurarse de que el ventilador esté apagado al inicio
    
    // Inicializar sensor de Temp/Hum (DHT22)
    dht.begin();

    // Intentamos iniciar el sensor de Presión (BMP280)
    if (bmp.begin(0x76)) {
        Serial.println(F("Sensor BMP280 encontrado e inicializado."));
        bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                        Adafruit_BMP280::SAMPLING_X2,
                        Adafruit_BMP280::SAMPLING_X16,
                        Adafruit_BMP280::FILTER_X16,
                        Adafruit_BMP280::STANDBY_MS_500);
        bmp_initialized = true; // Marcamos como inicializado
    } else {
        // Si falla, solo mostramos una advertencia y continuamos
        Serial.println(F("ADVERTENCIA: No se pudo encontrar un sensor BMP280 válido. Se reintentará periódicamente."));
        Serial.println("Sensor ID: ");
        Serial.println(bmp.sensorID(),HEX);
        bmp_initialized = false; // Marcamos como NO inicializado
    }
    
    Serial.println("Sensores inicializados.");
}

SensorData SensorManager::readAllSensors() {
    SensorData currentData;

    // Leer Temperatura y Humedad
    currentData.humidity = dht.readHumidity();
    currentData.temperature = dht.readTemperature();
    // Chequeo de errores para el DHT22
    if (isnan(currentData.humidity) || isnan(currentData.temperature)) {
        Serial.println(F("Error al leer del sensor DHT!"));
        currentData.humidity = -1.0;
        currentData.temperature = -1.0;
    }

    // Lectura de presión y reconexión
    if (bmp_initialized) {
        // Si está funcionando, leemos la presión normalmente
        currentData.pressure = bmp.readPressure() / 100.0F;
    } else {
        // Si NO está funcionando, le asignamos un valor de error
        currentData.pressure = -1.0; 
        
        // Intentamos reconectar cada 5 segundos
        if (millis() - last_bmp_reconnect_attempt > 5000) {
            last_bmp_reconnect_attempt = millis();
            Serial.println("Intentando reconectar con el sensor BMP280...");
            Serial.println(bmp.sensorID(),16);
            // Volvemos a llamar a init() del sensor
            if (bmp.begin(0x76)) {
                Serial.println("¡BMP280 reconectado exitosamente!");
                // Si reconecta, aplicamos la configuración de nuevo
                bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                                Adafruit_BMP280::SAMPLING_X2,
                                Adafruit_BMP280::SAMPLING_X16,
                                Adafruit_BMP280::FILTER_X16,
                                Adafruit_BMP280::STANDBY_MS_500);
                bmp_initialized = true; // Actualizamos el flag
            }
        }
    }

    // Leer CO2
    currentData.co2 = readCO2();

    return currentData;
}

int SensorManager::readCO2() {

    const unsigned long PREHEAT_TIME_MS = 60 * 1000UL; // 1 minuto
    if (millis() - preheat_start_time > PREHEAT_TIME_MS) {
        digitalWrite(FAN_PIN, HIGH); // Encendemos el ventilador
        Serial.println("Ventilador activado.\n");
        Serial.println("Precalentamiento del sensor de CO2 completado. El sensor está listo (READY).\n");
    }
    
    byte cmd[9] = {0xFF, 0x01, 0x86, 0, 0, 0, 0, 0, 0x79};
    Serial2.write(cmd, 9);
    
    // Esperar un poco para la respuesta
    unsigned long startTime = millis();
    while (Serial2.available() < 9) {
        if (millis() - startTime > 150) { // Timeout de 150ms
            Serial.println("Timeout esperando respuesta del sensor de CO2.");
            return -1;
        }
    }

    byte response[9];
    Serial2.readBytes(response, 9);

    if (response[0] == 0xFF && response[1] == 0x86) {
        // Cálculo del valor de CO2
        return (response[2] << 8) | response[3];
    } else {
        Serial.println("Respuesta inválida del sensor de CO2.");
        return -1;
    }
}


/**
 * @brief Calcula el checksum para un comando del sensor MH-Z19C.
 * @param packet Puntero a un array de 8 bytes (los primeros 8 bytes del comando).
 * @return El byte de checksum calculado.
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