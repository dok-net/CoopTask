#include "CoopTask.h"
#include <alloca.h>

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
        Serial.printf("CoopTask %s: bp = %p, taskStackTop = %p, taskStackTop + taskStackSize + sizeof(STACKCOOKIE) = %p\n", taskName.c_str(), bp, taskStackTop, taskStackTop + taskStackSize + sizeof(STACKCOOKIE));
        *reinterpret_cast<uint32_t*>(taskStackTop) = STACKCOOKIE;
        *reinterpret_cast<uint32_t*>(taskStackTop + taskStackSize + sizeof(STACKCOOKIE)) = STACKCOOKIE;
        func(*this);
        exit();
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
        if (*reinterpret_cast<uint32_t*>(taskStackTop + taskStackSize + sizeof(STACKCOOKIE)) != STACKCOOKIE)
        {
            printf("FATAL ERROR: CoopTask %s stack corrupted\n", name().c_str());
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
