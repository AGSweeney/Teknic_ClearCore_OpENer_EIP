/*******************************************************************************
 * Copyright (c) 2023, Peter Christen
 * All rights reserved.
 *
 ******************************************************************************/
#ifdef CLEARCORE
#include "ports/ClearCore/socket_types.h"
#endif
#include "generic_networkhandler.h"
#include "opener_api.h"
#include "cipethernetlink.h"
#include "ciptcpipinterface.h"
#include "trace.h"
#include "networkconfig.h"
#include "doublylinkedlist.h"
#include "cipconnectionobject.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "lwip/tcpip.h"
#include <stdlib.h>

#ifdef CLEARCORE
#include "ports/nvdata/nvdata.h"
#endif

volatile int g_end_stack = 0;
struct netif *g_netif = NULL;

void opener_init(struct netif *netif) {

  EipStatus eip_status = 0;

  g_netif = netif;

  if (IfaceLinkIsUp(netif)) {
    DoublyLinkedListInitialize(&connection_list,
                               CipConnectionObjectListArrayAllocator,
                               CipConnectionObjectListArrayFree);

    uint8_t iface_mac[6];
    IfaceGetMacAddress(netif, iface_mac);

    SetDeviceSerialNumber(123456789);

    EipUint16 unique_connection_id = rand();

#ifdef CLEARCORE
    NvdataLoad();
    CipDword config_method = g_tcpip.config_control & kTcpipCfgCtrlMethodMask;
    if (config_method == kTcpipCfgCtrlDhcp || 
        config_method == kTcpipCfgCtrlStaticIp ||
        g_tcpip.interface_configuration.ip_address != 0 || 
        (g_tcpip.hostname.string != NULL && g_tcpip.hostname.length > 0)) {
      OPENER_TRACE_INFO("Opener: Applying loaded network configuration\n");
      IfaceApplyConfiguration(netif, &g_tcpip);
    } else {
      GetHostName(netif, &g_tcpip.hostname);
    }
#else
    GetHostName(netif, &g_tcpip.hostname);
#endif

    eip_status = CipStackInit(unique_connection_id);

    CipEthernetLinkSetMac(iface_mac);

    g_end_stack = 0;

    eip_status = IfaceGetConfiguration(netif, &g_tcpip.interface_configuration);
    if (eip_status < 0) {
      OPENER_TRACE_WARN("Problems getting interface configuration\n");
    }

    eip_status = NetworkHandlerInitialize();
    if (eip_status != kEipStatusOk) {
      OPENER_TRACE_ERR("NetworkHandlerInitialize failed with status %d\n", eip_status);
      g_end_stack = 1;
    }
  }
  else {
    OPENER_TRACE_WARN("Network link is down, OpENer not started\n");
    g_end_stack = 1;
  }
  if ((g_end_stack == 0) && (eip_status == kEipStatusOk)) {
    OPENER_TRACE_INFO("OpENer: initialized successfully\n");
  } else {
    OPENER_TRACE_ERR("OpENer initialization failed: g_end_stack=%d, eip_status=%d\n", g_end_stack, eip_status);
  }
}

void opener_cyclic(void) {
  if (g_end_stack) {
    return;
  }

  if (!g_netif || !IfaceLinkIsUp(g_netif)) {
    OPENER_TRACE_INFO("Network link is down, exiting OpENer\n");
    g_end_stack = 1;
    return;
  }

  sys_check_timeouts();

#ifdef TCPIP_THREAD_TEST
  while (tcpip_thread_poll_one() > 0) {
  }
#endif

  if (kEipStatusOk != NetworkHandlerProcessCyclic()) {
    OPENER_TRACE_ERR("Error in NetworkHandler loop! Exiting OpENer!\n");
    g_end_stack = 1;
  }
}

void opener_shutdown(void) {
  if (!g_end_stack) {
    g_end_stack = 1;
    NetworkHandlerFinish();
    ShutdownCipStack();
  }
}

int opener_get_status(void) {
  return g_end_stack;
}

