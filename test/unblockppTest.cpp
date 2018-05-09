#include <algorithm>
#include <string>
#include <gtest/gtest.h>
#include <fstream>
#include <chrono>
#include <vector>
#include "nonblock.h"


TEST(unblockppTest, TEST_RUN_ON_MAIN_THREAD_FROM_REF_OBJ)
{
    std::thread::id threadId;
    NonBlk::run([](std::thread::id & id) {
        NonBlk::runOnMainThread([](std::thread::id & id) {
            id = std::this_thread::get_id();
        }, std::ref(id));
    }, std::ref(threadId));
    std::this_thread::sleep_for (std::chrono::milliseconds(1000));
    EXPECT_TRUE( std::this_thread::get_id() == threadId ) << " It is not main thread!! " << threadId << " == " << std::this_thread::get_id() << "\n";
}

TEST(unblockppTest, TEST_RUN_ON_MAIN_THREAD)
{
    std::thread::id threadId =  std::this_thread::get_id();

    NonBlk::run([threadId]() {
        NonBlk::runOnMainThread([threadId]() {
            EXPECT_TRUE( std::this_thread::get_id() == threadId ) << " It is not main thread!! " << threadId << " == " << std::this_thread::get_id() << "\n";
        });
    });
    std::this_thread::sleep_for (std::chrono::milliseconds(1000));
}

TEST(unblockppTest, CROSS_THREAD_MAIN_CALL)
{
    std::thread::id threadId =  std::this_thread::get_id();
    NonBlk::EventId ev;
    NonBlk::run([&]() {
       ev = NonBlk::pushEventToMainThread([&]() {
            EXPECT_TRUE( std::this_thread::get_id() == threadId ) << " It is not main thread!! " << threadId << " == " << std::this_thread::get_id() << "\n";
        });
    });

    std::thread([&](){
        std::chrono::milliseconds(500);
        NonBlk::runEventOnMainThread(ev);
    }).detach();

    std::this_thread::sleep_for (std::chrono::milliseconds(1500));
}

int main(int argc, char **argv) {
    NonBlk::enableMainThreadEvent();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
