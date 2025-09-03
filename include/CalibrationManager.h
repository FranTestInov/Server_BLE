#ifndef CALIBRATION_MANAGER_H
#define CALIBRATION_MANAGER_H

/**
 * @class CalibrationManager
 * @brief Gestiona el proceso de calibración a cero (400 ppm) del sensor MH-Z19C.
 *
 * Esta clase maneja una máquina de estados para el calentamiento/estabilización
 * y la posterior activación del pulso de calibración en el pin HD.
 */
class CalibrationManager
{
public:
    // --- Métodos Públicos ---
    CalibrationManager(); // Constructor
    void init();
    void startCalibration();
    void run();           // Este método se llamará repetidamente en el loop() principal
    bool isCalibrating(); // Para saber si un proceso de calibración está activo

private:
    // --- Definición de la Máquina de Estados ---
    enum CalibrationState
    {
        IDLE,
        PULSING
    };
    // --- Pines y Constantes ---
    static const int HD_PIN = 12;
    static const unsigned long PULSE_TIME_MS = 7000UL; // 7 segundos

    // --- Variables de Estado ---
    CalibrationState currentState; // Variable que guarda el estado actual
    unsigned long stateStartTime;  // Temporizador para los estados
};

#endif // CALIBRATION_MANAGER_H