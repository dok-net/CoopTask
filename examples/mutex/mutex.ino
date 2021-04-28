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
        Serial.print(F(" called haveMutex, despite "));
        Serial.print(hasMutex->name());
        Serial.println(F(" is known to have mutex."));
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
        Serial.println(F(" called yieldMutex, but no task currently has the mutex."));
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
    Serial.println(F("Mutex test"));

#if defined(ESP8266) && defined(USE_BUILTIN_TASK_SCHEDULER)
    CoopTaskBase::useBuiltinScheduler();
#endif

    firstTask = createCoopTask(F("first"), []()
        {
            {
                CoopMutexLock serialLock(serialMutex);
                Serial.print(CoopTaskBase::self()->name());
                Serial.println(F(" starts"));
            }
            for (int i = 0; i < 30; ++i)
            {
                {
                    CoopMutexLock serialLock(serialMutex);
                    Serial.print(CoopTaskBase::self()->name());
                    Serial.println(F(" locks mutex"));
                }
                {
                    CoopMutexLock lock(mutex);
                    if (!lock) {
                        CoopMutexLock serialLock(serialMutex);
                        Serial.print(F("failed to lock mutex in "));
                        Serial.println(CoopTaskBase::self()->name());
                    }
                    haveMutex();
                    {
                        CoopMutexLock serialLock(serialMutex);
                        Serial.print(CoopTaskBase::self()->name());
                        Serial.println(F(" has mutex"));
                    }
                    yield();
                    yieldMutex();
                }
                {
                    CoopMutexLock serialLock(serialMutex);
                    Serial.print(CoopTaskBase::self()->name());
                    Serial.print(F(" runs ("));
                    Serial.print(i);
                    Serial.println(')');
                }
                yield();
            }
            CoopMutexLock serialLock(serialMutex);
            Serial.print(F("exiting from task "));
            Serial.println(CoopTaskBase::self()->name());
            return 0;
        }
#if defined(ESP8266) || defined(ESP32)
    );
#else
    , 0x120);
#endif
    if (!firstTask) Serial.println(F("firstTask not created"));
    secondTask = createCoopTask(F("second"), []()
        {
            {
                CoopMutexLock serialLock(serialMutex);
                Serial.print(CoopTaskBase::self()->name());
                Serial.println(F(" starts"));
            }
            for (int i = 0; i < 30; ++i)
            {
                {
                    CoopMutexLock serialLock(serialMutex);
                    Serial.print(CoopTaskBase::self()->name());
                    Serial.println(F(" locks mutex"));
                }
                {
                    CoopMutexLock lock(mutex);
                    if (!lock) {
                        CoopMutexLock serialLock(serialMutex);
                        Serial.print(F("failed to lock mutex in "));
                        Serial.println(CoopTaskBase::self()->name());
                    }
                    haveMutex();
                    {
                        CoopMutexLock serialLock(serialMutex);
                        Serial.print(CoopTaskBase::self()->name());
                        Serial.println(F(" has mutex"));
                    }
                    yield();
                    yieldMutex();
                }
                {
                    CoopMutexLock serialLock(serialMutex);
                    Serial.print(CoopTaskBase::self()->name());
                    Serial.print(F(" runs ("));
                    Serial.print(i);
                    Serial.println(')');
                }
                yield();
            }
            CoopMutexLock serialLock(serialMutex);
            Serial.print(F("exiting from task "));
            Serial.println(CoopTaskBase::self()->name());
            return 0;
        }
#if defined(ESP8266) || defined(ESP32)
    );
#else
    , 0x120);
#endif
    if (!secondTask) Serial.println(F("secondTask not created"));
    thirdTask = createCoopTask(F("third"), []()
        {
            {
                CoopMutexLock serialLock(serialMutex);
                Serial.print(CoopTaskBase::self()->name());
                Serial.println(F(" starts"));
            }
            for (int i = 0; i < 10; ++i)
            {
                {
                    CoopMutexLock serialLock(serialMutex);
                    Serial.print(CoopTaskBase::self()->name());
                    Serial.println(F(" locks mutex"));
                }
                {
                    CoopMutexLock lock(mutex);
                    if (!lock) {
                        CoopMutexLock serialLock(serialMutex);
                        Serial.print(F("failed to lock mutex in "));
                        Serial.println(CoopTaskBase::self()->name());
                    }
                    haveMutex();
                    {
                        CoopMutexLock serialLock(serialMutex);
                        Serial.print(CoopTaskBase::self()->name());
                        Serial.println(F(" has mutex"));
                    }
                    yield();
                    yieldMutex();
                }
                {
                    CoopMutexLock serialLock(serialMutex);
                    Serial.print(CoopTaskBase::self()->name());
                    Serial.print(F(" runs ("));
                    Serial.print(i);
                    Serial.println(')');
                }
                yield();
            }
            for (int i = 0; i < 10; ++i)
            {
                {
                    CoopMutexLock serialLock(serialMutex);
                    Serial.print(CoopTaskBase::self()->name());
                    Serial.print(F(" still runs ("));
                    Serial.print(i);
                    Serial.println(')');
                }
                yield();
            }
            CoopMutexLock serialLock(serialMutex);
            Serial.print(F("exiting from task "));
            Serial.println(CoopTaskBase::self()->name());
            return 0;
        }
#if defined(ESP8266) || defined(ESP32)
    );
#else
    , 0x120);
#endif
    if (!thirdTask) Serial.println(F("thirdTask not created"));

#ifdef ESP32
    Serial.print(F("Loop free stack = ")); Serial.println(uxTaskGetStackHighWaterMark(NULL));
#endif
}

void taskReaper(const CoopTaskBase* const task)
{
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
