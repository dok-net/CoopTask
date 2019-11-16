# Warning
In this release on ESP8266 scanning WiFi networks and WPS are both broken in CoopTask tasks.
Using these features directly from loop() is unaffected.
This warning will be silently removed once ESP8266 Git master has accepted the necessary PR.

# CoopTask

An all-C++ implementation of a cooperative multitasking layer for ESP8266/ESP32,
Arduino boards, Linux, and Windows x86 and x86_64

During regular development it's built and tested on the ESP MCUs and
Arduino Pro/Pro Mini.

Tasks in this scheduler are stackful coroutines. They act almost the same as
the main ``setup()``/``loop()`` code in Arduino sketches, but there can be many of them
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
    taskBlink = createCoopTask(F("Blink"), loopBlink, 0x240);
#else
    taskBlink = createCoopTask(F("Blink"), loopBlink, 0x40);
#endif
    if (!taskBlink) Serial.println("CoopTask Blink not created");
}

void loop()
{
    runCoopTasks();
}
```

The ``runCoopTasks()`` scheduling helper has two optional callback arguments.

The first, ``reaper``, gets called each time a task exits. Retrieving the
exit code or deleting the CoopTask object would typically be performed in a
task reaper function.

The second callback, ``onDelay``, is called after each scheduling rountrip with the
total minimum delay (can be zero) of all managed tasks. A use scenario for this
is to put the MCU into a power saving sleep mode for the given duration.

## Using Arduino or Linux default loop stack space for CoopTask
Given that CoopTasks are scheduled from the Arduino default ``loop()`` or the
``main()`` function on Linux, any code in these functions is non-cooperative.
This is great for incompatible sketches or libraries, but otherwise puts the
memory of that main stack to waste. It is therefore good practice to allocate
the local stack for a single, infinitely running, CoopTask on the main stack.
Reserve enough stack to remain for ``loop()`` internals. In its most simple
form, borrowing from the example above, where taskBlink meets the requirement
of never returning, a CoopTask that uses the default stack space is created
like so:

```
#if defined(ESP8266) || defined(ESP32)
    taskBlink = createCoopTask<int, CoopTaskStackAllocatorFromLoop<>>(
        F("Blink"), loopBlink, 0x240);
#else
    taskBlink = createCoopTask<int, CoopTaskStackAllocatorFromLoop<>>(
        F("Blink"), loopBlink, 0x40);
#endif
```

## ESP8266 Core For Arduino specifics
ESP8266 Core For Arduino release 2.6.0 and later include all support for this
release of CoopTask.

## Arduino-ESP32 specifics
The ESP32 runs the Arduino API on top of the FreeRTOS real-time operating system.
This OS has all the capabilities for real-time programming and offers prioritized,
preemptive multitasking. The purpose of CoopTask on the other hand is to take
the complexity out of multi-threaded/tasked programming, and offers a cooperative
multi-tasking scheme instead.

Arduino-ESP32 has the necessary support for CoopTask beginning with
commit-ish c2b3f2d, dated Oct 4 2019, in Github master branch post release 1.4.0.

For Arduino sketches, and the libraries used in these, that never use the global
Arduino ``delay()``, don't make use of FreeRTOS ``vTaskDelay()``, and implement
delays only ever using the CoopTask metaphor ``CoopTaskBase::delay()``, CoopTask
would not require anything specific for the ESP32.

If the convenient Arduino ``delay()`` does get used, or there is any chance that
the FreeRTOS ``vTaskDelay()`` gets used, though, on the ESP32 it is necessary to
prevent unsolicited preemptive concurrency and control the CPU time for the
idle task.

This is being taken care of by CoopTask when using the ``runCoopTasks()``
scheduling helper in the Sketch ``loop()`` function.
