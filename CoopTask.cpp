#include "CoopTask.h"
#include <alloca.h>

std::atomic<uint32_t> CoopTask::stacks(0);

bool CoopTask::initialize()
{
	if (!cont) return false;
	auto val = setjmp(env);
	if (!val) {
		if (init) return false;
		auto stack = stacks.load() + 1;
		stacks.store(stack);
		auto sf = (char*)alloca(0x400 * stack - 0x100);
		sf[0] = 0xff;
		init = true;
		func(*this);
		cont = false;
		return false;
	}
	else cont = val > 1;
	return cont;
}

bool CoopTask::run()
{
	if (!cont) return false;
	auto val = setjmp(env);
	if (!val) {
		if (!init) return initialize();
		longjmp(env_yield, 0);
	}
	else cont = val > 1;
	return cont;
}
