#include "collection.h"
#include "encoder.h"
#include "motor.h"
#include "searchAlgorithm.h"
#include "sensor.h"
#include <Arduino.h>
#include <elapsedMillis.h>

RobotFSM fsm;
elapsedMillis updateSensorsTimer, updateFSMTimer, updatePWMTimer, buttonTimer;
bool pwmOn = false;

void setup()
{
    Serial.begin(9600);
    InitSensors();
    InitMotors();
    InitEncoders();
    initializeRobotFSM(&fsm);

    for ( int i = 0; i < 2 * CIRCULAR_BUF_SIZE; i++ ) {
        UpdateTOFL0();
        UpdateTOFL1();
    }

    Serial.println("Setup complete");
}

void loop()
{
    if ( BlueButtonState() && buttonTimer > 2500 && !pwmOn ) {
        pwmOn = true;
    }

    if ( updateFSMTimer > 70 ) {
        processFSM(&fsm);
        updateFSMTimer = 0;
    }

    if ( updatePWMTimer > 20 && pwmOn ) {
        PIDMotorControl(&leftMotor);
        PIDMotorControl(&rightMotor);
        updatePWMTimer = 0;
    }

    if ( updateSensorsTimer > 50 ) {
        UpdateTOFL0();
        UpdateTOFL1();
        UpdateIMU();
        updateSensorsTimer = 0;
    }
}
