/*
 * LockableObject.hpp
 *  C++ class for a scoped lock access to an object.
 *
 *  Created on: May 15, 2024
 *      Author: Paul Abbott, Lumitec LLC
 */
#pragma once

#include <shared_mutex>
#include <chrono>

template <typename protectedType>
class LockableObject
{
public:
    using mutex_type = std::shared_timed_mutex; // shared_timed_mutex to allow timed locks
    using read_lock = std::shared_lock<mutex_type>; // shared_lock for read access (multiple readers)
    using write_lock = std::unique_lock<mutex_type>; // unique_lock for write access (exclusive)

    // Ensure the minimum timeout duration to avoid contention on the mutex
    static constexpr auto minBlockTime = std::chrono::milliseconds(10);
    static constexpr auto maxBlockTime = std::chrono::milliseconds::max();

private:
    mutable mutex_type m_mutex{}; // Mutex for protecting access to the object
    protectedType m_protected{}; // The protected object
    static inline LockableObject *sm_instance{}; // Optional static pointer to a single instance of LockableObject<protectedType>

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
        // Constructor with timeout
        template<class Rep, class Period>
        ScopedAccess(mutex_type &m, objType &obj, const std::chrono::duration<Rep, Period>& timeout_duration)
            : m_protectedRef{obj},
              m_lock{m, timeout_duration}
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



    // Returns a read access object with the default timeout duration (10ms).
    ReadAccess getReadAccess()
    {
        return ReadAccess(m_mutex, m_protected, minBlockTime);
    }

    // Returns a read access object with the specified timeout duration. It will block until the timeout is reached.
    template<class Rep, class Period>
    ReadAccess getReadAccess(const std::chrono::duration<Rep, Period>& timeout_duration)
    {
        // Ensure the minimum timeout duration to avoid contention on the mutex
        auto actual_timeout = timeout_duration < minBlockTime ? minBlockTime : timeout_duration;
        return ReadAccess(m_mutex, m_protected, actual_timeout);
    }

    // Returns a write access object with the default timeout duration (max).
    WriteAccess getWriteAccess()
    {
        return WriteAccess(m_mutex, m_protected, maxBlockTime);
    }

    // Returns a write access object with the specified timeout duration. It will block until the timeout is reached.
    template<class Rep, class Period>
    WriteAccess getWriteAccess(const std::chrono::duration<Rep, Period>& timeout_duration)
    {
        // Ensure the minimum timeout duration to avoid contention on the mutex
        auto actual_timeout = timeout_duration < minBlockTime ? minBlockTime : timeout_duration;
        return WriteAccess(m_mutex, m_protected, actual_timeout);
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
