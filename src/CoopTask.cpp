#include "CoopTask.h"
#include <alloca.h>
#include <Arduino.h>

uint32_t CoopTask::stackframe(MAXSTACKFRAME);

bool CoopTask::initialize()
{
	if (!cont || init) return false;
	init = true;
	if (*this)
	{
		auto sf = (char*)alloca(taskStack);
		sf[0] = 0xff;
		func(*this);
	}
	cont = false;
	return false;
}

uint32_t CoopTask::run()
{
	if (!cont) return 0;
	if (delayed)
	{
		int32_t delay_rem = static_cast<int32_t>(delay_exp - millis());
		if (delay_rem > 0) return delay_rem;
		delayed = false;
	}
	auto val = setjmp(env);
	if (!val) {
		if (!init) return initialize();
		longjmp(env_yield, 0);
	}
	else
	{
		cont = val > 1;
	}
	return !cont ? 0 : (val > 2 ? val : 1);
}

void CoopTask::doYield(uint32_t val)
{
	if (!setjmp(env_yield))
	{
		longjmp(env, val);
	}
}

void CoopTask::yield()
{
	doYield(2);
}

void CoopTask::delay(uint32_t ms)
{
	delay_exp = millis() + ms;
	delayed = true;
	// CoopTask::run() sleeps task until delay_exp is reached
	doYield(ms > 2 ? ms : 2);
}

void CoopTask::exit()
{
	longjmp(env, 0);
}
