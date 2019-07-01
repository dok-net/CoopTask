#include "CoopTask.h"
#include <Schedule.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>


uint32_t reportCnt = 0;
uint32_t start;

void loop2(CoopTask& task)
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
		attachInterrupt(PIN, std::bind(&Button::buttonIsr, this), FALLING);
	};
	~Button() {
		detachInterrupt(PIN);
	}

	void IRAM_ATTR buttonIsr() {
		numberKeyPresses += 1;
		pressed = true;
	}

	static void IRAM_ATTR buttonIsr_static(Button* const self) {
		self->buttonIsr();
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

std::atomic<Button*> button1;

ESP8266WebServer server(80);

void loop3(CoopTask& task) {
	int preCount = 0;
	int count = 0;
	for (;;)
	{
		if (nullptr != button1 && 10 < (count = button1.load()->checkPressed())) {
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
		server.handleClient();
		MDNS.update();
		task.yield();
	}
	task.exit();
}

void loop4(CoopTask& task)
{
	for (;;) {
		for (int i = 0; i < 8; ++i)
		{
			digitalWrite(LED_BUILTIN, LOW);
			task.delay(125);
			digitalWrite(LED_BUILTIN, HIGH);
			task.delay(125);
		}
		task.delay(3000);
	}
	task.exit();
}

CoopTask* task1;
CoopTask* task2;
CoopTask* task3;
CoopTask* task4;

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

	if (MDNS.begin("esp8266")) {
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

	task1 = new CoopTask([](CoopTask& task)
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
	if (!*task1) Serial.println("CoopTask 1 out of stack");

	//task2 = new CoopTask(loop2);
	//if (!*task2) Serial.println("CoopTask 2 out of stack");

	task3 = new CoopTask(loop3, 0x600);
	if (!*task3) Serial.println("CoopTask 3 out of stack");

	//task4 = new CoopTask(loop4);
	//if (!*task4) Serial.println("CoopTask 4 out of stack");

	//schedule_recurrent_function_us([]() { return task1->run(); }, 0);
	//schedule_recurrent_function_us([]() { return task2->run(); }, 0);
	//schedule_recurrent_function_us([]() { return task3->run(); }, 0);
	//schedule_recurrent_function_us([]() { return task4->run(); }, 0);

	start = micros();
}

void loop()
{
	task1->run();
	//task2->run();
	task3->run();
	//task4->run();

	if (reportCnt > 100000)
	{
		Serial.print("Loop latency: ");
		Serial.print((micros() - start) / reportCnt);
		Serial.println("us");
		reportCnt = 0;
		start = micros();
	}
	++reportCnt;

}
