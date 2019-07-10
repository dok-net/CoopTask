#include "CoopTask.h"
#include <alloca.h>

char* CoopTask::coopStackTop = nullptr;
char* CoopTask::loopStackTop = nullptr;

CoopTask::operator bool()
{
	if (taskStackTop) return true;
	char* bp;
	if (!coopStackTop)
	{
		coopStackTop = reinterpret_cast<char*>(&bp) - MAXSTACKSPACE;
	}
	bp = coopStackTop + taskStackSize;
	if (reinterpret_cast<char*>(&bp) - bp >= static_cast<int32_t>(DEFAULTTASKSTACKSIZE))
	{
		taskStackTop = coopStackTop;
		coopStackTop = bp;
	}
	return taskStackTop;
}

bool CoopTask::initialize()
{
	if (!cont || init) return false;
	init = true;
	if (*this)
	{
		char* bp = static_cast<char*>(alloca(reinterpret_cast<char*>(&bp) - (taskStackTop + taskStackSize)));
		*reinterpret_cast<uint32_t*>(taskStackTop) = STACKCOOKIE;
		loopStackTop = bp;
		*reinterpret_cast<uint32_t*>(loopStackTop) = STACKCOOKIE;
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
		if (*reinterpret_cast<uint32_t*>(loopStackTop) != STACKCOOKIE)
		{
			printf("FATAL ERROR: loop() stack overflow\n");
			std::abort();
		}
		longjmp(env_yield, 0);
	}
	else
	{
		if (*reinterpret_cast<uint32_t*>(taskStackTop) != STACKCOOKIE)
		{
			printf("FATAL ERROR: CoopTask %s stack overflow\n", name().c_str());
			std::abort();
		}
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
