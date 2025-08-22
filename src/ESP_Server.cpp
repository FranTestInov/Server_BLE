#include <DHT.h>           //Cargamos la librería DHT
#define DHTTYPE   DHT22   //Definimos el modelo del sensor (hay //otros DHT)
#define DHTPIN    25     // Se define el pin 2 del Arduino para conectar el sensor DHT11
#include <Wire.h>
#include <Adafruit_BMP280.h>              
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

const int hdPin = 12;  // Pin para calibración manual
const int RXD2 = 16;   // UART2 RX (del sensor TX)
const int TXD2 = 17;   // UART2 TX (al sensor RX)

unsigned long lastMeasurementTime = 0;
const unsigned long interval = 2000;  // 2 segundos

bool isCalibrated = false;
bool waitingForConfirmation = false;  // empezamos esperando confirmación
bool warmingUp = false;

unsigned long calibrationStartTime = 0;
const unsigned long warmUpTime = 20 * 60 * 1000UL;  // 20 minutos

bool FLAG = false;

void manualCalibration() 
{
  digitalWrite(hdPin, LOW);
  delay(7000);  // Mínimo 7 segundos
  digitalWrite(hdPin, HIGH);
  Serial.println("Sensor calibrado manualmente a 400 ppm.");
}

int readCO2() 
{
  byte cmd[9] = { 0xFF, 0x01, 0x86, 0, 0, 0, 0, 0, 0x79 };
  Serial2.write(cmd, 9);
  delay(100);

  if (Serial2.available() >= 9) 
  {
    byte response[9];
    Serial2.readBytes(response, 9);

    if (response[0] == 0xFF && response[1] == 0x86) 
    {
      int co2 = (response[2] << 8) | response[3];
      //Serial.print("CO₂: ");
      //Serial.print(co2);
      //Serial.println(" ppm");
      return co2;
    } else {
      Serial.println("Respuesta inválida del sensor.");
    }
  } else {
    Serial.println("No se recibieron suficientes datos.");
  }
  return -1; // Retornar -1 si hay error
}


void calibrateCO2()
{
  static unsigned long lastPrint = 0;

  if (waitingForConfirmation)
  {
    Serial.println("Iniciando periodo de calentamiento de 20 minutos...");
    calibrationStartTime = millis();
    warmingUp = true;
    waitingForConfirmation = false;
  }

  if (warmingUp)
  {
    unsigned long now = millis();
    unsigned long elapsed = now - calibrationStartTime;

    if (elapsed >= warmUpTime)
    {
      Serial.println("Calentamiento completado. Iniciando calibración manual...");
      digitalWrite(hdPin, LOW);
      delay(7000);  // mínimo 7 segundos
      digitalWrite(hdPin, HIGH);
      Serial.println("Sensor calibrado manualmente a 400 ppm.");

      isCalibrated = false;
      warmingUp = false;
      FLAG = false;
    }
    else if (now - lastPrint >= 10000)  // mostrar cada 10 segundos
    {
      Serial.print("Calentando... quedan ");
      Serial.print((warmUpTime - elapsed) / 1000);
      Serial.println(" segundos.");
      lastPrint = now;
    }
  }
}

// Definir UUIDs para el servicio y las características
#define SERVICE_UUID              "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_TMP   "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_PRES  "cba1d466-344c-4be3-ab3f-189f80dd7518"
#define CHARACTERISTIC_UUID_HUM   "d2b2d3e1-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_CO2   "a1b2c3d4-5678-90ab-cdef-1234567890ab"  // Nuevo para CO2
#define CHARACTERISTIC_UUID_CALIBRATE "12345678-1234-1234-1234-123456789abc"  // UUID para calibración

//bool deviceConnected = false;
bool startCalibration = false;  // Variable para indicar inicio calibración

// Variables globales
bool deviceConnected = false; // Indica si un dispositivo está conectado
BLEServer *pServer = nullptr; // Puntero al servidor BLE
BLECharacteristic *pCharacteristicTemp = nullptr; // Puntero a la característica de temperatura
BLECharacteristic *pCharacteristicPres = nullptr; // Puntero a la característica de la presión
BLECharacteristic *pCharacteristicHum = nullptr;  // Puntero a la característica de la humedad
BLECharacteristic *pCharacteristicCO2 = nullptr;  // Característica CO2
BLECharacteristic *pCharacteristicCalibrate = nullptr;  // Nueva característica calibración

// Callback para manejar eventos de conexión y desconexión
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
        deviceConnected = true; // Dispositivo conectado
        Serial.println("Dispositivo conectado");
    }

    void onDisconnect(BLEServer *pServer) {
        deviceConnected = false; // Dispositivo desconectado
        Serial.println("Dispositivo desconectado");

        // Reiniciar la publicidad para permitir nuevas conexiones
        pServer->startAdvertising();
        Serial.println("Publicidad reiniciada");
    }
};

Adafruit_BMP280 bmp; // I2C
DHT dht(DHTPIN, DHTTYPE, 22); 

void setup() 
{
  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);  // UART2 para sensor co2
  pinMode(hdPin, OUTPUT);
  digitalWrite(hdPin, HIGH);  // HD en HIGH: no calibrar

  dht.begin(); 
  while ( !Serial ) delay(100);   // wait for native usb
  unsigned status;
  status = bmp.begin();
  if (!status) 
  {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                      "try a different address!"));
    Serial.print("SensorID was: 0x"); Serial.println(bmp.sensorID(),16);
    Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
    Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
    Serial.print("        ID of 0x60 represents a BME 280.\n");
    Serial.print("        ID of 0x61 represents a BME 680.\n");
    while (1) delay(10);
  } 
  /* Default settings from datasheet. */
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */

  BLEDevice::init("Sensor_1");
  // Crear el servidor BLE
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks()); // Asignar el callback
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Crear la característica para el mensaje
    pCharacteristicTemp = pService->createCharacteristic(
                         CHARACTERISTIC_UUID_TMP,
                         BLECharacteristic::PROPERTY_READ
                     );
  // Crear la característica para la presión
    pCharacteristicPres = pService->createCharacteristic(
                         CHARACTERISTIC_UUID_PRES,
                         BLECharacteristic::PROPERTY_READ
                     );    
  // Crear la característica para la humedad
  pCharacteristicHum = pService->createCharacteristic(
                         CHARACTERISTIC_UUID_HUM,
                         BLECharacteristic::PROPERTY_READ
                     ); 
  pCharacteristicCO2 = pService->createCharacteristic(
                        CHARACTERISTIC_UUID_CO2,
                        BLECharacteristic::PROPERTY_READ
                     );
  pCharacteristicCalibrate = pService->createCharacteristic(
                              CHARACTERISTIC_UUID_CALIBRATE,
                              BLECharacteristic::PROPERTY_READ
                     );
  pCharacteristicCalibrate->setValue("READY"); // Valor inicial


  // Iniciar el servicio
  pService->start();

  // Iniciar la publicidad (advertising)
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // Funciona bien con dispositivos iOS
  pAdvertising->setMinPreferred(0x12);  // Funciona bien con dispositivos Android
  BLEDevice::startAdvertising();
  Serial.println("Servidor BLE iniciado y publicitando...");
}

void loop() 
{
  if (pCharacteristicCalibrate)
  {
    std::string valueCalibrate = pCharacteristicCalibrate->getValue(); // Leer Value
    //Serial.print("valueCalibrate: ");
    //Serial.println(valueCalibrate);

    if (valueCalibrate == "D" && !warmingUp && !isCalibrated)
    {
      Serial.println("Iniciando calibración de CO₂...");
      waitingForConfirmation = true;
      FLAG = true;
      pCharacteristicCalibrate->setValue("READY");  // Opcional: marcar como hecho
    }
  }

  // Calibración en progreso
  if (!isCalibrated && FLAG)
  {
    calibrateCO2();  // Mostrar tiempo restante y calibrar
    return;  // No hacer nada más hasta que termine
  }
  //valueCalibrate = "E";
  //pCharacteristicCalibrate->setValue("READY");  // Opcional: marcar como hecho
  // Solo se ejecuta después de la calibración
  float H = dht.readHumidity();
  float T = dht.readTemperature();
  float P = bmp.readPressure();
  int C = readCO2();

  float pres = P / 100;

  Serial.print("Humedad: ");
  Serial.print(H);
  Serial.println("%");
  Serial.print("Temperatura: ");
  Serial.print(T);
  Serial.println("°C");
  Serial.print("Presion: ");
  Serial.print(pres);
  Serial.println("hPa");
  Serial.print("CO2: ");
  Serial.print(C);
  Serial.println("ppm");

  String tempStr = String(T, 2);
  String presStr = String(pres, 2);
  String humStr = String(H, 2);
  String co2Str = String(C);

  pCharacteristicTemp->setValue(tempStr.c_str());
  pCharacteristicPres->setValue(presStr.c_str());
  pCharacteristicHum->setValue(humStr.c_str());
  pCharacteristicCO2->setValue(co2Str.c_str());

  delay(2000);  // Para mantener el intervalo de lectura
}