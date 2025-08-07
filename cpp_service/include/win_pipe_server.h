#pragma once

#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <functional>
#include <map>
#include <atomic>
#include <windows.h>
#include "json_util.h"
#include "pipe_entity.h"

/**
 * 管理 Pipe 服务端创建、监听控制链接。创建数据连接，(要可以维护多个独立的数据连接) + 观察者
 */
class WinPipeServer {
public:
    // 客户端句柄访问保护
    static CRITICAL_SECTION clients_mutex_;
    // 临时的观察者 之后改一种写法
    using MessageCallback = std::function<void(const std::string& request, std::string& response)>;

    explicit WinPipeServer(const std::string& pipe_name);
    ~WinPipeServer();

    /**
     * 服务启动 创建管理通道
     * @param callback 设置消息回调
     */
    void start(const MessageCallback &callback);

    /**
     * 服务暂停
     */
    void stop();
private:

    /**
     * 保存所有的数据通道的信息指针
     * key是source,value是信息实体
     */
    std::map<std::string,ClientDataChanelInfo> _client_map;

    // 控制管道的链接监听
    void accept_loop();

    /**
     * 管理通道 处理多个客户端连接的方法
     * @param pipe 需要监听的客户端
     */
    void client_handler(HANDLE pipe);

    /**
     * 管理通道连接名称
     */
    std::string pipe_name_;
    /**
     * 管理通道是否开启的原子特征值
     */
    std::atomic<bool> running_;
    /**
     * 接受连接请求线程
     */
    std::thread accept_thread_;
    /**
     * 管理通道 处理req请求的线程
     */
    std::vector<std::thread> client_threads_;
    /**
     * 管理通道 句柄数组
     */
    std::vector<HANDLE> clients_;

    /**
     * 广播的同一回调方法
     */
    MessageCallback on_message_;
};
