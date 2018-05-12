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

/** command
  g++ -std=c++11 main.cpp -lnonblockpp
**/

int main() {

    std::thread::id mainThreadId = std::this_thread::get_id();
    bool done = false;;

    NonBlk::run([](std::thread::id && id, bool &done) {
        NonBlk::runOnMainThread([](std::thread::id && id, bool &done) {
            std::cout << "Success call with same thread id " << id << " ==  " << std::this_thread::get_id() << "\n";
            assert(id == std::this_thread::get_id() && " Thread Not the same??");
            //  Doing another heavy staff  thread without blocking
            NonBlk::run([](std::thread::id && id, bool &done) {
                NonBlk::EventId taskEv = NonBlk::pushTask([](std::thread::id && id, bool &done) {
                    std::cout << " Do background task " << "\n";
                    NonBlk::EventId dispatchMainThreadEv = NonBlk::pushEventToMainThread([](std::thread::id && id, bool &done) {
                        assert(id == std::this_thread::get_id() && " Thread Not the same??");
                        done = true;
                    }, std::move(id), std::ref(done));
                    NonBlk::runEventOnMainThread(dispatchMainThreadEv);
                }, std::move(id), std::ref(done));
                std::cout << " Triggering background task  " << "\n";
                NonBlk::runTask(taskEv);
            }, std::move(id), std::ref(done));

        }, std::move(id), std::ref(done));
    }, std::move(mainThreadId), std::ref(done));


    while (!done) {
        NonBlk::pollEvent();
        std::this_thread::sleep_for (std::chrono::milliseconds(16));
    }

    std::cout << " End Test.\n";


    return 0;
}
