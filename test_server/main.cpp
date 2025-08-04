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

    auto threadSend = std::thread([&server](){
        while(true){
            server.send_to_all_clients("test");
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
    if(threadSend.joinable()){
        threadSend.join();
    }
    return 0;
}