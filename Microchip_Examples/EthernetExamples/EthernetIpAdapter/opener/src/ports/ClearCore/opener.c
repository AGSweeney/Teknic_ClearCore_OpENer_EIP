#include "ClearCore.h"
#include "generic_networkhandler.h"
#include "opener_api.h"
#include "cipcommon.h"
#include "cipethernetlink.h"
#include "ciptcpipinterface.h"
#include "networkconfig.h"
#include "doublylinkedlist.h"
#include "cipconnectionobject.h"
#include "nvdata.h"
#include "lwip/netif.h"

// Redefine LOG_TRACE to use ClearCore's ConnectorUsb after including ClearCore.h
#undef LOG_TRACE
#define LOG_TRACE(...) do { \
    char trace_buf[256]; \
    snprintf(trace_buf, sizeof(trace_buf), __VA_ARGS__); \
    ConnectorUsb.SendLine(trace_buf); \
} while(0)

#include "trace.h"

static bool opener_initialized = false;
volatile int g_end_stack = 0;

void opener_init(struct netif *netif) {
    if (opener_initialized) {
        OPENER_TRACE_WARN("Opener already initialized, skipping\n");
        return;
    }

    EipStatus eip_status = 0;

    if (netif_is_link_up(netif)) {
        DoublyLinkedListInitialize(&connection_list,
                                   CipConnectionObjectListArrayAllocator,
                                   CipConnectionObjectListArrayFree);

        uint8_t iface_mac[6];
        IfaceGetMacAddress(netif, iface_mac);

        SetDeviceSerialNumber(123456789);

        EipUint16 unique_connection_id = (EipUint16)(Milliseconds() & 0xFFFF);

        eip_status = CipStackInit(unique_connection_id);

        CipClass *tcp_ip_class = GetCipClass(kCipTcpIpInterfaceClassCode);
        if (NULL != tcp_ip_class) {
            InsertGetSetCallback(tcp_ip_class, NvTcpipSetCallback, kNvDataFunc);
        }

        CipEthernetLinkSetMac(iface_mac);

        GetHostName(netif, &g_tcpip.hostname);

        g_end_stack = 0;

        eip_status = IfaceGetConfiguration(netif, &g_tcpip.interface_configuration);
        if (eip_status < 0) {
            OPENER_TRACE_WARN("Problems getting interface configuration\n");
        }

        eip_status = NetworkHandlerInitialize();
    } else {
        OPENER_TRACE_WARN("Network link is down, OpENer not started\n");
        g_end_stack = 1;
    }

    if ((g_end_stack == 0) && (eip_status == kEipStatusOk)) {
        opener_initialized = true;
        OPENER_TRACE_INFO("OpENer initialized successfully\n");
    } else {
        OPENER_TRACE_ERR("NetworkHandlerInitialize error %d\n", eip_status);
    }
}

void opener_process(void) {
    if (!opener_initialized || g_end_stack) {
        return;
    }

    if (kEipStatusOk != NetworkHandlerProcessCyclic()) {
        OPENER_TRACE_ERR("Error in NetworkHandler loop!\n");
        g_end_stack = 1;
    }
}

