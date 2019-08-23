#include <CoopTask.h>
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

#if defined(ESP32)
#define BUTTON1 17
#elif defined(ARDUINO_ESP8266_WEMOS_D1MINI)
#define BUTTON1 D3
#else
#define BUTTON1 0
#endif

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

    uint32_t testResetPressed() {
        if (pressed) {
            Serial.printf("Button on pin %u has been pressed %u times\n", PIN, numberKeyPresses);
            pressed = false;
        }
        return numberKeyPresses;
    }

private:
    const uint8_t PIN;
    volatile uint32_t numberKeyPresses = 0;
    volatile bool pressed = false;
};
#endif

int loopBlink() noexcept
{
    for (;;)
    {
        digitalWrite(LED_BUILTIN, LOW);
        delay(2000);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(3000);
    }
    return 0;
}

#if defined(ESP8266) || defined(ESP32)
Button* button1;

int loopButton() noexcept
{
    int preCount = 0;
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
        Serial.print("loopButton: count = ");
        Serial.println(count);
        if (nullptr != button1 && 8000 < button1->testResetPressed()) {
            delete button1;
            button1 = nullptr;
            CoopTask<>::exit();
        }
        yield();
    }
    return 0;
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
CoopTask<>* taskButton;
#endif
CoopTask<>* taskBlink;
CoopTask<uint32_t>* taskText;
CoopTask<>* taskReport0;
CoopTask<>* taskReport1;
CoopTask<>* taskReport2;
CoopTask<>* taskReport3;
CoopSemaphore reportSema(0);
#if defined(ESP8266) || defined(ESP32)
CoopTask<>* taskWeb;
#endif

void printStackReport(CoopTaskBase* task)
{
    if (!task) return;
    Serial.print(task->name().c_str());
    Serial.print(" free stack = ");
    Serial.println(task->getFreeStack());
}

uint32_t reportCnt;
uint32_t start;

// to demonstrate that yield and delay work in subroutines
void printReport()
{
    //CoopTask<>::delayMicroseconds(4000000);
    Serial.print("Loop latency: ");
    if (reportCnt)
    {
        Serial.print((micros() - start) / reportCnt);
        Serial.println("us");
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
    printStackReport(&CoopTask<>::self());
#if defined(ESP8266) || defined(ESP32)
    printStackReport(taskWeb);
#endif

    reportCnt = 0;
    start = micros();
};

class RAIITest
{
public:
    ~RAIITest()
    {
        Serial.print(CoopTaskBase::self().name());
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
    delay(500);

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

    pinMode(LED_BUILTIN, OUTPUT);

#if defined(ESP8266) || defined(ESP32)
    button1 = new Button(BUTTON1, reportSema);

    taskButton = new CoopTask<>(F("Button"), loopButton, 0x700);
    if (!*taskButton) Serial.printf("CoopTask %s out of stack\n", taskButton->name().c_str());
#endif

#if defined(ESP8266) || defined(ESP32)
    taskBlink = new CoopTask<>(F("Blink"), loopBlink, 0x240);
#else
    taskBlink = new CoopTask<>(F("Blink"), loopBlink, 0x40);
#endif
    if (!*taskBlink) Serial.println("CoopTask Blink out of stack");

    taskText = new CoopTask<uint32_t>(F("Text"), []()
        {
            RAIITest raii;
            Serial.println("Task1 - A");
            yield();
            Serial.println("Task1 - B");
            uint32_t start = millis();
            delay(6000);
            Serial.print("!!!Task1 - C - ");
            Serial.println(millis() - start);
            printStackReport(taskText);
#if defined(ESP32) || !defined(ARDUINO)
            throw (uint32_t)41L;
#endif
            //CoopTask<uint32_t>::exit(42);
            return (uint32_t)43L;
        }
#if defined(ESP8266) || defined(ESP32)
    , 0x380);
#else
    , 0x68);
#endif
    if (!*taskText) Serial.println("CoopTask Text out of stack");

    auto reportFunc = []() noexcept
    {
        for (;;) {
            if (!reportSema.wait())
            {
                Serial.println("report: wait failed");
                yield();
                continue;
            }
            Serial.println(CoopTask<>::self().name());
            printReport();
        }
        return 0;
    };
    taskReport0 = new CoopTask<>(F("Report0"), reportFunc
#if defined(ESP8266) || defined(ESP32)
        , 0x380);
#else
        , 0x68);
#endif
    if (!*taskReport0) Serial.println("CoopTask Report out of stack");
    taskReport1 = new CoopTask<>(F("Report1"), reportFunc
#if defined(ESP8266) || defined(ESP32)
        , 0x380);
#else
        , 0x68);
#endif
    if (!*taskReport1) Serial.println("CoopTask Report out of stack");
    taskReport2 = new CoopTask<>(F("Report2"), reportFunc
#if defined(ESP8266) || defined(ESP32)
        , 0x380);
#else
        , 0x68);
#endif
    if (!*taskReport2) Serial.println("CoopTask Report out of stack");
    taskReport3 = new CoopTask<>(F("Report3"), reportFunc
#if defined(ESP8266) || defined(ESP32)
        , 0x380);
#else
        , 0x68);
#endif
    if (!*taskReport3) Serial.println("CoopTask Report out of stack");

#if defined(ESP8266) || defined(ESP32)
    taskWeb = new CoopTask<>(F("Web"), []() noexcept
        {
            for (;;) {
                server.handleClient();
#ifdef ESP8266
                MDNS.update();
#endif
                yield();
            }
            return 0;
        }, 0x800);
    if (!*taskWeb) Serial.printf("CoopTask %s out of stack\n", taskWeb->name().c_str());

#if defined(ESP8266) // TODO: requires some PR to be merged: || defined(ESP32)
    scheduleTask(taskButton);
    scheduleTask(taskBlink);
    scheduleTask(taskText);
    scheduleTask(taskReport0);
    scheduleTask(taskReport1);
    scheduleTask(taskReport2);
    scheduleTask(taskReport3);
    scheduleTask(taskWeb);
#endif
#endif

    Serial.println("Scheduler test");

    reportCnt = 0;
    start = micros();
}

#if !defined(ESP8266) // TODO: requires some PR to be merged: && !defined(ESP32)
#if defined(ESP8266) || defined(ESP32)
uint32_t taskButtonRunnable = 1;
#endif
uint32_t taskBlinkRunnable = 1;
uint32_t taskTextRunnable = 1;
uint32_t taskReportRunnable0 = 1;
uint32_t taskReportRunnable1 = 1;
uint32_t taskReportRunnable2 = 1;
uint32_t taskReportRunnable3 = 1;
#if defined(ESP8266) || defined(ESP32)
uint32_t taskWebRunnable = 1;
#endif
#endif

void loop()
{
#if !defined(ESP8266) // TODO: requires some PR to be merged: && !defined(ESP32)
#if defined(ESP8266) || defined(ESP32)
    if (taskButtonRunnable != 0) taskButtonRunnable = taskButton->run();
#endif
    if (taskBlinkRunnable != 0) taskBlinkRunnable = taskBlink->run();
    if (taskTextRunnable != 0)
    {
        taskTextRunnable = taskText->run();
        // once when completed:
        if (!taskTextRunnable)
        {
            Serial.print(taskText->name());
            Serial.print(" returns = ");
            Serial.println(taskText->exitCode());
        }
    }
    if (taskReportRunnable0 != 0) taskReportRunnable0 = taskReport0->run();
    if (taskReportRunnable1 != 0) taskReportRunnable1 = taskReport1->run();
    if (taskReportRunnable2 != 0) taskReportRunnable2 = taskReport2->run();
    if (taskReportRunnable3 != 0) taskReportRunnable3 = taskReport3->run();
#if defined(ESP8266) || defined(ESP32)
    if (taskWebRunnable != 0) taskWebRunnable = taskWeb->run();
#endif
#endif

    // taskReport sleeps on first run(), and after each report.
    // It resets reportCnt to 0 on each report.
    ++reportCnt;
    if (reportCnt > 200000) {
        reportSema.post();
    }
}
