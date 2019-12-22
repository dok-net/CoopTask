/*
 Multiple Blinks

 Ported to CoopTask from the version that
 demonstrates the use of the Scheduler library for the Arduino Due.
 CoopTask works also on Arduino AVR, ESP8266, ESP32, and more.

 Hardware required :
 * LEDs connected to pins 11, 12, and 13

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

int led1 = 13;
int led2 = 12;
int led3 = 11;

void setup() {
    Serial.begin(9600);

    // Setup the 3 pins as OUTPUT
    pinMode(led1, OUTPUT);
    pinMode(led2, OUTPUT);
    pinMode(led3, OUTPUT);

    // Add "loop1", "loop2" and "loop3" to CoopTask scheduling.
    // "loop" is always started by default, and is not under the control of CoopTask. 
    createCoopTask<void>("loop1", loop1);
    createCoopTask<void>("loop2", loop2);
    createCoopTask<void>("loop3", loop3);
}

void taskReaper(const CoopTaskBase* const task)
{
    delete task;
}

void loop() {
    // loops forever by default
    runCoopTasks(taskReaper);
}

// Task no.1: blink LED with 1 second delay.
void loop1() {
    for (;;) // explicitly run forever without returning
    {
        digitalWrite(led1, HIGH);

        // IMPORTANT:
        // When multiple tasks are running 'delay' passes control to
        // other tasks while waiting and guarantees they get executed.
        delay(1000);

        digitalWrite(led1, LOW);
        delay(1000);
    }
}

// Task no.2: blink LED with 0.1 second delay.
void loop2() {
    for (;;) // explicitly run forever without returning
    {
        digitalWrite(led2, HIGH);
        delay(100);
        digitalWrite(led2, LOW);
        delay(100);
    }
}

// Task no.3: accept commands from Serial port
// '0' turns off LED
// '1' turns on LED
void loop3() {
    for (;;) // explicitly run forever without returning
    {
        if (Serial.available()) {
            char c = Serial.read();
            if (c == '0') {
                digitalWrite(led3, LOW);
                Serial.println("Led turned off!");
            }
            if (c == '1') {
                digitalWrite(led3, HIGH);
                Serial.println("Led turned on!");
            }
        }

        // IMPORTANT:
        // We must call 'yield' at a regular basis to pass
        // control to other tasks.
        yield();
    }
}
