/*
 * ReliableSharedMutex.hpp
 *
 * A more reliable shared mutex implementation using FreeRTOS primitives
 * to work around ESP-IDF std::shared_mutex issues.
 *
 * Author: Paul Abbott, Lumitec LLC
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

class ReliableSharedMutex {
private:
    SemaphoreHandle_t m_write_semaphore;     // Binary semaphore for write access
    SemaphoreHandle_t m_read_count_mutex;    // Mutex to protect reader count operations
    volatile int m_reader_count;             // Number of active readers
    volatile bool m_writer_waiting;          // Flag to indicate writer is waiting/active

public:
    ReliableSharedMutex() : m_reader_count(0), m_writer_waiting(false) {
        m_write_semaphore = xSemaphoreCreateBinary();
        xSemaphoreGive(m_write_semaphore); // Initially available
        
        m_read_count_mutex = xSemaphoreCreateMutex();
        
        configASSERT(m_write_semaphore != nullptr);
        configASSERT(m_read_count_mutex != nullptr);
    }

    ~ReliableSharedMutex() {
        if (m_write_semaphore) vSemaphoreDelete(m_write_semaphore);
        if (m_read_count_mutex) vSemaphoreDelete(m_read_count_mutex);
    }

    // Non-copyable
    ReliableSharedMutex(const ReliableSharedMutex&) = delete;
    ReliableSharedMutex& operator=(const ReliableSharedMutex&) = delete;

    void lock() {
        // Set writer waiting flag to prevent new readers
        xSemaphoreTake(m_read_count_mutex, portMAX_DELAY);
        m_writer_waiting = true;
        
        // Wait for all existing readers to finish
        while (m_reader_count > 0) {
            xSemaphoreGive(m_read_count_mutex);
            vTaskDelay(1); // Brief yield
            xSemaphoreTake(m_read_count_mutex, portMAX_DELAY);
        }
        
        // Take write semaphore for exclusive access
        xSemaphoreGive(m_read_count_mutex);
        xSemaphoreTake(m_write_semaphore, portMAX_DELAY);
    }

    bool try_lock() {
        // Try to set writer waiting flag
        if (xSemaphoreTake(m_read_count_mutex, 0) != pdTRUE) {
            return false;
        }
        
        // Check if there are active readers
        if (m_reader_count > 0) {
            xSemaphoreGive(m_read_count_mutex);
            return false;
        }
        
        // Try to get write semaphore
        if (xSemaphoreTake(m_write_semaphore, 0) != pdTRUE) {
            xSemaphoreGive(m_read_count_mutex);
            return false;
        }
        
        m_writer_waiting = true;
        xSemaphoreGive(m_read_count_mutex);
        return true;
    }

    void unlock() {
        // Reset writer waiting flag and release write semaphore
        xSemaphoreTake(m_read_count_mutex, portMAX_DELAY);
        m_writer_waiting = false;
        xSemaphoreGive(m_read_count_mutex);
        
        xSemaphoreGive(m_write_semaphore);
    }

    void lock_shared() {
        xSemaphoreTake(m_read_count_mutex, portMAX_DELAY);
        
        // Wait if a writer is waiting or active
        while (m_writer_waiting) {
            xSemaphoreGive(m_read_count_mutex);
            vTaskDelay(1); // Brief yield
            xSemaphoreTake(m_read_count_mutex, portMAX_DELAY);
        }
        
        // Increment reader count
        m_reader_count++;
        xSemaphoreGive(m_read_count_mutex);
    }

    bool try_lock_shared() {
        // For read locks, we should be more persistent about getting the count mutex
        // since reader-reader contention shouldn't cause failures when no writer is active
        const TickType_t max_wait = pdMS_TO_TICKS(50); // Up to 50ms for reader contention
        TickType_t start_time = xTaskGetTickCount();
        
        while ((xTaskGetTickCount() - start_time) < max_wait) {
            if (xSemaphoreTake(m_read_count_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                // Check if writer is waiting or active
                if (m_writer_waiting) {
                    xSemaphoreGive(m_read_count_mutex);
                    return false; // Correctly fail when writer is active
                }
                
                // Increment reader count and succeed
                m_reader_count++;
                xSemaphoreGive(m_read_count_mutex);
                return true;
            }
            // Brief yield if we couldn't get the mutex, then retry
            vTaskDelay(1);
        }
        
        // If we still can't get it after max_wait, something is wrong
        return false;
    }

    void unlock_shared() {
        xSemaphoreTake(m_read_count_mutex, portMAX_DELAY);
        m_reader_count--;
        xSemaphoreGive(m_read_count_mutex);
    }
};

// Lock classes compatible with std library
template<typename Mutex>
class shared_lock {
private:
    Mutex* m_mutex;
    bool m_owns;

public:
    // Default constructor
    shared_lock() : m_mutex(nullptr), m_owns(false) {}

    explicit shared_lock(Mutex& m) : m_mutex(&m), m_owns(false) {
        m_mutex->lock_shared();
        m_owns = true;
    }

    shared_lock(Mutex& m, std::try_to_lock_t) : m_mutex(&m), m_owns(false) {
        m_owns = m_mutex->try_lock_shared();
    }

    ~shared_lock() {
        if (m_owns && m_mutex) {
            m_mutex->unlock_shared();
        }
    }

    // Non-copyable
    shared_lock(const shared_lock&) = delete;
    shared_lock& operator=(const shared_lock&) = delete;

    // Movable
    shared_lock(shared_lock&& other) noexcept
        : m_mutex(other.m_mutex), m_owns(other.m_owns) {
        other.m_mutex = nullptr;
        other.m_owns = false;
    }

    bool owns_lock() const noexcept {
        return m_owns;
    }

    explicit operator bool() const noexcept {
        return owns_lock();
    }
};

template<typename Mutex>
class unique_lock {
private:
    Mutex* m_mutex;
    bool m_owns;

public:
    // Default constructor
    unique_lock() : m_mutex(nullptr), m_owns(false) {}

    explicit unique_lock(Mutex& m) : m_mutex(&m), m_owns(false) {
        m_mutex->lock();
        m_owns = true;
    }

    unique_lock(Mutex& m, std::try_to_lock_t) : m_mutex(&m), m_owns(false) {
        m_owns = m_mutex->try_lock();
    }

    ~unique_lock() {
        if (m_owns && m_mutex) {
            m_mutex->unlock();
        }
    }

    // Non-copyable
    unique_lock(const unique_lock&) = delete;
    unique_lock& operator=(const unique_lock&) = delete;

    // Movable
    unique_lock(unique_lock&& other) noexcept
        : m_mutex(other.m_mutex), m_owns(other.m_owns) {
        other.m_mutex = nullptr;
        other.m_owns = false;
    }

    bool owns_lock() const noexcept {
        return m_owns;
    }

    explicit operator bool() const noexcept {
        return owns_lock();
    }
};
