#include <Arduino.h>
#include <functional>
#include <atomic>
#include <csetjmp>

class CoopTask
{
public:
	CoopTask(std::function< void(CoopTask&) > _func) : func(_func)
	{
		delay_exp.store(0);
	}
	static std::atomic<uint32_t> stacks;
	jmp_buf env;
	jmp_buf env_yield;
	std::atomic<uint32_t> delay_exp;
	bool cont = true;
	bool init = false;
	std::function< void(CoopTask&) > func;

	bool initialize();
	bool run();

	void yield();
	void delay(uint32_t ms);
	void exit();

protected:
	uint32_t irqstate;
};
