#pragma once
#include <string>
#include <windows.h>
class ClientDataChanelInfo{
public:
    explicit ClientDataChanelInfo(std::string pipe_name, HANDLE pipe_handle, int delay);
    HANDLE pipe_handle;
private:
    std::string pipe_name;
    // 延迟时间 单位ms
    unsigned long delay;
    // gpu的索引 默认是-1
    int gpuIndex;
    bool _is_running;
};
