#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

/**
 * @struct SensorData
 * @brief Una estructura simple para contener todas las lecturas de los sensores.
 */
struct SensorData {
    float temperature;
    float humidity;
    float pressure;
    int co2;
};

#endif // SENSOR_MANAGER_H

// (Asegurarse de que esté entre el #define y el #endif)
#include <Adafruit_BMP280.h>
#include <DHT.h>

/**
 * @class SensorManager
 * @brief Gestiona la inicialización y lectura de todos los sensores del PCB1.
 */
class SensorManager {
public:
    SensorManager(); // Constructor
    void init();
    SensorData readAllSensors();

private:
    // --- Métodos Privados ---
    int readCO2(); // Función de ayuda interna para leer el sensor de CO2

    // --- Pines y Definiciones ---
    static const int DHT_PIN = 25;
    static const int HD_PIN = 12;
    static const int RXD2_PIN = 16;
    static const int TXD2_PIN = 17;

    // --- Objetos de Sensores ---
    DHT dht;
    Adafruit_BMP280 bmp;

    // --- Variables de estado ---
    bool bmp_initialized; // Flag para saber si el BMP280 está funcionando
    unsigned long last_bmp_reconnect_attempt; // Temporizador para reintentos
};