/*
 Name:		mutex.ino
 Created:	2019-08-04 22:40:11
 Author:	dok@dok-net.net
*/

#include <CoopTask.h>
#include <CoopSemaphore.h>
#include <CoopMutex.h>

CoopMutex serialMutex;
CoopMutex mutex;
CoopTaskBase* hasMutex(nullptr);

void haveMutex()
{
    if (hasMutex)
    {
        CoopMutexLock serialLock(serialMutex);
        Serial.print(CoopTaskBase::self()->name());
        Serial.print(" called haveMutex, despite ");
        Serial.print(hasMutex->name());
        Serial.println(" is known to have mutex.");
        return;
    }
    hasMutex = CoopTaskBase::self();
}

void yieldMutex()
{
    if (hasMutex != CoopTaskBase::self())
    {
        CoopMutexLock serialLock(serialMutex);
        Serial.print(CoopTaskBase::self()->name());
        Serial.println(" called yieldMutex, but no task currently has the mutex.");
        return;
    }
    hasMutex = nullptr;
}

CoopTask<int>* firstTask;
CoopTask<int>* secondTask;
CoopTask<int>* thirdTask;

#ifdef ESP32
TaskHandle_t yieldGuardHandle;
#endif

void setup() {
#ifdef ESP8266
    Serial.begin(74880);
#else
    Serial.begin(115200);
#endif
    while (!Serial) {}
    delay(500);
    Serial.println("Mutex test");

    firstTask = scheduleTask("first", []()
        {
            {
                CoopMutexLock serialLock(serialMutex);
                Serial.print(CoopTaskBase::self()->name());
                Serial.println(" starts");
            }
            for (int i = 0; i < 30; ++i)
            {
                {
                    CoopMutexLock serialLock(serialMutex);
                    Serial.print(CoopTaskBase::self()->name());
                    Serial.println(" locks mutex");
                }
                {
                    CoopMutexLock lock(mutex);
                    if (!lock) {
                        CoopMutexLock serialLock(serialMutex);
                        Serial.print("failed to lock mutex in ");
                        Serial.println(CoopTaskBase::self()->name());
                    }
                    haveMutex();
                    {
                        CoopMutexLock serialLock(serialMutex);
                        Serial.print(CoopTaskBase::self()->name());
                        Serial.println(" has mutex");
                    }
                    yield();
                    yieldMutex();
                }
                {
                    CoopMutexLock serialLock(serialMutex);
                    Serial.print(CoopTaskBase::self()->name());
                    Serial.print(" runs (");
                    Serial.print(i);
                    Serial.println(")");
                }
                yield();
            }
            return 0;
        }
#if defined(ESP8266) || defined(ESP32)
    );
#else
    , 0x120);
#endif
    if (!firstTask) Serial.println("firstTask not scheduled");
    secondTask = scheduleTask("second", []()
        {
            {
                CoopMutexLock serialLock(serialMutex);
                Serial.print(CoopTaskBase::self()->name());
                Serial.println(" starts");
            }
            for (int i = 0; i < 30; ++i)
            {
                {
                    CoopMutexLock serialLock(serialMutex);
                    Serial.print(CoopTaskBase::self()->name());
                    Serial.println(" locks mutex");
                }
                {
                    CoopMutexLock lock(mutex);
                    if (!lock) {
                        CoopMutexLock serialLock(serialMutex);
                        Serial.print("failed to lock mutex in ");
                        Serial.println(CoopTaskBase::self()->name());
                    }
                    haveMutex();
                    {
                        CoopMutexLock serialLock(serialMutex);
                        Serial.print(CoopTaskBase::self()->name());
                        Serial.println(" has mutex");
                    }
                    yield();
                    yieldMutex();
                }
                {
                    CoopMutexLock serialLock(serialMutex);
                    Serial.print(CoopTaskBase::self()->name());
                    Serial.print(" runs (");
                    Serial.print(i);
                    Serial.println(")");
                }
                yield();
            }
            return 0;
        }
#if defined(ESP8266) || defined(ESP32)
    );
#else
    , 0x120);
#endif
    if (!secondTask) Serial.println("secondTask not scheduled");
    thirdTask = scheduleTask("third", []()
        {
            {
                CoopMutexLock serialLock(serialMutex);
                Serial.print(CoopTaskBase::self()->name());
                Serial.println(" starts");
            }
            for (int i = 0; i < 10; ++i)
            {
                {
                    CoopMutexLock serialLock(serialMutex);
                    Serial.print(CoopTaskBase::self()->name());
                    Serial.println(" locks mutex");
                }
                {
                    CoopMutexLock lock(mutex);
                    if (!lock) {
                        CoopMutexLock serialLock(serialMutex);
                        Serial.print("failed to lock mutex in ");
                        Serial.println(CoopTaskBase::self()->name());
                    }
                    haveMutex();
                    {
                        CoopMutexLock serialLock(serialMutex);
                        Serial.print(CoopTaskBase::self()->name());
                        Serial.println(" has mutex");
                    }
                    yield();
                    yieldMutex();
                }
                {
                    CoopMutexLock serialLock(serialMutex);
                    Serial.print(CoopTaskBase::self()->name());
                    Serial.print(" runs (");
                    Serial.print(i);
                    Serial.println(")");
                }
                yield();
            }
            for (int i = 0; i < 10; ++i)
            {
                {
                    CoopMutexLock serialLock(serialMutex);
                    Serial.print(CoopTaskBase::self()->name());
                    Serial.print(" still runs (");
                    Serial.print(i);
                    Serial.println(")");
                }
                yield();
            }
            return 0;
        }
#if defined(ESP8266) || defined(ESP32)
    );
#else
    , 0x120);
#endif
    if (!thirdTask) Serial.println("thirdTask not scheduled");

#ifdef ESP32
    Serial.print("Loop free stack = "); Serial.println(uxTaskGetStackHighWaterMark(NULL));

    xTaskCreateUniversal([](void*)
        {
            for (;;)
            {
                vPortYield();
            }
        }, "YieldGuard", 8192, nullptr, 1, &yieldGuardHandle, CONFIG_ARDUINO_RUNNING_CORE);
#endif
}

// the loop function runs over and over again until power down or reset
void loop() {
#if !defined(ESP8266)
    uint32_t taskCount = 0;
    uint32_t minDelay = ~0UL;
    for (int i = 0; i < BasicCoopTask<>::getRunnableTasks().size(); ++i)
    {
        auto task = BasicCoopTask<>::getRunnableTasks()[i].load();
        if (task)
        {
            auto runResult = task->run();
            if (runResult < 0)
            {
                Serial.print("deleting task ");
                Serial.println(task->name());
                delete task;
            }
            if (task->delayed() && runResult < minDelay) minDelay = runResult;
            if (++taskCount >= BasicCoopTask<>::getRunnableTasksCount()) break;
        }
    }
#ifdef ESP32
    if (minDelay)
    {
        vTaskSuspend(yieldGuardHandle);
        vTaskDelay(1);
        vTaskResume(yieldGuardHandle);
    }
#endif
#endif
}
