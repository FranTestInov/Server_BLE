#include "BLEManager.h"
#include <Arduino.h> // Necesario para Serial.println()

// --- DEFINICIONES PARA EL SERVIDOR BLE ---
// (Movimos los UUIDs aquí para que estén contenidos en este módulo)
#define SERVICE_UUID                  "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_TMP       "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_PRES      "cba1d466-344c-4be3-ab3f-189f80dd7518"
#define CHARACTERISTIC_UUID_HUM       "d2b2d3e1-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_CO2       "a1b2c3d4-5678-90ab-cdef-1234567890ab"
#define CHARACTERISTIC_UUID_CALIBRATE "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID_STATUS    "abcdefab-1234-5678-90ab-abcdefabcdef"//Caracterisitca de estado del sensor de CO2: "PREHEATING", "READY", "CALIBRATING""

// Inicializamos la variable global
bool deviceConnected = false;

/**
 * @class MyServerCallbacks
 * @brief Gestiona los eventos de conexión y desconexión.
 */
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
        deviceConnected = true;
        Serial.println("Dispositivo conectado");
    }

    void onDisconnect(BLEServer *pServer) {
        deviceConnected = false;
        Serial.println("Dispositivo desconectado");
        pServer->startAdvertising();
        Serial.println("Publicidad reiniciada");
    }
};

// --- Variable para almacenar el comando recibido ---
String calibrationCommand = "";

/**
 * @class MyCharacteristicCallbacks
 * @brief Gestiona los eventos de escritura en una característica.
 */
class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
    /**
     * @brief Se ejecuta cuando un cliente BLE escribe en la característica.
     * @param pCharacteristic Puntero a la característica que fue escrita.
     */
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();

        if (value.length() > 0) {
            calibrationCommand = ""; // Limpiamos el comando anterior
            for (int i = 0; i < value.length(); i++) {
                calibrationCommand += value[i];
            }
            Serial.print("Comando de calibración recibido: ");
            Serial.println(calibrationCommand);
        }
    }
};

BLEManager::BLEManager() {
    // El constructor puede estar vacío por ahora
    pServer = nullptr;
    pCharacteristicTemp = nullptr;
    pCharacteristicPres = nullptr;
    pCharacteristicHum = nullptr;
    pCharacteristicCO2 = nullptr;
    pCharacteristicCalibrate = nullptr;
    pCharacteristicStatus = nullptr;
}

void BLEManager::init() {
    // --- Toda la lógica de inicialización del BLE se mueve aquí ---
    BLEDevice::init("SRV_NAME");
    
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Crear las características de los sensores
    pCharacteristicTemp = pService->createCharacteristic(CHARACTERISTIC_UUID_TMP, BLECharacteristic::PROPERTY_READ);
    pCharacteristicPres = pService->createCharacteristic(CHARACTERISTIC_UUID_PRES, BLECharacteristic::PROPERTY_READ);
    pCharacteristicHum = pService->createCharacteristic(CHARACTERISTIC_UUID_HUM, BLECharacteristic::PROPERTY_READ);
    pCharacteristicCO2 = pService->createCharacteristic(CHARACTERISTIC_UUID_CO2, BLECharacteristic::PROPERTY_READ);
    pCharacteristicCalibrate = pService->createCharacteristic(CHARACTERISTIC_UUID_CALIBRATE,BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    pCharacteristicCalibrate->setCallbacks(new MyCharacteristicCallbacks());
    pCharacteristicCalibrate->setValue("READY");
    // Característica de estado del sensor de CO2
    pCharacteristicStatus = pService->createCharacteristic(CHARACTERISTIC_UUID_STATUS, BLECharacteristic::PROPERTY_READ);
    pCharacteristicStatus->setValue("PREHEATING"); // Valor inicial

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    BLEAdvertisementData advertisementData;
    advertisementData.setName("SRV_NAME");
    advertisementData.setFlags(0x06);
    pAdvertising->setAdvertisementData(advertisementData);

    BLEAdvertisementData scanResponseData;
    scanResponseData.setCompleteServices(pService->getUUID());
    pAdvertising->setScanResponseData(scanResponseData);
    BLEDevice::startAdvertising();
    
    Serial.println("Servidor BLE iniciado y publicitando...");
}

void BLEManager::updateSensorValues(float temp, float hum, float pres, int co2, String status) {
    if (deviceConnected) {
        // Convierte los valores a string y los asigna a las características
        String tempStr = String(temp, 2);
        String presStr = String(pres, 2);
        String humStr = String(hum, 2);
        String co2Str = String(co2);
        String statusStr = String(status); // Aquí podrías actualizar el estado real del sensor de CO2

        pCharacteristicTemp->setValue(tempStr.c_str());
        pCharacteristicPres->setValue(presStr.c_str());
        pCharacteristicHum->setValue(humStr.c_str());
        pCharacteristicCO2->setValue(co2Str.c_str());
        pCharacteristicStatus->setValue(statusStr.c_str());
    }
}

bool BLEManager::isDeviceConnected() {
    return deviceConnected;
}

String BLEManager::getCalibrationCommand() {
    if (calibrationCommand != "") {
        String cmd = calibrationCommand;
        calibrationCommand = ""; // Reseteamos el comando una vez que se lee
        return cmd;
    }
    return ""; // Devolvemos un string vacío si no hay comandos nuevos
}
