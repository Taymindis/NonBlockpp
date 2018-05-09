#include <stdio.h>
#include <iostream>
#include <thread>
#include <deque>
#include <tuple>
#include <cstdlib>
#include <cstring>
#include <assert.h>
#include <nonblockpp/nonblock.h>
#include <atomic>

std::thread::id mainThreadId;

#define FIRST_STAGE_UPDATE 200
#define SEC_STAGE_UPDATE 501
#define LAST_STAGE_UPDATE 800


/** command 
  g++ -std=c++11 main.cpp -lnonblockpp 
**/

int main() {
    std::atomic<int> partialUpdate = {0};

    std::cout << "Enabling main thread event on main thread" << "\n";
    NonBlk::enableMainThreadEvent();

    mainThreadId = std::this_thread::get_id();

    NonBlk::run([](std::atomic<int> &updating) {
        while (++updating != FIRST_STAGE_UPDATE) {
            std::this_thread::sleep_for (std::chrono::milliseconds(16));
        }
        NonBlk::runOnMainThread([]() {
            std::cout << "Success call with same thread id " << mainThreadId << " ==  " << std::this_thread::get_id() << "\n";
        });
    }, std::ref(partialUpdate));

    // // Doing heavy staff on main thread without blocking
    while (1) {
        std::cout << "\r" << " Drawing some stuff " << partialUpdate << " frame update" << std::flush;
        std::this_thread::sleep_for (std::chrono::milliseconds(16));
        if (partialUpdate == FIRST_STAGE_UPDATE) {
            break;
        }
    }

    NonBlk::EventId dispatchMainThreadEv = 0;
    NonBlk::EventId taskEv = NonBlk::pushTask([&]() {
        std::cout << " Do background task " << "\n";
        dispatchMainThreadEv = NonBlk::pushEventToMainThread([&]() {
            partialUpdate = LAST_STAGE_UPDATE;
            std::cout << "\nSuccess call with same thread id " << mainThreadId << "==" << std::this_thread::get_id() << "\n";
            assert(mainThreadId == std::this_thread::get_id() && " Thread Not the same??");
        });

        while (++partialUpdate != SEC_STAGE_UPDATE)
            std::this_thread::sleep_for (std::chrono::milliseconds(16));

        NonBlk::runEventOnMainThread(dispatchMainThreadEv);

    });

    std::cout << " Triggering background task  " << "\n";
    NonBlk::runTask(taskEv);

    // Doing heavy staff on main thread without blocking
    while (partialUpdate != LAST_STAGE_UPDATE) {
        std::cout << "\r" << " Drawing some extra stuff again " << partialUpdate << " frame update with timestamp " << std::time(0) << std::flush;
        std::this_thread::sleep_for (std::chrono::milliseconds(16));
    }

    return 0;
}
