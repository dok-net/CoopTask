/*
 Name:		mutex.ino
 Created:	2019-08-04 22:40:11
 Author:	dok@dok-net.net
*/

#include <CoopTask.h>
#include <CoopSemaphore.h>
#include <CoopMutex.h>

CoopMutex mutex;
CoopTaskBase* hasMutex(nullptr);

void haveMutex()
{
    if (hasMutex)
    {
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
    Serial.println("Mutex test");

    firstTask = scheduleTask("first", []()
        {
            Serial.print(CoopTaskBase::self()->name());
            Serial.println(" starts");
            for (int i = 0; i < 30; ++i)
            {
                {
                    Serial.print(CoopTaskBase::self()->name());
                    Serial.println(" locks mutex");
                    CoopMutexLock lock(mutex);
                    if (!lock) {
                        Serial.print("failed to lock mutex in ");
                        Serial.println(CoopTaskBase::self()->name());
                    }
                    haveMutex();
                    Serial.print(CoopTaskBase::self()->name());
                    Serial.println(" has mutex");
                    yield();
                    yieldMutex();
                }
                Serial.print(CoopTaskBase::self()->name());
                Serial.print(" runs (");
                Serial.print(i);
                Serial.println(")");
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
            Serial.print(CoopTaskBase::self()->name());
            Serial.println(" starts");
            for (int i = 0; i < 30; ++i)
            {
                {
                    Serial.print(CoopTaskBase::self()->name());
                    Serial.println(" locks mutex");
                    CoopMutexLock lock(mutex);
                    if (!lock) {
                        Serial.print("failed to lock mutex in ");
                        Serial.println(CoopTaskBase::self()->name());
                    }
                    haveMutex();
                    Serial.print(CoopTaskBase::self()->name());
                    Serial.println(" has mutex");
                    yield();
                    yieldMutex();
                }
                Serial.print(CoopTaskBase::self()->name());
                Serial.print(" runs (");
                Serial.print(i);
                Serial.println(")");
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
            Serial.print(CoopTaskBase::self()->name());
            Serial.println(" starts");
            for (int i = 0; i < 10; ++i)
            {
                {
                    Serial.print(CoopTaskBase::self()->name());
                    Serial.println(" locks mutex");
                    CoopMutexLock lock(mutex);
                    if (!lock) {
                        Serial.print("failed to lock mutex in ");
                        Serial.println(CoopTaskBase::self()->name());
                    }
                    haveMutex();
                    Serial.print(CoopTaskBase::self()->name());
                    Serial.println(" has mutex");
                    yield();
                    yieldMutex();
                }
                Serial.print(CoopTaskBase::self()->name());
                Serial.print(" runs (");
                Serial.print(i);
                Serial.println(")");
                yield();
            }
            for (int i = 0; i < 10; ++i)
            {
                Serial.print(CoopTaskBase::self()->name());
                Serial.print(" still runs (");
                Serial.print(i);
                Serial.println(")");
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
}

// the loop function runs over and over again until power down or reset
void loop() {
#if !defined(ESP8266)
    uint32_t taskCount = 0;
    for (int i = 0; i < CoopTaskBase::getRunnableTasks().size(); ++i)
    {
        auto task = static_cast<CoopTask<>*>(CoopTaskBase::getRunnableTasks()[i].load());
        if (task)
        {
            if (task->run() < 0)
            {
                Serial.print("deleting task ");
                Serial.println(task->name());
                delete task;
            }
            if (task && ++taskCount >= CoopTaskBase::getRunnableTasksCount()) break;
        }
    }
#endif
}
