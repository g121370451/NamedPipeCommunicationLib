#include "win_pipe_connector.h"
#include <windows.h>
#include <iostream>

WinPipeConnector::WinPipeConnector(const std::string&  pipe_name)
    : pipe_name_(R"(\\.\pipe\)" + pipe_name) {
}

WinPipeConnector::~WinPipeConnector() {
    stop_receiving();
    disconnect();
}

bool WinPipeConnector::connect() {
    if (connected_) return true;

    pipe_handle_ = CreateFileA(
        pipe_name_.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if (pipe_handle_ == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to connect to pipe: " << GetLastError() << std::endl;
        return false;
    }

    connected_ = true;
    return true;
}

void WinPipeConnector::disconnect() {
    if (!connected_) return;

    CloseHandle(pipe_handle_);
    pipe_handle_ = INVALID_HANDLE_VALUE;
    connected_ = false;
}

bool WinPipeConnector::send(const std::string& message) const {
    if (!connected_) return false;

    DWORD bytes_written = 0;
    BOOL success = WriteFile(
        pipe_handle_,
        message.c_str(),
        static_cast<DWORD>(message.size()),
        &bytes_written,
        nullptr);

    return success && (bytes_written == message.size());
}

void WinPipeConnector::start_receiving(const MessageCallback& callback) {
    if (receiving_ || !connected_) return;
    receiving_ = true;
    recv_thread_ = std::thread(&WinPipeConnector::receive_loop, this, callback);
}

void WinPipeConnector::stop_receiving() {
    if (!receiving_) return;

    receiving_ = false;
    if (recv_thread_.joinable()) {
        recv_thread_.join();
    }
}

void WinPipeConnector::receive_loop(const MessageCallback& callback) {
    constexpr size_t BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];

    while (receiving_) {
        DWORD bytes_read = 0;
        BOOL success = ReadFile(
            pipe_handle_,
            buffer,
            BUFFER_SIZE - 1,
            &bytes_read,
            nullptr);

        if (!success || bytes_read == 0) {
            // 断线
            std::cerr << "Pipe read failed or closed: " << GetLastError() << std::endl;
            receiving_ = false;
            break;
        }

        buffer[bytes_read] = '\0';
        callback(std::string(buffer));
    }
}
