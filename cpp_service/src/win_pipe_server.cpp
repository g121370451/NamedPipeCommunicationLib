#include "win_pipe_server.h"

#include <iostream>
#include <thread>
#include "spdlog/spdlog.h"

CRITICAL_SECTION WinPipeServer::clients_mutex_;

WinPipeServer::WinPipeServer(const std::string &pipe_name)
        : pipe_name_(R"(\\.\pipe\)" + pipe_name), running_(false) {
    InitializeCriticalSection(&clients_mutex_);
}


WinPipeServer::~WinPipeServer() {
    stop();
    DeleteCriticalSection(&clients_mutex_);
}

void WinPipeServer::start(const MessageCallback &callback) {
    if (running_) return;
    running_ = true;
//    on_message_ = callback;
    accept_thread_ = std::thread(&WinPipeServer::accept_loop, this);
}

void WinPipeServer::stop() {
    if (!running_) return;
    running_ = false;

    // 断开所有客户端 clents
    EnterCriticalSection(&clients_mutex_);
    for (HANDLE client: clients_) {
        DisconnectNamedPipe(client);
        CloseHandle(client);
        client = INVALID_HANDLE_VALUE;
    }
    clients_.clear();
    LeaveCriticalSection(&clients_mutex_);

    // 断开监听线程
    if (accept_thread_.joinable()) accept_thread_.join();

    for (auto &t: client_threads_) {
        if (t.joinable()) t.join();
    }
    client_threads_.clear();
}

void WinPipeServer::accept_loop() {
    while (running_) {
        HANDLE pipe = CreateNamedPipeA(
                pipe_name_.c_str(),
                PIPE_ACCESS_DUPLEX // 双向数据传递
                | FILE_FLAG_OVERLAPPED,// 异步传递
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                PIPE_UNLIMITED_INSTANCES,// 可以多个客户端连接管理通道
                4096, 4096,
                1000, nullptr);

        if (pipe == INVALID_HANDLE_VALUE) {
            std::cerr << "Failed to create pipe: " << GetLastError() << std::endl;
            continue;
        }
        OVERLAPPED overlapped = {0};
        overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        // 启用命名管道服务器进程，等待客户端进程连接到命名管道实例。客户端进程通过调用 CreateFile或 CallNamedPipe函数进行连接。
        ConnectNamedPipe(pipe, &overlapped);
        // 异步等待事件触发
        DWORD waitResult = WaitForSingleObject(overlapped.hEvent, INFINITE);
        if (waitResult == WAIT_OBJECT_0 && running_) {
            DWORD bytesTransferred = 0;
            BOOL success = GetOverlappedResult(pipe, &overlapped, &bytesTransferred, FALSE);
            if (success) {
                std::cout << "有设备接入 " << std::endl;
                this->clients_.push_back(pipe);
                // 添加管理通道对当前事件的监听
                client_threads_.emplace_back(&WinPipeServer::client_handler, this, pipe);
            } else {
                CloseHandle(pipe);
            }
        } else {
            CloseHandle(pipe);
        }
    }
}

void WinPipeServer::client_handler(HANDLE pipe) {
    char buffer[4096];
    OVERLAPPED readOverlapped = {0};
    readOverlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);  // 创建手动重置事件

    while (running_) {
        DWORD bytesRead = 0;

        // 发起异步读取
        if (!ReadFile(pipe, buffer, 4095, &bytesRead, &readOverlapped)) {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                // 正常异步等待
                WaitForSingleObject(readOverlapped.hEvent, INFINITE);

                // 获取实际读取的字节数
                if (!GetOverlappedResult(pipe, &readOverlapped, &bytesRead, FALSE)) {
                    std::cerr << "GetOverlappedResult failed: " << GetLastError() << std::endl;
                    break;
                }
            } else {
                std::cerr << "ReadFile failed: " << err << std::endl;
                break;
            }
        }
        spdlog::info("rrr");
        // 处理接收到的数据
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::string request(buffer);
            std::string response;

            // 调用消息处理回调
            if (on_message_) {
                on_message_(request, response);
            } else {
                // 默认响应
                response = "test_response";
            }

            // 发送响应
            if (!response.empty()) {
                OVERLAPPED writeOverlapped = {0};
                writeOverlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

                DWORD bytesWritten = 0;
                if (!WriteFile(pipe, response.c_str(), static_cast<DWORD>(response.size()),
                               &bytesWritten, &writeOverlapped)) {
                    DWORD writeErr = GetLastError();
                    if (writeErr == ERROR_IO_PENDING) {
                        WaitForSingleObject(writeOverlapped.hEvent, INFINITE);
                        if (!GetOverlappedResult(pipe, &writeOverlapped, &bytesWritten, FALSE)) {
                            std::cerr << "Write operation failed: " << GetLastError() << std::endl;
                        }
                    } else {
                        std::cerr << "WriteFile failed: " << writeErr << std::endl;
                    }
                }
                CloseHandle(writeOverlapped.hEvent);
            }
        } else {
            // 客户端断开连接
            std::cout << "Client disconnected" << std::endl;
            break;
        }

        // 重置事件对象以备下次读取
        ResetEvent(readOverlapped.hEvent);
    }

// 清理客户端
    EnterCriticalSection(&clients_mutex_);
    auto it = std::find_if(clients_.begin(), clients_.end(), [pipe](HANDLE h) { return h == pipe; });
    if (it != clients_.end()) {
        clients_.erase(it);
    }
    LeaveCriticalSection(&clients_mutex_);

// 清理资源
    CloseHandle(readOverlapped.hEvent);
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
}
