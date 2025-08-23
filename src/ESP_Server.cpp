/**
 * @file ESP_Server.cpp
 * @brief Archivo principal para el PCB1 (nodo sensor).
 * @details Este archivo contiene las funciones setup() y loop() principales.
 * Su única responsabilidad es inicializar y coordinar los diferentes
 * módulos del sistema (BLE, Sensores, Calibración).
 */
#include <Arduino.h>
#include "BLEManager.h"
#include "SensorManager.h"
#include "CalibrationManager.h"


// --- OBJETOS GLOBALES DE LOS MÓDULOS ---
// Creamos una instancia para cada manager que controlará una parte del sistema.
BLEManager bleManager;
// SensorManager sensorManager;
// CalibrationManager calibrationManager;

/**
 * @brief Configuración inicial del microcontrolador.
 * @details Inicializa la comunicación serial y todos los módulos principales.
 */
void setup() {
    Serial.begin(115200); // Usamos una velocidad más alta para depuración
    while (!Serial); // Espera a que el puerto serial se conecte

    // Inicializamos cada uno de nuestros managers
    bleManager.init();
    // sensorManager.init();
    // calibrationManager.init();
    
    Serial.println("Sistema inicializado y listo.");
}

// Variables para controlar el tiempo de envío de datos
unsigned long lastUpdateTime = 0;
const int UPDATE_INTERVAL_MS = 500; // Intervalo de 500ms = 2 datos por segundo

/**
 * @brief Bucle principal del programa.
 * @details Se ejecuta repetidamente. Su función es leer los sensores
 * y actualizar el servidor BLE a una frecuencia fija.
 */
void loop() {
    // --- Lógica de temporización para no bloquear el procesador ---
    if (millis() - lastUpdateTime >= UPDATE_INTERVAL_MS) {
        lastUpdateTime = millis(); // Actualizamos el tiempo del último envío

        // --- 1. GENERACIÓN DE DATOS ALEATORIOS ---
        // Generamos números dentro de rangos realistas para cada sensor.
        // Para los floats, generamos un entero y lo dividimos para obtener decimales.
        float temp = random(2000, 2500) / 100.0; // Rango: 20.00 - 24.99 °C
        float hum = random(4000, 6000) / 100.0;  // Rango: 40.00 - 59.99 %
        float pres = random(100000, 102000) / 100.0; // Rango: 1000.00 - 1019.99 hPa
        int co2 = random(400, 2500);             // Rango: 400 - 2499 ppm

        // Mostramos en la consola los valores que estamos a punto de enviar
        Serial.printf("Enviando -> Temp: %.2f C, Hum: %.2f %%, Pres: %.2f hPa, CO2: %d ppm\n",
                      temp, hum, pres, co2);

        // --- Actualización del Servidor BLE ---
        // Le pasamos los datos al BLEManager para que los envíe
        if (bleManager.isDeviceConnected()) {
            bleManager.updateSensorValues(temp, hum, pres, co2);
        }

        // --- Lógica de Calibración (próximamente en CalibrationManager) ---
        // calibrationManager.run();
    }

    // Otras tareas que necesiten ejecutarse en cada ciclo podrían ir aquí
}