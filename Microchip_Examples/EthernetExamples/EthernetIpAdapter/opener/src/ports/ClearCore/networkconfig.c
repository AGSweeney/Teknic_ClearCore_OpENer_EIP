#include "ClearCore.h"
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "cipstring.h"
#include "networkconfig.h"
#include "cipcommon.h"
#include "ciperror.h"
#include "opener_api.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"

// Redefine LOG_TRACE to use ClearCore's ConnectorUsb after including ClearCore.h
#undef LOG_TRACE
#define LOG_TRACE(...) do { \
    char trace_buf[256]; \
    snprintf(trace_buf, sizeof(trace_buf), __VA_ARGS__); \
    ConnectorUsb.SendLine(trace_buf); \
} while(0)

#include "trace.h"

EipStatus IfaceGetMacAddress(TcpIpInterface *iface,
                             uint8_t *const physical_address) {
    memcpy(physical_address, iface->hwaddr, NETIF_MAX_HWADDR_LEN);
    return kEipStatusOk;
}

static EipStatus GetIpAndNetmaskFromInterface(
    TcpIpInterface *iface, CipTcpIpInterfaceConfiguration *iface_cfg) {
    iface_cfg->ip_address = ip4_addr_get_u32(ip_2_ip4(&iface->ip_addr));
    iface_cfg->network_mask = ip4_addr_get_u32(ip_2_ip4(&iface->netmask));
    return kEipStatusOk;
}

static EipStatus GetGatewayFromRoute(TcpIpInterface *iface,
                                     CipTcpIpInterfaceConfiguration *iface_cfg) {
    iface_cfg->gateway = ip4_addr_get_u32(ip_2_ip4(&iface->gw));
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
    const char *hostname_str = netif_get_hostname(iface);
    if (hostname_str) {
        SetCipStringByCstr(hostname, hostname_str);
    } else {
        SetCipStringByCstr(hostname, "ClearCore");
    }
}

EipStatus IfaceWaitForIp(TcpIpInterface *const iface,
                         int timeout,
                         volatile int *const abort_wait) {
    (void) abort_wait;
    uint32_t start_time = Milliseconds();
    
    while (!netif_is_up(iface) && 
           (timeout < 0 || (Milliseconds() - start_time) < (uint32_t)(timeout * 1000))) {
        Delay_ms(100);
    }
    
    if (netif_is_up(iface)) {
        return kEipStatusOk;
    }
    return kEipStatusError;
}


