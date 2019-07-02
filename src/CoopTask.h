#include <Arduino.h>
#include <functional>
#include <csetjmp>

class CoopTask
{
protected:
	static constexpr uint32_t MAXSTACKFRAME = 0x1000;
	static constexpr uint32_t DEFAULTTASKSTACKSIZE = 0x2c0;
	void doYield(uint32_t val);

public:
	CoopTask(std::function< void(CoopTask&) > _func, uint32_t stackSize = DEFAULTTASKSTACKSIZE) :
		func(_func), delay_exp(0)
	{
		if (stackframe > stackSize + DEFAULTTASKSTACKSIZE)
		{
			stackframe -= stackSize;
			taskStack = stackframe;
		}
		else taskStack = 0;
	}
	std::function< void(CoopTask&) > func;
	static uint32_t stackframe;
	uint32_t taskStack;
	jmp_buf env;
	jmp_buf env_yield;
	uint32_t delay_exp;
	bool cont = true;
	bool init = false;

	operator bool() {
		return taskStack;
	}

	bool initialize();
	// @returns: 0: exited. 1: runnable. >1: sleeps for x ms.
	uint32_t run();

	void yield();
	void delay(uint32_t ms);
	void exit();

protected:
	uint32_t irqstate;
};
