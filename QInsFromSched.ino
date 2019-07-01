#include "CoopTask.h"
#include <Schedule.h>

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

Button* button1;

void loop3(CoopTask& task) {
	int preCount = 0;
	int count = 0;
	for (;;)
	{
		if (nullptr != button1 && 10 < (count = button1->checkPressed())) {
			Serial.println(count);
			delete button1;
			button1 = nullptr;
			return;
			//task.exit();
		}
		if (preCount != count) {

			Serial.print("loop4: count = ");
			Serial.println(count);
			preCount = count;
		}
		task.yield();
	}
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
}

CoopTask* task1;
CoopTask* task2;
CoopTask* task3;
CoopTask* task4;

void setup()
{
	Serial.begin(74880);
	delay(500);
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

	task2 = new CoopTask(loop2);
	task3 = new CoopTask(loop3);
	task4 = new CoopTask(loop4);

	//schedule_recurrent_function_us([]() { return task1->run(); }, 0);
	//schedule_recurrent_function_us([]() { return task2->run(); }, 0);
	//schedule_recurrent_function_us([]() { return task3->run(); }, 0);
	//schedule_recurrent_function_us([]() { return task4->run(); }, 0);

	start = micros();
}

void loop()
{
	//task1->run();
	task2->run();
	task3->run();
	task4->run();

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
