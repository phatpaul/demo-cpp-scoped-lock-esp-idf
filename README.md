Demonstration of a bug as described here:
https://esp32.com/viewtopic.php?t=45980

My goal is to be able to write C++ code that uses scoped lock objects like this:

```c

if (auto dbAccess = MyConfigDbManager::getInstance().getReadAccess()) // get scoped read lock
{
    // Got a read lock. Do something with the protected object, i.e.
    dbAccess->doSomething();
    // when this block ends and dbAccess goes out of scope, the lock is released automatically.
}
else
{
     ESP_LOGE(TAG, "failed to get read lock.");
}
```

See my [LockableObject](components/cpp-scoped-lock/include/LockableObject.hpp) class implementation.

In the case of getReadAccess(), I'm using std::shared_lock<std::shared_mutex> to implement the lock because I want multiple threads to be able to get a read lock simultaneously.
But for getWriteAccess(), I'm using std::unique_lock<std::shared_mutex> so that it is exclusive. I.e. only 1 thread can have a write lock, and there can be no read locks active when a write lock is active.

It works >99% of the time, and I have implemented some [unit tests](components/cpp-scoped-lock/test/test_cpp-scoped-lock.cpp).

But occasionally, a GetReadAccess() fails unexpectedly. I know that there are no write locks during this time.
I created this GitHub repo to demonstrate the issue.
I hope someone can help me track down the cause of this bug. I have tested it on both IDF versions 4.4.8 and 5.4.1 with the same behavior.

```sh
# cd to unit test app dir
cd test-idf4
# or cd test-idf5

# Setup environment (only needed once per session)
# test-idf4/sourceme_IDF_ENV.sh assumes ESP-IDF installed in ~/esp/esp-idf-v4.4.8 
# test-idf5/sourceme_IDF_ENV.sh assumes ESP-IDF installed in ~/esp/esp-idf-v5.4.1
source sourceme_IDF_ENV.sh

# Build, flash, and monitor unit test app on target
idf build flash monitor
```

Let the test run and notice an occasional error. 
(If you don't see the error at first, restart the test #7 a few times.)
```
E (16735) [PocoConfigDb]: Thread 3 failed to get read lock.
```

## EDIT: Solved with short but non-zero blocking timeout!
 After further digging, the issue seems to be contention on accessing the mutex object by multiple readers at the same time. It is fixed by adding a very short but non-zero blocking timeout to aquire the ReadAccess lock.  I used `std::shared_timed_mutex` instead of `std::shared_mutex` and set a minimum block time of 10ms.