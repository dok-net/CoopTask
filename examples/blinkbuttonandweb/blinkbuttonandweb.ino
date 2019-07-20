#include <CoopTask.h>
#include <CoopSemaphore.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <Schedule.h>

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
public:
    Button(uint8_t reqPin) : pushSema(0), PIN(reqPin) {
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

int loopBlink()
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

int loopButton() {
    int preCount = 0;
    int count = 0;
    for (;;)
    {
        if (!button1->pushSema.wait())
        {
            Serial.println("loopButton: wait failed");
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
            CoopTask::exit();
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
CoopTask* taskButton;
#endif
CoopTask* taskBlink;
CoopTask* taskText;
CoopSemaphore reportSema(0);
CoopTask* taskReport;
#if defined(ESP8266) || defined(ESP32)
CoopTask* taskWeb;
#endif

void printStackReport(CoopTask* task)
{
    if (!task) return;
    Serial.print(task->name().c_str());
    Serial.print(" free stack = ");
    Serial.println(task->getFreeStack());
}

// to demonstrate that yield and delay work in subroutines
void printReport(uint32_t& reportCnt, uint32_t& start)
{
    //CoopTask::delayMicroseconds(4000000);
    Serial.print("Loop latency: ");
    Serial.print((micros() - start) / reportCnt);
    Serial.println("us");
#if defined(ESP8266) || defined(ESP32)
    printStackReport(taskButton);
#endif
    printStackReport(taskBlink);
    printStackReport(taskText);
    printStackReport(taskReport);
#if defined(ESP8266) || defined(ESP32)
    printStackReport(taskWeb);
#endif

    reportCnt = 0;
    start = micros();
};

#ifdef ESP8266
bool rescheduleTask(CoopTask* task, uint32_t repeat_us)
{
    if (task->sleeping())
        return false;
    auto stat = task->run();
    switch (stat)
    {
    case 0: // exited.
        return false;
        break;
    case 1: // runnable or sleeping.
        if (task->sleeping()) return false;
        if (!repeat_us) return true;
        schedule_recurrent_function_us([task]() { return rescheduleTask(task, 0); }, 0);
        return false;
        break;
    default: // delayed until millis() or micros() deadline, check delayIsMs().
        if (task->sleeping()) return false;
        auto next_repeat_us = static_cast<int32_t>(task->delayIsMs() ? (stat - millis()) * 1000 : stat - micros());
        if (next_repeat_us < 0) next_repeat_us = 0;
        if (next_repeat_us == repeat_us) return true;
        schedule_recurrent_function_us([task, next_repeat_us]() { return rescheduleTask(task, next_repeat_us); }, next_repeat_us);
        return false;
        break;
    }
    return false;
}

bool scheduleTask(CoopTask* task, bool wakeup = false)
{
    if (wakeup)
        task->sleep(false);
    schedule_recurrent_function_us([task]() { return rescheduleTask(task, 0); }, 0);
}
#endif

uint32_t reportCnt = 0;

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
    button1 = new Button(BUTTON1);

    taskButton = new CoopTask(F("Button"), loopButton, 0x700);
    if (!*taskButton) Serial.printf("CoopTask %s out of stack\n", taskButton->name().c_str());
#endif

#if defined(ESP8266) || defined(ESP32)
    taskBlink = new CoopTask(F("Blink"), loopBlink, 0x240);
#else
    taskBlink = new CoopTask(F("Blink"), loopBlink, 0x40);
#endif
    if (!*taskBlink) Serial.println("CoopTask Blink out of stack");

    taskText = new CoopTask(F("Text"), []()
        {
            Serial.println("Task1 - A");
            yield();
            Serial.println("Task1 - B");
            uint32_t start = millis();
            delay(6000);
            Serial.print("!!!Task1 - C - ");
            Serial.println(millis() - start);
            printStackReport(taskText);
            return 0;
        }
#if defined(ESP8266) || defined(ESP32)
    , 0x260);
#else
    , 0x60);
#endif
    if (!*taskText) Serial.println("CoopTask Text out of stack");

    taskReport = new CoopTask(F("Report"), []()
        {
            uint32_t start = micros();
            for (;;) {
                reportSema.wait();
                printReport(reportCnt, start);
            }
            return 0;
        }
#if defined(ESP8266) || defined(ESP32)
    , 0x380);
#else
    , 0x68);
#endif
    if (!*taskReport) Serial.println("CoopTask Report out of stack");

#if defined(ESP8266) || defined(ESP32)
    taskWeb = new CoopTask(F("Web"), []()
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

//#ifdef ESP8266
//    scheduleTask(taskButton);
//    scheduleTask(taskBlink);
//    scheduleTask(taskText);
//    scheduleTask(taskReport);
//    scheduleTask(taskWeb);
//#endif
#endif

    Serial.println("Scheduler test");
}

//#ifndef ESP8266
#if defined(ESP8266) || defined(ESP32)
uint32_t taskButtonRunnable = 1;
#endif
uint32_t taskBlinkRunnable = 1;
uint32_t taskTextRunnable = 1;
uint32_t taskReportRunnable = 1;
#if defined(ESP8266) || defined(ESP32)
uint32_t taskWebRunnable = 1;
#endif
//#endif

void loop()
{
//#ifndef ESP8266
#if defined(ESP8266) || defined(ESP32)
    if (taskButtonRunnable != 0) taskButtonRunnable = taskButton->run();
#endif
    if (taskBlinkRunnable != 0) taskBlinkRunnable = taskBlink->run();
    if (taskTextRunnable != 0) taskTextRunnable = taskText->run();
    if (taskReportRunnable != 0) taskReportRunnable = taskReport->run();
#if defined(ESP8266) || defined(ESP32)
    if (taskWebRunnable != 0) taskWebRunnable = taskWeb->run();
#endif
//#endif

    // taskReport sleeps on first run(), and after each report.
    // It resets reportCnt to 0 on each report.
    ++reportCnt;
    if (reportCnt > 200000) {
//#ifndef ESP8266
        reportSema.post();
//#else
//        // paranoid check to prevent taskReport from being duplicate scheduled.
//        if (taskReport->sleeping()) scheduleTask(taskReport, true);
//#endif
    }
}
