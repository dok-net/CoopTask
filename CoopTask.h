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

	void yield()
	{
		if (!setjmp(env_yield)) { longjmp(env, 2); };
	}

	void delay(uint32_t ms)
	{
		delay_exp.store(millis() + (ms));
		do {
			yield();
		} while (static_cast<int32_t>(delay_exp.load() - millis()) > 0);
	}

	void exit()
	{
		longjmp(env, 0);
	}
};
