#include "win_pipe_server.h"

#include <iostream>
#include <thread>
#include "spdlog/spdlog.h"
#include "client_handler.h"

CRITICAL_SECTION WinPipeServer::clients_mutex_;

WinPipeServer::WinPipeServer(const std::string &pipe_name)
        : pipe_name_(R"(\\.\pipe\)" + pipe_name), running_(false) {
    InitializeCriticalSection(&clients_mutex_);
    this->on_closed_ = [](ClientHandler* clientHandler){
        EnterCriticalSection(&clients_mutex_);
        clientHandler->stop();
        std::erase_if(this->client_threads_, [](const Task& t) {
            return t.isClosed();
        });
        LeaveCriticalSection(&clients_mutex_);
    };
}


WinPipeServer::~WinPipeServer() {
    stop();
    DeleteCriticalSection(&clients_mutex_);
}

void WinPipeServer::start() {
    if (running_) return;
    running_ = true;
    accept_thread_ = std::thread(&WinPipeServer::accept_loop, this);
    spdlog::info("管道开启-监听 {} 中....", pipe_name_);
}

void WinPipeServer::stop() {
    if (!running_) return;
    running_ = false;

    // 断开所有客户端 clients
    EnterCriticalSection(&clients_mutex_);
//    TODO 删除所有链接
    for (std::unique_ptr<ClientHandler> client: clients_) {
        DisconnectNamedPipe(client.get());
        CloseHandle(client.get());
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
                NMPWAIT_USE_DEFAULT_WAIT, nullptr);

        if (pipe == INVALID_HANDLE_VALUE) {
            std::cerr << "Failed to create pipe: " << GetLastError() << std::endl;
            continue;
        }
        OVERLAPPED overlapped = {0};
        overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (!overlapped.hEvent) {
            std::cerr << "CreateEvent 失败, GLE=" << GetLastError() << std::endl;
            CloseHandle(pipe);
            continue;
        }
        ConnectNamedPipe(pipe, &overlapped);
        DWORD waitResult = WaitForSingleObject(overlapped.hEvent, INFINITE);
        if (waitResult == WAIT_OBJECT_0 && running_) {
            DWORD bytesTransferred = 0;
            BOOL success = GetOverlappedResult(pipe, &overlapped, &bytesTransferred, FALSE);
            if (success) {
                std::cout << "新客户端连接，为其创建处理线程..." << std::endl;
                std::unique_ptr<ClientHandler> handler = std::make_unique<ClientHandler>(pipe);
                // 添加管理通道对当前事件的监听
                client_threads_.emplace_back(&ClientHandler::run, handler.get());
                this->clients_.push_back(std::move(handler));
            } else {
                CloseHandle(pipe);
            }
        } else {
            CloseHandle(pipe);
        }
    }
}