#pragma once
#include <string>

namespace protocol {
    const std::string CONTROL_PIPE_NAME = R"(\\.\pipe\)";
    // 控制消息类型
    namespace type {
        const std::string CLIENT_HELLO = "CLIENT_HELLO";
        const std::string SERVER_HELLO = "SERVER_HELLO";
        const std::string SETUP_DATA_CHANNEL = "SETUP_DATA_CHANNEL";
        const std::string DATA_CHANNEL_READY = "DATA_CHANNEL_READY";
        const std::string START_PERIODIC_DATA = "START_PERIODIC_DATA";
        const std::string STOP_PERIODIC_DATA = "STOP_PERIODIC_DATA";
        const std::string ACK = "ACK";
        const std::string GOODBYE = "GOODBYE";
        const std::string GET_GPU_LIST = "GET_GPU_LIST";
        const std::string SET_GPU = "SET_GPU";
    }

    // JSON字段名
    namespace key {
        const std::string TYPE = "type";
        const std::string SEQ = "seq";
        const std::string TIMESTAMP = "timestamp";
        const std::string PAYLOAD = "payload";
    }
}