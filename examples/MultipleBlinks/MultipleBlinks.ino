/*
 Multiple Blinks

 Ported to CoopTask from the version that
 demonstrates the use of the Scheduler library for the Arduino Due.
 CoopTask works on Arduino AVR (including ATtiny), ESP8266, ESP32, ARM Linux and PC OSs.

 created 8 Oct 2012
 by Cristian Maglie
 Modified by
 Scott Fitzgerald 19 Oct 2012
 Ported to CoopTask by
 Dirk O. Kaar 22 Dec 2019

 This example code is in the public domain

 http://www.arduino.cc/en/Tutorial/MultipleBlinks
*/

// Include CoopTask since we want to manage multiple tasks.
#include <CoopTask.h>
#include <CoopSemaphore.h>

// ATtiny85 max. working memory utilization, AVR 1.8.2 toolchain:
// "Minimum Memory Usage: 357 bytes (70% of a 512 byte maximum)"

#if defined(ARDUINO_attiny)
#define LED_BUILTIN 1
#endif

#if defined(ARDUINO_AVR_MICRO)
#define STACKSIZE_8BIT 92
#else
#define STACKSIZE_8BIT 40
#endif

CoopSemaphore taskSema(1, 1);
int taskToken = 1;

// Task no.1: blink LED with 1 second delay.
void loop1() {
    for (;;) // explicitly run forever without returning
    {
        taskSema.wait();
        if (1 != taskToken)
        {
            taskSema.post();
            yield();
            continue;
        }
        for (int i = 0; i < 3; ++i)
        {
            digitalWrite(LED_BUILTIN, HIGH);

            // IMPORTANT:
            // When multiple tasks are running 'delay' passes control to
            // other tasks while waiting and guarantees they get executed.
            delay(1000);

            digitalWrite(LED_BUILTIN, LOW);
            delay(1000);
        }
        taskToken = 2;
        taskSema.post();
    }
}

// Task no.2: blink LED with 0.25 second delay.
void loop2() {
    for (;;) // explicitly run forever without returning
    {
        taskSema.wait();
        if (2 != taskToken)
        {
            taskSema.post();
            yield();
            continue;
        }
        for (int i = 0; i < 6; ++i)
        {
            digitalWrite(LED_BUILTIN, HIGH);

            // IMPORTANT:
            // When multiple tasks are running 'delay' passes control to
            // other tasks while waiting and guarantees they get executed.
            delay(250);

            digitalWrite(LED_BUILTIN, LOW);
            delay(250);
        }
        taskToken = 3;
        taskSema.post();
    }
}

// Task no.3: blink LED with 0.05 second delay.
void loop3() {
    for (;;) // explicitly run forever without returning
    {
        taskSema.wait();
        if (3 != taskToken)
        {
            taskSema.post();
            yield();
            continue;
        }
        for (int i = 0; i < 6; ++i)
        {
            digitalWrite(LED_BUILTIN, HIGH);

            // IMPORTANT:
            // When multiple tasks are running 'delay' passes control to
            // other tasks while waiting and guarantees they get executed.
            delay(50);

            digitalWrite(LED_BUILTIN, LOW);
            delay(50);
        }
        taskToken = 1;
        taskSema.post();
    }
}

BasicCoopTask<CoopTaskStackAllocatorAsMember<sizeof(unsigned) >= 4 ? 800 : STACKSIZE_8BIT>> task1("l1", loop1);
BasicCoopTask<CoopTaskStackAllocatorAsMember<sizeof(unsigned) >= 4 ? 800 : STACKSIZE_8BIT>> task2("l2", loop2);
BasicCoopTask<CoopTaskStackAllocatorFromLoop<sizeof(unsigned) >= 4 ? 800 : STACKSIZE_8BIT>> task3("l3", loop3, sizeof(unsigned) >= 4 ? 800 : STACKSIZE_8BIT);

void setup() {
    //Serial.begin(9600);
    // Setup the 3 pins as OUTPUT
    pinMode(LED_BUILTIN, OUTPUT);

    // Add "loop1", "loop2" and "loop3" to CoopTask scheduling.
    // "loop" is always started by default, and is not under the control of CoopTask. 
    task1.scheduleTask();
    task2.scheduleTask();
    task3.scheduleTask();
}

void loop() {
    // loops forever by default
    runCoopTasks();
}
