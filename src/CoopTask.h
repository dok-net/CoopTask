#include <functional>
#include <csetjmp>
#include <Arduino.h>

class CoopTask
{
protected:
    static constexpr uint32_t STACKCOOKIE = 0xdeadbeef;
#ifdef ESP32
    static constexpr uint32_t MAXSTACKSPACE = 0x2000;
#else
    static constexpr uint32_t MAXSTACKSPACE = 0x1000;
#endif
    static constexpr uint32_t DEFAULTTASKSTACKSIZE = MAXSTACKSPACE - 2 * sizeof(STACKCOOKIE);
    void doYield(uint32_t val);
public:
    CoopTask(const String& name, std::function< void(CoopTask&) > _func, uint32_t stackSize = DEFAULTTASKSTACKSIZE) :
        taskName(name), func(_func), taskStackSize(stackSize), delay_exp(0)
    {
    }
    ~CoopTask()
    {
        delete[] taskStackTop;
    }
    const String taskName;
    std::function< void(CoopTask&) > func;
    uint32_t taskStackSize;
    char* taskStackTop = nullptr;
    jmp_buf env;
    jmp_buf env_yield;
    uint32_t delay_exp;
    bool init = false;
    bool cont = true;
    bool delayed = false;

    const String& name() const { return taskName; }
    operator bool();
    bool initialize();
    // @returns: 0: exited. 1: runnable. >1: sleeps for x ms.
    uint32_t run();

    void yield();
    void delay(uint32_t ms);
    void exit();
};
