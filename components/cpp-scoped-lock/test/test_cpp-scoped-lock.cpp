/*
  Unit tests for cpp scoped lock implementation.
*/

/* Enable this to show verbose logging for this file only. */
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "esp_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include <limits.h>
#include "unity.h"
#include "MyConfigDb.hpp"

#define TAG "[PocoConfigDb]"

TEST_CASE("Locking", TAG)
{
    auto dbMan = MyConfigDbManager();

    bool gotReadLock = false;
    if (auto readLock = dbMan.getReadAccess())
    {
        gotReadLock = true;
    }
    TEST_ASSERT_TRUE_MESSAGE(gotReadLock, "Expect to get read lock.");

    bool gotWriteLock = false;
    bool gotReadWhileWriteLock = false;
    if (auto writeLock = dbMan.getWriteAccess())
    {
        gotWriteLock = true;
        // getReadAccess should not block!
        if (auto readLock = dbMan.getReadAccess())
        {
            gotReadWhileWriteLock = true;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(gotWriteLock, "Expect to get write lock.");
    TEST_ASSERT_FALSE_MESSAGE(gotReadWhileWriteLock, "Should not get read lock while write.");

    gotReadLock = false;
    if (auto readLock = dbMan.getReadAccess())
    {
        gotReadLock = true;
    }
    TEST_ASSERT_TRUE_MESSAGE(gotReadLock, "Expect to get read lock after release write lock.");
}

// the global instance of the config structure.  This thing owns everything.
static MyConfigDbManager g_MyConfigDbManager{};

TEST_CASE("Locking with global", TAG)
{
    MyConfigDbManager::setStaticInstance(&g_MyConfigDbManager); // register dependencies with the VirtualSwitch class

    bool gotReadLock = false;
    if (auto dbAccess = MyConfigDbManager::getManager().getReadAccess()) // get scoped read lock
    {
        gotReadLock = true;
    }
    TEST_ASSERT_TRUE_MESSAGE(gotReadLock, "Expect to get read lock.");

    bool gotWriteLock = false;
    bool gotReadWhileWriteLock = false;
    if (auto writeLock = MyConfigDbManager::getManager().getWriteAccess()) // get EXCLUSIVE write lock
    {
        gotWriteLock = true;
        // getReadAccess should not block!
        if (auto dbAccess = MyConfigDbManager::getManager().getReadAccess()) // get scoped read lock
        {
            gotReadWhileWriteLock = true;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(gotWriteLock, "Expect to get write lock.");
    TEST_ASSERT_FALSE_MESSAGE(gotReadWhileWriteLock, "Should not get read lock while write.");

    gotReadLock = false;
    if (auto dbAccess = MyConfigDbManager::getManager().getReadAccess()) // get scoped read lock
    {
        gotReadLock = true;
    }
    TEST_ASSERT_TRUE_MESSAGE(gotReadLock, "Expect to get read lock after release write lock.");
}

TEST_CASE("Two simulaneous read locks are allowed", TAG)
{
    MyConfigDbManager::setStaticInstance(&g_MyConfigDbManager); // register dependencies with the VirtualSwitch class

    bool gotReadLock1 = false;
    if (auto dbAccess1 = MyConfigDbManager::getManager().getReadAccess()) // get scoped read lock
    {
        gotReadLock1 = true;

        bool gotReadLock2 = false;
        if (auto dbAccess2 = MyConfigDbManager::getManager().getReadAccess()) // get another scoped read lock
        {
            gotReadLock2 = true;
        }
        TEST_ASSERT_TRUE_MESSAGE(gotReadLock2, "Expect to get second read lock.");
    }
    TEST_ASSERT_TRUE_MESSAGE(gotReadLock1, "Expect to get first read lock.");
}

// TEST_CASE("Two simulaneous write locks are not allowed", TAG)
// {
//     MyConfigDbManager::setStaticInstance(&g_MyConfigDbManager); // register dependencies with the VirtualSwitch class

//     bool gotWriteLock1 = false;
//     if (auto dbAccess1 = MyConfigDbManager::getManager().getWriteAccess()) // get EXCLUSIVE write lock
//     {
//         gotWriteLock1 = true;

//         bool gotWriteLock2 = false;
//         if (auto dbAccess2 = MyConfigDbManager::getManager().getWriteAccess()) // try to get another EXCLUSIVE write lock
//         {
//             gotWriteLock2 = true;
//         }
//         TEST_ASSERT_FALSE_MESSAGE(gotWriteLock2, "Should not get second write lock.");
//     }
//     TEST_ASSERT_TRUE_MESSAGE(gotWriteLock1, "Expect to get first write lock.");
// }
// TODO: this test blocks forever!

TEST_CASE("Many simulaneous read locks are allowed in single thread", TAG)
{
    MyConfigDbManager::setStaticInstance(&g_MyConfigDbManager); // register dependencies with the VirtualSwitch class
    // we will try to get 10 read locks in a row
    // this should not block, and should not fail
    auto dbAccess0 = MyConfigDbManager::getManager().getReadAccess(); // get scoped read lock
    auto dbAccess1 = MyConfigDbManager::getManager().getReadAccess(); // get another scoped read lock
    auto dbAccess2 = MyConfigDbManager::getManager().getReadAccess(); // get another scoped read lock
    auto dbAccess3 = MyConfigDbManager::getManager().getReadAccess(); // get another scoped read lock
    auto dbAccess4 = MyConfigDbManager::getManager().getReadAccess(); // get another scoped read lock
    auto dbAccess5 = MyConfigDbManager::getManager().getReadAccess(); // get another scoped read lock
    if (!dbAccess0)
    {
        TEST_FAIL_MESSAGE("Failed to get read lock 0.");
    }
    if (!dbAccess1)
    {
        TEST_FAIL_MESSAGE("Failed to get read lock 1.");
    }
    if (!dbAccess2)
    {
        TEST_FAIL_MESSAGE("Failed to get read lock 2.");
    }
    if (!dbAccess3)
    {
        TEST_FAIL_MESSAGE("Failed to get read lock 3.");
    }
    if (!dbAccess4)
    {
        TEST_FAIL_MESSAGE("Failed to get read lock 4.");
    }
    if (!dbAccess5)
    {
        TEST_FAIL_MESSAGE("Failed to get read lock 5.");
    }
}

TEST_CASE("Many sequential read locks are allowed", TAG)
{
    MyConfigDbManager::setStaticInstance(&g_MyConfigDbManager); // register dependencies with the VirtualSwitch class
    // we will try to get 100 read locks in a row
    // this should not block, and should not fail
    for (int i = 0; i < 100; i++)
    {
        auto dbAccess = MyConfigDbManager::getManager().getReadAccess(); // get scoped read lock
        if (!dbAccess)
        {
            ESP_LOGE(TAG, "Failed to get read lock %d.", i);
            TEST_FAIL_MESSAGE("Failed to get read lock.");
        }
    }
}

#define NUM_READ_LOCKS 10
static SemaphoreHandle_t task_done_semphr = nullptr; // semaphore to signal tasks are done
static EventGroupHandle_t task_eventgroup = nullptr; // event group to signal tasks to stop
#define TRIGGER_TASK_STOP_BIT (1 << 0)
static volatile int read_locks_aquired = 0; // global counter for read locks acquired

static void multiThreadReadLockFunc(void *arg)
{
    // thread index is arg
    int threadIndex = *(int *)arg;
    ESP_LOGD(TAG, "Thread %d starting.", threadIndex);
    if (auto dbAccess = MyConfigDbManager::getManager().getReadAccess()) // get scoped read lock
    {
        read_locks_aquired++; // increment the global counter for read locks acquired
        // dbAccess-> // just to use the dbAccess and not optimize it out
        ESP_LOGD(TAG, "Thread %d got read lock.", threadIndex);
        // signal the test that we are done
        xSemaphoreGive(task_done_semphr);
        // wait for stop signal, while keeping the read lock... This should be the simultaneous part of the test.
        xEventGroupWaitBits(task_eventgroup,
                            TRIGGER_TASK_STOP_BIT, // wait for the stop signal
                            pdFALSE,               // don't clear the bit on exit (we want to keep it set for other threads)
                            pdTRUE,                // wait for all specified bits to be set
                            portMAX_DELAY          // wait indefinitely for the stop signal
        );
        // we are done, so delete ourselves
        ESP_LOGD(TAG, "Thread %d stopping.", threadIndex);
    }

    // signal the test that we are done
    xSemaphoreGive(task_done_semphr);
    vTaskDelete(nullptr); // delete this task when done
    // Note: we don't need to explicitly release the lock, it will be released when dbAccess goes out of scope
}

TEST_CASE("Multiple simultaneous readlocks in multiple threads", TAG)
{
    MyConfigDbManager::setStaticInstance(&g_MyConfigDbManager); // register dependencies with the VirtualSwitch class

    // create an event group to signal tasks to stop
    task_eventgroup = xEventGroupCreate();
    // create a counting semaphore to let tasks signal they are done
    task_done_semphr = xQueueCreateCountingSemaphore(NUM_READ_LOCKS, 0);

    // reset test tracking vars
    read_locks_aquired = 0;

    // create several tasks that will try to acquire read locks simultaneously
    for (int i = 0; i < NUM_READ_LOCKS; i++)
    {
        const int core_num = (i % portNUM_PROCESSORS); // pin to core 0 or 1, depending on the index
        xTaskCreatePinnedToCore(multiThreadReadLockFunc,
                                "ReadLockThread",
                                2048,                   // stack size
                                &i,                     // pass the index as an argument to the task
                                ESP_TASK_MAIN_PRIO + 1, // priority of the task, should be higher than the test task
                                nullptr,                // we don't need the task handle, so pass nullptr
                                core_num);              // pin to core 0 or 1, depending on the index
        // Note: we pass the index as an argument to the task so that each task can identify itself
        ESP_LOGD(TAG, "Created task %d on core %d.", i, core_num);
    }

    // Wait for all tasks to signal they are done doing work
    for (int k = 0; k < NUM_READ_LOCKS; k++)
    {
        xSemaphoreTake(task_done_semphr, portMAX_DELAY);
    }

    // cleanup before any assertions, because TEST_ASSERT fail will abort the test and not run cleanup code

    // trigger tasks to stop and delete themselves
    xEventGroupSetBits(task_eventgroup, TRIGGER_TASK_STOP_BIT);
    // Wait for all tasks to signal they are have stopped and deleted themselves
    for (int k = 0; k < NUM_READ_LOCKS; k++)
    {
        xSemaphoreTake(task_done_semphr, portMAX_DELAY);
    }
    // Note: we don't need to delete the tasks, they will delete themselves when they exit
    vSemaphoreDelete(task_done_semphr);
    vEventGroupDelete(task_eventgroup);

    // Check that we got the expected number of read locks
    TEST_ASSERT_EQUAL_MESSAGE(NUM_READ_LOCKS, read_locks_aquired, "Expected to acquire all read locks in all threads.");
}

#define ENABLE_STRESS_TEST 1 // set to 1 to enable stress test, 0 to disable

#if ENABLE_STRESS_TEST
static volatile int read_locks_failed = 0;     // global counter for read locks failed
static volatile bool stop_stress_test = false; // global flag to stop the stress test

static void stressReadLockFunc(void *arg)
{
    // thread index is arg
    int threadIndex = *(int *)arg;
    ESP_LOGD(TAG, "Thread %d starting.", threadIndex);
    // signal the test that we are running
    xSemaphoreGive(task_done_semphr);
    while (false == stop_stress_test)
    { // run until the stop signal is set{
        // wait for the stop signal to be cleared
        if (auto dbAccess = MyConfigDbManager::getManager().getReadAccess()) // get scoped read lock
        {
            read_locks_aquired++; // increment the global counter for read locks acquired
        }
        else
        {
            ESP_LOGE(TAG, "Thread %d failed to get read lock.", threadIndex);
            read_locks_failed++; // increment the global counter for read locks failed
        }
        vTaskDelay(1); // yield to other tasks
    }
    // signal the test that we are done
    xSemaphoreGive(task_done_semphr);
    vTaskDelete(nullptr); // delete this task when done
    // Note: we don't need to explicitly release the lock, it will be released when dbAccess goes out of scope
}

TEST_CASE("locking stress-test", TAG)
{
    MyConfigDbManager::setStaticInstance(&g_MyConfigDbManager); // register dependencies with the VirtualSwitch class

    // flag to signal tasks to stop
    stop_stress_test = false; // reset the stop flag
    // create a counting semaphore to let tasks signal they are done
    task_done_semphr = xQueueCreateCountingSemaphore(NUM_READ_LOCKS, 0);

    // reset test tracking vars
    read_locks_aquired = 0;
    read_locks_failed = 0;

    // WTF: if I add this line, the test passes. But commenting it out causes the test to fail.
    ESP_LOGD(TAG, "Starting stress test. Spawning tasks...");

    // create several tasks that will try to acquire read locks simultaneously
    for (int i = 0; i < NUM_READ_LOCKS; i++)
    {
        const int core_num = (i % portNUM_PROCESSORS);              // pin to core 0 or 1, depending on the index
        const int task_priority = ESP_TASK_MAIN_PRIO + 1 + (i % 4); // alternate priorities for the tasks
        xTaskCreatePinnedToCore(stressReadLockFunc,
                                "ReadLockThread",
                                2048,          // stack size
                                &i,            // pass the index as an argument to the task
                                task_priority, // priority of the task, should be higher than the test task
                                nullptr,       // we don't need the task handle, so pass nullptr
                                core_num);     // pin to core 0 or 1, depending on the index
        // Note: we pass the index as an argument to the task so that each task can identify itself
        ESP_LOGD(TAG, "Created task %d on core %d with priority %d.", i, core_num, task_priority);
    }

    // Wait for all tasks to signal they have started doing work
    for (int k = 0; k < NUM_READ_LOCKS; k++)
    {
        xSemaphoreTake(task_done_semphr, portMAX_DELAY);
    }

    ESP_LOGD(TAG, "All tasks started, now wait for stress test...");
    // Now we will let the tasks run for a while, acquiring read locks
    vTaskDelay(10000 / portTICK_PERIOD_MS); // let the tasks run for 1 second

    ESP_LOGD(TAG, "Stress test done, got %d read locks", read_locks_aquired);

    // Do cleanup before any assertions, because TEST_ASSERT fail will abort the test and not run cleanup code

    // trigger tasks to stop and delete themselves
    ESP_LOGD(TAG, "Stopping tasks...");
    stop_stress_test = true; // set the stop flag
    // Wait for all tasks to signal they are have stopped and deleted themselves
    for (int k = 0; k < NUM_READ_LOCKS; k++)
    {
        xSemaphoreTake(task_done_semphr, portMAX_DELAY);
    }
    // Note: we don't need to delete the tasks, they will delete themselves when they exit
    vSemaphoreDelete(task_done_semphr);

    TEST_ASSERT_EQUAL_MESSAGE(0, read_locks_failed, "Expected no read locks to fail during stress test.");
}
#endif // !DISABLE_STRESS_TEST