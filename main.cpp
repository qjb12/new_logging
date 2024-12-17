#include <string>
#include <sstream>
#include <mutex>
#include <iostream>
#include <pthread.h>
#include "log.h"

void* thread_func(void* arg) {
    ThreadSafeLogger::staticLog("Thread %d is running", int(pthread_self()));
    return nullptr;
}

int main(int argc, char* argv[]) {
    ThreadSafeLogger::language = argv[1];
    ThreadSafeLogger::staticLog("Main thread is running");
    pthread_t threads[20];
    for (int i = 0; i < 20; ++i) {
        pthread_create(&threads[i], nullptr, thread_func, nullptr);
    }

    for (int i = 0; i < 20; ++i) {
        pthread_join(threads[i], nullptr);
    }
    return 0;
}
