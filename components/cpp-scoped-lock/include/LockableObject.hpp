/*
 * LockableObject.hpp
 *  C++ class for a scoped lock access to an object.
 *
 *  Created on: May 15, 2024
 *      Author: Paul Abbott, Lumitec LLC
 */
#pragma once

#include <mutex>
#include <shared_mutex>

template <typename protectedType>
class LockableObject
{
public:
    using mutex_type = std::shared_mutex;
    using read_lock = std::shared_lock<mutex_type>;
    using write_lock = std::unique_lock<mutex_type>;

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
            : m_protectedRef{obj}
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
        // a scoped lock that exists as long as this instance of ScopedAccess class lives
        lockType m_lock;
        // Reference to the protected resource
        objType &m_protectedRef;
    };

    using ReadAccess = ScopedAccess<protectedType, read_lock>;
    using WriteAccess = ScopedAccess<protectedType, write_lock>;

    ReadAccess getReadAccess()
    {
        return ReadAccess(m_mutex, m_protected, false); // don't block for read
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
