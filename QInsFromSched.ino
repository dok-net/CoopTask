#include "CoopTask.h"

uint32_t reportCnt = 0;
uint32_t start;

CoopTask task1;
void loop1(CoopTask& task)
{
	Serial.println("Loop1 - A");
	scheduler_yield(task);
	Serial.println("Loop1 - B");
	scheduler_delay(task, 10000);
	Serial.println("!!!Loop1 - C");
	scheduler_exit(task);
}

CoopTask task2;
void loop2(CoopTask& task)
{
	Serial.println("Loop2 - A");
	scheduler_yield(task);
	Serial.println("Loop2 - B");
	scheduler_delay(task, 5000);
	Serial.println("!!!Loop2 - C");
	scheduler_exit(task);
}

CoopTask task3;
void loop3(CoopTask& task)
{
	Serial.println("Loop3 - A");
	scheduler_yield(task);
	Serial.println("Loop3 - B");
	scheduler_delay(task, 15000);
	Serial.println("!!!Loop3 - C");
	scheduler_exit(task);
}

void setup()
{
	Serial.begin(74880);
	delay(500);
	Serial.println("Scheduler test");
	start = micros();
}

void loop()
{
	task1.run(loop1, 1024);
	task2.run(loop2, 2048);
	task3.run(loop3, 3072);

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
