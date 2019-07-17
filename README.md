# CoopTask
An all-C++ implementation of a cooperative multitasking layer for ESP8266/ESP32 and Arduino boards

Regular development builds and tests are performed for the ESP MCUs and Arduino Pro/Pro Mini.

Tasks in this scheduler behave almost the same as the main setup()/loop() code in Arduino sketches,
but there can be many of them on the same device simultaneously.

Use the normal global delay() function to suspend execution of a task for the given number of
milliseconds, use yield() to give up the CPU, both return after other cooperative tasks have run awhile.

A simple blink task can be written just like this:

```
int blink_task() {
	// like setup():
    pinMode(LED_BUILTIN, OUTPUT);
    
    // like loop():
    for (;;)
    {
        digitalWrite(LED_BUILTIN, LOW);
        delay(2000);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(2000);
    }
    
    // tasks can return or exit() and leave an exit code
    return 0;
}
``` 