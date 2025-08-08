#pragma once
#include <string>
#include <windows.h>

#include "json.hpp"

enum DataChanelStatus {
    LISTENING = 0,
    RUNNING = 1,
    STOPPED = 2,
    DESTROYED = 3
};

class ClientDataChanelInfo{
public:
    explicit ClientDataChanelInfo(std::string pipe_name, HANDLE pipe_handle, int delay);
    HANDLE getHandle() const;
private:
    HANDLE pipe_handle;
    std::string pipe_name;
    // 延迟时间 单位ms
    unsigned long delay;
    bool _is_running;
    // gpu的索引 默认是-1
    int gpuIndex;
};

class SeqGenerator {
public:
    SeqGenerator(uint32_t start = 0) : seq_(start) {}

    // 前置 ++obj
    uint32_t operator++() {
        uint32_t oldSeq, newSeq;
        do {
            oldSeq = seq_.load(std::memory_order_relaxed);
            newSeq = (oldSeq == UINT32_MAX) ? 0 : oldSeq + 1;
        } while (!seq_.compare_exchange_weak(
            oldSeq, newSeq,
            std::memory_order_release,
            std::memory_order_relaxed
        ));
        return newSeq; // 返回递增后的值
    }

    // 后置 obj++
    uint32_t operator++(int) {
        uint32_t oldSeq, newSeq;
        do {
            oldSeq = seq_.load(std::memory_order_relaxed);
            newSeq = (oldSeq == UINT32_MAX) ? 0 : oldSeq + 1;
        } while (!seq_.compare_exchange_weak(
            oldSeq, newSeq,
            std::memory_order_release,
            std::memory_order_relaxed
        ));
        return oldSeq; // 返回递增前的值
    }

private:
    std::atomic<uint32_t> seq_;
};