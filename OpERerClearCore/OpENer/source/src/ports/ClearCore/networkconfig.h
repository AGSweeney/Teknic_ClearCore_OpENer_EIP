/*******************************************************************************
 * Copyright (c) 2009, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

#define IfaceLinkIsUp(iface)  netif_is_link_up(iface)

#ifdef CLEARCORE
#include "opener_api.h"
#include "ciptcpipinterface.h"

#ifdef __cplusplus
extern "C" {
#endif

EipStatus IfaceApplyConfiguration(TcpIpInterface *iface, CipTcpIpObject *tcpip);

#ifdef __cplusplus
}
#endif
#endif

