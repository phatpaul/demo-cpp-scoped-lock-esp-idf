/*
 * LockableObject.hpp
 *  C++ class for a scoped lock access to an object.
 *
 *  Created on: May 15, 2024
 *      Author: Paul Abbott, Lumitec LLC
 */
#pragma once

#include <shared_mutex>
#include "ReliableSharedMutex.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

template <typename protectedType>
class LockableObject
{
public:
    // Option 1: Use reliable FreeRTOS-based implementation
    using mutex_type = ReliableSharedMutex;
    using read_lock = shared_lock<mutex_type>;
    using write_lock = unique_lock<mutex_type>;

    // Option 2: Keep std implementation for comparison (comment out above and uncomment below)
    // using mutex_type = std::shared_mutex;
    // using read_lock = std::shared_lock<mutex_type>;
    // using write_lock = std::unique_lock<mutex_type>;

private:
    mutable mutex_type m_mutex{};
    protectedType m_protected{};
    static inline LockableObject *sm_instance{}; // Optional instance pointer

public:
    // Optional Set Static Instance (when used as a singleton)
    static void setStaticInstance(LockableObject *ptr)
    {
        sm_instance = ptr;
    };
    // Optional Get Static Instance (when used as a singleton)
    static LockableObject &getInstance()
    {
        assert(sm_instance); // Dependency must be provided before use!
        return *sm_instance;
    };

    // returns a scoped lock that allows multiple readers but excludes writers
    // you should check that the lock was actually aquired with: if(lock.owns_lock())
    //read_lock lock_for_reading() const { return read_lock(m_mutex, std::try_to_lock); }

    // returns a scoped lock that allows only one writer and no one else
    //write_lock lock_for_writing() { return write_lock(m_mutex); }
    //-- Prefer to only allow access to the protected object using ScopedAccess class

    template <class objType, class lockType>
    class ScopedAccess
    {
    public:
        // Constructor
        ScopedAccess(mutex_type &m, objType &obj, bool blocking = true)
            : m_protectedRef{obj},
              m_lock{blocking ? lockType(m) : lockType(m, std::try_to_lock)}
        {
            // Direct initialization in member initializer list
        }

        // only allow access to the pointer with -> operator to prevent copying the protected object
        objType *operator->() const { return &m_protectedRef; }

        // operator bool
        //  - returns whether the exclusive lock is still active
        //  - enables use in 'if' expression to introduce local scope
        //
        explicit operator bool() const &
        {
            return m_lock.owns_lock();
        }

    private:
        // Reference to the protected resource
        objType &m_protectedRef;
        // a scoped lock that exists as long as this instance of ScopedAccess class lives
        lockType m_lock;
    };

    using ReadAccess = ScopedAccess<protectedType, read_lock>;
    using WriteAccess = ScopedAccess<protectedType, write_lock>;

    ReadAccess getReadAccess()
    {
        return ReadAccess(m_mutex, m_protected, false); // don't block for read
    }

    // Alternative with retry logic for ESP-IDF shared_mutex bug workaround
    ReadAccess getReadAccessWithRetry(int max_retries = 3)
    {
        static const char* TAG = "LockableObject";
        for (int attempt = 0; attempt < max_retries; ++attempt) {
            auto access = ReadAccess(m_mutex, m_protected, false);
            if (access) {
                if (attempt > 0) {
                    ESP_LOGW(TAG, "Read lock succeeded on attempt %d (task: %s)",
                             attempt + 1, pcTaskGetTaskName(xTaskGetCurrentTaskHandle()));
                }
                return access;
            }
            ESP_LOGW(TAG, "Read lock failed on attempt %d (task: %s), retrying...",
                     attempt + 1, pcTaskGetTaskName(xTaskGetCurrentTaskHandle()));
            // Brief delay before retry
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        // Final attempt
        auto final_access = ReadAccess(m_mutex, m_protected, false);
        if (!final_access) {
            ESP_LOGE(TAG, "Read lock failed after %d attempts (task: %s)",
                     max_retries + 1, pcTaskGetTaskName(xTaskGetCurrentTaskHandle()));
        }
        return final_access;
    }
    WriteAccess getWriteAccess()
    {
        return WriteAccess(m_mutex, m_protected, true); // block for write
    }

    // Clear the protected object, effectively resetting it.
    void reset(void)
    {
        if (auto lock = getWriteAccess())
        {
            m_protected = {}; // Clear the protected object. I hope this properly deconstructs the old object.
        }
    }
};
