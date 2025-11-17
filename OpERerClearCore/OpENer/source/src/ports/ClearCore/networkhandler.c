/*******************************************************************************
 * Copyright (c) 2009, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

#include "networkhandler.h"

#include "opener_error.h"
#include "trace.h"
#include "encap.h"
#include "opener_user_conf.h"
#include "lwip/sockets.h"
#include "lwip/errno.h"
#ifdef CLEARCORE
#include "ports/ClearCore/socket_types.h"
#include "ports/ClearCore/clearcore_wrapper.h"
#ifndef O_NONBLOCK
#define O_NONBLOCK 1
#endif
#endif

MilliSeconds GetMilliSeconds(void) {
  return (MilliSeconds)GetMillis();
}

MicroSeconds GetMicroSeconds(void) {
  return (MicroSeconds)(GetMillis() * 1000);
}

EipStatus NetworkHandlerInitializePlatform(void) {
  return kEipStatusOk;
}

void ShutdownSocketPlatform(int socket_handle) {
  if (0 != shutdown(socket_handle, SHUT_RDWR)) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR("Failed shutdown() socket %d - Error Code: %d - %s\n",
                     socket_handle,
                     error_code,
                     error_message);
    FreeErrorMessage(error_message);
  }
}

void CloseSocketPlatform(int socket_handle) {
  close(socket_handle);
}

int SetSocketToNonBlocking(int socket_handle) {
  return fcntl(socket_handle, F_SETFL, fcntl(socket_handle,
                                             F_GETFL,
                                             0) | O_NONBLOCK);
}

int SetQosOnSocket(const int socket,
                   CipUsint qos_value) {
  int set_tos = qos_value << 2;
  return setsockopt(socket, IPPROTO_IP, IP_TOS, &set_tos, sizeof(set_tos));
}

