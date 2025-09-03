#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

/**
 * @class BLEManager
 * @brief Gestiona toda la funcionalidad del servidor Bluetooth de Baja Energía (BLE).
 * * Esta clase encapsula la creación del servidor, servicios, características,
 * y la actualización de los valores de los sensores.
 */
class BLEManager
{
public:
    // --- Métodos Públicos ---
    BLEManager(); // Constructor
    void init();
    void updateSensorValues(float temp, float hum, float pres, int co2, String systemStatus, String coolerStatus);
    bool isDeviceConnected();
    String getCalibrationCommand();

private:
    // --- Atributos ---
    // --- Punteros a Objetos BLE ---
    BLEServer *pServer;
    BLECharacteristic *pCharacteristicTemp;
    BLECharacteristic *pCharacteristicPres;
    BLECharacteristic *pCharacteristicHum;
    BLECharacteristic *pCharacteristicCO2;
    BLECharacteristic *pCharacteristicCalibrate;
    BLECharacteristic *pCharacteristicSystemState;
    BLECharacteristic *pCharacteristicCoolerState;
};

// --- Variable Externa ---
// Hacemos que la variable de conexión sea accesible globalmente de forma segura.
extern bool deviceConnected;

#endif // BLE_MANAGER_H