# CoopTask
An all-C++ implementation of a cooperative multitasking layer for ESP8266/ESP32,
Arduino boards, Linux, and Windows x86 and x86_64

During regular development it's built and tested on the ESP MCUs and
Arduino Pro/Pro Mini.

Tasks in this scheduler are stackful coroutines. They act almost the same as
the main setup()/loop() code in Arduino sketches, but there can be many of them
simultaneously on the same device. It's even powerful enough to run the
ESP8266 and ESP32 WebServer in a CoopTask.

Use the normal global delay() function to suspend execution of a task for the
given number of milliseconds, use yield() to give up the CPU, both return after
other cooperative tasks have run awhile.

A simple blink task can be written just like this:

```
#include <CoopTask.h>

int loopBlink()
{
	// like setup():
    pinMode(LED_BUILTIN, OUTPUT);
    
    // like loop():
    for (;;)
    {
        digitalWrite(LED_BUILTIN, LOW);
        delay(2000);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(3000);
    }

    // tasks can return or exit() and leave an exit code
    return 0;
}

CoopTask* taskBlink;

void setup()
{
    Serial.begin(115200);
    delay(500);

#if defined(ESP8266) || defined(ESP32)
    taskBlink = new CoopTask(F("Blink"), loopBlink, 0x240);
#else
    taskBlink = new CoopTask(F("Blink"), loopBlink, 0x40);
#endif
    if (!*taskBlink) Serial.println("CoopTask Blink out of stack");

//#ifdef ESP8266
//    scheduleTask(taskBlink);
//#endif
}

//#ifndef ESP8266
uint32_t taskBlinkRunnable = 1;
//#endif

void loop()
{
//#ifndef ESP8266
    if (taskBlinkRunnable != 0) taskBlinkRunnable = taskBlink->run();
//#endif
}
```