#pragma once

#include <string>
#include <thread>
#include <vector>
#include <functional>
#include <atomic>
#include <windows.h>
#include "json_util.h"

/**
 * 管理 Pipe 服务端创建、监听、接入连接(要可以维护多个连接) + 观察者
 */
class WinPipeServer {
public:
    // 客户端句柄访问保护
    static CRITICAL_SECTION clients_mutex_;
    using MessageCallback = std::function<void(const std::string& request, std::string& response)>;

    explicit WinPipeServer(const std::string& pipe_name);
    ~WinPipeServer();

    /**
     * 服务启动
     * @param callback 设置消息回调
     */
    void start(const MessageCallback &callback);

    /**
     * 服务暂停
     */
    void stop();

    /**
     * 广播发送数据
     * @param message 进入message回调的消息
     * @return 是否发送成功
     */
    bool send_to_all_clients(const std::string& message) const;
private:
    void accept_loop();

    /**
     * 使用客户端线程异步监听客户端消息
     * @param pipe 需要监听的客户端
     */
    void client_handler(HANDLE pipe);

    /**
     * 连接名称
     */
    std::string pipe_name_;
    /**
     * 是否开启的原子特征值
     */
    std::atomic<bool> running_;
    /**
     * 接受连接请求线程
     */
    std::thread accept_thread_;
    /**
     * 处理客户端连接的req请求的线程
     */
    std::vector<std::thread> client_threads_;
    /**
     * 客户端句柄数组
     */
    std::vector<HANDLE> clients_;

    /**
     * 广播的同一回调方法
     */
    MessageCallback on_message_;
};
