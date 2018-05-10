#include <algorithm>
#include <string>
#include <gtest/gtest.h>
#include <fstream>
#include <chrono>
#include <vector>
#include <atomic>
#include "nonblock.h"


TEST(unblockppTest, TEST_RUN_ON_MAIN_THREAD_FROM_REF_OBJ)
{
    std::thread::id threadId =  std::this_thread::get_id();
    std::atomic<int> done = {0};
    NonBlk::run([&](std::thread::id & id) {
        NonBlk::runOnMainThread([&](std::thread::id & id) {
            EXPECT_TRUE( std::this_thread::get_id() == id ) << " It is not main thread!! " << id << " == " << std::this_thread::get_id() << "\n";
            done++;
        }, std::ref(id));
    }, std::ref(threadId));

    while (!done) {
        NonBlk::pollEvent();
    }
}

TEST(unblockppTest, TEST_RUN_ON_MAIN_THREAD)
{
    std::thread::id threadId =  std::this_thread::get_id();
    std::atomic<int> done = {0};
    NonBlk::run([&]() {
        NonBlk::runOnMainThread([&]() {
            EXPECT_TRUE( std::this_thread::get_id() == threadId ) << " It is not main thread!! " << threadId << " == " << std::this_thread::get_id() << "\n";
            done++;
        });
    });
    
    while (!done) {
        NonBlk::pollEvent();
    }
}

TEST(unblockppTest, CROSS_THREAD_MAIN_CALL)
{
    std::thread::id threadId =  std::this_thread::get_id();
    std::atomic<int> done = {0};
    NonBlk::EventId ev = 0;
    std::atomic<bool> hasEvent = {false};
    NonBlk::run([&]() {
        ev = NonBlk::pushEventToMainThread([&]() {
            EXPECT_TRUE( std::this_thread::get_id() == threadId ) << " It is not main thread!! " << threadId << " == " << std::this_thread::get_id() << "\n";
            done = 1;
        });

        hasEvent = true;
    });

    std::thread([&]() {
        while (!hasEvent);
        NonBlk::runEventOnMainThread(ev);
    }).detach();
    while (!done) {
        NonBlk::pollEvent();
    }
}

int main(int argc, char **argv) {

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
