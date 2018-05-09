//
//  Created by Taymindis Woon on 6/5/18.
//  Copyright © 2018 Taymindis Woon. All rights reserved.
//

#include <stdio.h>
#include <iostream>
#include <thread>
#include <deque>
#include <tuple>
#include <cstdlib>
#include <cstring>


/*************** API scope ********************************/

namespace NonBlk {

using EventId = uintptr_t;

void enableMainThreadEvent();

void runAllTask();
void removeAllTask();
void runAllEventOnMainThread();
void removeAllEvent();

void runTask(EventId);
void runEventOnMainThread(EventId);


template <typename Process>
void run(Process process);

template <typename Process, typename T>
void run(Process process, T t);

template <typename Process, typename... Ts>
void run(Process process, Ts... ts);

template <typename Then>
void runOnMainThread(Then then);

template <typename Then, typename T>
void runOnMainThread(Then then, T t);

template <typename Then, typename... Ts>
void runOnMainThread(Then then, Ts... ts);


template <typename Process>
EventId pushTask(Process process);

template <typename Process, typename T>
EventId pushTask(Process process, T t);

template <typename Process, typename... Ts>
EventId pushTask(Process process, Ts... ts);

template <typename Then>
EventId pushEventToMainThread(Then then);

template <typename Then, typename T>
EventId pushEventToMainThread(Then then, T t);

template <typename Then, typename... Ts>
EventId pushEventToMainThread(Then then, Ts... ts);


}
/*************** End API scope ********************************/


/*************** PRIVATE scope ********************************/
namespace __NonBlk {
/*** Ref https://stackoverflow.com/questions/10766112/c11-i-can-go-from-multiple-args-to-tuple-but-can-i-go-from-tuple-to-multiple ***/
template <typename F, typename Tuple, bool Done, int Total, int... N>
struct call_impl
{
    static void callee(F f, Tuple && t)
    {
        call_impl < F, Tuple, Total == 1 + sizeof...(N), Total, N..., sizeof...(N) >::callee(f, std::forward<Tuple>(t));
    }
};

template <typename F, typename Tuple, int Total, int... N>
struct call_impl<F, Tuple, true, Total, N...>
{
    static void callee(F f, Tuple && t)
    {
        f(std::get<N>(std::forward<Tuple>(t))...);
    }
};

template <typename F, typename Tuple>
void apply(F &&f, Tuple && t)
{
    typedef typename std::decay<Tuple>::type ttype;
    call_impl<F, Tuple, 0 == std::tuple_size<ttype>::value, std::tuple_size<ttype>::value>::callee(f, std::forward<Tuple>(t));
}
// -------------------- End ------------------------------


class Event {
public:
    virtual void _dispatch() = 0;
    virtual ~Event() {}
};

template <typename Call>
class Bus: public Event {
public:
    explicit Bus(Call &&call);
    void _dispatch() override;
    ~Bus();
private:
    u_char* _call;
};

template <typename Call, typename T>
class BusVar: public Event {
public:
    explicit BusVar(Call &&call, T &&t);
    void _dispatch() override;
    ~BusVar();
private:
    u_char* _call;
    T _t;
};

template <typename Call, typename... Ts>
class BusVars: public Event {
public:
    explicit BusVars(Call &&call, Ts&&... ts);
    void _dispatch() override;
    ~BusVars();
private:
    u_char* _call;
    std::tuple<Ts...> _ts;
};

template <typename Call>
Bus<Call>::Bus(Call &&call) {
    this->_call = (u_char*) malloc(sizeof(Call));
    std::memcpy(this->_call, (u_char*) &call, sizeof(Call));
}

template <typename Call>
void Bus<Call>::_dispatch() {
    Call *call = (Call*) this->_call;
    (*call)();
}

template <typename Call>
Bus<Call>::~Bus() {
    free(this->_call);
}

template <typename Call, typename T>
BusVar<Call, T>::BusVar(Call &&call, T &&t): _t(t) {
    this->_call = (u_char*) malloc(sizeof(Call));
    std::memcpy(this->_call, (u_char*) &call, sizeof(Call));
}

template <typename Call, typename T>
void BusVar<Call, T>::_dispatch() {
    Call *call = (Call*) this->_call;
    (*call)(_t);
}

template <typename Call, typename T>
BusVar<Call, T>::~BusVar() {
    free(this->_call);
}

template <typename Call, typename... Ts>
BusVars<Call, Ts...>::BusVars(Call &&call, Ts&&... ts) : _ts(std::make_tuple(ts...)) {
    this->_call = (u_char*) malloc(sizeof(Call));
    std::memcpy(this->_call, (u_char*) &call, sizeof(Call));
}

template <typename Call, typename... Ts>
void BusVars<Call, Ts...>::_dispatch() {
    Call *call = (Call*) this->_call;
    apply(std::move(*call), std::move(this->_ts));

}

template <typename Call, typename... Ts>
BusVars<Call, Ts...>::~BusVars() {
    free(this->_call);
}

using UniqEvent = std::unique_ptr<Event>;
void registerMainThreadEvents();
void dispatchMainThreadEvents(UniqEvent &&ev);
NonBlk::EventId pushTask(UniqEvent &&ev);
NonBlk::EventId pushEventToMainThread(UniqEvent &&ev);
}

namespace NonBlk {

template <typename Process>
void run(Process process) {
    std::thread([](Process && process) {
        process();
    }, std::move(process)).detach();
}

template <typename Process, typename T>
void run(Process process, T t) {
    std::thread([](Process && process, T && t) {
        process(t);
    }, std::move(process), std::move(t)).detach();
}

template <typename Process, typename... Ts>
void run(Process process, Ts... ts) {
    std::thread([](Process && process, Ts && ...ts) {
        process(ts...);
    }, std::move(process), std::move(ts)...).detach();
}

template <typename Then>
void runOnMainThread(Then then) {
    __NonBlk::UniqEvent event(new __NonBlk::Bus<Then>(std::move(then)));
    __NonBlk::dispatchMainThreadEvents(std::move(event));
}

template <typename Then, typename T>
void runOnMainThread(Then then, T t) {
    __NonBlk::UniqEvent event(new __NonBlk::BusVar<Then, T>(std::move(then), std::move(t)));
    __NonBlk::dispatchMainThreadEvents(std::move(event));
}

template <typename Then, typename... Ts>
void runOnMainThread(Then then, Ts... ts) {
    __NonBlk::UniqEvent event(new __NonBlk::BusVars<Then, Ts...>(std::move(then), std::move(ts)...));
    __NonBlk::dispatchMainThreadEvents(std::move(event));
}

template <typename Process>
EventId pushTask(Process process) {
    __NonBlk::UniqEvent event(new __NonBlk::Bus<Process>(std::move(process)));
    return __NonBlk::pushTask(std::move(event));
}

template <typename Process, typename T>
EventId pushTask(Process process, T t) {
    __NonBlk::UniqEvent event(new __NonBlk::BusVar<Process, T>(std::move(process), std::move(t)));
    return __NonBlk::pushTask(std::move(event));
}

template <typename Process, typename... Ts>
EventId pushTask(Process process, Ts... ts) {
    __NonBlk::UniqEvent event(new __NonBlk::BusVars<Process, Ts...>(std::move(process), std::move(ts)...));
    return __NonBlk::pushTask(std::move(event));
}

template <typename Then>
EventId pushEventToMainThread(Then then) {
    __NonBlk::UniqEvent event(new __NonBlk::Bus<Then>(std::move(then)));
    return __NonBlk::pushEventToMainThread(std::move(event));
}

template <typename Then, typename T>
EventId pushEventToMainThread(Then then, T t) {
    __NonBlk::UniqEvent event(new __NonBlk::BusVar<Then, T>(std::move(then), std::move(t)));
    return __NonBlk::pushEventToMainThread(std::move(event));
}

template <typename Then, typename... Ts>
EventId pushEventToMainThread(Then then, Ts... ts) {
    __NonBlk::UniqEvent event(new __NonBlk::BusVars<Then, Ts...>(std::move(then), std::move(ts)...));
    return __NonBlk::pushEventToMainThread(std::move(event));
}
/*************** End PRIVATE scope ********************************/

}
