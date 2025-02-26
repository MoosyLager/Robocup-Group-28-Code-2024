#include "searchAlgorithm.h"
#include "circularBuf.h"
#include "collection.h"
#include "motor.h"
#include "sensor.h"
#include <elapsedMillis.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

elapsedMillis weightWatchDog;
elapsedMillis rotationCounter;
elapsedMillis moduleCounter;
elapsedMillis turnTimer;
elapsedMillis avoidanceTimer;
elapsedMillis collectionJawsWatchDog;
float initialHeading = 0;
float targetHeading = 0;
bool onLeft = false;

void initializeRobotFSM(RobotFSM* fsm)
{
    fsm->currentState = CALIBRATING;            // Start in CALIBRATING state
    fsm->lastMainState = HUNTING;           // Set last state to the initial state
    fsm->previousState = HUNTING;           // Set previous state to the initial state
    fsm->huntState = SEARCH;                // Start in SEARCH state
    fsm->returnState = HOMESEEK;            // Start in HOMESEEK state
    fsm->collectedWeights = 0;              // No weights collected at the start
    fsm->calibrated = false;                // Robot is not calibrated at the start
    fsm->weightPos = NONE;                  // No weight detected at the start
    fsm->avoidanceSide = NO_WALL;           // No obstacle detected at the start
    fsm->evasiveManeuverCompleted = false;  // Evasive maneuver not completed at the start
    fsm->targetHeading = 0;
    fsm->distToTravel = 0;
    fsm->weightNum = 0;
}

void checkCalibration(RobotFSM* fsm)
{
    
}

// Main FSM processing function
void processFSM(RobotFSM* fsm)
{
    // Checks if robot is calibrated
    // If calibrated, check if robot is too close to the wall
    // If not too close to the wall, check if there are weights onboard to start HUNTING  RETURNING
    if ( fsm->currentState != fsm->previousState ) {
        // State edge detection
        fsm->previousState = fsm->currentState;  // Update the last state
    }
    bool metalDetected = !ReadInductiveSensor();
    if ( metalDetected && fsm->weightNum < 3 ) {
        fsm->currentState = HUNTING;
        fsm->huntState = COLLECT;
    }

    if ( !fsm->currentState == CALIBRATING ) {
        checkWallDistancesTop(fsm);
    } 


    switch ( fsm->currentState ) {
        case CALIBRATING:
            handleCalibrating(fsm);
            break;
        case HUNTING:
            handleHunting(fsm);
            break;
        case AVOIDING:
            handleAvoiding(fsm);
            break;
        case RETURNING:
            handleReturning(fsm);
            break;
        default:
            break;
    }
}

/*
MAIN STATE FUNCTIONS
*/


// State functions
void handleCalibrating(RobotFSM* fsm)
{

}

void handleHunting(RobotFSM* fsm)
{
    switch ( fsm->huntState ) {
        case SEARCH:
            handleSearching(fsm);
            break;
        case CHASE:
            handleChasing(fsm);
            break;
        case COLLECT:
            handleCollecting(fsm);
            break;
        default:
            break;
    }
}

void checkWallDistances(RobotFSM* fsm)
{
    const int numFunctions = NUM_TOF_L0 + NUM_TOF_L1;

    // Iterate through each distance function and check for obstacles
    for ( int i = 0; i < numFunctions; i++ ) {
        uint16_t distance = distanceFunctions[i]();  // Call the function
        // Could change this to test both sensors top and bottom
        if ( distance < AVOIDANCE_THRESHOLD ) {
            fsm->weightPos = NONE;         // Reset the weight position if an obstacle is found
            fsm->evasiveManeuverCompleted = false;
            fsm->currentState = AVOIDING;  // Change the state if below threshold
            avoidanceTimer = 0;
            // Determine side of detection based on the function index
            if ( i % 2 == 0 ) {
                fsm->avoidanceSide = LEFT;  // L0 corresponds to the left side
                fsm->targetHeading =  GetOrientationYaw() + 35;
                onLeft = true;
            } else {
                fsm->avoidanceSide = RIGHT;  // L1 corresponds to the right side
                fsm->targetHeading = GetOrientationYaw() - 35;
                onLeft = false;
            }
            return;                        // Exit early since an obstacle was found
        } else {
            fsm->avoidanceSide = NO_WALL;  // Reset the avoidance side if no obstacle is found
        }
    }
}

void checkWallDistancesTop(RobotFSM* fsm)
{
    const int numFunctionsTop = 4;

    // Iterate through each distance function and check for obstacles
    for ( int i = 0; i < numFunctionsTop; i++ ) {
        uint16_t distance = distanceFunctionsTop[i]();  // Call the function
        // Could change this to test both sensors top and bottom
        if ( distance < AVOIDANCE_THRESHOLD ) {
            fsm->weightPos = NONE;         // Reset the weight position if an obstacle is found
            fsm->evasiveManeuverCompleted = false;
            fsm->currentState = AVOIDING;  // Change the state if below threshold
            avoidanceTimer = 0;
            // Determine side of detection based on the function index
            if ( i % 2 == 0 ) {
                fsm->avoidanceSide = LEFT;  // L0 corresponds to the left side
                fsm->targetHeading = GetOrientationYaw() + 40;
                Serial.println("On left");
                onLeft = true;
            } else {
                fsm->avoidanceSide = RIGHT;  // L1 corresponds to the right side
                fsm->targetHeading = GetOrientationYaw() - 40;
                Serial.println("On right");
                onLeft = false;
            }               // Exit early since an obstacle was found
        } else {
            fsm->avoidanceSide = NO_WALL;  // Reset the avoidance side if no obstacle is found
        }
    }
    Serial.print(fsm->targetHeading);
    // If no obstacle is found
    // Need to be careful that the robot doesn't get stuck in this state
}

void handleAvoiding(RobotFSM* fsm)
{
    if ( !(fsm->evasiveManeuverCompleted) ) {
        if ( onLeft ) {
            move(2000, -4000);
            // Rotate clockwise
        } else if ( !onLeft ) {
            move(-4000, 2000);
            // Rotate counter-clockwise
        }
        Serial.println("Avoiding");
    } else {
        fsm->currentState = fsm->lastMainState;
        fsm->huntState = SEARCH;
    }
    // Rotation Condition Met
    if ( (checkTargetHeading(fsm->targetHeading) && (fsm->avoidanceSide == NO_WALL)) ) {
        Serial.println("Evasive maneuver completed");
        fsm->evasiveManeuverCompleted = true;
        avoidanceTimer = 0;
    } else if ( avoidanceTimer > EVASIVE_MANEUVER_TIMEOUT ) {
        Serial.println("Avoidance timeout");
        fsm->evasiveManeuverCompleted = true;
        // fsm->targetHeading = truncateHeading(GetOrientationYaw());
        avoidanceTimer = 0;
    }
}

void handleReturning(RobotFSM* fsm)
{

    switch ( fsm->returnState ) {
        case HOMESEEK:
            handleHomeSeeking(fsm);
            break;
        case DEPOSIT:
            // handleDepositing(fsm);
            break;
        default:
            break;
    }
}

/*
SEARCHING SUB-STATE FUNCTIONS
*/
void handleSearching(RobotFSM* fsm)
{
    Serial.println("Searching");

    static int currentCount = 0;
    static bool completeRotation = false;

    if ( weightDetected() ) {
        fsm->huntState = CHASE;
        Serial.println("Weight Detected");
    }
    if ( completeRotation && !checkTargetHeading(fsm->targetHeading) ) {
        move(4000, -4000);
        // Clockwise rotation
        if ( rotationCounter - currentCount > ROTATION_FAILURE_TIMEOUT ) {
            Serial.println("Failed to rotate, resetting");
            completeRotation = false;
            currentCount = rotationCounter;
            fsm->targetHeading = GetOrientationYaw();
        }
    } else {
        moveForward(3300);
        completeRotation = false;
    }
    if ( rotationCounter - currentCount > ROTATION_TIMEOUT ) {
        Serial.println("Rotating");
        completeRotation = true;
        currentCount = rotationCounter;
        int targetHead = GetOrientationYaw() + 345;
        fsm->targetHeading = truncateHeading(targetHead);
        // Checks whether target heading is in the range 0-360
    }
}

void handleChasing(RobotFSM* fsm)
{

    static double rangeLeft = 0;
    static double rangeRight = 0;
    static int distToTravel = 0;

    static bool linedUp = false;
    static bool calculatedDist = false;

    if (!calculatedDist ) {
        // Check for weight directly ahead (highest priority)
        if (detectedCentreRight() || detectedCentreLeft()) {
            fsm->weightPos = AHEAD;
            Serial.println("Weight Ahead");
            moveForward(1500);  // Move towards the weight ahead
        } else if (fsm->weightPos == ON_FAR_LEFT) {
            if (!detectedCentreLeft() && !detectedCentreRight()) {
                // Continue rotating clockwise (towards far left) until it is detected by the center left sensor
                Serial.println("Pursuing weight on Far Left, rotating clockwise");
                move(-1800, 1800);
            } else {
                // Weight is now in the central sensor range
                fsm->weightPos = AHEAD;  // Transition to AHEAD state
                Serial.println("Weight detected by central sensor, switching to AHEAD");
            }
        } else if (fsm->weightPos == ON_FAR_RIGHT) {
            if (!detectedCentreRight() && !detectedCentreLeft()) {
                // Continue rotating counter-clockwise (towards far right) until it is detected by the center right sensor
                Serial.println("Pursuing weight on Far Right, rotating counter-clockwise");
                move(1800, -1800);
            } else {
                // Weight is now in the central sensor range
                fsm->weightPos = AHEAD;  // Transition to AHEAD state
                Serial.println("Weight detected by central sensor (Right), switching to AHEAD");
            }
        } else if (fsm->weightPos != ON_FAR_LEFT && fsm->weightPos != ON_FAR_RIGHT) {
            if (detectedFarRight()) {
                // Latch onto far right and ignore the left
                fsm->weightPos = ON_FAR_RIGHT;
                Serial.println("Detected Weight on Far Right, rotating to pursue");
                move(1800, -1800);  // Start rotating counter-clockwise
            } 
            else if (detectedFarLeft()) {
                // Latch onto far left and ignore the right
                fsm->weightPos = ON_FAR_LEFT;
                Serial.println("Detected Weight on Far Left, rotating to pursue");
                move(-1800, 1800);  // Start rotating clockwise
            } 
            else {
                Serial.println("No weight anywhere");
            }
        }

        if ( fsm->weightPos == AHEAD ) {
            if ( !detectedCentreRight() && !detectedCentreLeft() ) {
                Serial.print("Weight Ahead!, but not seen");  // May need to change this based on dist
                moveForward(1000);
            } else if ( detectedCentreRight() && !detectedCentreLeft() ) {
                Serial.print("Weight seen only by right");
                if ( GetL1BR() < SENSOR_LOGIC_CROSSOVER_LOW ) {
                    Serial.print(" at close range, rotate CW");
                    // Rotate CW but also move backwards
                    move(700, -1200);
                } else if (GetL1BR() > SENSOR_LOGIC_CROSSOVER_HIGH) {
                    Serial.print(" at far range, rotate CCW");
                    // Rotate CCW but also move forwards
                    move(0, 1200);
                }
            } else if ( detectedCentreLeft() && !detectedCentreRight() ) {
                Serial.print("Weight seen only by left");
                if ( GetL1BL() < SENSOR_LOGIC_CROSSOVER_LOW ) {
                    Serial.print(" at close range, rotate CCW");
                    // Rotate CCW but also move backwards
                    move(-1200, 700);
                } else if (GetL1BL() > SENSOR_LOGIC_CROSSOVER_HIGH) {
                    Serial.print(" at far range, rotate CW");
                    // Rotate CW but also move forwards
                    move(1200, 0);
                }
            } else {
                Serial.print("Weight in both sights ");
                if ( !linedUp ) {
                    Serial.print("but not lined up!");
                    rangeLeft = GetL1BL();
                    rangeRight = GetL1BR();
                    if (rangeLeft > rangeRight) {
                        move(800, -800);
                        Serial.println(" Rotating CW!");
                    } else {
                        move(-800, 800);
                        Serial.println(" Rotating CCW!");
                    }
                    linedUp = isLinedUp(rangeLeft, rangeRight);
                } else {
                    Serial.println("Is lined up!");
                    if ( !calculatedDist ) {
                        double s = (230 + rangeLeft + rangeRight) / 2;
                        int area = sqrt(s*(s-rangeLeft)*(s-rangeRight)*(s-230));
                        distToTravel = 2 * area / 230 + LINEAR_OFFSET;
                        Serial.print("Distance to travel: ");
                        Serial.println(distToTravel);
                        leftMotor.targetMotorPos = (distToTravel) * ENCODER_PER_DIST  + leftMotor.currentMotorPos;
                        rightMotor.targetMotorPos = (distToTravel) * ENCODER_PER_DIST + rightMotor.currentMotorPos;
                        fsm->distToTravel = distToTravel;
                        calculatedDist = true;
                    }
                }
            }
        }
    } else {                                                                                                                       // If the distance has been calculated
        moveForward(3000);
        if (!ReadInductiveSensor()) {  // Might have to change to current
            fsm->huntState = COLLECT;
            collectionJawsWatchDog = 0;
        }
        Serial.print("Left Motor Error: ");
        Serial.print(leftMotor.targetMotorPos - leftMotor.currentMotorPos);
        Serial.print(" Right Motor Error: ");
        Serial.println(rightMotor.targetMotorPos - rightMotor.currentMotorPos);
    }

    /**
     * Weight Watchdog
     */


    if ( weightDetected() ) {
        weightWatchDog = 0;
    } else if ( weightWatchDog > LOST_WEIGHT_TIMEOUT ) {
        Serial.println("Lost weight");
        fsm->huntState = SEARCH;
        fsm->weightPos = NONE;
        weightWatchDog = 0;
    }
}

void handleCollecting(RobotFSM * fsm)
{
    Serial.println("Collecting");
    leftMotor.servoDriver.writeMicroseconds(1500);
    rightMotor.servoDriver.writeMicroseconds(1500);
    static bool atLimit = false;
    static bool timeOut = false;
    bool atLimitSwtichDatum = !CollectorPosition();

    if (!timeOut) {
        Serial.print("0");
        if (!atLimit) {
            Serial.print("1");
            if(atLimitSwtichDatum) {
                Serial.println("At limit");
                atLimit = true;
                collectionMotor.currentMotorPos = 0;
                collectionMotor.targetMotorPos = collectionMotorPos + COLLECTOR_STOP_POS;
            } else {
                SetMotorSpeed(&collectionMotor, MAX_MOTOR_VAL);
            }
        } else {
            Serial.print("targetMotorPos: ");
            Serial.print(collectionMotor.targetMotorPos);
            Serial.print(" currentMotorPos: ");
            Serial.println(collectionMotor.currentMotorPos);
            if (collectionMotor.targetMotorPos >= collectionMotorPos) {
                SetMotorSpeed(&collectionMotor, MOTOR_STOP_VAL);
                atLimit = false;
                collectorActuating = false;
                fsm->huntState = SEARCH;
                fsm->currentState = HUNTING;
                fsm->lastMainState = HUNTING;
            }
        }
    }

    if ( collectionJawsWatchDog > COLLECTION_TIMEOUT ) {
        timeOut = true;
        // Needs a way to get out of the stuck weight
        Serial.println("Stuck weight");
        if ( (abs(collectionMotor.targetMotorPos - collectionMotor.currentMotorPos - COLLECTOR_TICKS_PER_REV)) > COLLECTOR_STOP_THRESHOLD ) {
            SetMotorSpeed(&collectionMotor, MIN_MOTOR_VAL);
        } else {
            SetMotorSpeed(&collectionMotor, MOTOR_STOP_VAL);
            atLimit = false;
            collectorActuating = false;
            fsm->huntState = SEARCH;
            fsm->currentState = HUNTING;
            fsm->lastMainState = HUNTING;
            collectionJawsWatchDog = 0;
        }
    }
}

/*
RETURNING SUB-STATE FUNCTIONS
*/
void handleHomeSeeking(RobotFSM * fsm)
{
        // float heading_error = truncateHeading(targetHeading - GetOrientationYaw());
        // leftMotor.targetMotorSpeed = 1800 + heading_error * 10;
        // rightMotor.targetMotorSpeed = 1800 - heading_error * 10;
        // if ( abs(heading_error) < 5 && colourSensor == HOME ) {
        //     fsm->returnState = DEPOSIT;
        // }
}

void handleDepositing(RobotFSM * fsm)
{
    // ifColourSensor
    //
    // Set Heading to reverse of inital heading
    // Open jaws
    // Lift Ramp
    // Then lower ramp
    // Reset weights deposited
    // fsm->currentState = HUNTING;
    // fsm->lastMainState = HUNTING;
    // fsm->huntState = SEARCH;
}