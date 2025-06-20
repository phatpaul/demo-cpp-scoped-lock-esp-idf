Demonstration of a bug

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