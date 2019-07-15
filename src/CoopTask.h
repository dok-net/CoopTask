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
    static constexpr uint32_t DELAYMICROS_THREASHOLD = 50;

#if defined(ESP8266) || defined(ESP32)
    typedef std::function< int() > taskfunc_t;
#else
    typedef int(*taskfunc_t)();
#endif

    const String taskName;
    taskfunc_t func;
    uint32_t taskStackSize;
    char* taskStackTop = nullptr;
    jmp_buf env;
    jmp_buf env_yield;
    // true: delay_exp is vs. millis(); false: delay_exp is vs. micros().
    bool delay_ms = false;
    uint32_t delay_exp = 0;
    bool init = false;
    bool cont = true;
    int _exitCode = 0;
    bool delayed = false;
    bool sleeping = false;

    static CoopTask* current;

    bool initialize();
    void doYield(uint32_t val);

    void _exit(int code = 0);
    void _yield();
    void _sleep();
    void _delay(uint32_t ms);
    void _delayMicroseconds(uint32_t us);

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

    // @returns: true if the CoopTask object is ready to run, including stack allocation.
    //           false if either initialization has failed, or the task has exited().
    operator bool();
    // @returns: 0: exited. 1: runnable or sleeping. >1: delayed until millis() or micros() deadline, check delayIsMs().
    uint32_t run();


    // @returns: default exit code is 0, using exit() the task can set a different value.
    int exitCode() { return _exitCode; }

    bool delayIsMs() { return delay_ms; }

    void sleep(const bool state) { sleeping = state; }

    /// use only in running CoopTask function. As stack unwinding is corrupted
    /// by exit(), which among other issues breaks the RAII idiom,
    /// using regular return is to be preferred in most cases.
    // @param code default exit code is 0, use exit() to set a different value.
    static void exit(int code = 0) { current->_exit(code); }
    /// use only in running CoopTask function.
    static void yield() { current->_yield(); }
    /// use only in running CoopTask function.
    static void sleep() { current->_sleep(); }
    /// use only in running CoopTask function.
    static void delay(uint32_t ms) { current->_delay(ms); }
    /// use only in running CoopTask function.
    static void delayMicroseconds(uint32_t us) { current->_delayMicroseconds(us); }
};
