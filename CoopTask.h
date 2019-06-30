#include <functional>
#include <atomic>
#include <csetjmp>

class CoopTask
{
public:
	jmp_buf env;
	jmp_buf envy;
	std::atomic<uint32_t> delay_exp;
	bool cont = true;
	bool init = false;

	void run(std::function< void(CoopTask&) > func, uint32_t stackOffset)
	{
		if (!cont) return;
		auto val = setjmp(env);
		if (!val) {
			if (!init) {
				auto sf = (char*)alloca(stackOffset);
				sf[stackOffset - 1] = 1;
				init = true;
				func(*this);
				cont = false;
				return;
			}
			longjmp(envy, 0);
		}
		else cont = val > 1;
	}

	void yield()
	{
		if (!setjmp(envy)) { longjmp(env, 2); };
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
