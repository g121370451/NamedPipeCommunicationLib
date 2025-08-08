#include "client_handler.h"
#include <spdlog/spdlog.h>
#include <time.h>

void ClientHandler::run() {
    bool handshake_ok = this->performHandshake();
    if (!handshake_ok) {
        spdlog::error("客户端 {} 握手失败, 线程即将退出.", this->m_clientId);
        // TODO在此处清理此函数内分配的资源
        return;
    }
    spdlog::info("客户端 {} 握手成功, 启动主通信循环.", m_clientId);
    m_sessionActive = true;

    // 启动一个专门用于从控制管道读取消息的线程
    std::thread controlReadThread(&ClientHandler::controlLoop, this);

    // 启动一个专门用于从数据管道读取消息的线程
    std::thread dataReadThread(&ClientHandler::periodicDataLoop, this);

    // 等待所有I/O线程结束后再退出
    controlReadThread.join();
    dataReadThread.join();

    spdlog::info("客户端 {} 会话结束，线程正常退出.", m_clientId);
}

void ClientHandler::processControlMessage(const nlohmann::json &msg) {

}


bool ClientHandler::performHandshake() {
    return false;
}

void ClientHandler::periodicDataLoop() {

}

void ClientHandler::controlLoop() {

}

ClientHandler::~ClientHandler() {
    if (m_hControlPipe != INVALID_HANDLE_VALUE) CloseHandle(m_hControlPipe);
    if (m_hDataPipe != INVALID_HANDLE_VALUE) CloseHandle(m_hDataPipe);
    if (m_hReadEvent) CloseHandle(m_hReadEvent);
    if (m_hWriteEvent) CloseHandle(m_hWriteEvent);
}

ClientHandler::ClientHandler(HANDLE hControlPipe)
        : m_hControlPipe(hControlPipe),
          m_hDataPipe(INVALID_HANDLE_VALUE) {
    time_t time(0LL);
    char strTime[26];
    ctime_s(strTime, sizeof(strTime), &time);
    this->m_clientId = strTime;

    // 初始化OVERLAPPED结构体，必须清零
    ZeroMemory(&m_readOvl, sizeof(m_readOvl));
    ZeroMemory(&m_writeOvl, sizeof(m_writeOvl));

    // 创建与OVERLAPPED关联的事件
    // TRUE表示手动重置，FALSE表示初始为无信号状态
    m_hReadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    m_hWriteEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    // 初始化读取缓冲区
//    m_readBuffer.resize(4096);
}

bool ClientHandler::sendControlMessage(const nlohmann::json &msgJson) {
    std::string msgStr = msgJson.dump();
    DWORD bytesWritten = 0;

    ResetEvent(m_hWriteEvent);
    m_writeOvl.hEvent = m_hWriteEvent;

    BOOL success = WriteFile(
            m_hControlPipe,
            msgStr.c_str(),
            static_cast<DWORD>(msgStr.length()),
            nullptr,
            &m_writeOvl);

    // 3. 处理WriteFile的即时返回
    if (!success && GetLastError() == ERROR_IO_PENDING) {
        // 可以设置超时，INFINITE表示无限等待
        DWORD waitResult = WaitForSingleObject(m_hWriteEvent, INFINITE);
        if (waitResult != WAIT_OBJECT_0) {
            spdlog::error("[Handler: {} ] 错误: 等待写入事件失败, GLE= {}", m_clientId, GetLastError());
            return false;
        }
    } else if (!success) {
        spdlog::error("[Handler: {} ] 错误: 写入时发生即时错误, GLE= {}", m_clientId, GetLastError());
        return false;
    }
    success = GetOverlappedResult(m_hControlPipe, &m_writeOvl, &bytesWritten, FALSE);

    if (!success || bytesWritten != msgStr.length()) {
        spdlog::error("[Handler: {} ] 错误: GetOverlappedResult报告写入失败或未完全写入, GLE= {}", m_clientId, GetLastError());
        return false;
    }

    return true;
}

bool ClientHandler::readControlMessage(nlohmann::json &msgJson) {
    DWORD bytesRead = 0;

    // 1. 重置读取事件
    ResetEvent(m_hReadEvent);
    m_readOvl.hEvent = m_hReadEvent;

    std::vector<char> m_readBuffer;
    m_readBuffer.resize(4096);
    // 2. 发起异步读取
    BOOL success = ReadFile(
            m_hControlPipe,
            m_readBuffer.data(),
            static_cast<DWORD>(m_readBuffer.size()),
            NULL,
            &m_readOvl);

    // 3. 处理ReadFile的即时返回
    if (!success && GetLastError() == ERROR_IO_PENDING) {
        // 操作正在后台进行，等待完成
        DWORD waitResult = WaitForSingleObject(m_hReadEvent, INFINITE);
        if (waitResult != WAIT_OBJECT_0) {
            spdlog::error("[Handler: {} ] 错误: 等待读取事件失败, GLE=", m_clientId, GetLastError());
            return false;
        }
    } else if (!success) {
        // 检查是否是客户端断开连接的正常情况
        if (GetLastError() == ERROR_BROKEN_PIPE) {
            spdlog::info("[Handler: {} ] 信息: 客户端断开了管道连接", m_clientId);
        } else {
            spdlog::error("[Handler: {} ] 错误: 读取时发生即时错误, GLE=", m_clientId, GetLastError());
        }
        return false;
    }

    // 4. 获取异步操作的最终结果
    success = GetOverlappedResult(m_hControlPipe, &m_readOvl, &bytesRead, FALSE);

    if (!success || bytesRead == 0) {
        if (GetLastError() == ERROR_BROKEN_PIPE) {
            s
            std::cout << "[Handler:" << m_clientId << "] 信息: 客户端断开了管道连接 (GetOverlappedResult)。" << std::endl;
        } else {
            std::cerr << "[Handler:" << m_clientId << "] 错误: GetOverlappedResult报告读取失败, GLE=" << GetLastError() << std::endl;
        }
        return false;
    }

    // 5. 解析收到的数据
    try {
        // 只解析实际读取到的字节
        msgJson = nlohmann::json::parse(m_readBuffer.begin(), m_readBuffer.begin() + bytesRead);
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "[Handler:" << m_clientId << "] 错误: JSON解析失败 - " << e.what() << std::endl;
        // 可以在这里打印出接收到的原始数据以供调试
        std::cerr << "  原始数据: " << std::string(m_readBuffer.data(), bytesRead) << std::endl;
        return false;
    }

    return true;
}
