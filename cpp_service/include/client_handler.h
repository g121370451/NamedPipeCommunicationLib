#pragma once
#include <windows.h>
#include <thread>
#include <atomic>
#include "json.hpp"

/**
 * 管理通道的客户端响应 使用状态机的模式
 */
class ClientHandler {
public:
    explicit ClientHandler(HANDLE hControlPipe);
    ~ClientHandler();

    /**
     * 调用performHandshake()，SERVER_HELLO，如果握手成功，进入主循环controlLoop();
     */
    void run();

private:
    /**
     * 主循环 使用readFile从控制管道一直读取信息
     * 读取的信息使用processControlMessage() 处理结果
     */
    void controlLoop();

    /**
     * 发送结构化的数据 线程怎么分配还没想好
     */
    void periodicDataLoop();

    /**
     * 读取客户端的CLIENT_HELLO, 生成这个客户端连接的uuid构建data_pipe_name
     * 创建并监听这个pipe，依然使用监听线程，等待客户端连接
     * 发送SETUP_DATA_CHANNEL 消息携带这个目标地址
     * 如果收到了客户端的DATA_CHANNEL_READY 表示可以传递数据了
     * 最后监听5000ms 如果没连上 代表连接失败。重新尝试3次。再失败，写日志返回异常。
     * @return 最终的状态
     */
    bool performHandshake();

    /**
    * 一个新线程 使用switch或if-else if根据消息的type字段进行分发。
    * 如果type是START_PERIODIC_DATA，则设置m_isPushingData = true;并启动m_periodicDataThread，该线程执行periodicDataLoop()。
    * 如果type是STOP_PERIODIC_DATA，则设置m_isPushingData = false;，等待m_periodicDataThread退出。
    * 如果type是GOODBYE，则关闭会话。
     * @param msg 同步返回的确认信息
     */
    void processControlMessage(const nlohmann::json& msg);

    // TODO: 主动发送数据给客户端 还这个需求 不用实现
    void sendControlMessage(const nlohmann::json& msg);

    HANDLE m_hControlPipe;
    HANDLE m_hDataPipe;
    std::atomic<bool> m_sessionActive;
    std::thread m_periodicDataThread;
    std::atomic<bool> m_isPushingData;
};