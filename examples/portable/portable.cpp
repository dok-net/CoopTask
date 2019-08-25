// portable.cpp
// This is a basic portable example, without a scheduler.
// All tasks are run round-robin inside a for loop.
// It shows CoopTask creation, synchronization, and termination.

#include <iostream>
#include "CoopTask.h"
#include "CoopSemaphore.h"

void printStackReport(BasicCoopTask<>& task)
{
    if (!task) return;
    std::cerr << task.name().c_str() << " free stack = " << task.getFreeStack() << std::endl;
}

int main()
{
    CoopSemaphore terminatorSema(0);
    CoopSemaphore helloSema(0);

    CoopTask<> hello(std::string("hello"), [&terminatorSema, &helloSema]() noexcept
        {
            std::cerr << "Hello" << std::endl;
            yield();
            for (int x = 0; x < 10; ++x)
            {
                std::cerr << "Loop" << std::endl;
                helloSema.wait(2000);
            }
            terminatorSema.post();
            return 0;
        }, 0x2000);
    if (!hello) std::cerr << hello.name() << " CoopTask not created" << std::endl;


    bool keepBlinking = true;

    CoopTask<> terminator(std::string("terminator"), [&keepBlinking, &terminatorSema]() noexcept
        {
            if (!terminatorSema.wait()) std::cerr << "terminatorSema.wait() failed" << std::endl;
            keepBlinking = false;
            return 0;
        }, 0x2000);
    if (!terminator) std::cerr << terminator.name() << " CoopTask not created" << std::endl;

    CoopTask<std::string> blink(std::string("blink"), [&keepBlinking]()
        {
            while (keepBlinking)
            {
                std::cerr << "LED on" << std::endl;
                delay(1000);
                std::cerr << "LED off" << std::endl;
                delay(1000);
            }
            throw std::string("sixtynine");
            return "fortytwo";
        }, 0x2000);
    if (!blink) std::cerr << blink.name() << " CoopTask not created" << std::endl;

    CoopTask<> report(std::string("report"), [&hello, &blink]() noexcept
        {
            for (;;) {
                delay(5000);
                printStackReport(hello);
                printStackReport(blink);
            }
            return 0;
        }, 0x2000);
    if (!report) std::cerr << report.name() << " CoopTask not created" << std::endl;

    for (;;)
    {
        terminator.run();
        hello.run();
        // once: hello posts terminatorSema -> terminator sets keepBlinking = false -> blink exits -> break leaves for-loop -> program exits
        if (blink.run() < 0)
        {
            std::cerr << blink.name() << " returns = " << blink.exitCode() << std::endl;
            break;
        }
        report.run();
    }
    return 0;
}
