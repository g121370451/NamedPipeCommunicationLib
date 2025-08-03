#include <iostream>

#include "json.hpp"
#include "win_pipe_server.h"

// 创建一个server pipe
int main() {
    WinPipeServer server("sensor_data");
    server.start([](const std::string& request, const std::string& response) {
        std::cout << "服务端推送 send: " << request << ":" << response << std::endl;
    });
    while (true) {
        const bool res = server.send_to_all_clients("一个测试消息");
    }
    return 0;
}