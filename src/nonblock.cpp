//
//  Created by Taymindis Woon on 6/5/18.
//  Copyright © 2018 Taymindis Woon. All rights reserved.
//
#include "nonblock.h"
#include <mutex>
#include <cassert>
#include <csignal>
#include <unistd.h>
#include <atomic>

#ifndef __APPLE__
#include <time.h>
#endif

namespace __NonBlk {

static std::mutex _savedProcessMutex;
static std::mutex _savedEventMutex;
static std::mutex _event_mutex;
static std::deque<UniqEvent> eventQueue;
static std::deque<UniqEvent> savedProcessQueue;
static std::deque<UniqEvent> savedEventQueue;

#ifdef __APPLE__
static std::atomic_flag nextTimerReady = ATOMIC_FLAG_INIT;
#define __NONBLK_EVENT_NOTIFY__() ({\
while(nextTimerReady.test_and_set(std::memory_order_acquire));\
ualarm(1 /*u seconds*/, 0/*interval*/);\
})
#else

typedef void (*extra_handler)(int);
typedef void (*extra_sigaction)(int, siginfo_t*, void*);
static extra_handler h = 0;
static extra_sigaction esa = 0;
static struct sigaction nonblkAct;

#ifdef __ANDROID__
static std::atomic_flag nextTimerReady = ATOMIC_FLAG_INIT;
struct sigevent sev;
static timer_t timerid;
static struct itimerspec its;
#define __NONBLK_EVENT_NOTIFY__() ({\
while(nextTimerReady.test_and_set(std::memory_order_acquire));\
if(timer_settime(timerid, 0, &its, NULL)!=0)nextTimerReady.clear(std::memory_order_release);\
})
#define __NONBLK_EVENT_SIGNAL__ -8989
#else
static union sigval val = {0};
static pid_t main_pid = 0;
#define __NONBLK_EVENT_NOTIFY__() sigqueue(main_pid, SIGALRM, val)
#define __NONBLK_EVENT_SIGNAL__ -8989
#endif

#endif


/** Only strictly one at a time due to main thread is only 1 **/
void mainThreadEventTrigger(int unused) {
    std::lock_guard<std::mutex> lock(_event_mutex);
    while (!eventQueue.empty()) {
        UniqEvent event = std::move(eventQueue.front());
        eventQueue.pop_front();
        event->_dispatch();
    }
    nextTimerReady.clear(std::memory_order_release);
}

void dispatchMainThreadEvents(UniqEvent &&ev) {
    {
        std::lock_guard<std::mutex> lock(_event_mutex);
        eventQueue.push_back(std::move(ev));
    }
    __NONBLK_EVENT_NOTIFY__();
}

void swapEventToMainThread() {
    std::lock_guard<std::mutex> lock(_savedEventMutex);
    while (!savedEventQueue.empty()) {
        std::lock_guard<std::mutex> lock(_event_mutex);
        eventQueue.push_back(std::move(savedEventQueue.front()));
        savedEventQueue.pop_front();
    }
}

void triggerMainThreadEvents() {
    __NONBLK_EVENT_NOTIFY__();
}

NonBlk::EventId pushTask(UniqEvent &&ev) {
    std::lock_guard<std::mutex> lock(_savedProcessMutex);
    savedProcessQueue.push_back(std::move(ev));
    return (NonBlk::EventId)&savedProcessQueue.back();
}

NonBlk::EventId pushEventToMainThread(UniqEvent &&ev) {
    std::lock_guard<std::mutex> lock(_savedEventMutex);
    savedEventQueue.push_back(std::move(ev));
    return (NonBlk::EventId)&savedEventQueue.back();
}

#ifndef __APPLE__
void eventHandler2(int sig, siginfo_t *info, void *ctx) {
    if (info->si_int == __NONBLK_EVENT_SIGNAL__) {
        mainThreadEventTrigger(sig);
    } else {
        h(sig);
    }
}

void eventHandler3(int sig, siginfo_t *info, void *ctx) {
    // printf("recv a sig=%d data=%d data=%d\n",
    //        sig, info->si_value.sival_int, info->si_int);
    if (info->si_int == __NONBLK_EVENT_SIGNAL__) {
        mainThreadEventTrigger(sig);
    } else {
        esa(sig, info, ctx);
    }
}


bool chainEventSignals(extra_handler x) {
    if (x) { // Means not first registered
        h = x;
    }
    nonblkAct.sa_sigaction = eventHandler2;
    sigemptyset(&nonblkAct.sa_mask);
    nonblkAct.sa_flags = SA_SIGINFO;
    if (sigaction(SIGALRM, &nonblkAct, NULL) < 0)
        return false;
    return true;
}

bool chainEventSignals(extra_sigaction x) {
    if (x) { // Means not first registered
        esa = x;
    }
    nonblkAct.sa_sigaction = eventHandler3;
    sigemptyset(&nonblkAct.sa_mask);
    nonblkAct.sa_flags = SA_SIGINFO;
    if (sigaction(SIGALRM, &nonblkAct, NULL) < 0)
        return false;
    return true;
}

#endif
}

namespace NonBlk {
void enableMainThreadEvent() {
#ifdef __APPLE__
    assert( (!signal(SIGALRM, __NonBlk::mainThreadEventTrigger)) && " NonBlock:: Sigalarm event had been registered by");
#else

#ifdef __ANDROID__
    __NonBlk::sev.sigev_notify = SIGEV_SIGNAL;
    __NonBlk::sev.sigev_signo = SIGALRM;
    __NonBlk::sev.sigev_value.sival_int = __NONBLK_EVENT_SIGNAL__;

    // Create the timer
    timer_create(CLOCK_REALTIME, &__NonBlk::sev, &__NonBlk::timerid);
    __NonBlk::its.it_value.tv_sec = 0;
    __NonBlk::its.it_value.tv_nsec = 1;
    // __NonBlk::its.it_interval.tv_sec = __NonBlk::its.it_value.tv_sec;
    // __NonBlk::its.it_interval.tv_nsec = __NonBlk::its.it_value.tv_nsec;
#else
    __NonBlk::main_pid = getpid();
    __NonBlk::val.sival_int = __NONBLK_EVENT_SIGNAL__;
#endif
    assert( __NonBlk::chainEventSignals(signal(SIGALRM, __NonBlk::mainThreadEventTrigger)) &&
            " Unable to chain the events, please contact support for more information");
#endif
}

void runAllTask() {
    std::unique_lock<std::mutex> lock(__NonBlk::_savedProcessMutex);
    while (!__NonBlk::savedProcessQueue.empty()) {
        __NonBlk::UniqEvent event = std::move(__NonBlk::savedProcessQueue.front());
        __NonBlk::savedProcessQueue.pop_front();

        lock.unlock();
        std::thread([](__NonBlk::UniqEvent && ev) {
            ev->_dispatch();
        }, std::move(event)).detach();
        lock.lock();
    }
}

void runTask(EventId evId) {
    std::lock_guard<std::mutex> lock(__NonBlk::_savedProcessMutex);
    size_t qSize = __NonBlk::savedProcessQueue.size();
    for (uint i = 0; i < qSize; i++) {
        if (evId == (EventId) & (__NonBlk::savedProcessQueue[i]) ) {
            std::thread([i](__NonBlk::UniqEvent && ev) {
                ev->_dispatch();
            }, std::move(__NonBlk::savedProcessQueue[i])).detach();
            __NonBlk::savedProcessQueue.erase (__NonBlk::savedProcessQueue.begin() + i);
            break;
        }
    }
}

void removeAllTask() {
    std::lock_guard<std::mutex> lock(__NonBlk::_savedProcessMutex);
    if (!__NonBlk::savedProcessQueue.empty()) {
        __NonBlk::savedProcessQueue.clear();
    }
}

void runAllEventOnMainThread() {
    __NonBlk::swapEventToMainThread();
    __NonBlk::triggerMainThreadEvents();
}

void runEventOnMainThread(EventId evId) {
    std::lock_guard<std::mutex> lock(__NonBlk::_savedEventMutex);
    size_t qSize = __NonBlk::savedEventQueue.size();
    for (uint i = 0; i < qSize; i++) {
        if (evId == (EventId) & (__NonBlk::savedEventQueue[i]) ) {
            do {
                std::lock_guard<std::mutex> lock(__NonBlk::_event_mutex);
                __NonBlk::eventQueue.push_back(std::move(__NonBlk::savedEventQueue[i])); // Push to main thread
                __NonBlk::savedEventQueue.erase (__NonBlk::savedEventQueue.begin() + i); // erase the current
            } while (0);
            __NonBlk::triggerMainThreadEvents(); // trigger main thread
            break;
        }
    }
}

void removeAllEvent() {
    std::lock_guard<std::mutex> lock(__NonBlk::_savedEventMutex);
    if (!__NonBlk::savedEventQueue.empty()) {
        __NonBlk::savedEventQueue.clear();
    }
}

}