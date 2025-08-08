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
