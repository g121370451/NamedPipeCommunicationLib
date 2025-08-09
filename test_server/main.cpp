#include <iostream>

#include "json.hpp"
#include "win_pipe_server.h"

// 创建一个server pipe
int main() {
    std::system("chcp 65001");
    WinPipeServer server("sensor_data");
    server.start();

    auto threadSend = std::thread([&server](){
        while(true){
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
    if(threadSend.joinable()){
        threadSend.join();
    }
    return 0;
}