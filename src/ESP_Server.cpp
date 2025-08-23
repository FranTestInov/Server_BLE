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
SensorManager sensorManager;
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
    sensorManager.init();
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

        SensorData data = sensorManager.readAllSensors();

        // Mostramos en la consola los valores reales
        Serial.printf("Enviando -> Temp: %.2f C, Hum: %.2f %%, Pres: %.2f hPa, CO2: %d ppm\n",
                       data.temperature, data.humidity, data.pressure, data.co2);

        // --- Actualización del Servidor BLE ---
        // Le pasamos los datos al BLEManager para que los envíe
        if (bleManager.isDeviceConnected()) {
            bleManager.updateSensorValues(data.temperature, data.humidity, data.pressure, data.co2);
        }

        // --- Lógica de Calibración (próximamente en CalibrationManager) ---
        // calibrationManager.run();
    }

    // Otras tareas que necesiten ejecutarse en cada ciclo podrían ir aquí
}