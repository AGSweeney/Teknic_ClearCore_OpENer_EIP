/*******************************************************************************
 * Copyright (c) 2009, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

/** @file generic_networkhandler.c
 *  @author Martin Melik Merkumians
 *  @brief This file includes all platform-independent functions of the network handler to reduce code duplication
 *
 *  The generic network handler delegates platform-dependent tasks to the platform network handler
 */

#ifdef CLEARCORE
#include "ports/ClearCore/socket_types.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"
#include "lwip/tcpip.h"
#endif
#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
#ifdef CLEARCORE
#include "ports/ClearCore/sample_application/ethlinkcbs.h"
#endif
#endif
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <signal.h>

#include "generic_networkhandler.h"

#include "typedefs.h"

#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
#ifdef CLEARCORE
void ClearCoreUpdateInStats(CipUdint bytes, bool is_broadcast);
void ClearCoreUpdateOutStats(CipUdint bytes, bool is_broadcast);
void ClearCoreIncrementInErrors(void);
void ClearCoreIncrementOutErrors(void);
#endif
#endif
#include "trace.h"
#include "opener_error.h"
#include "encap.h"
#include "ciptcpipinterface.h"
#include "opener_user_conf.h"
#include "cipqos.h"

#define MAX_NO_OF_TCP_SOCKETS 10

/** @brief Ethernet/IP standard port */

/* ----- Windows size_t PRI macros ------------- */
#if defined(__MINGW32__) || defined(STM32) /* This is a Mingw compiler or STM32 target (GCC) */
#define PRIuSZT PRIuPTR
#define PRIxSZT PRIxPTR
#else
/* Even the Visual Studio compilers / libraries since VS2015 know that now. */
#define PRIuSZT "zu"
#define PRIxSZT "zx"
#endif /* if defined(__MINGW32__) */

#if defined(_WIN32)
/* Most network functions take their I/O buffers as (char *) pointers that
 *  triggers a warning with our CipOctet (aka unsigned char) buffers. */
#define NWBUF_CAST  (void *)
#else
#define NWBUF_CAST
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#define MSG_NOSIGNAL_PRAGMA_MESSAGE \
  "MSG_NOSIGNAL not defined. Check if your system stops on SIGPIPE, as this can happen with the send() function"
#if defined(_WIN32)
#pragma message(MSG_NOSIGNAL_PRAGMA_MESSAGE)
#elif !defined(CLEARCORE)
#warning MSG_NOSIGNAL_PRAGMA_MESSAGE
#endif /* defined(_WIN32) */
#endif

SocketTimer g_timestamps[OPENER_NUMBER_OF_SUPPORTED_SESSIONS];

//EipUint8 g_ethernet_communication_buffer[PC_OPENER_ETHERNET_BUFFER_SIZE]; /**< communication buffer */
/* global vars */
fd_set master_socket;
fd_set read_socket;

int highest_socket_handle;
int g_current_active_tcp_socket;

struct timeval g_time_value;
MilliSeconds g_actual_time;
MilliSeconds g_last_time;

NetworkStatus g_network_status;

#ifdef CLEARCORE
static void udp_unicast_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);
static void udp_io_messaging_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);
#endif

/** @brief Size of the timeout checker function pointer array
 */
#define OPENER_TIMEOUT_CHECKER_ARRAY_SIZE 10

/** @brief function pointer array for timer checker functions
 */
TimeoutCheckerFunction timeout_checker_array[OPENER_TIMEOUT_CHECKER_ARRAY_SIZE];

/** @brief handle any connection request coming in the TCP server socket.
 *
 */
void CheckAndHandleTcpListenerSocket(void);

/** @brief Checks and processes request received via the UDP unicast socket, currently the implementation is port-specific
 *
 */
void CheckAndHandleUdpUnicastSocket(void);

/** @brief Checks and handles incoming messages via UDP broadcast
 *
 */
void CheckAndHandleUdpGlobalBroadcastSocket(void);

/** @brief check if on the UDP consuming socket data has been received and if yes handle it correctly
 *
 */
void CheckAndHandleConsumingUdpSocket(void);

/** @brief Handles data on an established TCP connection, processed connection is given by socket
 *
 *  @param socket The socket to be processed
 *  @return kEipStatusOk on success, or kEipStatusError on failure
 */
EipStatus HandleDataOnTcpSocket(int socket);

void CheckEncapsulationInactivity(int socket_handle);

void RemoveSocketTimerFromList(const int socket_handle);

/*************************************************
* Function implementations from now on
*************************************************/

EipStatus NetworkHandlerInitialize(void) {

  if( kEipStatusOk != NetworkHandlerInitializePlatform() ) {
    return kEipStatusError;
  }

  SocketTimerArrayInitialize(g_timestamps, OPENER_NUMBER_OF_SUPPORTED_SESSIONS);
  /* Activate the current DSCP values to become the used set of values. */
  CipQosUpdateUsedSetQosValues();
  /* Make sure the multicast configuration matches the current IP address. */
  CipTcpIpCalculateMulticastIp(&g_tcpip);
  /* Freeze IP and network mask matching to the socket setup. This is needed
   *  for the off subnet multicast routing check later. */
  g_network_status.ip_address = g_tcpip.interface_configuration.ip_address;
  g_network_status.network_mask = g_tcpip.interface_configuration.network_mask;
  /* Initialize encapsulation layer here because it accesses the IP address. */
  EncapsulationInit();

  /* clear the master and temp sets */
  FD_ZERO(&master_socket);
  FD_ZERO(&read_socket);

  /* create a new TCP socket */
  if( ( g_network_status.tcp_listener =
          socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) ) == -1 ) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler tcp_listener: error allocating socket, %d - %s\n",
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    return kEipStatusError;
  }
  OPENER_TRACE_INFO("networkhandler: TCP listener socket created: %d\n",
                    g_network_status.tcp_listener);

  int set_socket_option_value = 1; //Represents true for used set socket options
  /* Activates address reuse */
  if(setsockopt( g_network_status.tcp_listener, SOL_SOCKET, SO_REUSEADDR,
                 (char *) &set_socket_option_value,
                 sizeof(set_socket_option_value) ) == -1) {
    OPENER_TRACE_ERR(
      "networkhandler tcp_listener: error setting socket option SO_REUSEADDR\n");
    return kEipStatusError;
  }

  if(SetSocketToNonBlocking(g_network_status.tcp_listener) < 0) {
    OPENER_TRACE_ERR(
      "networkhandler tcp_listener: error setting socket to non-blocking on new socket\n");
    return kEipStatusError;
  }

#ifdef CLEARCORE
  g_network_status.udp_unicast_listener = udp_new();
  if (g_network_status.udp_unicast_listener == NULL) {
    OPENER_TRACE_ERR("networkhandler: failed to create UDP unicast PCB\n");
    return kEipStatusError;
  }

  g_network_status.udp_global_broadcast_listener = g_network_status.udp_unicast_listener;

  if (udp_bind(g_network_status.udp_unicast_listener, IP_ADDR_ANY, kOpenerEthernetPort) != ERR_OK) {
    OPENER_TRACE_ERR("networkhandler: failed to bind UDP PCB to port %d\n", kOpenerEthernetPort);
    udp_remove(g_network_status.udp_unicast_listener);
    g_network_status.udp_unicast_listener = NULL;
    g_network_status.udp_global_broadcast_listener = NULL;
    return kEipStatusError;
  }

  udp_recv(g_network_status.udp_unicast_listener, udp_unicast_recv_callback, NULL);

  OPENER_TRACE_INFO("networkhandler: UDP PCB created and bound to port %d (handles both unicast and broadcast)\n", kOpenerEthernetPort);
#else
  /* create a new UDP socket */
  if( ( g_network_status.udp_global_broadcast_listener =
          socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) ) == kEipInvalidSocket ) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler udp_global_broadcast_listener: error allocating socket, %d - %s\n",
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    return kEipStatusError;
  }
  OPENER_TRACE_INFO("networkhandler: UDP global broadcast socket created: %d\n",
                    g_network_status.udp_global_broadcast_listener);

  /* create a new UDP socket */
  if( ( g_network_status.udp_unicast_listener =
          socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) ) == kEipInvalidSocket ) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler udp_unicast_listener: error allocating socket, %d - %s\n",
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    return kEipStatusError;
  }
  OPENER_TRACE_INFO("networkhandler: UDP unicast socket created: %d\n",
                    g_network_status.udp_unicast_listener);

  /* Activates address reuse */
  set_socket_option_value = 1;
  if(setsockopt( g_network_status.udp_global_broadcast_listener, SOL_SOCKET,
                 SO_REUSEADDR, (char *) &set_socket_option_value,
                 sizeof(set_socket_option_value) )
     == -1) {
    OPENER_TRACE_ERR(
      "networkhandler udp_global_broadcast_listener: error setting socket option SO_REUSEADDR\n");
    return kEipStatusError;
  }

  if(SetSocketToNonBlocking(g_network_status.udp_global_broadcast_listener) <
     0) {
    OPENER_TRACE_ERR(
      "networkhandler udp_global_broadcast_listener: error setting socket to non-blocking on new socket\n");
    return kEipStatusError;
  }

  /* Activates address reuse */
  set_socket_option_value = 1;
  if(setsockopt( g_network_status.udp_unicast_listener, SOL_SOCKET,
                 SO_REUSEADDR,
                 (char *) &set_socket_option_value,
                 sizeof(set_socket_option_value) ) == -1) {
    OPENER_TRACE_ERR(
      "networkhandler udp_unicast_listener: error setting socket option SO_REUSEADDR\n");
    return kEipStatusError;
  }

  if(SetSocketToNonBlocking(g_network_status.udp_unicast_listener) < 0) {
    OPENER_TRACE_ERR(
      "networkhandler udp_unicast_listener: error setting socket to non-blocking\n");
    return kEipStatusError;
  }

  struct sockaddr_in my_address = {
    .sin_family = AF_INET,
    .sin_port = htons(kOpenerEthernetPort),
    .sin_addr.s_addr = g_network_status.ip_address
  };

  if( ( bind( g_network_status.udp_unicast_listener,
              (struct sockaddr *) &my_address,
              sizeof(struct sockaddr) ) ) == -1 ) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler udp_unicast_listener: error with UDP bind: %d - %s\n",
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    return kEipStatusError;
  }
  OPENER_TRACE_INFO("networkhandler: UDP unicast bound to port %d\n",
                    kOpenerEthernetPort);

  /* have QoS DSCP explicit appear on UDP responses to unicast messages */
  if(SetQosOnSocket( g_network_status.udp_unicast_listener,
                     CipQosGetDscpPriority(kConnectionObjectPriorityExplicit) )
     != 0) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler udp_unicast_listener: error set QoS %d: %d - %s\n",
      g_network_status.udp_unicast_listener,
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    /* print message but don't abort by intent */
  }

  struct sockaddr_in global_broadcast_address = {
    .sin_family = AF_INET,
    .sin_port = htons(kOpenerEthernetPort),
    .sin_addr.s_addr = htonl(INADDR_ANY)
  };

  /* enable the UDP socket to receive broadcast messages */
  set_socket_option_value = 1;
  if( 0 >
      setsockopt( g_network_status.udp_global_broadcast_listener, SOL_SOCKET,
                  SO_BROADCAST, (char *) &set_socket_option_value,
                  sizeof(int) ) ) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler udp_global_broadcast_listener: error with setting broadcast receive: %d - %s\n",
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    return kEipStatusError;
  }

  if( ( bind( g_network_status.udp_global_broadcast_listener,
              (struct sockaddr *) &global_broadcast_address,
              sizeof(struct sockaddr) ) ) == -1 ) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler udp_global_broadcast_listener: error with UDP bind: %d - %s\n",
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    return kEipStatusError;
  }
  OPENER_TRACE_INFO("networkhandler: UDP broadcast bound to port %d (INADDR_ANY)\n",
                    kOpenerEthernetPort);

  /* have QoS DSCP explicit appear on UDP responses to broadcast messages */
  if(SetQosOnSocket( g_network_status.udp_global_broadcast_listener,
                     CipQosGetDscpPriority(kConnectionObjectPriorityExplicit) )
     != 0) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler udp_global_broadcast_listener: error set QoS %d: %d - %s\n",
      g_network_status.udp_global_broadcast_listener,
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    /* print message but don't abort by intent */
  }
#endif

  struct sockaddr_in tcp_address = {
    .sin_family = AF_INET,
    .sin_port = htons(kOpenerEthernetPort),
    .sin_addr.s_addr = g_network_status.ip_address
  };

  /* bind the new socket to port 0xAF12 (CIP) */
  if( ( bind( g_network_status.tcp_listener, (struct sockaddr *) &tcp_address,
              sizeof(struct sockaddr) ) ) == -1 ) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler tcp_listener: error with TCP bind: %d - %s\n",
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    return kEipStatusError;
  }

  /* Make QoS DSCP explicit already appear on SYN connection establishment.
   * A newly accept()ed TCP socket inherits the setting from this socket.
   */
  if(SetQosOnSocket( g_network_status.tcp_listener,
                     CipQosGetDscpPriority(kConnectionObjectPriorityExplicit) )
     != 0) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler tcp_listener: error set QoS %d: %d - %s\n",
      g_network_status.tcp_listener,
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    /* print message but don't abort by intent */
  }

  /* switch socket in listen mode */
  if( ( listen(g_network_status.tcp_listener,
               MAX_NO_OF_TCP_SOCKETS) ) == -1 ) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler tcp_listener: error with listen: %d - %s\n",
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    return kEipStatusError;
  }

  /* add the listener socket to the master set */
  FD_SET(g_network_status.tcp_listener, &master_socket);
#ifndef CLEARCORE
  FD_SET(g_network_status.udp_unicast_listener, &master_socket);
  FD_SET(g_network_status.udp_global_broadcast_listener, &master_socket);
#endif
  
#ifdef CLEARCORE
  OPENER_TRACE_INFO("networkhandler: Socket setup complete - TCP=%d, UDP_unicast=PCB, UDP_broadcast=PCB\n",
                    g_network_status.tcp_listener);
#else
  OPENER_TRACE_INFO("networkhandler: Socket setup complete - TCP=%d, UDP_unicast=%d, UDP_broadcast=%d\n",
                    g_network_status.tcp_listener,
                    g_network_status.udp_unicast_listener,
                    g_network_status.udp_global_broadcast_listener);
#endif

  /* keep track of the biggest file descriptor */
#ifdef CLEARCORE
  highest_socket_handle = g_network_status.tcp_listener;
#else
  highest_socket_handle = GetMaxSocket(g_network_status.tcp_listener,
                                       g_network_status.udp_global_broadcast_listener,
                                       0,
                                       g_network_status.udp_unicast_listener);
#endif

  g_last_time = GetMilliSeconds(); /* initialize time keeping */
  g_network_status.elapsed_time = 0;

  return kEipStatusOk;
}

void CloseUdpSocket(int socket_handle) {
#ifdef CLEARCORE
  if (socket_handle != 0) {
    struct udp_pcb *pcb = (struct udp_pcb *)socket_handle;
    if (pcb != NULL) {
      udp_remove(pcb);
      OPENER_TRACE_STATE("Closing UDP PCB\n");
    }
  }
#else
  OPENER_TRACE_STATE("Closing UDP socket %d\n", socket_handle);
  CloseSocket(socket_handle);
#endif
}

void CloseTcpSocket(int socket_handle) {
  OPENER_TRACE_STATE("Closing TCP socket %d\n", socket_handle);
  ShutdownSocketPlatform(socket_handle);
  RemoveSocketTimerFromList(socket_handle);
  CloseSocket(socket_handle);
}

void RemoveSocketTimerFromList(const int socket_handle) {
  SocketTimer *socket_timer = NULL;
  while( NULL != ( socket_timer = SocketTimerArrayGetSocketTimer(g_timestamps,
                                                                 OPENER_NUMBER_OF_SUPPORTED_SESSIONS,
                                                                 socket_handle) ) )
  {
    SocketTimerClear(socket_timer);
  }
}

EipBool8 CheckSocketSet(int socket) {
  EipBool8 return_value = false;
  if( FD_ISSET(socket, &read_socket) ) {
    if( FD_ISSET(socket, &master_socket) ) {
      return_value = true;
    } else {
      OPENER_TRACE_INFO("socket: %d closed with pending message\n", socket);
    }
    FD_CLR(socket, &read_socket);
    /* remove it from the read set so that later checks will not find it */
  }
  return return_value;
}

void CheckAndHandleTcpListenerSocket(void) {
  int new_socket = kEipInvalidSocket;
  /* see if this is a connection request to the TCP listener*/
  if( true == CheckSocketSet(g_network_status.tcp_listener) ) {
    int accept_retries = 10;
    
    while(accept_retries > 0) {
      new_socket = accept(g_network_status.tcp_listener, NULL, NULL);
      
      if(new_socket != kEipInvalidSocket) {

        if(SetSocketToNonBlocking(new_socket) < 0) {
          OPENER_TRACE_ERR("networkhandler: error setting new socket %d to non-blocking\n", new_socket);
          CloseTcpSocket(new_socket);
          return;
        }

        FD_SET(new_socket, &master_socket);
        /* add newfd to master set */
        if(new_socket > highest_socket_handle) {
          highest_socket_handle = new_socket;
        }

        OPENER_TRACE_STATE("networkhandler: opened new TCP connection on fd %d\n",
                           new_socket);
        accept_retries--;
        continue;
      }
      
      int error_code = GetSocketErrorNumber();
      if(error_code == OPENER_SOCKET_WOULD_BLOCK) {
        break;
      }
      char *error_message = GetErrorMessage(error_code);
      OPENER_TRACE_ERR("networkhandler: error on accept: %d - %s\n",
                       error_code, error_message);
      FreeErrorMessage(error_message);
      return;
    }
  }
}

EipStatus NetworkHandlerProcessCyclic(void) {

#ifndef CLEARCORE
  CheckAndHandleUdpUnicastSocket();
  CheckAndHandleUdpGlobalBroadcastSocket();
#endif

  read_socket = master_socket;

  g_time_value.tv_sec = 0;
  g_time_value.tv_usec = 1000; /* 1 ms timeout - make select() more responsive */

  static uint32_t cyclic_call_count = 0;
  cyclic_call_count++;
  if ((cyclic_call_count % 10000) == 0) {
    OPENER_TRACE_INFO("networkhandler: cyclic call #%lu, highest_socket=%d\n",
                      (unsigned long)cyclic_call_count, highest_socket_handle);
  }

#ifndef CLEARCORE
  FD_CLR(g_network_status.udp_unicast_listener, &read_socket);
  FD_CLR(g_network_status.udp_global_broadcast_listener, &read_socket);
#endif

  int ready_socket = select(highest_socket_handle + 1,
                            &read_socket,
                            0,
                            0,
                            &g_time_value);

  if(ready_socket == kEipInvalidSocket) {
    if(EINTR == errno) /* we have somehow been interrupted. The default behavior is to go back into the select loop. */
    {
      return kEipStatusOk;
    } else {
      int error_code = GetSocketErrorNumber();
      char *error_message = GetErrorMessage(error_code);
      OPENER_TRACE_ERR("networkhandler: error with select: %d - %s\n",
                       error_code,
                       error_message);
      FreeErrorMessage(error_message);
      return kEipStatusError;
    }
  }

  if(ready_socket > 0) {
    CheckAndHandleTcpListenerSocket();
#ifndef CLEARCORE
    CheckAndHandleConsumingUdpSocket();
#endif

    for(int socket = 0; socket <= highest_socket_handle; socket++) {
#ifndef CLEARCORE
      if( socket == g_network_status.udp_unicast_listener ||
          socket == g_network_status.udp_global_broadcast_listener ) {
        continue;
      }
#endif
      if( true == CheckSocketSet(socket) ) {
        if( kEipStatusError == HandleDataOnTcpSocket(socket) ) /* if error */
        {
          OPENER_TRACE_ERR("networkhandler: Error processing TCP socket %d, closing\n", socket);
          CloseTcpSocket(socket);
          RemoveSession(socket); /* clean up session and close the socket */
        }
      }
    }
  }

  for(int socket = 0; socket <= highest_socket_handle; socket++) {
    CheckEncapsulationInactivity(socket);
  }

  /* Check if all connections from one originator times out */
  //CheckForTimedOutConnectionsAndCloseTCPConnections();
  //OPENER_TRACE_INFO("Socket Loop done\n");
  g_actual_time = GetMilliSeconds();
  g_network_status.elapsed_time += g_actual_time - g_last_time;
  g_last_time = g_actual_time;
  //OPENER_TRACE_INFO("Elapsed time: %u\n", g_network_status.elapsed_time);

  /* check if we had been not able to update the connection manager for several kOpenerTimerTickInMilliSeconds.
   * This should compensate the jitter of the windows timer
   */
  if(g_network_status.elapsed_time >= kOpenerTimerTickInMilliSeconds) {
    /* call manage_connections() in connection manager every kOpenerTimerTickInMilliSeconds ms */
    ManageConnections(g_network_status.elapsed_time);

    /* Call timeout checker functions registered in timeout_checker_array */
    for (size_t i = 0; i < OPENER_TIMEOUT_CHECKER_ARRAY_SIZE; i++) {
      if (NULL != timeout_checker_array[i]) {
        (timeout_checker_array[i])(g_network_status.elapsed_time);
      }
    }

    g_network_status.elapsed_time = 0;
  }
  return kEipStatusOk;
}

EipStatus NetworkHandlerFinish(void) {
  CloseTcpSocket(g_network_status.tcp_listener);
#ifdef CLEARCORE
  if (g_network_status.udp_unicast_listener != NULL) {
    udp_remove(g_network_status.udp_unicast_listener);
    g_network_status.udp_unicast_listener = NULL;
  }
  g_network_status.udp_global_broadcast_listener = NULL;
  if (g_network_status.udp_io_messaging != NULL) {
    udp_remove(g_network_status.udp_io_messaging);
    g_network_status.udp_io_messaging = NULL;
  }
#else
  CloseUdpSocket(g_network_status.udp_unicast_listener);
  CloseUdpSocket(g_network_status.udp_global_broadcast_listener);
#endif
  return kEipStatusOk;
}

void CheckAndHandleUdpGlobalBroadcastSocket(void) {
#ifdef CLEARCORE
  return;
#else
  static uint32_t call_count = 0;
  static uint32_t ewouldblock_count = 0;
  static uint32_t last_error_trace = 0;
  call_count++;

  int packet_count = 0;
  while(true) {
      struct sockaddr_in from_address = { 0 };
      socklen_t from_address_length = sizeof(from_address);

      CipOctet incoming_message[PC_OPENER_ETHERNET_BUFFER_SIZE] = { 0 };
      int received_size = recvfrom(g_network_status.udp_global_broadcast_listener,
                                   NWBUF_CAST incoming_message,
                                   sizeof(incoming_message),
                                   0,
                                   (struct sockaddr *) &from_address,
                                   &from_address_length);

      if(received_size <= 0) {
        int error_code = GetSocketErrorNumber();
        if(error_code == OPENER_SOCKET_WOULD_BLOCK) {
          ewouldblock_count++;
          if(packet_count == 0) {
            if ((call_count - last_error_trace) >= 10000) {
              OPENER_TRACE_INFO("CheckAndHandleUdpGlobalBroadcastSocket: call #%lu, socket=%d, EWOULDBLOCK count=%lu (no packets received)\n",
                                (unsigned long)call_count, g_network_status.udp_global_broadcast_listener, (unsigned long)ewouldblock_count);
              last_error_trace = call_count;
              ewouldblock_count = 0;
            }
            return;
          }
          break;
        }
        char *error_message = GetErrorMessage(error_code);
        OPENER_TRACE_ERR(
          "networkhandler: error on recvfrom UDP global broadcast port: %d - %s\n",
          error_code,
          error_message);
        FreeErrorMessage(error_message);
        return;
      }
      
      packet_count++;

      OPENER_TRACE_STATE(
        "networkhandler: unsolicited UDP message on EIP global broadcast socket\n");

      OPENER_TRACE_INFO("Data received on global broadcast UDP: %d bytes from %d.%d.%d.%d:%d\n",
                        received_size,
                        (from_address.sin_addr.s_addr >> 0) & 0xFF,
                        (from_address.sin_addr.s_addr >> 8) & 0xFF,
                        (from_address.sin_addr.s_addr >> 16) & 0xFF,
                        (from_address.sin_addr.s_addr >> 24) & 0xFF,
                        ntohs(from_address.sin_port));

      EipUint8 *receive_buffer = (EipUint8 *)&incoming_message[0];
      int remaining_bytes = 0;
      ENIPMessage outgoing_message;
      InitializeENIPMessage(&outgoing_message);
      EipStatus need_to_send = HandleReceivedExplictUdpData(
        g_network_status.udp_unicast_listener,
        /* sending from unicast port, due to strange behavior of the broadcast port */
        &from_address,
        receive_buffer,
        received_size,
        &remaining_bytes,
        false,
        &outgoing_message);

      receive_buffer += received_size - remaining_bytes;
      received_size = remaining_bytes;

      if(need_to_send > 0) {
        OPENER_TRACE_INFO("Sending UDP broadcast reply: %d bytes\n",
                          outgoing_message.used_message_length);

        int sent = sendto( g_network_status.udp_unicast_listener,  /* sending from unicast port, due to strange behavior of the broadcast port */
                           (char *) outgoing_message.message_buffer,
                           outgoing_message.used_message_length, 0,
                           (struct sockaddr *) &from_address, sizeof(from_address));
        if(sent != outgoing_message.used_message_length) {
          OPENER_TRACE_ERR(
            "networkhandler: UDP broadcast response send failed: sent=%d, expected=%d\n",
            sent, outgoing_message.used_message_length);
        } else {
          OPENER_TRACE_INFO("UDP broadcast reply sent successfully: %d bytes\n", sent);
        }
      } else {
        OPENER_TRACE_INFO("No UDP broadcast response to send (need_to_send=%d)\n", need_to_send);
      }
      if(remaining_bytes > 0) {
        OPENER_TRACE_ERR("Request on broadcast UDP port had too many data (%d)",
                         remaining_bytes);
      }
    }
#endif
}

void CheckAndHandleUdpUnicastSocket(void) {
#ifdef CLEARCORE
  return;
#else
  static uint32_t call_count = 0;
  static uint32_t ewouldblock_count = 0;
  static uint32_t last_error_trace = 0;
  call_count++;
  
  int packet_count = 0;
  while(true) {
      struct sockaddr_in from_address = { 0 };
      socklen_t from_address_length = sizeof(from_address);

      CipOctet incoming_message[PC_OPENER_ETHERNET_BUFFER_SIZE] = { 0 };
      
      int received_size = recvfrom(g_network_status.udp_unicast_listener,
                                   NWBUF_CAST incoming_message,
                                   sizeof(incoming_message),
                                   0,
                                   (struct sockaddr *) &from_address,
                                   &from_address_length);

      if(received_size <= 0) {
        int error_code = GetSocketErrorNumber();
        if(error_code == OPENER_SOCKET_WOULD_BLOCK) {
          ewouldblock_count++;
          if(packet_count == 0) {
            if ((call_count - last_error_trace) >= 10000) {
              OPENER_TRACE_INFO("CheckAndHandleUdpUnicastSocket: call #%lu, socket=%d, EWOULDBLOCK count=%lu (no packets received)\n",
                                (unsigned long)call_count, g_network_status.udp_unicast_listener, (unsigned long)ewouldblock_count);
              last_error_trace = call_count;
              ewouldblock_count = 0;
            }
            return;
          }
          break;
        }
        char *error_message = GetErrorMessage(error_code);
        OPENER_TRACE_ERR(
          "networkhandler: error on recvfrom UDP unicast port: %d - %s\n",
          error_code,
          error_message);
        FreeErrorMessage(error_message);
        return;
      }
      
      packet_count++;

      OPENER_TRACE_STATE(
        "networkhandler: unsolicited UDP message on EIP unicast socket\n");

      OPENER_TRACE_INFO("Data received on UDP unicast: %d bytes from %d.%d.%d.%d:%d\n",
                        received_size,
                        (from_address.sin_addr.s_addr >> 0) & 0xFF,
                        (from_address.sin_addr.s_addr >> 8) & 0xFF,
                        (from_address.sin_addr.s_addr >> 16) & 0xFF,
                        (from_address.sin_addr.s_addr >> 24) & 0xFF,
                        ntohs(from_address.sin_port));

      EipUint8 *receive_buffer = &incoming_message[0];
      int remaining_bytes = 0;
      ENIPMessage outgoing_message;
      InitializeENIPMessage(&outgoing_message);
      EipStatus need_to_send = HandleReceivedExplictUdpData(
        g_network_status.udp_unicast_listener,
        &from_address,
        receive_buffer,
        received_size,
        &remaining_bytes,
        true,
        &outgoing_message);

      OPENER_TRACE_INFO("HandleReceivedExplictUdpData returned: %d, remaining_bytes: %d\n",
                        need_to_send, remaining_bytes);

      receive_buffer += received_size - remaining_bytes;
      received_size = remaining_bytes;

      if(need_to_send > 0) {
        OPENER_TRACE_INFO("Sending UDP unicast reply: %d bytes to %d.%d.%d.%d:%d\n",
                          outgoing_message.used_message_length,
                          (from_address.sin_addr.s_addr >> 0) & 0xFF,
                          (from_address.sin_addr.s_addr >> 8) & 0xFF,
                          (from_address.sin_addr.s_addr >> 16) & 0xFF,
                          (from_address.sin_addr.s_addr >> 24) & 0xFF,
                          ntohs(from_address.sin_port));

        int sent = sendto( g_network_status.udp_unicast_listener,
                           (char *) outgoing_message.message_buffer,
                           outgoing_message.used_message_length, 0,
                           (struct sockaddr *) &from_address,
                           sizeof(from_address));
        if(sent != outgoing_message.used_message_length) {
          OPENER_TRACE_ERR(
            "networkhandler: UDP unicast response send failed: sent=%d, expected=%d\n",
            sent, outgoing_message.used_message_length);
        } else {
          OPENER_TRACE_INFO("UDP unicast reply sent successfully: %d bytes\n", sent);
        }
      } else {
        OPENER_TRACE_INFO("No UDP response to send (need_to_send=%d)\n", need_to_send);
      }
      if (remaining_bytes > 0) {
        OPENER_TRACE_ERR(
          "Request on unicast UDP port had too many data (%d)",
          remaining_bytes);
      }
    }
#endif
}

EipStatus SendUdpData(const struct sockaddr_in *const address,
                      const ENIPMessage
                      *const outgoing_message) {

#if defined(OPENER_TRACE_ENABLED)
  static char ip_str[INET_ADDRSTRLEN];
  OPENER_TRACE_INFO(
    "UDP packet to be sent to: %s:%d\n",
    inet_ntop(AF_INET, &address->sin_addr, ip_str, sizeof ip_str),
    ntohs(address->sin_port) );
#endif

#ifdef CLEARCORE
  if (g_network_status.udp_io_messaging == NULL) {
    OPENER_TRACE_ERR("networkhandler: UDP IO messaging PCB is NULL\n");
    return kEipStatusError;
  }
  ip_addr_t addr;
  ip4_addr_set_u32(&addr, address->sin_addr.s_addr);
  struct pbuf *tx_buf = pbuf_alloc(PBUF_TRANSPORT, outgoing_message->used_message_length, PBUF_RAM);
  if (tx_buf == NULL) {
    OPENER_TRACE_ERR("networkhandler: Failed to allocate pbuf for UDP send\n");
    return kEipStatusError;
  }
  memcpy(tx_buf->payload, outgoing_message->message_buffer, outgoing_message->used_message_length);
  err_t err = udp_sendto(g_network_status.udp_io_messaging, tx_buf, &addr, ntohs(address->sin_port));
  pbuf_free(tx_buf);
  if (err != ERR_OK) {
    OPENER_TRACE_ERR("networkhandler: error with udp_sendto in SendUDPData: err=%d\n", err);
    return kEipStatusError;
  }
  return kEipStatusOk;
#else
  int sent_length = sendto( g_network_status.udp_io_messaging,
                            (char *)outgoing_message->message_buffer,
                            outgoing_message->used_message_length, 0,
                            (struct sockaddr *) address, sizeof(*address) );
  if(sent_length < 0) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler: error with sendto in SendUDPData: %d - %s\n",
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    return kEipStatusError;
  }

  if(sent_length != outgoing_message->used_message_length) {
    OPENER_TRACE_WARN(
      "data length sent_length mismatch; probably not all data was sent in SendUdpData, sent %d of %d\n",
      sent_length,
      outgoing_message->used_message_length);
    return kEipStatusError;
  }

  return kEipStatusOk;
#endif
}

EipStatus HandleDataOnTcpSocket(int socket) {
  int remaining_bytes = 0;
  long data_sent = PC_OPENER_ETHERNET_BUFFER_SIZE;

  /* We will handle just one EIP packet here the rest is done by the select
   * method which will inform us if more data is available in the socket
     because of the current implementation of the main loop this may not be
     the fastest way and a loop here with a non blocking socket would better
     fit*/

  /*Check how many data is here -- read the first four bytes from the connection */
  CipOctet incoming_message[PC_OPENER_ETHERNET_BUFFER_SIZE] = { 0 };

  long number_of_read_bytes = recv(socket, NWBUF_CAST incoming_message, 4, 0); /*TODO we may have to set the socket to a non blocking socket */

  SocketTimer *const socket_timer = SocketTimerArrayGetSocketTimer(g_timestamps,
                                                                   OPENER_NUMBER_OF_SUPPORTED_SESSIONS,
                                                                   socket);
  if(number_of_read_bytes == 0) {
    OPENER_TRACE_ERR(
      "networkhandler: socket: %d - connection closed by client.\n",
      socket);
    RemoveSocketTimerFromList(socket);
    RemoveSession(socket);
    return kEipStatusError;
  }
  if(number_of_read_bytes < 0) {
    int error_code = GetSocketErrorNumber();
    if(OPENER_SOCKET_WOULD_BLOCK == error_code) {
      return kEipStatusOk;
    }
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR("networkhandler: error on recv: %d - %s\n",
                     error_code,
                     error_message);
    FreeErrorMessage(error_message);
#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
    ClearCoreIncrementInErrors();
#endif
    return kEipStatusError;
  }

  const EipUint8 *read_buffer = &incoming_message[2]; /* at this place EIP stores the data length */
  size_t data_size = GetUintFromMessage(&read_buffer) +
                     ENCAPSULATION_HEADER_LENGTH - 4; /* -4 is for the 4 bytes we have already read*/
  /* (NOTE this advances the buffer pointer) */
  if( (PC_OPENER_ETHERNET_BUFFER_SIZE - 4) < data_size ) { /*TODO can this be handled in a better way?*/
    OPENER_TRACE_ERR(
      "too large packet received will be ignored, will drop the data\n");
    /* Currently we will drop the whole packet */

    do {
      OPENER_TRACE_INFO(
        "Entering consumption loop, remaining data to receive: %ld\n",
        data_sent);
      number_of_read_bytes = recv(socket,
                                  NWBUF_CAST & incoming_message[0],
                                  data_sent,
                                  0);

      if(number_of_read_bytes == 0) /* got error or connection closed by client */
      {
        int error_code = GetSocketErrorNumber();
        char *error_message = GetErrorMessage(error_code);
        OPENER_TRACE_ERR(
          "networkhandler: socket: %d - connection closed by client: %d - %s\n",
          socket,
          error_code,
          error_message);
        FreeErrorMessage(error_message);
        RemoveSocketTimerFromList(socket);
        return kEipStatusError;
      }
      if(number_of_read_bytes < 0) {
        int error_code = GetSocketErrorNumber();
        char *error_message = GetErrorMessage(error_code);
        if(OPENER_SOCKET_WOULD_BLOCK == error_code) {
          return kEipStatusOk;
        } OPENER_TRACE_ERR("networkhandler: error on recv: %d - %s\n",
                           error_code,
                           error_message);
        FreeErrorMessage(error_message);
        return kEipStatusError;
      }
      data_size -= number_of_read_bytes;
      if( (data_size < PC_OPENER_ETHERNET_BUFFER_SIZE) && (data_size != 0) ) {
        data_sent = data_size;
      }
    } while(0 < data_size);
    SocketTimerSetLastUpdate(socket_timer, g_actual_time);
    return kEipStatusOk;
  }

  number_of_read_bytes = recv(socket,
                              NWBUF_CAST & incoming_message[4],
                              data_size,
                              0);

  if(0 == number_of_read_bytes) /* got error or connection closed by client */
  {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler: socket: %d - connection closed by client: %d - %s\n",
      socket,
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    RemoveSocketTimerFromList(socket);
    RemoveSession(socket);
    return kEipStatusError;
  }
  if(number_of_read_bytes < 0) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    if(OPENER_SOCKET_WOULD_BLOCK == error_code) {
      return kEipStatusOk;
    } OPENER_TRACE_ERR("networkhandler: error on recv: %d - %s\n",
                       error_code,
                       error_message);
    FreeErrorMessage(error_message);
    return kEipStatusError;
  }

  if( (unsigned) number_of_read_bytes == data_size ) {
    /*we got the right amount of data */
    data_size += 4;
    /*TODO handle partial packets*/

#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
    ClearCoreUpdateInStats((CipUdint)data_size, false);
#endif

    g_current_active_tcp_socket = socket;

    struct sockaddr sender_address;
    memset( &sender_address, 0, sizeof(sender_address) );
    socklen_t fromlen = sizeof(sender_address);
    if(getpeername(socket, (struct sockaddr *) &sender_address, &fromlen) < 0) {
      int error_code = GetSocketErrorNumber();
      char *error_message = GetErrorMessage(error_code);
      OPENER_TRACE_ERR("networkhandler: could not get peername: %d - %s\n",
                       error_code,
                       error_message);
      FreeErrorMessage(error_message);
    }

    ENIPMessage outgoing_message;
    InitializeENIPMessage(&outgoing_message);
    EipStatus need_to_send = HandleReceivedExplictTcpData(socket,
                                                          incoming_message,
                                                          data_size,
                                                          &remaining_bytes,
                                                          &sender_address,
                                                          &outgoing_message);
    if(NULL != socket_timer) {
      SocketTimerSetLastUpdate(socket_timer, g_actual_time);
    }

    g_current_active_tcp_socket = kEipInvalidSocket;

    if(remaining_bytes != 0) {
      OPENER_TRACE_WARN(
        "Warning: received packet was to long: %d Bytes left!\n",
        remaining_bytes);
    }

    if(need_to_send > 0) {
      data_sent = send(socket,
                       (char *) outgoing_message.message_buffer,
                       outgoing_message.used_message_length,
                       MSG_NOSIGNAL);
      SocketTimerSetLastUpdate(socket_timer, g_actual_time);
      if(data_sent < 0) {
        int error_code = GetSocketErrorNumber();
        char *error_message = GetErrorMessage(error_code);
        OPENER_TRACE_ERR("TCP reply: send failed on socket %d - error %d: %s\n",
                         socket, error_code, error_message);
        FreeErrorMessage(error_message);
#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
        ClearCoreIncrementOutErrors();
#endif
      } else if(data_sent != outgoing_message.used_message_length) {
        OPENER_TRACE_WARN(
          "TCP response was not fully sent: exp %" PRIuSZT ", sent %ld\n",
          outgoing_message.used_message_length,
          data_sent);
#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
        ClearCoreUpdateOutStats((CipUdint)data_sent, false);
#endif
      } else {
#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
        ClearCoreUpdateOutStats((CipUdint)outgoing_message.used_message_length, false);
#endif
      }
#ifdef CLEARCORE
      for(int i = 0; i < 10; i++) {
        if(tcpip_thread_poll_one() <= 0) {
          break;
        }
      }
#endif
    }

    return kEipStatusOk;
  } else {
    /* we got a fragmented packet currently we cannot handle this will
     * for this we would need a network buffer per TCP socket
     *
     * However with typical packet sizes of EIP this shouldn't be a big issue.
     */
    /*TODO handle fragmented packets */
  }
  return kEipStatusError;
}

/** @brief Create the UDP socket for the implicit IO messaging, one socket handles all connections
 *
 * @return the socket handle if successful, else kEipInvalidSocket */
int CreateUdpSocket(void) {

#ifdef CLEARCORE
  g_network_status.udp_io_messaging = udp_new();
  if (g_network_status.udp_io_messaging == NULL) {
    OPENER_TRACE_ERR("networkhandler: cannot create UDP IO messaging PCB\n");
    return kEipInvalidSocket;
  }

  if (udp_bind(g_network_status.udp_io_messaging, IP_ADDR_ANY, kOpenerEipIoUdpPort) != ERR_OK) {
    OPENER_TRACE_ERR("networkhandler: cannot bind UDP IO messaging PCB\n");
    udp_remove(g_network_status.udp_io_messaging);
    g_network_status.udp_io_messaging = NULL;
    return kEipInvalidSocket;
  }

  udp_recv(g_network_status.udp_io_messaging, udp_io_messaging_recv_callback, NULL);

  OPENER_TRACE_INFO("networkhandler: UDP IO messaging PCB created and bound to port %d\n", kOpenerEipIoUdpPort);
  return (int)(intptr_t)g_network_status.udp_io_messaging;
#else
  /* create a new UDP socket */
  g_network_status.udp_io_messaging = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (g_network_status.udp_io_messaging == kEipInvalidSocket) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR("networkhandler: cannot create UDP socket: %d- %s\n",
                     error_code,
                     error_message);
    FreeErrorMessage(error_message);
    return kEipInvalidSocket;
  }

  if (SetSocketToNonBlocking(g_network_status.udp_io_messaging) < 0) {
    OPENER_TRACE_ERR(
      "networkhandler udp_io_messaging: error setting socket to non-blocking on new socket\n");
    CloseUdpSocket(g_network_status.udp_io_messaging);
    OPENER_ASSERT(false);/* This should never happen! */
    return kEipInvalidSocket;
  }

  OPENER_TRACE_INFO("networkhandler: UDP socket %d\n",
                    g_network_status.udp_io_messaging);

  int option_value = 1;
  if (setsockopt( g_network_status.udp_io_messaging, SOL_SOCKET, SO_REUSEADDR,
                  (char *)&option_value, sizeof(option_value) ) < 0) {
    OPENER_TRACE_ERR(
      "error setting socket option SO_REUSEADDR on %s UDP socket\n");
    CloseUdpSocket(g_network_status.udp_io_messaging);
    return kEipInvalidSocket;
  }

  /* The bind on UDP sockets is necessary as the ENIP spec wants the source port to be specified to 2222 */
  struct sockaddr_in source_addr = {
    .sin_family = AF_INET,
    .sin_addr.s_addr = htonl(INADDR_ANY),
    .sin_port = htons(kOpenerEipIoUdpPort)
  };

  if (bind( g_network_status.udp_io_messaging, (struct sockaddr *)&source_addr,
            sizeof(source_addr) ) < 0) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR("error on bind UDP for producing messages: %d - %s\n",
                     error_code,
                     error_message);
    FreeErrorMessage(error_message);
    CloseUdpSocket(g_network_status.udp_io_messaging);
    return kEipInvalidSocket;
  }

  /* add new socket to the master list */
  FD_SET(g_network_status.udp_io_messaging, &master_socket);

  if (g_network_status.udp_io_messaging > highest_socket_handle) {
    OPENER_TRACE_INFO("New highest socket: %d\n",
                      g_network_status.udp_io_messaging);
    highest_socket_handle = g_network_status.udp_io_messaging;
  }
  return g_network_status.udp_io_messaging;
#endif
}

/** @brief Set the Qos the socket for implicit IO messaging
 *
 * @return 0 if successful, else the error code */
int SetQos(CipUsint qos_for_socket) {
#ifdef CLEARCORE
  return 0;
#else
  if (SetQosOnSocket( g_network_status.udp_io_messaging,
                      CipQosGetDscpPriority(qos_for_socket) ) !=
      0) { /* got error */
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR("networkhandler: error on set QoS on socket: %d - %s\n",
                     error_code, error_message);
    FreeErrorMessage(error_message);
    return error_code;
  }
  return 0;
#endif
}

/** @brief Set the socket options for Multicast Producer
 *
 * @return 0 if successful, else the error code */
int SetSocketOptionsMulticastProduce(void) {
#ifdef CLEARCORE
  return 0;
#else
  if (g_tcpip.mcast_ttl_value != 1) {
    /* we need to set a TTL value for the socket */
    if (setsockopt( g_network_status.udp_io_messaging, IPPROTO_IP,
                    IP_MULTICAST_TTL, NWBUF_CAST & g_tcpip.mcast_ttl_value,
                    sizeof(g_tcpip.mcast_ttl_value) ) < 0) {
      int error_code = GetSocketErrorNumber();
      char *error_message = GetErrorMessage(error_code);
      OPENER_TRACE_ERR(
        "networkhandler: could not set the TTL to: %d, error: %d - %s\n",
        g_tcpip.mcast_ttl_value, error_code, error_message);
      FreeErrorMessage(error_message);
      return error_code;
    }
  }
  /* Need to specify the interface for outgoing multicast packets on a
     device with multiple interfaces. */
  struct in_addr my_addr = {.s_addr = g_network_status.ip_address};
  if (setsockopt(g_network_status.udp_io_messaging, IPPROTO_IP, IP_MULTICAST_IF,
                 NWBUF_CAST & my_addr.s_addr, sizeof my_addr.s_addr) < 0) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler: could not set the multicast interface, error: %d "
      "- %s\n",
      error_code, error_message);
    FreeErrorMessage(error_message);
    return error_code;
  }
  return 0;
#endif
}

/** @brief Get the peer address
 *
 * @return peer address if successful, else any address (0) */
EipUint32 GetPeerAddress(void) {
  struct sockaddr_in peer_address;
  socklen_t peer_address_length = sizeof(peer_address);

  if (getpeername(g_current_active_tcp_socket, (struct sockaddr *)&peer_address,
                  &peer_address_length) < 0) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR("networkhandler: could not get peername: %d - %s\n",
                     error_code, error_message);
    FreeErrorMessage(error_message);
    return htonl(INADDR_ANY);
  }
  return peer_address.sin_addr.s_addr;
}

void CheckAndHandleConsumingUdpSocket(void) {
#ifdef CLEARCORE
  return;
#else
  DoublyLinkedListNode *iterator = connection_list.first;

  CipConnectionObject *current_connection_object = NULL;

  /* see a message of the registered UDP socket has been received     */
  while(NULL != iterator) {
    current_connection_object = (CipConnectionObject *) iterator->data;
    iterator = iterator->next; /* do this at the beginning as the close function may can make the entry invalid */

    if( (kEipInvalidSocket !=
         current_connection_object->socket[kUdpCommuncationDirectionConsuming])
        && ( true ==
             CheckSocketSet(current_connection_object->socket[
                              kUdpCommuncationDirectionConsuming
                            ]) ) ) {
      OPENER_TRACE_INFO("Processing UDP consuming message\n");
      struct sockaddr_in from_address = { 0 };
      socklen_t from_address_length = sizeof(from_address);
      CipOctet incoming_message[PC_OPENER_ETHERNET_BUFFER_SIZE] = { 0 };

      int received_size = recvfrom(g_network_status.udp_io_messaging,
                                   NWBUF_CAST incoming_message,
                                   sizeof(incoming_message),
                                   0,
                                   (struct sockaddr *) &from_address,
                                   &from_address_length);
      if(0 == received_size) {
        int error_code = GetSocketErrorNumber();
        char *error_message = GetErrorMessage(error_code);
        OPENER_TRACE_ERR(
          "networkhandler: socket: %d - connection closed by client: %d - %s\n",
          current_connection_object->socket[
            kUdpCommuncationDirectionConsuming],
          error_code,
          error_message);
        FreeErrorMessage(error_message);
        current_connection_object->connection_close_function(
          current_connection_object);
        continue;
      }

      if(0 > received_size) {
        int error_code = GetSocketErrorNumber();
        if(OPENER_SOCKET_WOULD_BLOCK == error_code) {
          return; // No fatal error, resume execution
        } 
        char *error_message = GetErrorMessage(error_code);
        OPENER_TRACE_ERR("networkhandler: error on recv: %d - %s\n",
                           error_code,
                           error_message);
        FreeErrorMessage(error_message);
        current_connection_object->connection_close_function(
          current_connection_object);
        continue;
      }

      HandleReceivedConnectedData(incoming_message, received_size,
                                  &from_address);

    }
  }
#endif
}

void CloseSocket(const int socket_handle) {
  OPENER_TRACE_INFO("networkhandler: closing socket %d\n", socket_handle);

  if(kEipInvalidSocket != socket_handle) {
    FD_CLR(socket_handle, &master_socket);
    CloseSocketPlatform(socket_handle);
  } OPENER_TRACE_INFO("networkhandler: closing socket done %d\n",
                      socket_handle);
}

int GetMaxSocket(int socket1,
                 int socket2,
                 int socket3,
                 int socket4) {
  if( (socket1 > socket2) && (socket1 > socket3) && (socket1 > socket4) ) {
    return socket1;
  }

  if( (socket2 > socket1) && (socket2 > socket3) && (socket2 > socket4) ) {
    return socket2;
  }

  if( (socket3 > socket1) && (socket3 > socket2) && (socket3 > socket4) ) {
    return socket3;
  }

  return socket4;
}

void CheckEncapsulationInactivity(int socket_handle) {
  if(0 < g_tcpip.encapsulation_inactivity_timeout) { //*< Encapsulation inactivity timeout is enabled
    SocketTimer *socket_timer = SocketTimerArrayGetSocketTimer(g_timestamps,
                                                               OPENER_NUMBER_OF_SUPPORTED_SESSIONS,
                                                               socket_handle);

//    OPENER_TRACE_INFO("Check socket %d - socket timer: %p\n",
//                      socket_handle,
//                      socket_timer);
    if(NULL != socket_timer) {
      MilliSeconds diff_milliseconds = g_actual_time - SocketTimerGetLastUpdate(
        socket_timer);

      if( diff_milliseconds >=
          (MilliSeconds) (1000UL * g_tcpip.encapsulation_inactivity_timeout) ) {

        CipSessionHandle encapsulation_session_handle =
          GetSessionFromSocket(socket_handle);

        CloseClass3ConnectionBasedOnSession(encapsulation_session_handle);

        CloseTcpSocket(socket_handle);
        RemoveSession(socket_handle);
      }
    }
  }
}

void RegisterTimeoutChecker(TimeoutCheckerFunction timeout_checker_function) {
  for (size_t i = 0; i < OPENER_TIMEOUT_CHECKER_ARRAY_SIZE; i++) {
    if (NULL == timeout_checker_array[i]) { // find empty array element
      timeout_checker_array[i] = timeout_checker_function; // add function pointer to array
      break;
    }
  }
}

#ifdef CLEARCORE
static void udp_unicast_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
  if (p == NULL) {
    return;
  }

  struct sockaddr_in from_address = { 0 };
  from_address.sin_family = AF_INET;
  from_address.sin_port = htons(port);
  from_address.sin_addr.s_addr = ip4_addr_get_u32(ip_2_ip4(addr));

  CipOctet incoming_message[PC_OPENER_ETHERNET_BUFFER_SIZE] = { 0 };
  size_t received_size = p->tot_len;
  if (received_size > sizeof(incoming_message)) {
    received_size = sizeof(incoming_message);
  }

  pbuf_copy_partial(p, incoming_message, received_size, 0);
  pbuf_free(p);

  uint32_t addr_u32 = ip4_addr_get_u32(ip_2_ip4(addr));
  bool is_broadcast = (addr_u32 == 0xFFFFFFFF || 
                       (addr_u32 & 0xFF000000) == 0xE0000000);

#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
  ClearCoreUpdateInStats((CipUdint)received_size, is_broadcast);
#endif

  OPENER_TRACE_INFO("Data received on UDP (%s): %d bytes from %d.%d.%d.%d:%d\n",
                    is_broadcast ? "broadcast" : "unicast",
                    (int)received_size,
                    ip4_addr1(ip_2_ip4(addr)),
                    ip4_addr2(ip_2_ip4(addr)),
                    ip4_addr3(ip_2_ip4(addr)),
                    ip4_addr4(ip_2_ip4(addr)),
                    port);

  EipUint8 *receive_buffer = &incoming_message[0];
  int remaining_bytes = 0;
  ENIPMessage outgoing_message;
  InitializeENIPMessage(&outgoing_message);
  EipStatus need_to_send = HandleReceivedExplictUdpData(
    (int)(intptr_t)g_network_status.udp_unicast_listener,
    &from_address,
    receive_buffer,
    received_size,
    &remaining_bytes,
    !is_broadcast,
    &outgoing_message);

  if(need_to_send > 0) {
    struct pbuf *tx_buf = pbuf_alloc(PBUF_TRANSPORT, outgoing_message.used_message_length, PBUF_RAM);
    if (tx_buf != NULL) {
      memcpy(tx_buf->payload, outgoing_message.message_buffer, outgoing_message.used_message_length);
      err_t err = udp_sendto(g_network_status.udp_unicast_listener, tx_buf, addr, port);
      pbuf_free(tx_buf);
      if (err == ERR_OK) {
#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
        ClearCoreUpdateOutStats((CipUdint)outgoing_message.used_message_length, is_broadcast);
#endif
        OPENER_TRACE_INFO("UDP reply sent successfully: %d bytes\n", outgoing_message.used_message_length);
      } else {
#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
        ClearCoreIncrementOutErrors();
#endif
        OPENER_TRACE_ERR("UDP reply send failed: err=%d\n", err);
      }
    } else {
      OPENER_TRACE_ERR("Failed to allocate pbuf for UDP reply\n");
    }
  }
}

static void udp_io_messaging_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
  if (p == NULL) {
    return;
  }

  struct sockaddr_in from_address = { 0 };
  from_address.sin_family = AF_INET;
  from_address.sin_port = htons(port);
  from_address.sin_addr.s_addr = ip4_addr_get_u32(ip_2_ip4(addr));

  CipOctet incoming_message[PC_OPENER_ETHERNET_BUFFER_SIZE] = { 0 };
  size_t received_size = p->tot_len;
  if (received_size > sizeof(incoming_message)) {
    received_size = sizeof(incoming_message);
  }

  pbuf_copy_partial(p, incoming_message, received_size, 0);
  pbuf_free(p);

#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
  uint32_t addr_u32 = ip4_addr_get_u32(ip_2_ip4(addr));
  bool is_broadcast = (addr_u32 == 0xFFFFFFFF || 
                     (addr_u32 & 0xFF000000) == 0xE0000000);
  ClearCoreUpdateInStats((CipUdint)received_size, is_broadcast);
#endif

  HandleReceivedConnectedData(incoming_message, received_size, &from_address);
}
#endif
