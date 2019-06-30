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
};

#define scheduler_yield(task) { if (!setjmp((task).envy)) { longjmp((task).env, 2); } }
#define scheduler_exit(task) { longjmp((task).env, 0); }
#define scheduler_delay(task, ms) \
{ \
	(task).delay_exp.store(millis() + (ms)); \
	do { \
		scheduler_yield(task); \
	} while (static_cast<int32_t>((task).delay_exp.load() - millis()) > 0); \
}
