#include <CoopTask.h>
#include <CoopMutex.h>
#include <CoopSemaphore.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

ESP8266WebServer server(80);
#elif defined(ESP32)
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>

WebServer server(80);
#endif

#if !defined(ESP8266) && !defined(ESP32)
#define ICACHE_RAM_ATTR
#endif

#ifndef IRAM_ATTR
#define IRAM_ATTR ICACHE_RAM_ATTR
#endif

#if defined(ESP8266)
constexpr auto LEDON = LOW;
constexpr auto LEDOFF = HIGH;
#else
constexpr auto LEDON = HIGH;
constexpr auto LEDOFF = LOW;
#endif

#if defined(ESP32)
#define BUTTON1 17
//#define BUTTON1 GPIO_NUM_27
#elif defined(ARDUINO_ESP8266_WEMOS_D1MINI)
#define BUTTON1 D3
#else
#define BUTTON1 0
#endif

#define USE_BUILTIN_TASK_SCHEDULER

#if defined(ESP8266) || defined(ESP32)
class Button {
protected:
    CoopSemaphore& reportSema;
public:
    Button(uint8_t reqPin, CoopSemaphore& _reportSema) : reportSema(_reportSema), pushSema(0), PIN(reqPin) {
        pinMode(PIN, INPUT_PULLUP);
        attachInterruptArg(PIN, Button::buttonIsr_static, this, FALLING);
    };
    ~Button() {
        detachInterrupt(PIN);
    }

    CoopSemaphore pushSema;

    void IRAM_ATTR buttonIsr() {
        numberKeyPresses += 1;
        pressed = true;
        pushSema.post();
        reportSema.post();
    }

    static void IRAM_ATTR buttonIsr_static(void* const self) {
        reinterpret_cast<Button*>(self)->buttonIsr();
    }

    unsigned testResetPressed() {
        if (pressed) {
            Serial.printf("Button on pin %u has been pressed %u times\n", PIN, numberKeyPresses);
            pressed = false;
        }
        return numberKeyPresses;
    }

private:
    const uint8_t PIN;
    volatile unsigned numberKeyPresses = 0;
    volatile bool pressed = false;
};
#endif

CoopMutex serialMutex;

CoopSemaphore blinkSema(0);

void loopBlink() noexcept
{
    for (;;)
    {
        digitalWrite(LED_BUILTIN, LEDOFF);
        blinkSema.wait(1000);
        digitalWrite(LED_BUILTIN, LEDON);
        CoopTask<>::delay(4000);
    }
}

#if defined(ESP8266) || defined(ESP32)
Button* button1;

void loopButton() noexcept
{
    int count = 0;
    for (;;)
    {
        if (!button1->pushSema.wait())
        {
            Serial.println("loopButton: wait failed");
            yield();
            continue;
        }
        else
        {
            ++count;
        }
        {
            CoopMutexLock serialLock(serialMutex);

            Serial.print("loopButton: count = ");
            Serial.println(count);
        }
        if (nullptr != button1 && 8000 < button1->testResetPressed()) {
            delete button1;
            button1 = nullptr;
            CoopTask<>::exit();
        }
        yield();
    }
}

void handleRoot() {
    server.send(200, "text/plain", "hello from esp8266!");
}

void handleNotFound() {
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i = 0; i < server.args(); i++) {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
}
#endif

#if defined(ESP8266) || defined(ESP32)
CoopTask<void>* taskButton;
#endif
CoopTask<void, CoopTaskStackAllocatorFromLoop<>>* taskBlink;
CoopTask<unsigned>* taskText;
CoopTask<void>* taskReport0;
CoopTask<void>* taskReport1;
CoopTask<void>* taskReport2;
#if defined(ESP8266) || defined(ESP32)
CoopTask<void>* taskReport3;
CoopTask<void>* taskReport4;
CoopTask<void>* taskWeb;
#endif
CoopSemaphore reportSema(0);

void printStackReport(CoopTaskBase* task)
{
    if (!task) return;
    Serial.print(task->name().c_str());
    Serial.print(" free stack = ");
    Serial.println(task->getFreeStack());
}

uint32_t iterations = 0;
uint32_t start;

// to demonstrate that yield and delay work in subroutines
void printReport()
{
    //CoopTask<>::delayMicroseconds(4000000);
    Serial.print("cycle period/us = ");
    if (iterations)
    {
        Serial.println(1.0F * (micros() - start) / iterations);
    }
    else
    {
        Serial.println("N/A");
    }
#if defined(ESP8266) || defined(ESP32)
    printStackReport(taskButton);
#endif
    printStackReport(taskBlink);
    printStackReport(taskText);
    printStackReport(CoopTask<>::self());
#if defined(ESP8266) || defined(ESP32)
    printStackReport(taskWeb);
#endif

    iterations = 0;
};

class RAIITest
{
public:
    ~RAIITest()
    {
        CoopMutexLock serialLock(serialMutex);
        Serial.print(CoopTaskBase::self()->name());
        Serial.println(" stack unwound, RAIITest object destructed");
    }
};

void setup()
{
#ifdef ESP8266
    Serial.begin(74880);
#else
    Serial.begin(115200);
#endif
    while (!Serial) {}
    delay(500);

    Serial.println("Scheduler test");

#if defined(ESP8266) || defined(ESP32)
    WiFi.mode(WIFI_STA);
    WiFi.begin();

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    if (MDNS.begin("esp")) {
        Serial.println("MDNS responder started");
    }

    server.on("/", handleRoot);

    server.on("/inline", []() {
        server.send(200, "text/plain", "this works as well");
        });

    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("HTTP server started");
#endif


#if defined(ESP8266) && defined(USE_BUILTIN_TASK_SCHEDULER)
    CoopTaskBase::useBuiltinScheduler();
#endif

    pinMode(LED_BUILTIN, OUTPUT);

#if defined(ESP8266) || defined(ESP32)
    button1 = new Button(BUTTON1, reportSema);

    taskButton = new CoopTask<void>(F("Button"), loopButton,
#if defined(ESP8266)
        0x700);
#elif defined(ESP32)
        0x940);
#endif
    if (!*taskButton) Serial.printf("CoopTask %s out of stack\n", taskButton->name().c_str());
#endif

    taskBlink = new CoopTask<void, CoopTaskStackAllocatorFromLoop<>>(F("Blink"), loopBlink,
#if defined(ESP8266)
        0x400);
#elif defined(ESP32)
        0x540);
#else
        0x40);
#endif
    if (!*taskBlink) Serial.println("CoopTask Blink out of stack");

    taskText = new CoopTask<unsigned>(F("Text"), []() -> unsigned
        {
            RAIITest raii;
            Serial.println("Task1 - A");
            yield();
            Serial.println("Task1 - B");
            uint32_t start = millis();
            CoopTask<>::delay(6000);
            {
                CoopMutexLock serialLock(serialMutex);
                Serial.print("!!!Task1 - C - ");
                Serial.println(millis() - start);
                printStackReport(taskText);
            }
#if !defined(ARDUINO)
            throw static_cast<unsigned>(41);
#endif
            //CoopTask<unsigned>::exit(42);
            return 43;
        }
#if defined(ESP8266)
    , 0x380);
#elif defined(ESP32)
    , 0x4c0);
#else
    , 0x70);
#endif
    if (!*taskText) Serial.println("CoopTask Text out of stack");

    auto reportFunc = []() noexcept
    {
        uint32_t count = 0;
        for (;;) {
            if (!reportSema.wait(120000))
            {
                {
                    CoopMutexLock serialLock(serialMutex);
                    Serial.print(CoopTaskBase::self()->name().c_str());
                    Serial.println(": wait failed");
                }
                yield();
                continue;
            }
            {
                CoopMutexLock serialLock(serialMutex);
                Serial.print(CoopTask<>::self()->name());
                Serial.print(" (");
                Serial.print(++count);
                Serial.println("x)");
                printReport();
            }
            yield();
            reportSema.setval(0);
        }
    };
    taskReport0 = new CoopTask<void>(F("Report0"), reportFunc
#if defined(ESP8266) || defined(ESP32)
        , 0x600);
#else
        , 0x70);
#endif
    if (!*taskReport0) Serial.println("CoopTask Report out of stack");
    taskReport1 = new CoopTask<void>(F("Report1"), reportFunc
#if defined(ESP8266) || defined(ESP32)
        , 0x600);
#else
        , 0x70);
#endif
    if (!*taskReport1) Serial.println("CoopTask Report out of stack");
    taskReport2 = new CoopTask<void>(F("Report2"), reportFunc
#if defined(ESP8266) || defined(ESP32)
        , 0x600);
#else
        , 0x70);
#endif
    if (!*taskReport2) Serial.println("CoopTask Report out of stack");

#if defined(ESP8266) || defined(ESP32)
    taskReport3 = new CoopTask<void>(F("Report3"), reportFunc
        , 0x600);
    if (!*taskReport3) Serial.println("CoopTask Report out of stack");
    taskReport4 = new CoopTask<void>(F("Report4"), reportFunc
        , 0x600);
    if (!*taskReport4) Serial.println("CoopTask Report out of stack");

    taskWeb = new CoopTask<void>(F("Web"), []() noexcept
        {
            for (;;) {
                server.handleClient();
#ifdef ESP8266
                MDNS.update();
#endif
                yield();
            }
        },
#if defined(ESP8266)
        0x800);
#else
        0xa00);
#endif
    if (!*taskWeb) Serial.printf("CoopTask %s out of stack\n", taskWeb->name().c_str());

    if (!taskButton->scheduleTask()) { Serial.printf("Could not schedule task %s\n", taskButton->name().c_str()); }
    if (!taskReport3->scheduleTask()) { Serial.printf("Could not schedule task %s\n", taskReport3->name().c_str()); }
    if (!taskReport4->scheduleTask()) { Serial.printf("Could not schedule task %s\n", taskReport4->name().c_str()); }
    if (!taskWeb->scheduleTask()) { Serial.printf("Could not schedule task %s\n", taskWeb->name().c_str()); }
#endif

    if (!taskBlink->scheduleTask()) { Serial.print("Could not schedule task "); Serial.println(taskBlink->name().c_str()); }
    if (!taskText->scheduleTask()) { Serial.print("Could not schedule task "); Serial.println(taskText->name().c_str()); }
    if (!taskReport0->scheduleTask()) { Serial.print("Could not schedule task "); Serial.println(taskReport0->name().c_str()); }
    if (!taskReport1->scheduleTask()) { Serial.print("Could not schedule task "); Serial.println(taskReport1->name().c_str()); }
    if (!taskReport2->scheduleTask()) { Serial.print("Could not schedule task "); Serial.println(taskReport2->name().c_str()); }

#ifdef ESP32
    Serial.print("Loop free stack = "); Serial.println(uxTaskGetStackHighWaterMark(NULL));
#endif
}

void taskReaper(const CoopTaskBase* const task)
{
    if (task == taskText)
    {
        Serial.print(task->name()); Serial.print(" returns = "); Serial.println(taskText->exitCode());
        delete task;
        taskText = nullptr;
    }
}

void loop()
{
#if defined(ESP8266) && defined(USE_BUILTIN_TASK_SCHEDULER)
    if (taskText && !*taskText)
    {
        Serial.print(taskText->name()); Serial.print(" returns = "); Serial.println(taskText->exitCode());
        delete taskText;
        taskText = nullptr;
    }
#else
    runCoopTasks(taskReaper);
#endif

    // taskReport sleeps on first run(), and after each report.
    // It resets iterations to 0 on each report.
    if (!iterations) start = micros();
    ++iterations;
#ifdef ESP32_FREERTOS
    if (iterations >= 50000)
#else
    if (iterations >= 200000)
#endif
    {
        reportSema.post();
    }
}
