/*
 Name:		mutex.ino
 Created:	2019-08-04 22:40:11
 Author:	dok@dok-net.net
*/

#include <CoopTask.h>
#include <CoopSemaphore.h>
#include <CoopMutex.h>

#define USE_BUILTIN_TASK_SCHEDULER

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

void setup() {
#ifdef ESP8266
    Serial.begin(74880);
#else
    Serial.begin(115200);
#endif
    while (!Serial) {}
    delay(500);
    Serial.println("Mutex test");

#if defined(ESP8266) && defined(USE_BUILTIN_TASK_SCHEDULER)
    CoopTaskBase::useBuiltinScheduler();
#endif

    firstTask = createCoopTask("first", []()
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
    if (!firstTask) Serial.println("firstTask not created");
    secondTask = createCoopTask("second", []()
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
    if (!secondTask) Serial.println("secondTask not created");
    thirdTask = createCoopTask("third", []()
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
    if (!thirdTask) Serial.println("thirdTask not created");

#ifdef ESP32
    Serial.print("Loop free stack = "); Serial.println(uxTaskGetStackHighWaterMark(NULL));
#endif
}

void taskReaper(const CoopTaskBase* const task)
{
    Serial.print("deleting task ");
    Serial.println(task->name());
    delete task;
}

// the loop function runs over and over again until power down or reset
void loop()
{
#if defined(ESP8266) && defined(USE_BUILTIN_TASK_SCHEDULER)
    if (firstTask && !*firstTask)
    {
        taskReaper(firstTask); firstTask = nullptr;
    }
    if (secondTask && !*secondTask)
    {
        taskReaper(secondTask); secondTask = nullptr;
    }
    if (thirdTask && !*thirdTask)
    {
        taskReaper(thirdTask); thirdTask = nullptr;
    }
#else
    runCoopTasks(taskReaper);
#endif
}
