#include "CoopTask.h"
#include <alloca.h>

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

bool CoopTask::run()
{
	if (!cont) return false;
	auto val = setjmp(env);
	if (!val) {
		if (!init) return initialize();
		irqstate = xt_rsil(15);
		longjmp(env_yield, 0);
	}
	else
	{
		xt_wsr_ps(irqstate);
		cont = val > 1;
	}
	return cont;
}

void CoopTask::yield()
{
	if (!setjmp(env_yield))
	{
		irqstate = xt_rsil(15);
		longjmp(env, 2);
	}
	else
	{
		xt_wsr_ps(irqstate);
	}
}

void CoopTask::delay(uint32_t ms)
{
	delay_exp.store(millis() + ms);
	do {
		yield();
	} while (static_cast<int32_t>(delay_exp.load() - millis()) > 0);
}

void CoopTask::exit()
{
	irqstate = xt_rsil(15);
	longjmp(env, 0);
}
