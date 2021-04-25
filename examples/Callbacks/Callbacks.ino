#include <CoopTaskBase.h>
#include <CoopTask.h>
#include <CoopSemaphore.h>
#include <CoopMutex.h>
#include <BasicCoopTask.h>

#if defined(ARDUINO_attiny)
#define LED_BUILTIN 1

struct DummySerial {
    void print(const __FlashStringHelper* s = nullptr) {}
    void println(const __FlashStringHelper* s = nullptr) {}
    void println(long unsigned int) {}
    void flush() {}
};
DummySerial Serial;
#endif

CoopTask<void>* blinkTask = nullptr;
CoopTask<void>* switchTask = nullptr;

void blinkFunction()
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    for (;;)
    {
        yield(); // A
        digitalWrite(LED_BUILTIN, LOW);
        delay(5000); // B
        digitalWrite(LED_BUILTIN, HIGH);
        CoopTask<void>::sleep(); // D
    }
}

void switchFunction()
{
    for (;;)
    {
        yield(); // A
        Serial.println(F("Switch on"));
        delay(100); // B
        Serial.println(F("Switch off"));
        CoopTask<void>::sleep(); // C
    }
}

bool delayCb(uint32_t ms)
{
    Serial.print(F("delayDb, ms = "));
    Serial.println(ms);
    delay(ms);
    return true;
}

bool sleepCb()
{
    Serial.println(F("sleepCb"));
    Serial.flush();
    delay(10000);
    if (blinkTask) blinkTask->wakeup();
    if (switchTask) switchTask->wakeup();
    return true;
}

void setup()
{
#if !defined(ARDUINO_attiny)
    Serial.begin(74880);
    while (!Serial);
    delay(100);
    Serial.println();
    Serial.println(F("runTasks callback test"));
#endif

    runCoopTasks(nullptr, delayCb, sleepCb);
    Serial.println(F("no tasks yet, sleepCb()?"));

    blinkTask = new CoopTask<void>(F("blink"), blinkFunction);
    switchTask = new CoopTask<void>(F("switch"), switchFunction);
    blinkTask->scheduleTask();
    switchTask->scheduleTask();
}

// Add the main program code into the continuous loop() function
void loop()
{
    runCoopTasks(nullptr, delayCb, sleepCb);
    Serial.println(F("A - both tasks yielded, no Cb?"));
    yield();
    runCoopTasks(nullptr, delayCb, sleepCb);
    Serial.println(F("B - both tasks delayed, delayCb(100)?"));
    yield();
    runCoopTasks(nullptr, delayCb, sleepCb);
    Serial.println(F("C - blink task delayed, switch task sleeping, delayCb(4900)?"));
    yield();
    runCoopTasks(nullptr, delayCb, sleepCb);
    Serial.println(F("D - both tasks sleeping, sleepCb()?"));
}
