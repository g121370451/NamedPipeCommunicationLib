#pragma once

#include <string>
#include <thread>
#include <functional>
#include <windows.h>

class WinPipeConnector {
public:
    using MessageCallback = std::function<void(const std::string&)>;

    explicit WinPipeConnector(const std::string& pipe_name);
    ~WinPipeConnector();

    /**
     * 创建连接 创建连接句柄
     * @return 是否连接成功
     */
    bool connect();

    /**
     * 断开连接 断开连接句柄
     */
    void disconnect();

    /**
     *
     * @param callback
     */
    void start_receiving(const MessageCallback& callback);
    void stop_receiving();

    /**
     * 同步发送数据
     * @param message 数据
     * @return 发送是否成功
     */
    bool send(const std::string& message) const;

    /**
     * 判断当前链接是否初始化
     * @return 是否连接
     */
    bool is_connected() const { return connected_; }

private:
    std::string pipe_name_;
    HANDLE pipe_handle_ = INVALID_HANDLE_VALUE;
    std::thread recv_thread_;
    bool receiving_ = false;
    bool connected_ = false;

    /**
     * 等待server的消息处理线程
     * @param callback 事件处理回调
     */
    void receive_loop(const MessageCallback& callback);
};
