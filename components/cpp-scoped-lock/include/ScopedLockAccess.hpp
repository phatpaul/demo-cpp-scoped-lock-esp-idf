/*
 * ScopedLockAccess.hpp
 *  C++ class for a scoped lock access to a Db instance.
 *
 *  Created on: May 15, 2024
 *      Author: Paul Abbott
 */
#pragma once

#include <shared_mutex>

template <typename dbType>
class ScopedLockAccess
{
public:
    using mutex_type = std::shared_mutex;
    using read_lock = std::shared_lock<mutex_type>;
    using write_lock = std::unique_lock<mutex_type>;

private:
    mutable mutex_type m_mutex{};
    dbType m_db{};
    static inline ScopedLockAccess *sm_instance{}; // Optional instance pointer

public:
    // Set Static Dependencies
    static void setStaticInstance(ScopedLockAccess *ptr)
    {
        sm_instance = ptr;
    };
    static ScopedLockAccess &getManager()
    {
        assert(sm_instance); // Dependency must be provided before use!
        return *sm_instance;
    };

    // returns a scoped lock that allows multiple readers but excludes writers
    // you should check that the lock was actually aquired with: if(lock.owns_lock())
    //read_lock lock_for_reading() const { return read_lock(m_mutex, std::try_to_lock); }

    // returns a scoped lock that allows only one writer and no one else
    write_lock lock_for_writing() { return write_lock(m_mutex); }

    template <class T, class lockType>
    class Access
    {
    public:
        // Constructor
        Access(mutex_type &m, T &obj, bool blocking = true)
            : m_db{obj}
        {
            if (blocking)
            {
                m_lock = lockType(m);
            }
            else // non-blocking
            {
                m_lock = lockType(m, std::try_to_lock);
            }
        }

        // only allow access to the pointer with -> operator to prevent copying the protected object
        dbType *operator->() { return &m_db; }

        // operator bool
        //  - returns whether the exclusive lock is still active
        //  - enables use in 'if' expression to introduce local scope
        //
        explicit operator bool() const &
        {
            return m_lock.owns_lock();
        }

    private:
        // a scoped lock that exists as long as this instance of Access class lives
        lockType m_lock;
        // Reference to the protected resource
        T &m_db;
    };

    using ReadAccess = Access<dbType, read_lock>;
    using WriteAccess = Access<dbType, write_lock>;

    ReadAccess getReadAccess()
    {
        return ReadAccess(m_mutex, m_db, false); // don't block for read
    }
    WriteAccess getWriteAccess()
    {
        return WriteAccess(m_mutex, m_db, true); // block for write
    }
    void resetDb(void)
    {
        auto lock = lock_for_writing();
        m_db = {}; // I hope this properly deconstructs the old stuff...
    }
};
