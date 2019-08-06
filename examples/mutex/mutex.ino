/*
 Name:		mutex.ino
 Created:	2019-08-04 22:40:11
 Author:	dok@dok-net.net
*/

#include <CoopTask.h>
#include <CoopSemaphore.h>
#include <CoopMutex.h>

CoopMutex mutex;
BasicCoopTask* hasMutex(nullptr);

void haveMutex()
{
    if (hasMutex)
    {
        Serial.print(BasicCoopTask::self().name());
        Serial.print(" called haveMutex, despite ");
        Serial.print(hasMutex->name());
        Serial.println(" is known to have mutex.");
        return;
    }
    hasMutex = &BasicCoopTask::self();
}

void yieldMutex()
{
    if (hasMutex != &BasicCoopTask::self())
    {
        Serial.print(BasicCoopTask::self().name());
        Serial.println(" called yieldMutex, but no task currently has the mutex.");
        return;
    }
    hasMutex = nullptr;
}

BasicCoopTask* firstTask;
BasicCoopTask* secondTask;
BasicCoopTask* thirdTask;

#ifndef ESP8266
BasicCoopTask** tasks[] = { &firstTask, &secondTask, &thirdTask };
#endif

void setup() {
    Serial.begin(115200);
    Serial.println("Mutex test");

    firstTask = scheduleTask("first", []()
        {
            Serial.print(BasicCoopTask::self().name());
            Serial.println(" starts");
            for (int i = 0; i < 30; ++i)
            {
                {
                    CoopMutexLock lock(mutex);
                    if (!lock) {
                        Serial.print("failed to lock mutex in ");
                        Serial.println(BasicCoopTask::self().name());
                    }
                    haveMutex();
                    Serial.print(BasicCoopTask::self().name());
                    Serial.println(" has mutex");
                    yield();
                    yieldMutex();
                }
                Serial.print(BasicCoopTask::self().name());
                Serial.println(" runs");
                yield();
            }
            return 0;
        }
#if defined(ESP8266) || defined(ESP32)
    );
#else
    , 0x140);
#endif
    if (!firstTask) Serial.println("firstTask not scheduled");
    secondTask = scheduleTask("second", []()
        {
            Serial.print(BasicCoopTask::self().name());
            Serial.println(" starts");
            for (int i = 0; i < 30; ++i)
            {
                {
                    CoopMutexLock lock(mutex);
                    if (!lock) {
                        Serial.print("failed to lock mutex in ");
                        Serial.println(BasicCoopTask::self().name());
                    }
                    haveMutex();
                    Serial.print(BasicCoopTask::self().name());
                    Serial.println(" has mutex");
                    yield();
                    yieldMutex();
                }
                Serial.print(BasicCoopTask::self().name());
                Serial.println(" runs");
                yield();
            }
            return 0;
        }
#if defined(ESP8266) || defined(ESP32)
    );
#else
    , 0x140);
#endif
    if (!secondTask) Serial.println("secondTask not scheduled");
    thirdTask = scheduleTask("third", []()
        {
            Serial.print(BasicCoopTask::self().name());
            Serial.println(" starts");
            for (int i = 0; i < 10; ++i)
            {
                {
                    CoopMutexLock lock(mutex);
                    if (!lock) {
                        Serial.print("failed to lock mutex in ");
                        Serial.println(BasicCoopTask::self().name());
                    }
                    haveMutex();
                    Serial.print(BasicCoopTask::self().name());
                    Serial.println(" has mutex");
                    yield();
                    yieldMutex();
                }
                Serial.print(BasicCoopTask::self().name());
                Serial.println(" runs");
                yield();
            }
            for (int i = 0; i < 10; ++i)
            {
                Serial.print(BasicCoopTask::self().name());
                Serial.println(" still runs");
                yield();
            }
            return 0;
        }
#if defined(ESP8266) || defined(ESP32)
    );
#else
    , 0x140);
#endif
    if (!thirdTask) Serial.println("thirdTask not scheduled");
}

// the loop function runs over and over again until power down or reset
void loop() {
#ifndef ESP8266
    for (auto task : tasks)
    {
        if (!*task) continue;
        if (!(*task)->run())
        {
            Serial.print("deleting task ");
            Serial.println((*task)->name());
            delete *task;
            *task = nullptr;
        }
    }
#endif
}
