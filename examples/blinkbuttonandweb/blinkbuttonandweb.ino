#include <CoopTask.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

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

void loopButton(CoopTask& task) {
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
		task.yield();
	}
	task.exit();
}

CoopTask* taskWeb;
CoopTask* taskBlink;
CoopTask* taskButton;

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

	//task1 = new CoopTask([](CoopTask& task)
	//	{
	//		Serial.println("Task1 - A");
	//		task.yield();
	//		Serial.println("Task1 - B");
	//		uint32_t start = millis();
	//		task.delay(6000);
	//		Serial.print("!!!Task1 - C - ");
	//		Serial.println(millis() - start);
	//		task.exit();
	//	});

	taskWeb = new CoopTask([](CoopTask& task)
		{
			for (;;)
			{
				server.handleClient();
				MDNS.update();
				task.yield();
			}}, 0x600);
	if (!*taskWeb) Serial.println("CoopTask Web out of stack");

	taskBlink = new CoopTask(loopBlink, 0x2e0);
	if (!*taskBlink) Serial.println("CoopTask Blink out of stack");

	taskButton = new CoopTask(loopButton, 0x400);
	if (!*taskButton) Serial.println("CoopTask Button out of stack");

	start = micros();
}

void loop()
{
	taskWeb->run();
	taskBlink->run();
	taskButton->run();

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
