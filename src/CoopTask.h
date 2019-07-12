#if defined(ESP8266) || defined(ESP32)
#include <functional>
#include <csetjmp>
#else
#include <setjmp.h>
#endif
#include <Arduino.h>

class CoopTask
{
protected:
    static constexpr uint32_t STACKCOOKIE = 0xdeadbeef;
#if defined(ESP32)
    static constexpr uint32_t MAXSTACKSPACE = 0x2000;
#elif defined (ESP8266)
    static constexpr uint32_t MAXSTACKSPACE = 0x1000;
#else
    static constexpr uint32_t MAXSTACKSPACE = 0x180;
#endif
    static constexpr uint32_t DEFAULTTASKSTACKSIZE = MAXSTACKSPACE - 2 * sizeof(STACKCOOKIE);

#if defined(ESP8266) || defined(ESP32)
    typedef std::function< void(CoopTask&) > taskfunc_t;
#else
    typedef void (*taskfunc_t)(CoopTask&);
#endif

    const String taskName;
    taskfunc_t func;
    uint32_t taskStackSize;
    char* taskStackTop = nullptr;
    jmp_buf env;
    jmp_buf env_yield;
    // true: delay_exp is vs. millis(); false: delay_exp is vs. micros()
    bool delay_ms = false;
    uint32_t delay_exp = 0;
    bool init = false;
    bool cont = true;
    bool delayed = false;

    static CoopTask* current;

    bool initialize();
    void doYield(uint32_t val);

public:
    CoopTask(const String& name, taskfunc_t _func, uint32_t stackSize = DEFAULTTASKSTACKSIZE) :
        taskName(name), func(_func), taskStackSize(stackSize)
    {
    }
    ~CoopTask()
    {
        delete[] taskStackTop;
    }

    const String& name() const { return taskName; }

    // @returns: true if the CoopTask objects is completely ready to run, including stack allocation.
    operator bool();
    // @returns: 0: exited. 1: runnable. >1: sleeps for x ms or us, check delayIsMs().
    uint32_t run();
    bool delayIsMs() { return delay_ms; }

    void _yield();
    void _delay(uint32_t ms);
    void _delayMicroseconds(uint32_t us);
    void _exit();

    static void yield() { current->_yield(); }
    static void delay(uint32_t ms) { current->_delay(ms); }
    static void delayMicroseconds(uint32_t us) { current->_delayMicroseconds(us); }
    static void exit() { current->_exit(); }
};
