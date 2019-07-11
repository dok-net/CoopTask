#include <CoopTask.h>
//#include <Schedule.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

ESP8266WebServer server(80);
#else
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>

WebServer server(80);
#endif

uint32_t reportCnt = 0;
uint32_t start;

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

void loopBlink(CoopTask& task)
{
	for (;;)
	{
		digitalWrite(LED_BUILTIN, LOW);
		task.delay(2000);
		digitalWrite(LED_BUILTIN, HIGH);
		task.delay(3000);
	}
	task.exit();
}

Button* button1;

void loopButton(CoopTask& task) {
	int preCount = 0;
	int count = 0;
	for (;;)
	{
		if (nullptr != button1 && 8000 < (count = button1->checkPressed())) {
			Serial.println(count);
			delete button1;
			button1 = nullptr;
			task.exit();
		}
		if (preCount != count) {

			Serial.print("loop4: count = ");
			Serial.println(count);
			preCount = count;
		}
		task.yield();
	}
	task.exit();
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

//bool schedWrap(CoopTask* task)
//{
//	auto stat = task->run();
//	switch (stat)
//	{
//	case 0: break;
//	case 1: schedule_recurrent_function_us([task]() { return schedWrap(task); }, 0); break;
//	default:
//		Serial.print("Delayed scheduling (ms): "); Serial.println(stat);
//		schedule_recurrent_function_us([task]() { return schedWrap(task); }, stat * 1000); break;
//	}
//	return false;
//}

CoopTask* taskButton;
CoopTask* taskBlink;
CoopTask* taskText;

void setup()
{
	Serial.begin(74880);
	delay(500);

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

	Serial.println("Scheduler test");

	pinMode(LED_BUILTIN, OUTPUT);

	button1 = new Button(BUTTON1);

	taskButton = new CoopTask(F("Button"), loopButton);
	if (!*taskButton) Serial.println(F("CoopTask Button out of stack"));

	taskBlink = new CoopTask(F("Blink"), loopBlink);
	if (!*taskBlink) Serial.println(F("CoopTask Blink out of stack"));

	//schedule_recurrent_function_us([]() { return schedWrap(taskBlink); }, 0);

#ifdef ESP32
	taskText = new CoopTask(F("Text"), [](CoopTask& task)
		{
			Serial.println("Task1 - A");
			task.yield();
			Serial.println("Task1 - B");
			uint32_t start = millis();
			task.delay(6000);
			Serial.print("!!!Task1 - C - ");
			Serial.println(millis() - start);
			task.exit();
		});
	if (!*taskText) Serial.println(F("CoopTask Text out of stack"));
#endif

	start = micros();
}

uint32_t taskButtonRunnable = 1;
uint32_t taskBlinkRunnable = 1;
#ifdef ESP32
uint32_t taskTextRunnable = 1;
#endif

void loop()
{
	if (taskButtonRunnable != 0) taskButtonRunnable = taskButton->run();
	if (taskBlinkRunnable != 0) taskBlinkRunnable = taskBlink->run();
#ifdef ESP32
	if (taskTextRunnable != 0) taskTextRunnable = taskText->run();
#endif

	// report
	if (reportCnt > 100000)
	{
		Serial.print("Loop latency: ");
		Serial.print((micros() - start) / reportCnt);
		Serial.println("us");
		reportCnt = 0;
		start = micros();
	}
	++reportCnt;

	// web
	server.handleClient();
#ifdef ESP8266
	MDNS.update();
#endif
}
