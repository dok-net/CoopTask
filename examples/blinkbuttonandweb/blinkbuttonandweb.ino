#include <CoopTask.h>

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
    Button(uint8_t reqPin) : PIN(reqPin) {
        pinMode(PIN, INPUT_PULLUP);
        attachInterruptArg(PIN, Button::buttonIsr_static, this, FALLING);
    };
    ~Button() {
        detachInterrupt(PIN);
    }

    void IRAM_ATTR buttonIsr() {
        numberKeyPresses += 1;
        pressed = true;
    }

    static void IRAM_ATTR buttonIsr_static(void* const self) {
        reinterpret_cast<Button*>(self)->buttonIsr();
    }

    uint32_t checkPressed() {
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

void loopBlink()
{
    for (;;)
    {
        digitalWrite(LED_BUILTIN, LOW);
        CoopTask::delay(2000);
        digitalWrite(LED_BUILTIN, HIGH);
        CoopTask::delay(3000);
    }
    CoopTask::exit();
}

#if defined(ESP8266) || defined(ESP32)
Button* button1;

void loopButton() {
    int preCount = 0;
    int count = 0;
    for (;;)
    {
        if (nullptr != button1 && 8000 < (count = button1->checkPressed())) {
            Serial.println(count);
            delete button1;
            button1 = nullptr;
            CoopTask::exit();
        }
        if (preCount != count) {

            Serial.print("loop4: count = ");
            Serial.println(count);
            preCount = count;
        }
        CoopTask::yield();
    }
    CoopTask::exit();
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
CoopTask* taskReport;
#if defined(ESP8266) || defined(ESP32)
CoopTask* taskWeb;
#endif

// to demonstrate that yield and delay work in subroutines
void printReport(uint32_t& reportCnt, uint32_t& start)
{
    CoopTask::delayMicroseconds(4000000);
    Serial.print("Loop latency: ");
    Serial.print((micros() - start) / reportCnt);
    Serial.println("us");
    reportCnt = 0;
    start = micros();
};

#ifdef ESP8266
bool schedWrap(CoopTask* task)
{
    auto stat = task->run();
    switch (stat)
    {
    case 0: break;
    case 1: schedule_recurrent_function_us([task]() { return schedWrap(task); }, 0); break;
    default:
        schedule_recurrent_function_us([task]() { return schedWrap(task); }, stat * (task->delayIsMs() ? 1000 : 1)); break;
    }
    return false;
}
#endif

uint32_t reportCnt = 0;

void setup()
{
    Serial.begin(74880);
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

    Serial.println("Scheduler test");

    pinMode(LED_BUILTIN, OUTPUT);

#if defined(ESP8266) || defined(ESP32)
    button1 = new Button(BUTTON1);

    taskButton = new CoopTask(F("Button"), loopButton);
    if (!*taskButton) Serial.printf("CoopTask %s out of stack\n", taskButton->name().c_str());
#endif

    taskBlink = new CoopTask(F("Blink"), loopBlink);
    if (!*taskBlink) Serial.println("CoopTask Blink out of stack");

    taskText = new CoopTask(F("Text"), []()
        {
            Serial.println("Task1 - A");
            CoopTask::yield();
            Serial.println("Task1 - B");
            uint32_t start = millis();
            CoopTask::delay(6000);
            Serial.print("!!!Task1 - C - ");
            Serial.println(millis() - start);
            //CoopTask::exit();
        });
    if (!*taskText) Serial.println("CoopTask Text out of stack");

    taskReport = new CoopTask(F("Report"), []()
        {
            uint32_t start = micros();
            for (;;) {
                printReport(reportCnt, start);
            }
        });
    if (!*taskReport) Serial.println("CoopTask Report out of stack");

#if defined(ESP8266) || defined(ESP32)
    taskWeb = new CoopTask(F("Web"), []()
        {
            for (;;) {
                server.handleClient();
#ifdef ESP8266
                MDNS.update();
#endif
                CoopTask::yield();
            }
        });
    if (!*taskWeb) Serial.printf("CoopTask %s out of stack\n", taskWeb->name().c_str());

    //#ifdef ESP8266
    //    schedule_recurrent_function_us([]() { return schedWrap(taskButton); }, 0);
    //    schedule_recurrent_function_us([]() { return schedWrap(taskBlink); }, 0);
    //    schedule_recurrent_function_us([]() { return schedWrap(taskText); }, 0);
    //    schedule_recurrent_function_us([]() { return schedWrap(taskReport); }, 0);
    //    schedule_recurrent_function_us([]() { return schedWrap(taskWeb); }, 0);
    //#endif
#endif
}

#if defined(ESP8266) || defined(ESP32)
uint32_t taskButtonRunnable = 1;
#endif
uint32_t taskBlinkRunnable = 1;
uint32_t taskTextRunnable = 1;
uint32_t taskReportRunnable = 1;
#if defined(ESP8266) || defined(ESP32)
uint32_t taskWebRunnable = 1;
#endif

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

    if (reportCnt == 1) taskReport->sleep(true);
    ++reportCnt;
    if (reportCnt > 100000) taskReport->sleep(false);
}
