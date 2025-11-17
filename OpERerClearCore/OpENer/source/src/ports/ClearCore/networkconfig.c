/*******************************************************************************
 * Copyright (c) 2009, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef CLEARCORE
#include "ports/ClearCore/socket_types.h"
#include "opener_api.h"
#include "ciptcpipinterface.h"
extern EipStatus IfaceApplyConfiguration(TcpIpInterface *iface, CipTcpIpObject *tcpip);
#endif
#include "cipstring.h"
#include "networkconfig.h"
#include "cipcommon.h"
#include "ciperror.h"
#include "trace.h"
#include "opener_api.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"
#include "lwip/netifapi.h"

EipStatus IfaceGetMacAddress(TcpIpInterface *iface,
                             uint8_t *const physical_address) {
  memcpy(physical_address, iface->hwaddr, NETIF_MAX_HWADDR_LEN);

  return kEipStatusOk;
}

static EipStatus GetIpAndNetmaskFromInterface(
    TcpIpInterface *iface, CipTcpIpInterfaceConfiguration *iface_cfg) {
  iface_cfg->ip_address = iface->ip_addr.addr;
  iface_cfg->network_mask = iface->netmask.addr;

  return kEipStatusOk;
}

static EipStatus GetGatewayFromRoute(TcpIpInterface *iface,
                                     CipTcpIpInterfaceConfiguration *iface_cfg) {
  iface_cfg->gateway = iface->gw.addr;

  return kEipStatusOk;
}

EipStatus IfaceGetConfiguration(TcpIpInterface *iface,
                                CipTcpIpInterfaceConfiguration *iface_cfg) {
  CipTcpIpInterfaceConfiguration local_cfg;
  EipStatus status;

  memset(&local_cfg, 0x00, sizeof local_cfg);

  status = GetIpAndNetmaskFromInterface(iface, &local_cfg);
  if (kEipStatusOk == status) {
    status = GetGatewayFromRoute(iface, &local_cfg);
  }
  if (kEipStatusOk == status) {
    ClearCipString(&iface_cfg->domain_name);
    *iface_cfg = local_cfg;
  }
  return status;
}

void GetHostName(TcpIpInterface *iface,
                 CipString *hostname) {
  (void)iface;
  ClearCipString(hostname);
}


