#include "client_handler.h"
#include <spdlog/spdlog.h>
#include <ctime>

#include "protocol.h"
#include "TimerSleep.h"

void ClientHandler::run() {
    bool handshake_ok = this->performHandshake();
    if (!handshake_ok) {
        spdlog::error("客户端 {} 握手失败, 线程即将退出.", this->m_clientId);
        // TODO在此处清理此函数内分配的资源 这里不删除 可能会导致泄漏
        return;
    }
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

void ClientHandler::stop() {
    if(m_sessionActive){
        m_sessionActive = false;
    }
}

void ClientHandler::processControlMessage(const nlohmann::json &msg) {
    // 处理消息
    // 1. 如果是获取GpuList
    if (msg.at(protocol::key::TYPE) == protocol::type::GET_GPU_LIST) {
        spdlog::info("获取全部gpu的列表");
        nlohmann::json getGpuList;
        getGpuList[protocol::key::TYPE] = protocol::type::GET_GPU_LIST;
        getGpuList[protocol::key::SEQ] = m_seq_counter++;
        getGpuList[protocol::key::PAYLOAD] = {{"ServerVersion", "1.0.0"}};
        if (!sendControlMessage(getGpuList)) {
            spdlog::error("[Server] 错误: 向客户端 [{}] 发送GPU_LIST失败。", this->m_clientId);
        }
    }
    // 2. 如果是设置GpuIndex
    else if(msg.at(protocol::key::TYPE) == protocol::type::SET_GPU){
        // 设置gpu索引
        spdlog::info("重新设置gpu的索引");
    }
}


bool ClientHandler::performHandshake() {
    spdlog::info("[Server] 开始与客户端 [{}] 进行握手...", this->m_clientId);
    try {
        // 1. 等待并接收 SERVER_HELLO
        nlohmann::json serverHello;
        if (!readControlMessage(serverHello, 10) ||
            serverHello.at(protocol::key::TYPE) != protocol::type::SERVER_HELLO) {
            spdlog::error("[Server] 错误: 未收到客户端 [{}] 有效的SERVER_HELLO响应。", this->m_clientId);
            return false;
        }
        spdlog::info("[Server] 收到客户端 [ {} ] SERVER_HELLO。", this->m_clientId);
        // 2. 发送 CLIENT_HELLO
        nlohmann::json clientHello;
        clientHello[protocol::key::TYPE] = protocol::type::CLIENT_HELLO;
        clientHello[protocol::key::SEQ] = m_seq_counter++;
        clientHello[protocol::key::PAYLOAD] = {{"ServerVersion", "1.0.0"}};
        if (!sendControlMessage(clientHello)) {
            spdlog::error("[Server] 错误: 向客户端 [{}] 发送CLIENT_HELLO失败。", this->m_clientId);
            return false;
        }
        spdlog::info("[Server] 已向客户端 [{}] 发送 CLIENT_HELLO。", this->m_clientId);

        // 3. 等待并接收 SETUP_DATA_CHANNEL
        nlohmann::json setupMsg;
        if (!readControlMessage(setupMsg, 10) ||
            setupMsg.at(protocol::key::TYPE) != protocol::type::SETUP_DATA_CHANNEL) {
            spdlog::error("[Server] 错误: 未收到[{}]有效的SETUP_DATA_CHANNEL指令。", this->m_clientId);
            return false;
        }
        spdlog::info("[Server] 创建[{}]数据通道", this->m_clientId);

        // 3.1. 创建专属的数据管道 protocol::CONTROL_PIPE_NAME + this->m_clientId
        auto dataPipeName = protocol::CONTROL_PIPE_NAME + this->m_clientId;
        m_hDataPipe = CreateNamedPipeA(
                dataPipeName.c_str(),
                PIPE_ACCESS_DUPLEX // 双向数据传递
                | FILE_FLAG_OVERLAPPED,// 异步传递
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                1,// 专属数据通道
                8192, 8192,
                NMPWAIT_USE_DEFAULT_WAIT, nullptr);

        if (m_hDataPipe == INVALID_HANDLE_VALUE) {
            spdlog::error("[Server] 错误: 创建专属数据管道 [{}] 失败, GLE={}", dataPipeName, GetLastError());
            return false;
        }
        spdlog::info("[Server] 已成功创建数据通道[{}]。", dataPipeName);
        OVERLAPPED overlapped = {0};
        overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (!overlapped.hEvent) {
            spdlog::error("[Server] 创建数据通道连接监听失败, GLE=", GetLastError());
            CloseHandle(m_hDataPipe);
            return false;
        }
        ConnectNamedPipe(m_hDataPipe, &overlapped);

        // 4. 发送 DATA_CHANNEL_READY 确认
        nlohmann::json dataReadyMsg;
        dataReadyMsg[protocol::key::TYPE] = protocol::type::DATA_CHANNEL_READY;
        dataReadyMsg[protocol::key::SEQ] = m_seq_counter++;
        dataReadyMsg[protocol::key::PAYLOAD] = {{"DataPipeName", dataPipeName}};
        if (!sendControlMessage(dataReadyMsg)) {
            spdlog::error("[Server] 错误: 发送[{}]DATA_CHANNEL_READY失败。", this->m_clientId);
            return false;
        }
        // 4.1. 等待链接
        DWORD waitResult = WaitForSingleObject(overlapped.hEvent, 10000);
        if (waitResult == WAIT_OBJECT_0) {
            DWORD bytesTransferred = 0;
            BOOL success = GetOverlappedResult(m_hDataPipe, &overlapped, &bytesTransferred, FALSE);
            if (!success) {
                spdlog::error("[Server] 错误: [{}] 数据通道连接时内部错误,GLE={}", this->m_clientId, GetLastError());
                return false;
            }
        } else {
            spdlog::error("[Server] 错误: 等待[{}] 数据通道连接超时,GLE={}", this->m_clientId, GetLastError());
            CloseHandle(m_hDataPipe);
            return false;
        }
    } catch (const nlohmann::json::exception &e) {
        spdlog::error("[Server] 错误: JSON处理异常 - {}", e.what());
        return false;
    }
    spdlog::info("[Client] 握手成功！双通道通信已建立。");
    return true;
}

void ClientHandler::periodicDataLoop() {
    while (m_sessionActive) {
        // TODO 这里使用hwinfo的sdk 周期的获取传感器数据 来发送给主进程
        nlohmann::json dataReadyMsg;
        dataReadyMsg[protocol::key::TYPE] = protocol::type::DATA_CHANNEL_READY;
        dataReadyMsg[protocol::key::SEQ] = m_seq_counter++;
        dataReadyMsg[protocol::key::PAYLOAD] = {{"test", "data"}};
        sendDataMessage(dataReadyMsg);
        sleep_for_ms(1000);
    }
}

void ClientHandler::controlLoop() {
    while (m_sessionActive) {
        nlohmann::json readMsg;
        auto res = readControlMessage(readMsg, 0);
        if (res) {
            // 处理这条控制语句 + 逻辑处理
            this->processControlMessage(readMsg);
            // 包括设置gpu 读取gpu数据 gpu数据的返回是在control还是data 可以考究一下 暂时觉得control好一点
            spdlog::info("TODO 控制通道读取的数据为{}", to_string(readMsg));
        }
    }
}

ClientHandler::~ClientHandler() {

    if (m_hControlPipe != INVALID_HANDLE_VALUE) {
        DisconnectNamedPipe(m_hControlPipe);
        CloseHandle(m_hControlPipe);
        m_hControlPipe = INVALID_HANDLE_VALUE;
    }
    if (m_hDataPipe != INVALID_HANDLE_VALUE) {
        DisconnectNamedPipe(m_hDataPipe);
        CloseHandle(m_hDataPipe);
        m_hDataPipe = INVALID_HANDLE_VALUE;
    }
    if (m_hReadEvent != INVALID_HANDLE_VALUE){
        CloseHandle(m_hReadEvent);
        m_hReadEvent = INVALID_HANDLE_VALUE;
    }
    if (m_hDataWriteEvent != INVALID_HANDLE_VALUE){
        CloseHandle(m_hDataWriteEvent);
        m_hDataWriteEvent = INVALID_HANDLE_VALUE;
    }
    if (m_hWriteEvent != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hWriteEvent);
        m_hWriteEvent = INVALID_HANDLE_VALUE;
    }
}

ClientHandler::ClientHandler(HANDLE hControlPipe)
        : m_hControlPipe(hControlPipe),
          m_hDataPipe(INVALID_HANDLE_VALUE),
          m_readOvl(0),
          m_writeOvl(0),
          m_writeDataOvl(0) {
    time_t time(0LL);
    char strTime[26];
    ctime_s(strTime, sizeof(strTime), &time);
    this->m_clientId = strTime;

    // 初始化OVERLAPPED结构体，必须清零
    ZeroMemory(&m_readOvl, sizeof(m_readOvl));
    ZeroMemory(&m_writeOvl, sizeof(m_writeOvl));
    ZeroMemory(&m_writeDataOvl, sizeof(m_writeDataOvl));

    // 创建与OVERLAPPED关联的事件
    // TRUE表示手动重置，FALSE表示初始为无信号状态
    m_hReadEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    m_hWriteEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    m_hDataWriteEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

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
            spdlog::error("[Control Sender: {} ] 错误: 等待写入事件失败, GLE= {}", m_clientId, GetLastError());
            return false;
        }
    } else if (!success) {
        auto errorCode = GetLastError();
        if (errorCode == ERROR_NO_DATA || errorCode == ERROR_BROKEN_PIPE) {
            this->m_sessionActive = false;
            spdlog::info("[Control Sender: {} ] 客户端连接断开: GLE={}", this->m_clientId, errorCode);
        } else {
            spdlog::error("[Control Sender: {} ] 错误: 写入时发生即时错误, GLE= {}", m_clientId, GetLastError());
        }
        return false;
    }
    success = GetOverlappedResult(m_hControlPipe, &m_writeOvl, &bytesWritten, FALSE);

    if (!success || bytesWritten != msgStr.length()) {
        spdlog::error("[Control Sender: {} ] 错误: GetOverlappedResult报告写入失败或未完全写入, GLE= {}", m_clientId,
                      GetLastError());
        return false;
    }
    return true;
}

bool ClientHandler::readControlMessage(nlohmann::json &msgJson, int delay) {
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
            nullptr,
            &m_readOvl);

    auto errorCode = GetLastError();
    // 3. 处理ReadFile的即时返回
    if (!success && errorCode == ERROR_IO_PENDING) {
        // 操作正在后台进行，等待完成
        DWORD waitResult = WaitForSingleObject(m_hReadEvent, delay == 0 ? INFINITE : delay * 1000);
        if (waitResult != WAIT_OBJECT_0) {
            spdlog::error("[Control Reader: {} ] 错误: 等待读取事件失败, GLE=", m_clientId, errorCode);
            return false;
        }
    } else if (!success) {
        // 检查是否是客户端断开连接的正常情况
        if (errorCode == ERROR_BROKEN_PIPE || errorCode == ERROR_NO_DATA) {
            this->m_sessionActive = false;
            spdlog::info("[Control Reader: {} ] 信息: 客户端断开了管道连接", m_clientId);
        } else {
            spdlog::error("[Control Reader: {} ] 错误: 读取时发生即时错误, GLE=", m_clientId, errorCode);
        }
        return false;
    }

    // 4. 获取异步操作的最终结果
    success = GetOverlappedResult(m_hControlPipe, &m_readOvl, &bytesRead, FALSE);
    errorCode = GetLastError();
    if (!success || bytesRead == 0) {
        if (errorCode == ERROR_BROKEN_PIPE || errorCode == ERROR_NO_DATA) {
            this->m_sessionActive = false;
            spdlog::info("[Control Reader: {} ] 信息: 客户端断开了管道连接 (GetOverlappedResult)", m_clientId);
        } else {
            spdlog::error("[Control Reader: {} ] 错误: GetOverlappedResult报告读取失败, GLE=", m_clientId, errorCode);
        }
        return false;
    }
    // 5. 解析收到的数据
    try {
        // 只解析实际读取到的字节
        msgJson = nlohmann::json::parse(m_readBuffer.begin(), m_readBuffer.begin() + bytesRead);
    } catch (const nlohmann::json::exception &e) {
        spdlog::error("[Control Reader: {} ] 错误: JSON解析失败- {}, 原始数据为: {}", m_clientId, e.what(),
                      std::string(m_readBuffer.data(), bytesRead));
        return false;
    }

    return true;
}

bool ClientHandler::sendDataMessage(const nlohmann::json &msgJson) {
    std::string msgStr = msgJson.dump();
    DWORD bytesWritten = 0;

    ResetEvent(m_hDataWriteEvent);
    m_writeDataOvl.hEvent = m_hDataWriteEvent;

    BOOL success = WriteFile(
            m_hDataPipe,
            msgStr.c_str(),
            static_cast<DWORD>(msgStr.length()),
            nullptr,
            &m_writeDataOvl);

    // 3. 处理WriteFile的即时返回
    if (!success && GetLastError() == ERROR_IO_PENDING) {
        // 可以设置超时，INFINITE表示无限等待
        DWORD waitResult = WaitForSingleObject(m_hDataWriteEvent, 1000);
        if (waitResult != WAIT_OBJECT_0) {
            spdlog::error("[DataChanel: {} ] 错误: 等待写入事件失败, GLE= {}", m_clientId, GetLastError());
            return false;
        }
    } else if (!success) {
        auto errorCode = GetLastError();
        if (errorCode == ERROR_NO_DATA || errorCode == ERROR_BROKEN_PIPE) {
            this->m_sessionActive = false;
            spdlog::info("[DataChanel: {} ] 客户端连接断开: GLE= {}", m_clientId, GetLastError());
        } else {
            spdlog::error("[DataChanel: {} ] 错误: 写入时发生即时错误, GLE= {}", m_clientId, GetLastError());
        }
        return false;
    }
    success = GetOverlappedResult(m_hDataPipe, &m_writeDataOvl, &bytesWritten, FALSE);

    if (!success || bytesWritten != msgStr.length()) {
        spdlog::error("[DataChanel: {} ] 错误: GetOverlappedResult报告写入失败或未完全写入, GLE= {}", m_clientId,
                      GetLastError());
        return false;
    }
    return true;
}
