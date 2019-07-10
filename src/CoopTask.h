#include <functional>
#include <csetjmp>
#include <Arduino.h>

class CoopTask
{
protected:
#ifdef ESP32
	static constexpr uint32_t MAXSTACKSPACE = 0x1a00;
#else
	static constexpr uint32_t MAXSTACKSPACE = 0xfe0;
#endif
	static constexpr uint32_t DEFAULTTASKSTACKSIZE = 0x280;
	static constexpr uint32_t STACKCOOKIE = 0xdeadbeef;
	void doYield(uint32_t val);
	static char* coopStackTop;
	static char* loopStackTop;
public:
	CoopTask(const String& name, std::function< void(CoopTask&) > _func, uint32_t stackSize = DEFAULTTASKSTACKSIZE) :
		taskName(name), func(_func), taskStackSize(stackSize), delay_exp(0)
	{
	}
	const String taskName;
	std::function< void(CoopTask&) > func;
	uint32_t taskStackSize;
	char* taskStackTop = nullptr;
	jmp_buf env;
	jmp_buf env_yield;
	uint32_t delay_exp;
	bool init = false;
	bool cont = true;
	bool delayed = false;

	const String& name() const { return taskName; }
	operator bool();
	bool initialize();
	// @returns: 0: exited. 1: runnable. >1: sleeps for x ms.
	uint32_t run();

	void yield();
	void delay(uint32_t ms);
	void exit();

protected:
	uint32_t irqstate;
};
