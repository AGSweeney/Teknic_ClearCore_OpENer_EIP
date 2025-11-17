#include "ClearCore.h"
#include "networkhandler.h"
#include "opener_error.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <fcntl.h>
#include <unistd.h>

// Redefine LOG_TRACE to use ClearCore's ConnectorUsb after including ClearCore.h
#undef LOG_TRACE
#define LOG_TRACE(...) do { \
    char trace_buf[256]; \
    snprintf(trace_buf, sizeof(trace_buf), __VA_ARGS__); \
    ConnectorUsb.SendLine(trace_buf); \
} while(0)

#include "trace.h"

MilliSeconds GetMilliSeconds(void) {
    return (MilliSeconds)Milliseconds();
}

MicroSeconds GetMicroSeconds(void) {
    return (MicroSeconds)(Milliseconds() * 1000);
}

EipStatus NetworkHandlerInitializePlatform(void) {
    return kEipStatusOk;
}

void ShutdownSocketPlatform(int socket_handle) {
    shutdown(socket_handle, SHUT_RDWR);
}

void CloseSocketPlatform(int socket_handle) {
    close(socket_handle);
}

int SetSocketToNonBlocking(int socket_handle) {
    int flags = fcntl(socket_handle, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    return fcntl(socket_handle, F_SETFL, flags | O_NONBLOCK);
}

int SetQosOnSocket(const int socket, CipUsint qos_value) {
    int set_tos = qos_value << 2;
    return setsockopt(socket, IPPROTO_IP, IP_TOS, &set_tos, sizeof(set_tos));
}

