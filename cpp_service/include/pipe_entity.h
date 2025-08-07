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
    bool start();
    bool stop();
    bool destroy();
    DataChanelStatus status();

    // TODO 之后可以考虑把这个单独提一个类型出去
    // 设置gpu 获取gpu列表。考虑是放在控制还是数据 之后再说
    // 先发送长度帧首 再发送湖北经
    bool sendData();
private:
    HANDLE pipe_handle;
    std::string pipe_name;
    // 延迟时间 单位ms
    unsigned long delay;
    bool _is_running;
    // gpu的索引 默认是-1
    int gpuIndex;
};
