#include "CoopTask.h"
#include <alloca.h>

CoopTask* CoopTask::current = nullptr;

CoopTask::operator bool()
{
    if (taskStackTop) return true;
    if (taskStackSize <= MAXSTACKSPACE - 2 * sizeof(STACKCOOKIE))
        taskStackTop = new (std::nothrow) char[taskStackSize + 2 * sizeof(STACKCOOKIE)];
    return taskStackTop;
}

bool CoopTask::initialize()
{
    if (!cont || init) return false;
    init = true;
    if (*this)
    {
        char* bp = static_cast<char*>(alloca(reinterpret_cast<char*>(&bp) - (taskStackTop + taskStackSize + sizeof(STACKCOOKIE))));
        //Serial.printf("CoopTask %s: bp = %p, taskStackTop = %p, taskStackTop + taskStackSize + sizeof(STACKCOOKIE) = %p\n", taskName.c_str(), bp, taskStackTop, taskStackTop + taskStackSize + sizeof(STACKCOOKIE));
        *reinterpret_cast<uint32_t*>(taskStackTop) = STACKCOOKIE;
        *reinterpret_cast<uint32_t*>(taskStackTop + taskStackSize + sizeof(STACKCOOKIE)) = STACKCOOKIE;
        func(*this);
        _exit();
    }
    cont = false;
    return false;
}

uint32_t CoopTask::run()
{
    if (!cont) return 0;
    if (delayed)
    {
        int32_t delay_rem = delay_ms ? static_cast<int32_t>(delay_exp - millis()) : static_cast<int32_t>(delay_exp - micros());
        if (delay_rem > 0) return delay_rem;
        delayed = false;
    }
    auto val = setjmp(env);
    if (!val) {
        current = this;
        if (!init) return initialize();
        if (*reinterpret_cast<uint32_t*>(taskStackTop + taskStackSize + sizeof(STACKCOOKIE)) != STACKCOOKIE)
        {
            printf("FATAL ERROR: CoopTask %s stack corrupted\n", name().c_str());
            std::abort();
        }

        longjmp(env_yield, 0);
    }
    else
    {
        current = nullptr;
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

void CoopTask::_yield()
{
    doYield(2);
}

void CoopTask::_delay(uint32_t ms)
{
    delay_ms = true;
    delay_exp = millis() + ms;
    delayed = true;
    // CoopTask::run() sleeps task until delay_exp is reached
    doYield(ms > 2 ? ms : 2);
}

void CoopTask::_delayMicroseconds(uint32_t us)
{
    delay_ms = false;
    delay_exp = micros() + us;
    delayed = true;
    // CoopTask::run() sleeps task until delay_exp is reached
    doYield(us > 2 ? us : 2);
}

void CoopTask::_exit()
{
    longjmp(env, 0);
}
