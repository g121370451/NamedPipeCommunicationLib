#include <iostream>

#include "json.hpp"
#include "win_pipe_connector.h"

// 创建一个server pipe
int main() {
    std::system("chcp 65001");
    WinPipeConnector connector("sensor_data");
    connector.connect();
    auto a = connector.is_connected();
    std::cout << a << std::endl;
    connector.start_receiving([](const std::string& str) {
        std::cout << str << std::endl;
    });
    auto test = std::thread([](){while(true){

    }});
    if(test.joinable()){
        test.join();
    }
}