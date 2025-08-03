#include <iostream>

#include "json.hpp"
#include "win_pipe_server.h"

// 创建一个server pipe
int main() {
    std::system("chcp 65001");
    WinPipeServer server("sensor_data");
    server.start([](const std::string& request, std::string& response) {
        // std::cout << "服务端推送 send: " << request << ":" << response << std::endl;
        response = "服务端推送 send: 100";
    });
    while (true) {
        const bool res = server.send_to_all_clients("一个测试消息");
    }
    return 0;
}