/**
 * @file BLEManager.cpp
 * @brief Implementación de la clase BLEManager para la gestión del servidor BLE.
 * @details Este archivo contiene toda la lógica para inicializar el servidor BLE,
 * definir servicios y características, gestionar conexiones y actualizar los
 * valores de los sensores para que un cliente pueda leerlos.
 * @author Francisco Aguirre
 * @date 2025-08-28
 */

#include "BLEManager.h"
#include <Arduino.h> // Necesario para Serial.println()

// --- DEFINICIONES PARA EL SERVIDOR BLE ---

/** @def SERVICE_UUID
 * @brief UUID principal del servicio BLE que agrupa todas las características. */
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
/** @def CHARACTERISTIC_UUID_TMP
 * @brief UUID para la característica de temperatura (lectura). */
#define CHARACTERISTIC_UUID_TMP "beb5483e-36e1-4688-b7f5-ea07361b26a8"
/** @def CHARACTERISTIC_UUID_PRES
 * @brief UUID para la característica de presión (lectura). */
#define CHARACTERISTIC_UUID_PRES "cba1d466-344c-4be3-ab3f-189f80dd7518"
/** @def CHARACTERISTIC_UUID_HUM
 * @brief UUID para la característica de humedad (lectura). */
#define CHARACTERISTIC_UUID_HUM "d2b2d3e1-36e1-4688-b7f5-ea07361b26a8"
/** @def CHARACTERISTIC_UUID_CO2
 * @brief UUID para la característica de CO2 (lectura). */
#define CHARACTERISTIC_UUID_CO2 "a1b2c3d4-5678-90ab-cdef-1234567890ab"
/** @def CHARACTERISTIC_UUID_CALIBRATE
 * @brief UUID para la característica de calibración (lectura/escritura). */
#define CHARACTERISTIC_UUID_CALIBRATE "12345678-1234-1234-1234-123456789abc"
/** @def CHARACTERISTIC_UUID_SYSTEM_STATE
 * @brief UUID para la característica de estado del sistema (lectura). */
#define CHARACTERISTIC_UUID_SYSTEM_STATE "c1a7d131-15e1-413f-b565-8123c5a31a1e"
/** @def CHARACTERISTIC_UUID_COOLER_STATE
 * @brief UUID para la característica de estado del ventilador (lectura/escritura). */
#define CHARACTERISTIC_UUID_COOLER_STATE "d2b8d232-26f1-4688-b7f5-ea07361b26a8"

// --- Variables Globales ---

/** @brief Flag global que indica el estado de la conexión BLE. */
bool deviceConnected = false;
/** @brief Flag global volátil para solicitar el cambio de estado del ventilador.
 * @details Es `volatile` porque se modifica en un callback (contexto de interrupción)
 * y se lee en el bucle principal. */
volatile bool toggleCoolerRequest = false;

/**
 * @class MyServerCallbacks
 * @brief Gestiona los eventos de conexión y desconexión del servidor BLE.
 * @details Esta clase hereda de `BLEServerCallbacks` para implementar
 * comportamiento personalizado cuando un cliente se conecta o desconecta.
 */
class MyServerCallbacks : public BLEServerCallbacks
{
    /**
     * @brief Método llamado cuando un cliente BLE se conecta.
     * @param pServer Puntero al servidor BLE.
     */
    void onConnect(BLEServer *pServer)
    {
        deviceConnected = true;
        Serial.println("Dispositivo conectado");
    }

    /**
     * @brief Método llamado cuando un cliente BLE se desconecta.
     * @param pServer Puntero al servidor BLE.
     */
    void onDisconnect(BLEServer *pServer)
    {
        deviceConnected = false;
        Serial.println("Dispositivo desconectado");
        pServer->startAdvertising(); // Reinicia la publicidad para permitir nuevas conexiones.
        Serial.println("Publicidad reiniciada");
    }
};

/** @brief Almacena el último comando de calibración recibido. */
String calibrationCommand = "";

/**
 * @class MyCharacteristicCallbacks
 * @brief Gestiona los eventos de escritura en la característica de calibración.
 */
class MyCharacteristicCallbacks : public BLECharacteristicCallbacks
{
    /**
     * @brief Se ejecuta cuando un cliente BLE escribe en la característica de calibración.
     * @param pCharacteristic Puntero a la característica que fue escrita.
     */
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0)
        {
            calibrationCommand = ""; // Limpia el comando anterior.
            for (int i = 0; i < value.length(); i++)
            {
                calibrationCommand += value[i];
            }
            Serial.print("Comando de calibración recibido: ");
            Serial.println(calibrationCommand);
        }
    }
};

/**
 * @class CoolerCharacteristicCallbacks
 * @brief Gestiona los eventos de escritura en la característica del ventilador.
 */
class CoolerCharacteristicCallbacks : public BLECharacteristicCallbacks
{
    /**
     * @brief Se ejecuta cuando un cliente BLE escribe en la característica del ventilador.
     * @details Cualquier escritura activa el flag `toggleCoolerRequest` para que
     * el bucle principal procese la solicitud de cambio de estado.
     * @param pCharacteristic Puntero a la característica que fue escrita.
     */
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        toggleCoolerRequest = true;
        Serial.println("Solicitud para alternar el estado del cooler recibida vía BLE.");
    }
};

/**
 * @brief Constructor de la clase BLEManager.
 * @details Inicializa todos los punteros de objetos BLE a `nullptr`.
 */
BLEManager::BLEManager()
{
    pServer = nullptr;
    pCharacteristicTemp = nullptr;
    pCharacteristicPres = nullptr;
    pCharacteristicHum = nullptr;
    pCharacteristicCO2 = nullptr;
    pCharacteristicCalibrate = nullptr;
    pCharacteristicSystemState = nullptr;
    pCharacteristicCoolerState = nullptr;
}

/**
 * @brief Inicializa y configura el servidor BLE.
 * @details Crea el dispositivo BLE, el servidor, el servicio y todas las
 * características con sus propiedades y callbacks correspondientes.
 * Finalmente, inicia la publicidad BLE.
 */
void BLEManager::init()
{
    BLEDevice::init("SRV_NAME");

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    // --- Creación de Características ---
    pCharacteristicTemp = pService->createCharacteristic(CHARACTERISTIC_UUID_TMP, BLECharacteristic::PROPERTY_READ);
    pCharacteristicPres = pService->createCharacteristic(CHARACTERISTIC_UUID_PRES, BLECharacteristic::PROPERTY_READ);
    pCharacteristicHum = pService->createCharacteristic(CHARACTERISTIC_UUID_HUM, BLECharacteristic::PROPERTY_READ);
    pCharacteristicCO2 = pService->createCharacteristic(CHARACTERISTIC_UUID_CO2, BLECharacteristic::PROPERTY_READ);

    pCharacteristicCalibrate = pService->createCharacteristic(CHARACTERISTIC_UUID_CALIBRATE, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    pCharacteristicCalibrate->setCallbacks(new MyCharacteristicCallbacks());
    pCharacteristicCalibrate->setValue("READY");

    pCharacteristicSystemState = pService->createCharacteristic(CHARACTERISTIC_UUID_SYSTEM_STATE, BLECharacteristic::PROPERTY_READ);
    pCharacteristicSystemState->setValue("PREHEATING");

    pCharacteristicCoolerState = pService->createCharacteristic(CHARACTERISTIC_UUID_COOLER_STATE, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    pCharacteristicCoolerState->setCallbacks(new CoolerCharacteristicCallbacks());
    pCharacteristicCoolerState->setValue("OFF");

    pService->start();

    // --- Configuración de la Publicidad (Advertising) ---
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    BLEAdvertisementData advertisementData;
    advertisementData.setName("SRV_NAME");
    advertisementData.setFlags(0x06); // General Discoverable Mode
    pAdvertising->setAdvertisementData(advertisementData);

    BLEAdvertisementData scanResponseData;
    scanResponseData.setCompleteServices(pService->getUUID());
    pAdvertising->setScanResponseData(scanResponseData);

    BLEDevice::startAdvertising();
    Serial.println("Servidor BLE iniciado y publicitando...");
}

/**
 * @brief Actualiza los valores de todas las características BLE.
 * @details Si un dispositivo está conectado, convierte los datos de los sensores
 * y los estados del sistema a strings y los asigna a sus características
 * correspondientes.
 * @param temp Temperatura actual.
 * @param hum Humedad actual.
 * @param pres Presión actual.
 * @param co2 Concentración de CO2 actual.
 * @param systemStatus Estado actual del sistema (ej. "PREHEATING").
 * @param coolerStatus Estado actual del ventilador (ej. "ON").
 */
void BLEManager::updateSensorValues(float temp, float hum, float pres, int co2, String systemStatus, String coolerStatus)
{
    if (deviceConnected)
    {
        String tempStr = String(temp, 2);
        String presStr = String(pres, 2);
        String humStr = String(hum, 2);
        String co2Str = String(co2);

        pCharacteristicTemp->setValue(tempStr.c_str());
        pCharacteristicPres->setValue(presStr.c_str());
        pCharacteristicHum->setValue(humStr.c_str());
        pCharacteristicCO2->setValue(co2Str.c_str());
        pCharacteristicSystemState->setValue(systemStatus.c_str());
        pCharacteristicCoolerState->setValue(coolerStatus.c_str());
    }
}

/**
 * @brief Verifica si hay un cliente BLE conectado.
 * @return bool `true` si un dispositivo está conectado, `false` en caso contrario.
 */
bool BLEManager::isDeviceConnected()
{
    return deviceConnected;
}

/**
 * @brief Obtiene el último comando de calibración recibido.
 * @details Devuelve el comando y lo limpia para evitar procesarlo múltiples veces.
 * @return String El comando de calibración, o un string vacío si no hay ninguno nuevo.
 */
String BLEManager::getCalibrationCommand()
{
    if (calibrationCommand != "")
    {
        String cmd = calibrationCommand;
        calibrationCommand = ""; // Resetea el comando una vez leído.
        return cmd;
    }
    return "";
}
