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

CoopTask<>* taskBlink;

void setup()
{
    Serial.begin(115200);
    delay(500);

#if defined(ESP8266) || defined(ESP32)
    taskBlink = new CoopTask<>(F("Blink"), loopBlink, 0x240);
#else
    taskBlink = new CoopTask<>(F("Blink"), loopBlink, 0x40);
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

## Additional Arduino-ESP32 specifics
The ESP32 runs the Arduino API on top of the FreeRTOS real-time operating system.
This OS has all the capabilities for real-time programming and offers prioritized,
preemptive multitasking. The purpose of CoopTask on the other hand is to take
the complexity out of multi-threaded/tasked programming, and offers a cooperative
multi-tasking scheme instead.

Arduino-ESP32 has the necessary support for CoopTask beginning with
commit-ish c2b3f2d, dated Oct 4 2019, in Github master branch post release 1.4.0.

For Arduino sketches, and the libraries used in these, that never use the global
Arduino `delay()`, don't make use of FreeRTOS `vTaskDelay()`, and implement
delays only ever using the CoopTask metaphor `CoopTaskBase::delay()`, CoopTask
doesn't require anything specific for the ESP32.

If there is any chance that the Arduino or FreeRTOS delay gets used though,
on the ESP32 it is necessary to prevent unsolicited preemptive concurrency
and control CPU time for the idle task.

See the examples/mutex or examples/blinkbuttonandweb for sample code. Basically:

```
#ifdef ESP32
TaskHandle_t yieldGuardHandle;
#endif

void setup()
{
// ...

#ifdef ESP32
    xTaskCreateUniversal([](void*)
        {
            for (;;)
            {
                vPortYield();
            }
        }, "YieldGuard", 0x200, nullptr, 1, &yieldGuardHandle, CONFIG_ARDUINO_RUNNING_CORE);
#endif
}

void loop() {
#if !defined(ESP8266)
    uint32_t taskCount = 0;
    uint32_t minDelay = ~0UL;
    for (int i = 0; i < CoopTaskBase::getRunnableTasks().size(); ++i)
    {
        auto task = CoopTaskBase::getRunnableTasks()[i].load();
        if (task)
        {
            auto runResult = task->run();
            if (runResult < 0)
            {
            	// probably want to identify task, and inspect return value
                // delete task;
            }
            if (task->delayed() && runResult < minDelay) minDelay = runResult;
            if (++taskCount >= CoopTaskBase::getRunnableTasksCount()) break;
        }
    }
#ifdef ESP32
    if (minDelay)
    {
        vTaskSuspend(yieldGuardHandle);
        vTaskDelay(1);
        vTaskResume(yieldGuardHandle);
    }
#endif
#endif
}
```