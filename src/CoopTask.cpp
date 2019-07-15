#include "CoopTask.h"
#include <alloca.h>

CoopTask* CoopTask::current = nullptr;

CoopTask::operator bool()
{
    if (!cont) return false;
    if (taskStackTop) return true;
    if (taskStackSize <= MAXSTACKSPACE - 2 * sizeof(STACKCOOKIE))
    {
#if defined(ESP8266) || defined(ESP32)
        taskStackTop = new (std::nothrow) char[taskStackSize + 2 * sizeof(STACKCOOKIE)];
#else
        taskStackTop = new char[taskStackSize + 2 * sizeof(STACKCOOKIE)];
#endif
    }
    return taskStackTop;
}

bool CoopTask::initialize()
{
    if (!cont || init) return false;
    init = true;
    if (*this)
    {
        // fill stack with magic values to check overflow, corruption, and high water mark
        for (uint32_t pos = 0; pos <= (taskStackSize + sizeof(STACKCOOKIE)) / sizeof(uint32_t); ++pos)
        {
            reinterpret_cast<uint32_t*>(taskStackTop)[pos] = STACKCOOKIE;
        }
        char* bp = static_cast<char*>(alloca(reinterpret_cast<char*>(&bp) - (taskStackTop + taskStackSize + sizeof(STACKCOOKIE))));
        //Serial.printf("CoopTask %s: bp = %p, taskStackTop = %p, taskStackTop + taskStackSize + sizeof(STACKCOOKIE) = %p\n", taskName.c_str(), bp, taskStackTop, taskStackTop + taskStackSize + sizeof(STACKCOOKIE));
        _exit(func());
    }
    cont = false;
    return false;
}

uint32_t CoopTask::run()
{
    if (!cont) return 0;
    if (sleeping) return 1;
    if (delayed)
    {
        int32_t delay_rem = delay_ms ? static_cast<int32_t>(delay_exp - millis()) : static_cast<int32_t>(delay_exp - micros());
        if (delay_rem > 0) return delay_rem;
        delayed = false;
    }
    auto val = setjmp(env);
    // val = 0: init; 1: exit() task; 2: yield task; 3: sleep task; >3: delay task until target delay_exp
    if (!val) {
        current = this;
        if (!init) return initialize();
        if (*reinterpret_cast<uint32_t*>(taskStackTop + taskStackSize + sizeof(STACKCOOKIE)) != STACKCOOKIE)
        {
            printf("FATAL ERROR: CoopTask %s stack corrupted\n", name().c_str());
            ::abort();
        }

        longjmp(env_yield, 0);
    }
    else
    {
        current = nullptr;
        if (*reinterpret_cast<uint32_t*>(taskStackTop) != STACKCOOKIE)
        {
            printf("FATAL ERROR: CoopTask %s stack overflow\n", name().c_str());
            ::abort();
        }
        cont &= val > 1;
        sleeping = val == 3;
        delayed = val > 3;
    }
    if (!cont) {
        delete[] taskStackTop;
        taskStackTop = nullptr;
        return 0;
    }
    switch (val)
    {
    case 2:
    case 3:
        return 1;
        break;
    default:
        return delay_exp > 2 ? delay_exp : 2;
        break;
    }
}

uint32_t CoopTask::getFreeStack()
{
    if (!taskStackTop) return 0;
    uint32_t pos;
    for (pos = 1; pos < (taskStackSize + sizeof(STACKCOOKIE)) / sizeof(uint32_t); ++pos)
    {
        if (STACKCOOKIE != reinterpret_cast<uint32_t*>(taskStackTop)[pos])
            break;
    }
    return (pos - 1) * sizeof(uint32_t);
}

void CoopTask::doYield(uint32_t val)
{
    if (!setjmp(env_yield))
    {
        longjmp(env, val);
    }
}

void CoopTask::_exit(int code)
{
    _exitCode = code;
    longjmp(env, 1);
}

void CoopTask::_yield()
{
    doYield(2);
}

void CoopTask::_sleep()
{
    doYield(3);
}

void CoopTask::_delay(uint32_t ms)
{
    delay_ms = true;
    delay_exp = millis() + ms;
    // CoopTask::run() sleeps task until delay_exp is reached
    doYield(4);
}

void CoopTask::_delayMicroseconds(uint32_t us)
{
    if (us < DELAYMICROS_THREASHOLD) {
        ::delayMicroseconds(us);
        return;
    }
    delay_ms = false;
    delay_exp = micros() + us;
    // CoopTask::run() sleeps task until delay_exp is reached
    doYield(4);
}
