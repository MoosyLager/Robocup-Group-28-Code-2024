#include "sensor.h"
#include "circularBuf.h"

// The Arduino pin connected to the XSHUT pin of each sensor
const uint8_t xShutPinsL0[8] = {0, 1};
const uint8_t xShutPinsL1[8] = {};

SX1509 io;  // SX1509 object to use throughout
VL53L0X sensorsL0[NUM_TOF_L0];
VL53L1X sensorsL1[NUM_TOF_L1];

circularBuf_t sensorL0Data[NUM_TOF_L0];
circularBuf_t sensorL1Data[NUM_TOF_L1];

void initCircularBuffers(circularBuf_t *buffers)
{
    for (uint32_t i = 0; i < NUM_TOF_L0; i++) {
        // Initialize each buffer in the array with the given size
        circularBufTInit(&buffers[i], CIRCULAR_BUF_SIZE);
    }

    for (uint32_t i = 0; i < NUM_TOF_L1; i++) {
        // Initialize each buffer in the array with the given size
        circularBufTInit(&buffers[i], CIRCULAR_BUF_SIZE);
    }
}

/**
 * Initialise the IO board, I2C bus and the sensor
 */
void InitSensors()
{
    io.begin(SX1509_ADDRESS);
    Wire.begin();
    Wire.setClock(400000);
    InitTOFL0();
    InitTOFL1();
}

/**
 * Initialise all connected VL53L0X Sensors
 */
void InitTOFL0()
{
    // Disable/reset all sensors by driving their XSHUT pins low.
    for ( uint8_t i = 0; i < NUM_TOF_L0; i++ ) {
        io.pinMode(xShutPinsL0[i], OUTPUT);
        io.digitalWrite(xShutPinsL0[i], LOW);
    }

    // Enable, initialise, and start each sensor
    for ( uint8_t i = 0; i < NUM_TOF_L0; i++ ) {
        // Stop driving this sensor's XSHUT low. This should allow the carrier
        // board to pull it high. (We do NOT want to drive XSHUT high since it is
        // not level shifted.) Then wait a bit for the sensor to start up.
        // pinMode(xshutPins[i], INPUT);
        io.digitalWrite(xShutPinsL0[i], HIGH);
        delay(10);

        sensorsL0[i].setTimeout(500);
        if ( !sensorsL0[i].init() ) {
            Serial.print("Failed to detect and initialise sensor L0 ");
            Serial.print(i);
            while ( 1 )
                ;
        }

        // Each sensor must have its address changed to a unique value other than
        // the default of 0x29 (except for the last one, which could be left at
        // the default). To make it simple, we'll just count up from 0x2A.
        sensorsL0[i].setAddress(VL53L0X_ADDRESS_START + i);

        sensorsL0[i].startContinuous(50);
    }
}

/**
 * Initialise all connected VL53L1X Sensors
 */
void InitTOFL1()
{
    // Disable/reset all sensors by driving their XSHUT pins low.
    for ( uint8_t i = 0; i < NUM_TOF_L1; i++ ) {
        io.pinMode(xShutPinsL1[i], OUTPUT);
        io.digitalWrite(xShutPinsL1[i], LOW);
    }

    // Enable, initialise, and start each sensor
    for ( uint8_t i = 0; i < NUM_TOF_L1; i++ ) {
        // Stop driving this sensor's XSHUT low. This should allow the carrier
        // board to pull it high. (We do NOT want to drive XSHUT high since it is
        // not level shifted.) Then wait a bit for the sensor to start up.
        // pinMode(xshutPins[i], INPUT);
        io.digitalWrite(xShutPinsL1[i], HIGH);
        delay(10);

        sensorsL1[i].setTimeout(500);
        if ( !sensorsL1[i].init() ) {
            Serial.print("Failed to detect and initialise sensor L1 ");
            Serial.print(i);
            while ( 1 )
                ;
        }

        // Each sensor must have its address changed to a unique value other than
        // the default of 0x29 (except for the last one, which could be left at
        // the default). To make it simple, we'll just count up from 0x2A.
        sensorsL1[i].setAddress(VL53L1X_ADDRESS_START + i);

        sensorsL1[i].startContinuous(50);
    }
}

void ReadTOFL0()
{
    for (uint8_t i = 0; i < NUM_TOF_L0; i++) {
        // Read sensor data
        uint16_t sensorValue = sensorsL0[i].readRangeContinuousMillimeters();
        
        // Write sensor data to circular buffer
        circularBufTWrite(&sensorL0Data[i], sensorValue);
        
        // Calculate the average from the circular buffer
        uint32_t avg = findBufAvg(&sensorL0Data[i]);
        
        // Print the average value
        Serial.print("Sensor L0 ");
        Serial.print(i);
        Serial.print(" Avg: ");
        Serial.print(avg);
        
        if (sensorsL0[i].timeoutOccurred()) {
            Serial.print(" TIMEOUT");
        }
        Serial.print('\t');
    }
    Serial.println();
}

void ReadTOFL1()
{
    for (uint8_t i = 0; i < NUM_TOF_L1; i++) {
        // Read sensor data
        uint16_t sensorValue = sensorsL1[i].readRangeContinuousMillimeters();
        
        // Write sensor data to circular buffer
        circularBufTWrite(&sensorL1Data[i], sensorValue);
        
        // Calculate the average from the circular buffer
        uint32_t avg = findBufAvg(&sensorL1Data[i]);
        
        // Print the average value
        Serial.print("Sensor L1 ");
    }
}


VL53L0X *returnL0()
{
    return sensorsL0;
}

VL53L1X *returnL1()
{
    return sensorsL1;
}

uint32_t findBufAvg(circularBuf_t* buffer) {
	uint32_t i;
    uint32_t sum = 0;
	for (i = 0; i < CIRCULAR_BUF_SIZE; i++) {
		sum = sum + circularBufTRead(buffer);
	}

	int32_t meanDist = ((2 * sum + CIRCULAR_BUF_SIZE) / 2 / CIRCULAR_BUF_SIZE);
    return meanDist;
}

bool detectedWeight(uint32_t avg1, uint32_t avg2) {
    // Find the absolute difference between the two averages
    uint32_t diff = abs((int32_t)(avg1 - avg2));
    uint32_t smallerAvg = (avg1 < avg2) ? avg1 : avg2;
    uint32_t threshold = (smallerAvg * 30) / 100;
    return diff > threshold;
}

