#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "pico/util/queue.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define STEP_DELAY 2 // 0.002s
#define CALIBRATION_SENSOR 28
#define DROP_SENSOR 27
#define MOTOR_PIN_IN1 2
#define MOTOR_PIN_IN2 3
#define MOTOR_PIN_IN3 6
#define MOTOR_PIN_IN4 13

#define CALIBRATION_ROUNDS 3
#define HALF_OF_OPENING_LENGTH 155
#define N 8

#define BUFFER_SIZE 256

void initializePins();
void goForwards(int *currentStep);
void goBackwards(int *currentStep);
void goForwardsN(int *currentStep, const int stepsToTake);
void goBackwardsN(int *currentStep, const int stepsToTake);
int calibrate(int *currentStep, bool *calibrationTracker);
void divideIntoNParts(int dividedStepCount[], const int averageStepCount, const int parts);
void handleCommands(int *commandToExecute, bool *commandReceivedStatus, int *n);
bool parseNumberFromString(char *string, int *number, const int stringLength);

int stepSequence[8][4] = {
    {1, 0, 0, 0},
    {1, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0},
    {0, 0, 1, 1},
    {0, 0, 0, 1},
    {1, 0, 0, 1}};

int main()
{
    stdio_init_all();

    bool isCalibrated = false;
    int step = 0;
    // int direction = 1; // 1 = forwards, -1 = backwards
    //  int step_count = 4096; // 5.625*(1/64) per step, 4096 steps is 360°
    int nSteps = 0;
    int dividedStepCount[N] = {0};
    int dividedStepCountIndex = 0;
    int command = 0;
    bool commandReceivedStatus = false;
    int n = 0;

    initializePins();

    while (1)
    {
        if (uart_is_readable(uart0))
        {
            handleCommands(&command, &commandReceivedStatus, &n);
        }

        if (commandReceivedStatus == true)
        {
            switch (command)
            {
            case 1: // Calibrate

                printf("Calibrating...\n");
                nSteps = calibrate(&step, &isCalibrated);
                divideIntoNParts(dividedStepCount, nSteps, N);
                printf("Calibration done!\n");
                break;

            case 2: // Status info

                if (isCalibrated == false)
                {
                    printf("Status not available!\n");
                    break;
                }
                else
                {
                    printf("Calibration status: %s\n", isCalibrated ? "true" : "false");
                    printf("Step count: %d\n", nSteps);
                    break;
                }

            case 3: // Run

                printf("Running...\n");

                if (isCalibrated == false)
                {
                    printf("Motor not calibrated!\n");
                    break;
                }

                if (n > 0)
                {
                    for (int i = 0; i < n; i++)
                    {
                        goForwardsN(&step, dividedStepCount[dividedStepCountIndex]);
                        dividedStepCountIndex++;
                        if (dividedStepCountIndex > N - 1)
                        {
                            dividedStepCountIndex = 0;
                        }
                    }
                }
                else if (n < 0)
                {
                    for (int i = 0; i > n; i--)
                    {
                        goBackwardsN(&step, dividedStepCount[dividedStepCountIndex]);
                        dividedStepCountIndex--;
                        if (dividedStepCountIndex < 0)
                        {
                            dividedStepCountIndex = N - 1;
                        }
                    }
                }
                else
                {
                    for (int i = 0; i < N; i++)
                    {
                        goForwardsN(&step, dividedStepCount[dividedStepCountIndex]);
                        dividedStepCountIndex++;
                        if (dividedStepCountIndex > N - 1)
                        {
                            dividedStepCountIndex = 0;
                        }
                    }
                }

                printf("Run done!\n");
                break;

            default:

                printf("WTF?\n");

                break;
            }

            commandReceivedStatus = false;
            command = 0;
        }
    }

    return 0;
}

void initializePins()
{
    printf("FUNCTION: initializePins\n");
    // Initialize GPIO pins
    gpio_init(CALIBRATION_SENSOR);
    gpio_init(DROP_SENSOR);
    gpio_init(MOTOR_PIN_IN1);
    gpio_init(MOTOR_PIN_IN2);
    gpio_init(MOTOR_PIN_IN3);
    gpio_init(MOTOR_PIN_IN4);

    // Set GPIO directions
    gpio_set_dir(CALIBRATION_SENSOR, GPIO_IN);
    gpio_pull_up(CALIBRATION_SENSOR);

    gpio_set_dir(DROP_SENSOR, GPIO_IN);
    gpio_pull_up(DROP_SENSOR);

    gpio_set_dir(MOTOR_PIN_IN1, GPIO_OUT);
    gpio_set_dir(MOTOR_PIN_IN2, GPIO_OUT);
    gpio_set_dir(MOTOR_PIN_IN3, GPIO_OUT);
    gpio_set_dir(MOTOR_PIN_IN4, GPIO_OUT);
}

void goForwards(int *currentStep)
{
    if (*currentStep > 7)
    {
        *currentStep = 0;
    }
    gpio_put(MOTOR_PIN_IN1, stepSequence[*currentStep][0]);
    gpio_put(MOTOR_PIN_IN2, stepSequence[*currentStep][1]);
    gpio_put(MOTOR_PIN_IN3, stepSequence[*currentStep][2]);
    gpio_put(MOTOR_PIN_IN4, stepSequence[*currentStep][3]);
    ++*currentStep;

    sleep_ms(STEP_DELAY);
}

void goBackwards(int *currentStep)
{
    if (*currentStep < 0)
    {
        *currentStep = 7;
    }
    gpio_put(MOTOR_PIN_IN1, stepSequence[*currentStep][0]);
    gpio_put(MOTOR_PIN_IN2, stepSequence[*currentStep][1]);
    gpio_put(MOTOR_PIN_IN3, stepSequence[*currentStep][2]);
    gpio_put(MOTOR_PIN_IN4, stepSequence[*currentStep][3]);
    --*currentStep;

    sleep_ms(STEP_DELAY);
}

void goForwardsN(int *currentStep, const int stepsToTake)
{
    printf("FUNCTION: goForwardsN\n");
    for (int i = 0; i < stepsToTake; i++)
    {
        if (*currentStep > 7)
        {
            *currentStep = 0;
        }
        gpio_put(MOTOR_PIN_IN1, stepSequence[*currentStep][0]);
        gpio_put(MOTOR_PIN_IN2, stepSequence[*currentStep][1]);
        gpio_put(MOTOR_PIN_IN3, stepSequence[*currentStep][2]);
        gpio_put(MOTOR_PIN_IN4, stepSequence[*currentStep][3]);
        ++*currentStep;

        sleep_ms(STEP_DELAY);
    }
}

void goBackwardsN(int *currentStep, const int stepsToTake)
{
    printf("FUNCTION: goBackwardsN\n");
    for (int i = 0; i < stepsToTake; i++)
    {
        if (*currentStep < 0)
        {
            *currentStep = 7;
        }
        gpio_put(MOTOR_PIN_IN1, stepSequence[*currentStep][0]);
        gpio_put(MOTOR_PIN_IN2, stepSequence[*currentStep][1]);
        gpio_put(MOTOR_PIN_IN3, stepSequence[*currentStep][2]);
        gpio_put(MOTOR_PIN_IN4, stepSequence[*currentStep][3]);
        --*currentStep;

        sleep_ms(STEP_DELAY);
    }
}

int calibrate(int *currentStep, bool *calibrationTracker)
{
    printf("FUNCTION: calibrate\n");
    int stepCounterArray[3] = {0, 0, 0};
    int stepCounter = 0;
    int roundCounter = 0;

    // Find calibration opening.
    while (gpio_get(CALIBRATION_SENSOR))
    {
        goForwards(currentStep);
    }

    // Go back to the start of the opening.
    while (!gpio_get(CALIBRATION_SENSOR))
    {
        goBackwards(currentStep);
    }

    // Re-find calibration opening.
    while (gpio_get(CALIBRATION_SENSOR))
    {
        goForwards(currentStep);
    }

    while (roundCounter < CALIBRATION_ROUNDS)
    {
        while (!gpio_get(CALIBRATION_SENSOR))
        {
            goForwards(currentStep);
            stepCounter++;
        }
        while (gpio_get(CALIBRATION_SENSOR))
        {
            goForwards(currentStep);
            stepCounter++;
        }
        stepCounterArray[roundCounter] = stepCounter;
        roundCounter++;
        stepCounter = 0;
    }

    int averageStepCount = 0;
    for (int i = 0; i < CALIBRATION_ROUNDS; i++)
    {
        printf("Step count %d: %d\n", i, stepCounterArray[i]);
        averageStepCount += stepCounterArray[i];
    }
    averageStepCount /= CALIBRATION_ROUNDS;

    for (int i = 0; i < (averageStepCount + HALF_OF_OPENING_LENGTH); i++)
    {
        goForwards(currentStep);
    }

    printf("Average step count: %d\n", averageStepCount);

    *calibrationTracker = true;
    return averageStepCount;
}

void divideIntoNParts(int dividedStepCount[], const int averageStepCount, const int parts)
{
    printf("FUNCTION: divideIntoNParts\n");
    int division = averageStepCount / parts;
    int remainder = averageStepCount % parts;

    for (int i = 0; i < parts - 1; ++i)
    {
        dividedStepCount[i] = division;
        printf("Divided step count %d: %d\n", i, dividedStepCount[i]);
    }

    dividedStepCount[parts - 1] = division + remainder;
    printf("Divided step count %d: %d\n", parts - 1, dividedStepCount[parts - 1]);
}

void handleCommands(int *commandToExecute, bool *commandReceivedStatus, int *n)
{
    printf("FUNCTION: handleCommands\n");
    sleep_ms(100); // Wait for the command to be fully received
    char buffer[BUFFER_SIZE] = {0};
    int bufferIndex = 0;

    // Assume that the command is not longer than 256 characters
    while (uart_is_readable(uart0))
    {
        buffer[bufferIndex] = uart_getc(uart0);
        bufferIndex++;
    }
    buffer[bufferIndex] = '\0';

    if (strncmp(buffer, "calib", 5) == 0)
    {
        *commandToExecute = 1;
        *commandReceivedStatus = true;
    }
    else if (strncmp(buffer, "status", 6) == 0)
    {
        *commandToExecute = 2;
        *commandReceivedStatus = true;
    }
    else if (strncmp(buffer, "run", 3) == 0)
    {
        if (bufferIndex == 3)
        {
            *n = N;
            *commandToExecute = 3;
            *commandReceivedStatus = true;
        }
        else if (parseNumberFromString(buffer, n, bufferIndex) == true)
        {
            *commandToExecute = 3;
            *commandReceivedStatus = true;
        }
        else
        {
            *commandReceivedStatus = false;
        }
    }
    else
    {
        printf("Unrecognized command!\n");
        *commandReceivedStatus = false;
    }
}

bool parseNumberFromString(char *string, int *number, const int stringLength)
{
    printf("FUNCTION: parseNumberFromString\n");

    // Find first digit.
    int firstDigitIndex = 0;
    while (!isdigit(string[firstDigitIndex]) && firstDigitIndex < stringLength)
    {
        firstDigitIndex++;
    }

    // Check if the number is negative.
    if (strncmp(&string[firstDigitIndex - 1], "-", 1) == 0)
    {
        firstDigitIndex--;
    }

    // Find last digit.
    int lastDigitIndex = firstDigitIndex + 1;
    while (isdigit(string[lastDigitIndex]) && lastDigitIndex < stringLength)
    {
        lastDigitIndex++;
    }

    // Copy the number string to a new buffer.
    char numberString[BUFFER_SIZE];
    for (int i = 0; i < lastDigitIndex - firstDigitIndex; i++)
    {
        numberString[i] = string[firstDigitIndex + i];
    }
    numberString[lastDigitIndex - firstDigitIndex] = '\0';

    printf("Number string: %s\n", numberString);

    char *endPtr;
    *number = strtol(numberString, &endPtr, 10);

    // Check for conversion errors
    if (endPtr == numberString)
    {
        printf("No valid number found!\n");
        return false;
    }
    else
    {
        printf("Parsed number: %d\n", *number);
        return true;
    }
}