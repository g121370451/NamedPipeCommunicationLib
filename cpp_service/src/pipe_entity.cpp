#include "pipe_entity.h"

ClientDataChanelInfo::ClientDataChanelInfo(std::string pipe_name, HANDLE pipe_handle,
                                           int delay) : pipe_name(std::move(pipe_name)),
                                                        pipe_handle(pipe_handle),
                                                        delay(delay),
                                                        gpuIndex(-1),
                                                        _is_running(false) {
}


HANDLE ClientDataChanelInfo::getHandle() const {
    return this->pipe_handle;
}

bool ClientDataChanelInfo::start() {

}

