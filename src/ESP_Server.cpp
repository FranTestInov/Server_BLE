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

extern volatile bool toggleCoolerRequest;
bool wasCalibrating = false;

// --- OBJETOS GLOBALES DE LOS MÓDULOS ---
// Creamos una instancia para cada manager que controlará una parte del sistema.
BLEManager bleManager;
SensorManager sensorManager;
CalibrationManager calibrationManager;

void scan();

/**
 * @brief Configuración inicial del microcontrolador.
 * @details Inicializa la comunicación serial y todos los módulos principales.
 */
void setup()
{
    Wire.begin();
    Serial.begin(115200); // Usamos una velocidad más alta para depuración
    while (!Serial)
        ; // Espera a que el puerto serial se conecte

    // Inicializamos cada uno de nuestros managers
    bleManager.init();
    sensorManager.init();
    calibrationManager.init();

    Serial.println("Sistema inicializado y listo.");
    // scan();
}

// Variables para controlar el tiempo de envío de datos
unsigned long lastUpdateTime = 0;
const int UPDATE_INTERVAL_MS = 500; // Intervalo de 500ms = 2 datos por segundo

/**
 * @brief Bucle principal del programa.
 * @details Se ejecuta repetidamente. Su función es leer los sensores
 * y actualizar el servidor BLE a una frecuencia fija.
 */
void loop()
{
    // Preguntamos al BLEManager si ha llegado un nuevo comando.
    String cmd = bleManager.getCalibrationCommand();
    if (cmd == "START_CAL")
    { // Usamos un comando más descriptivo
        calibrationManager.startCalibration();
        sensorManager.setSystemState(CALIBRATING);
    }

    // El calibrationManager se encarga de su propia máquina de estados interna.
    calibrationManager.run();

    if (wasCalibrating && !calibrationManager.isCalibrating())
    {
        sensorManager.setSystemState(READY); // Volver al estado READY
    }
    wasCalibrating = calibrationManager.isCalibrating(); // Actualizar el estado para el próximo ciclo.

    if (toggleCoolerRequest)
    {
        sensorManager.setFanState(!sensorManager.getFanState()); // Alternamos el estado
        toggleCoolerRequest = false;                             // Reseteamos la bandera para la próxima petición
    }

    if (!calibrationManager.isCalibrating())
    {
        // --- Lógica de temporización para no bloquear el procesador ---
        if (millis() - lastUpdateTime >= UPDATE_INTERVAL_MS)
        {
            lastUpdateTime = millis(); // Actualizamos el tiempo del último envío

            SensorData data = sensorManager.readAllSensors();

            // Mostramos en la consola los valores reales
            Serial.printf("Enviando -> Temp: %.2f C, Hum: %.2f %%, Pres: %.2f hPa, CO2: %d ppm\n",
                          data.temperature, data.humidity, data.pressure, data.co2);

            // --- Actualización del Servidor BLE ---
            // Le pasamos los datos al BLEManager para que los envíe
            if (bleManager.isDeviceConnected())
            {
                // Obtenemos los estados actuales desde el SensorManager
                String systemStateStr = "UNKNOWN";
                switch (sensorManager.getState())
                {
                case PREHEATING:
                    systemStateStr = "PREHEATING";
                    break;
                case READY:
                    systemStateStr = "READY";
                    break;
                case CALIBRATING:
                    systemStateStr = "CALIBRATING";
                    break;
                }

                String coolerStateStr = sensorManager.getFanState() ? "ON" : "OFF";

                // Le pasamos todos los datos, incluidos los nuevos estados, al BLEManager
                bleManager.updateSensorValues(data.temperature, data.humidity, data.pressure, data.co2, systemStateStr, coolerStateStr);
            }
        }
        // Otras tareas que necesiten ejecutarse en cada ciclo podrían ir aquí
    }
}

void scan()
{
    byte error, address;
    int nDevices;

    Serial.println("Escaneando...");
    nDevices = 0;
    for (address = 1; address < 127; address++)
    {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0)
        {
            Serial.print("Dispositivo I2C encontrado en la dirección 0x");
            if (address < 16)
            {
                Serial.print("0");
            }
            Serial.println(address, HEX);
            nDevices++;
        }
        else if (error == 4)
        {
            Serial.print("Error desconocido en la dirección 0x");
            if (address < 16)
            {
                Serial.print("0");
            }
            Serial.println(address, HEX);
        }
    }
    if (nDevices == 0)
    {
        Serial.println("No se encontraron dispositivos I2C\n");
    }
    else
    {
        Serial.println("Escaneo finalizado\n");
    }
    delay(5000); // Espera 5 segundos para el próximo escaneo
}