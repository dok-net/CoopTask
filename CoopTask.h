#include <Arduino.h>
#include <functional>
#include <atomic>
#include <csetjmp>

class CoopTask
{
protected:
	static constexpr uint32_t MAXSTACKFRAME = 0x1000;
	static constexpr uint32_t DEFAULTTASKSTACKSIZE = 0x2c0;

public:
	CoopTask(std::function< void(CoopTask&) > _func, uint32_t stackSize = DEFAULTTASKSTACKSIZE) :
		func(_func), taskStackSize(stackSize)
	{
		delay_exp.store(0);
	}
	static uint32_t stackframe;
	uint32_t taskStackSize;
	jmp_buf env;
	jmp_buf env_yield;
	std::atomic<uint32_t> delay_exp;
	bool cont = true;
	bool init = false;
	std::function< void(CoopTask&) > func;

	operator bool() {
		return (stackframe > 2 * taskStackSize);
	}

	bool initialize();
	bool run();

	void yield();
	void delay(uint32_t ms);
	void exit();

protected:
	uint32_t irqstate;
};
