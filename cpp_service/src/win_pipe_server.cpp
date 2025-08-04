#include "win_pipe_server.h"

#include <iostream>
#include <thread>
#include "spdlog/spdlog.h"

CRITICAL_SECTION WinPipeServer::clients_mutex_;

WinPipeServer::WinPipeServer(const std::string& pipe_name)
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
    on_message_ = callback;
    accept_thread_ = std::thread(&WinPipeServer::accept_loop, this);
}

void WinPipeServer::stop() {
    if (!running_) return;
    running_ = false;

    // 断开所有客户端 clents
    EnterCriticalSection(&clients_mutex_);
    for (HANDLE client : clients_) {
        DisconnectNamedPipe(client);
        CloseHandle(client);
        client = INVALID_HANDLE_VALUE;
    }
    clients_.clear();
    LeaveCriticalSection(&clients_mutex_);

    // 断开监听线程
    if (accept_thread_.joinable()) accept_thread_.join();

    for (auto& t : client_threads_) {
        if (t.joinable()) t.join();
    }
    client_threads_.clear();
}
void WinPipeServer::accept_loop() {
    while (running_) {
        HANDLE pipe = CreateNamedPipeA(
            pipe_name_.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            4096, 4096,
            1000, nullptr);

        if (pipe == INVALID_HANDLE_VALUE) {
            std::cerr << "Failed to create pipe: " << GetLastError() << std::endl;
            continue;
        }

        // 启用命名管道服务器进程，等待客户端进程连接到命名管道实例。客户端进程通过调用 CreateFile或 CallNamedPipe函数进行连接。
        const BOOL connected = ConnectNamedPipe(pipe, nullptr) ?
                         TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (connected && running_) {
            std::cout << "有设备接入 " << std::endl;
            EnterCriticalSection(&clients_mutex_);
            clients_.push_back(pipe);
            LeaveCriticalSection(&clients_mutex_);

            client_threads_.emplace_back(&WinPipeServer::client_handler, this, pipe);
        } else {
            CloseHandle(pipe);
        }
    }
}

void WinPipeServer::client_handler(const HANDLE pipe) {
    char buffer[4096];
    DWORD bytesRead;

    while (running_) {
        if (const BOOL success = ReadFile(pipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr); !success || bytesRead == 0) break;

        buffer[bytesRead] = '\0';
        std::string request(buffer);
        std::string response;

        if (on_message_) {
            on_message_(request, response);
        }

        if (!response.empty()) {
            DWORD bytesWritten;
            WriteFile(pipe, response.c_str(), static_cast<DWORD>(response.size()), &bytesWritten, nullptr);
        }
    }

    // 清理客户端
    EnterCriticalSection(&clients_mutex_);
    std::erase_if(clients_, [&](HANDLE h) { return h == pipe; });
    LeaveCriticalSection(&clients_mutex_);

    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
}

bool WinPipeServer::send_to_all_clients(const std::string& message) const {
    EnterCriticalSection(&clients_mutex_);
    char message1[] = "Hello";
    for (const HANDLE client : clients_) {
        spdlog::info("发送设备{},大小为{}",GetProcessId(client),static_cast<DWORD>(message.size()));
        DWORD bytesWritten;
        const BOOL success = WriteFile(client, message1, 6, &bytesWritten, nullptr);
//        const BOOL success = WriteFile(client, message.c_str(), static_cast<DWORD>(message.size()), &bytesWritten, nullptr);
        if (!success) {
            std::cerr << "Failed to send message to client." << std::endl;
        }
        std::cout <<"written size is " << bytesWritten << std::endl;
    }
    LeaveCriticalSection(&clients_mutex_);
    std::cout << "发送完毕" << std::endl;
    return true;
}
