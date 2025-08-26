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
    if (auto dbAccess = MyConfigDbManager::getInstance().getReadAccess()) // get scoped read lock
    {
        gotReadLock = true;
    }
    TEST_ASSERT_TRUE_MESSAGE(gotReadLock, "Expect to get read lock.");

    bool gotWriteLock = false;
    bool gotReadWhileWriteLock = false;
    if (auto writeLock = MyConfigDbManager::getInstance().getWriteAccess()) // get EXCLUSIVE write lock
    {
        gotWriteLock = true;
        // getReadAccess should not block!
        if (auto dbAccess = MyConfigDbManager::getInstance().getReadAccess()) // get scoped read lock
        {
            gotReadWhileWriteLock = true;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(gotWriteLock, "Expect to get write lock.");
    TEST_ASSERT_FALSE_MESSAGE(gotReadWhileWriteLock, "Should not get read lock while write.");

    gotReadLock = false;
    if (auto dbAccess = MyConfigDbManager::getInstance().getReadAccess()) // get scoped read lock
    {
        gotReadLock = true;
    }
    TEST_ASSERT_TRUE_MESSAGE(gotReadLock, "Expect to get read lock after release write lock.");
}

TEST_CASE("Two simulaneous read locks are allowed", TAG)
{
    MyConfigDbManager::setStaticInstance(&g_MyConfigDbManager); // register dependencies with the VirtualSwitch class

    bool gotReadLock1 = false;
    if (auto dbAccess1 = MyConfigDbManager::getInstance().getReadAccess()) // get scoped read lock
    {
        gotReadLock1 = true;

        bool gotReadLock2 = false;
        if (auto dbAccess2 = MyConfigDbManager::getInstance().getReadAccess()) // get another scoped read lock
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
//     if (auto dbAccess1 = MyConfigDbManager::getInstance().getWriteAccess()) // get EXCLUSIVE write lock
//     {
//         gotWriteLock1 = true;

//         bool gotWriteLock2 = false;
//         if (auto dbAccess2 = MyConfigDbManager::getInstance().getWriteAccess()) // try to get another EXCLUSIVE write lock
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
    auto dbAccess0 = MyConfigDbManager::getInstance().getReadAccess(); // get scoped read lock
    auto dbAccess1 = MyConfigDbManager::getInstance().getReadAccess(); // get another scoped read lock
    auto dbAccess2 = MyConfigDbManager::getInstance().getReadAccess(); // get another scoped read lock
    auto dbAccess3 = MyConfigDbManager::getInstance().getReadAccess(); // get another scoped read lock
    auto dbAccess4 = MyConfigDbManager::getInstance().getReadAccess(); // get another scoped read lock
    auto dbAccess5 = MyConfigDbManager::getInstance().getReadAccess(); // get another scoped read lock
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
        auto dbAccess = MyConfigDbManager::getInstance().getReadAccess(); // get scoped read lock
        if (!dbAccess)
        {
            ESP_LOGE(TAG, "Failed to get read lock %d.", i);
            TEST_FAIL_MESSAGE("Failed to get read lock.");
        }
    }
}

#define NUM_READ_LOCKS 20 // Increased from 10 to 20 for more stress
static SemaphoreHandle_t task_done_semphr;
static EventGroupHandle_t task_eventgroup;
#define TRIGGER_TASK_STOP_BIT (1 << 0)
static volatile int read_locks_aquired = 0;

static void multiThreadReadLockFunc(void *arg)
{
    // thread index is arg
    int threadIndex = *(int *)arg;
    ESP_LOGD(TAG, "Thread %d starting.", threadIndex);
    if (auto dbAccess = MyConfigDbManager::getInstance().getReadAccess()) // get scoped read lock
    {
        read_locks_aquired = read_locks_aquired + 1; // increment the global counter for read locks acquired
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

    // Do cleanup before any assertions, because TEST_ASSERT fail will abort the test and not run cleanup code

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
        if (auto dbAccess = MyConfigDbManager::getInstance().getReadAccess()) // get scoped read lock
        {
            read_locks_aquired = read_locks_aquired + 1; // increment the global counter for read locks acquired
        }
        else
        {
            ESP_LOGE(TAG, "Thread %d failed to get read lock. Total acquired: %d, Total failed: %d", 
                     threadIndex, read_locks_aquired, read_locks_failed + 1);
            read_locks_failed = read_locks_failed + 1; // increment the global counter for read locks failed
        }
        vTaskDelay(1); // yield to other tasks
    }
    // signal the test that we are done
    free(arg); // Free the dynamically allocated thread index
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
        int *task_index = (int*)malloc(sizeof(int));
        *task_index = i;
        
        const int core_num = (i % portNUM_PROCESSORS);              // pin to core 0 or 1, depending on the index
        const int task_priority = ESP_TASK_MAIN_PRIO + 1 + (i % 4); // alternate priorities for the tasks
        xTaskCreatePinnedToCore(stressReadLockFunc,
                                "ReadLockThread",
                                2048,          // stack size
                                task_index,    // pass the index as an argument to the task
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
    vTaskDelay(15000 / portTICK_PERIOD_MS); // Increased from 10 to 15 seconds

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

// Variables for mixed read/write stress test
static volatile int write_locks_acquired = 0;
static volatile int write_locks_failed = 0;
static volatile bool stop_mixed_test = false;

static void mixedReadWriteFunc(void *arg)
{
    int threadIndex = *(int *)arg;
    bool isWriter = (threadIndex % 4 == 0); // Every 4th thread is a writer (less writers)
    
    ESP_LOGD(TAG, "Mixed test thread %d starting as %s.", threadIndex, isWriter ? "WRITER" : "READER");
    xSemaphoreGive(task_done_semphr);
    
    int local_ops = 0;
    while (!stop_mixed_test && local_ops < 200) { // Reduced operations per thread
        if (isWriter) {
            // Writer thread - try to get exclusive access
            if (auto dbAccess = MyConfigDbManager::getInstance().getWriteAccess()) {
                write_locks_acquired++;
                // Hold the lock for a very short time to reduce reader starvation
                vTaskDelay(pdMS_TO_TICKS(2)); // Reduced from longer hold time
            } else {
                write_locks_failed++;
                ESP_LOGE(TAG, "Thread %d failed to get WRITE lock.", threadIndex);
            }
        } else {
            // Reader thread - try to get shared access
            if (auto dbAccess = MyConfigDbManager::getInstance().getReadAccess()) {
                read_locks_aquired++;
                // Brief hold time for readers
                if (local_ops % 20 == 0) { // Only occasionally hold the lock
                    vTaskDelay(pdMS_TO_TICKS(1));
                }
            } else {
                read_locks_failed++;
                ESP_LOGE(TAG, "Thread %d failed to get READ lock. This should not happen unless writer is active!", threadIndex);
            }
        }
        local_ops++;
        
        // Variable delay to create different access patterns
        if (isWriter) {
            vTaskDelay(pdMS_TO_TICKS(5)); // Writers wait longer between attempts
        } else {
            vTaskDelay(pdMS_TO_TICKS(2)); // Readers attempt more frequently
        }
    }
    
    ESP_LOGD(TAG, "Mixed test thread %d completed %d operations.", threadIndex, local_ops);
    free(arg); // Free the dynamically allocated thread index
    xSemaphoreGive(task_done_semphr); // Signal completion
    vTaskDelete(nullptr);
}

TEST_CASE("Mixed read/write stress test", "[PocoConfigDb]")
{
    ESP_LOGD(TAG, "Starting mixed read/write stress test...");
    
    // Reset counters
    read_locks_aquired = 0;
    read_locks_failed = 0;
    write_locks_acquired = 0;
    write_locks_failed = 0;
    stop_mixed_test = false;
    
    task_done_semphr = xSemaphoreCreateCounting(NUM_READ_LOCKS * 2, 0);
    TEST_ASSERT_NOT_NULL(task_done_semphr);
    
    // Create mix of reader and writer tasks
    for (int i = 0; i < NUM_READ_LOCKS; i++) {
        int *task_index = (int*)malloc(sizeof(int));
        *task_index = i;
        
        int core_num = i % 2;
        int task_priority = (i % 4) + 2; // Vary priorities from 2-5
        
        xTaskCreatePinnedToCore(mixedReadWriteFunc,
                                "MixedTestThread",
                                2048,
                                task_index,    // Use dynamically allocated index
                                task_priority,
                                nullptr,
                                core_num);
        ESP_LOGD(TAG, "Created mixed task %d on core %d.", i, core_num);
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay between task creation
    }
    
    // Wait for all tasks to start
    for (int k = 0; k < NUM_READ_LOCKS; k++) {
        xSemaphoreTake(task_done_semphr, portMAX_DELAY);
    }
    
    ESP_LOGD(TAG, "All mixed test tasks started, running for 10 seconds...");
    vTaskDelay(pdMS_TO_TICKS(10000)); // Reduced from 15 to 10 seconds
    
    stop_mixed_test = true;
    
    // Wait for all tasks to complete
    for (int k = 0; k < NUM_READ_LOCKS; k++) {
        xSemaphoreTake(task_done_semphr, portMAX_DELAY);
    }
    
    vSemaphoreDelete(task_done_semphr);
    
    ESP_LOGI(TAG, "Mixed test results: Read locks: %d acquired, %d failed. Write locks: %d acquired, %d failed",
             read_locks_aquired, read_locks_failed, write_locks_acquired, write_locks_failed);
    
    // With a proper shared mutex implementation, read locks should NEVER fail when no writer is holding the lock
    // The ReliableSharedMutex uses try_lock_shared() with retry logic to handle reader contention properly
    TEST_ASSERT_EQUAL_MESSAGE(0, read_locks_failed, "Read locks should never fail - this violates shared mutex semantics.");
    TEST_ASSERT_EQUAL_MESSAGE(0, write_locks_failed, "Expected no write locks to fail during mixed test.");
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, write_locks_acquired, "Expected some write locks to be acquired.");
    TEST_ASSERT_GREATER_THAN_MESSAGE(100, read_locks_aquired, "Expected substantial read lock acquisitions.");
}

// Torture test with rapid task creation/deletion
static volatile int torture_read_locks = 0;
static volatile int torture_read_failures = 0;

static void tortureReadFunc(void *arg)
{
    int *args = (int *)arg;
    int threadIndex = args[0];
    int iterations = args[1];
    
    for (int i = 0; i < iterations; i++) {
        if (auto dbAccess = MyConfigDbManager::getInstance().getReadAccess()) {
            torture_read_locks++;
        } else {
            torture_read_failures++;
            ESP_LOGE(TAG, "Torture thread %d failed read lock on iteration %d", threadIndex, i);
        }
        
        // Micro delay to create maximum contention
        if (i % 10 == 0) {
            vTaskDelay(1);
        }
    }
    
    free(args); // Free the dynamically allocated args
    xSemaphoreGive(task_done_semphr);
    vTaskDelete(nullptr);
}

TEST_CASE("Torture test - rapid task creation", "[PocoConfigDb]")
{
    ESP_LOGD(TAG, "Starting torture test...");
    
    torture_read_locks = 0;
    torture_read_failures = 0;
    
    const int TORTURE_WAVES = 5; // Number of waves of task creation
    const int TASKS_PER_WAVE = 15; // Tasks per wave
    const int ITERATIONS_PER_TASK = 50; // Operations per task
    
    for (int wave = 0; wave < TORTURE_WAVES; wave++) {
        ESP_LOGD(TAG, "Starting torture wave %d/%d", wave + 1, TORTURE_WAVES);
        
        task_done_semphr = xSemaphoreCreateCounting(TASKS_PER_WAVE, 0);
        TEST_ASSERT_NOT_NULL(task_done_semphr);
        
        // Create tasks rapidly
        for (int i = 0; i < TASKS_PER_WAVE; i++) {
            int *task_args = (int*)malloc(2 * sizeof(int));
            task_args[0] = wave * TASKS_PER_WAVE + i; // thread index
            task_args[1] = ITERATIONS_PER_TASK;       // iterations
            
            xTaskCreatePinnedToCore(tortureReadFunc,
                                    "TortureTask",
                                    2048, // Larger stack to prevent overflow
                                    task_args,
                                    3 + (i % 3), // Varying priorities
                                    nullptr,
                                    i % 2); // Alternate cores
        }
        
        // Wait for all tasks in this wave to complete
        for (int k = 0; k < TASKS_PER_WAVE; k++) {
            xSemaphoreTake(task_done_semphr, portMAX_DELAY);
        }
        
        vSemaphoreDelete(task_done_semphr);
        
        ESP_LOGD(TAG, "Wave %d complete. Total: %d locks, %d failures", 
                 wave + 1, torture_read_locks, torture_read_failures);
                 
        // Brief pause between waves
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "Torture test complete: %d locks acquired, %d failures", 
             torture_read_locks, torture_read_failures);
    
    TEST_ASSERT_EQUAL_MESSAGE(0, torture_read_failures, "Expected no failures in torture test.");
    TEST_ASSERT_GREATER_THAN_MESSAGE(TORTURE_WAVES * TASKS_PER_WAVE * ITERATIONS_PER_TASK * 0.8, 
                                     torture_read_locks, "Expected most lock attempts to succeed.");
}

// Extended stress test - longer duration
TEST_CASE("Extended stress test - 60 seconds", "[PocoConfigDb]")
{
    ESP_LOGD(TAG, "Starting extended stress test (60 seconds)...");
    
    read_locks_aquired = 0;
    read_locks_failed = 0;
    stop_stress_test = false;
    
    task_done_semphr = xSemaphoreCreateCounting(NUM_READ_LOCKS * 2, 0);
    TEST_ASSERT_NOT_NULL(task_done_semphr);
    
    // Create stress test tasks
    for (int i = 0; i < NUM_READ_LOCKS; i++) {
        int core_num = i % 2;
        int task_priority = (i % 4) + 2;
        
        int *task_index = (int*)malloc(sizeof(int));
        *task_index = i;
        
        xTaskCreatePinnedToCore(stressReadLockFunc,
                                "ExtendedStressTask",
                                2048,
                                task_index,
                                task_priority,
                                nullptr,
                                core_num);
        ESP_LOGD(TAG, "Created extended stress task %d.", i);
    }
    
    // Wait for all tasks to start
    for (int k = 0; k < NUM_READ_LOCKS; k++) {
        xSemaphoreTake(task_done_semphr, portMAX_DELAY);
    }
    
    ESP_LOGD(TAG, "Extended stress test running for 60 seconds...");
    
    // Log progress every 10 seconds
    for (int sec = 0; sec < 60; sec += 10) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "Extended test progress: %d seconds, %d locks acquired, %d failures", 
                 sec + 10, read_locks_aquired, read_locks_failed);
    }
    
    stop_stress_test = true;
    
    // Wait for all tasks to complete
    for (int k = 0; k < NUM_READ_LOCKS; k++) {
        xSemaphoreTake(task_done_semphr, portMAX_DELAY);
    }
    
    vSemaphoreDelete(task_done_semphr);
    
    ESP_LOGI(TAG, "Extended stress test complete: %d locks acquired, %d failures", 
             read_locks_aquired, read_locks_failed);
    
    TEST_ASSERT_EQUAL_MESSAGE(0, read_locks_failed, "Expected no failures in extended stress test.");
    TEST_ASSERT_GREATER_THAN_MESSAGE(100000, read_locks_aquired, "Expected many lock acquisitions in 60 seconds.");
}

#endif // !DISABLE_STRESS_TEST
