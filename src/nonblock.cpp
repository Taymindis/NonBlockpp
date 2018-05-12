/**
* BSD 3-Clause License
*
* Copyright (c) 2018, Taymindis Woon
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice, this
*   list of conditions and the following disclaimer.
*
* * Redistributions in binary form must reproduce the above copyright notice,
*   this list of conditions and the following disclaimer in the documentation
*   and/or other materials provided with the distribution.
*
* * Neither the name of the copyright holder nor the names of its
*   contributors may be used to endorse or promote products derived from
*   this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/
#include "nonblock.h"
#include <mutex>
#include <cassert>
#include <unistd.h>
#include <atomic>

namespace __NonBlk {

static std::mutex _savedProcessMutex;
static std::mutex _savedEventMutex;
static std::mutex _event_mutex;
static std::deque<UniqEvent> eventQueue;
static std::deque<UniqEvent> savedProcessQueue;
static std::deque<UniqEvent> savedEventQueue;

/** Only strictly one at a time due to main thread is only 1 **/
void pollForDispatch() {
    std::unique_lock<std::mutex> lock(_event_mutex);
    while (!eventQueue.empty()) {
        UniqEvent event = std::move(eventQueue.front());
        eventQueue.pop_front();
        // unlock during the individual task
        lock.unlock();
        event->_dispatch();
        lock.lock();
    }
}

void dispatchMainThreadEvents(UniqEvent &&ev) {
    std::lock_guard<std::mutex> lock(_event_mutex);
    eventQueue.push_back(std::move(ev));
}

void swapEventToMainThread() {
    std::lock_guard<std::mutex> lock(_savedEventMutex);
    while (!savedEventQueue.empty()) {
        std::lock_guard<std::mutex> lock(_event_mutex);
        eventQueue.push_back(std::move(savedEventQueue.front()));
        savedEventQueue.pop_front();
    }
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

}

namespace NonBlk {
void pollEvent() {
    __NonBlk::pollForDispatch();
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
            std::thread([](__NonBlk::UniqEvent && ev) {
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
}

void runEventOnMainThread(EventId evId) {
    std::lock_guard<std::mutex> lock(__NonBlk::_savedEventMutex);
    size_t qSize = __NonBlk::savedEventQueue.size();
    for (uint i = 0; i < qSize; i++) {
        if (evId == (EventId) & (__NonBlk::savedEventQueue[i]) ) {
            std::lock_guard<std::mutex> lock(__NonBlk::_event_mutex);
            __NonBlk::eventQueue.push_back(std::move(__NonBlk::savedEventQueue[i])); // Push to main thread
            __NonBlk::savedEventQueue.erase (__NonBlk::savedEventQueue.begin() + i); // erase the current
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