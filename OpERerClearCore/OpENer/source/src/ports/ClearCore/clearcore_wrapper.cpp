#ifdef CLEARCORE
#include "ClearCore.h"
#include "NvmManager.h"
#include "ports/ClearCore/clearcore_wrapper.h"
#include "lwip/opt.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"
#include "lwip/netifapi.h"
#include "ports/ClearCore/networkconfig.h"
#include "ciptcpipinterface.h"
#include "trace.h"
#include "lwip/inet.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <core_cm4.h>

extern "C" {
unsigned long GetMillis(void) {
    return Milliseconds();
}

void ConnectorLed_SetState(int state) {
    ConnectorLed.State(state != 0);
}

void ConnectorIO0_Initialize(void) {
    ConnectorIO0.Mode(Connector::OUTPUT_DIGITAL);
}

void ConnectorIO0_SetState(int state) {
    ConnectorIO0.State(state != 0);
}

void ConnectorIO1_Initialize(void) {
    ConnectorIO1.Mode(Connector::OUTPUT_DIGITAL);
}

void ConnectorIO1_SetState(int state) {
    ConnectorIO1.State(state != 0);
}

void ConnectorIO2_Initialize(void) {
    ConnectorIO2.Mode(Connector::OUTPUT_DIGITAL);
}

void ConnectorIO2_SetState(int state) {
    ConnectorIO2.State(state != 0);
}

void ConnectorIO3_Initialize(void) {
    ConnectorIO3.Mode(Connector::OUTPUT_DIGITAL);
}

void ConnectorIO3_SetState(int state) {
    ConnectorIO3.State(state != 0);
}

void ConnectorIO4_Initialize(void) {
    ConnectorIO4.Mode(Connector::OUTPUT_DIGITAL);
}

void ConnectorIO4_SetState(int state) {
    ConnectorIO4.State(state != 0);
}

void ConnectorIO5_Initialize(void) {
    ConnectorIO5.Mode(Connector::OUTPUT_DIGITAL);
}

void ConnectorIO5_SetState(int state) {
    ConnectorIO5.State(state != 0);
}

void ConnectorDI6_Initialize(void) {
    ConnectorDI6.Mode(Connector::INPUT_DIGITAL);
}

int ConnectorDI6_GetState(void) {
    return ConnectorDI6.State() ? 1 : 0;
}

void ConnectorDI7_Initialize(void) {
    ConnectorDI7.Mode(Connector::INPUT_DIGITAL);
}

int ConnectorDI7_GetState(void) {
    return ConnectorDI7.State() ? 1 : 0;
}

void ConnectorDI8_Initialize(void) {
    ConnectorDI8.Mode(Connector::INPUT_DIGITAL);
}

int ConnectorDI8_GetState(void) {
    return ConnectorDI8.State() ? 1 : 0;
}

void ConnectorA9_Initialize(void) {
    ConnectorA9.Mode(Connector::INPUT_DIGITAL);
}

int ConnectorA9_GetState(void) {
    return ConnectorA9.State() ? 1 : 0;
}

void ConnectorA10_Initialize(void) {
    ConnectorA10.Mode(Connector::INPUT_DIGITAL);
}

int ConnectorA10_GetState(void) {
    return ConnectorA10.State() ? 1 : 0;
}

void ConnectorA11_Initialize(void) {
    ConnectorA11.Mode(Connector::INPUT_DIGITAL);
}

int ConnectorA11_GetState(void) {
    return ConnectorA11.State() ? 1 : 0;
}

void ConnectorA12_Initialize(void) {
    ConnectorA12.Mode(Connector::INPUT_DIGITAL);
}

int ConnectorA12_GetState(void) {
    return ConnectorA12.State() ? 1 : 0;
}

void ClearCoreTraceOutput(const char *format, ...) {
    static char buffer[512];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer) - 1, format, args);
    va_end(args);
    
    if (len > 0) {
        buffer[len] = '\0';
        
        if (len > 0 && buffer[len - 1] != '\n') {
            if (len < (int)sizeof(buffer) - 1) {
                buffer[len] = '\n';
                buffer[len + 1] = '\0';
            }
        }
        
        ConnectorUsb.Send(buffer);
        ConnectorUsb.Flush();
    }
}
}

int ClearCoreEepromRead(uint16_t address, uint8_t *data, size_t length) {
    if (data == NULL || length == 0) {
        return -1;
    }
    
    if (address + length > 8192) {
        return -1;
    }
    
    ClearCore::NvmManager &nvm = ClearCore::NvmManager::Instance();
    nvm.BlockRead((ClearCore::NvmManager::NvmLocations)address, (int)length, data);
    
    return 0;
}

int ClearCoreEepromWrite(uint16_t address, const uint8_t *data, size_t length) {
    if (data == NULL || length == 0) {
        return -1;
    }
    
    if (address + length > 8192) {
        return -1;
    }
    
    ClearCore::NvmManager &nvm = ClearCore::NvmManager::Instance();
    bool success = nvm.BlockWrite((ClearCore::NvmManager::NvmLocations)address, (int)length, data);
    
    return success ? 0 : -1;
}

extern "C" {
EipStatus IfaceApplyConfiguration(TcpIpInterface *iface, CipTcpIpObject *tcpip) {
    if (iface == NULL || tcpip == NULL) {
        return kEipStatusError;
    }
    
    OPENER_TRACE_INFO("IfaceApplyConfiguration: CALLED - This should only happen at startup!\n");
    
    CipDword config_method = tcpip->config_control & kTcpipCfgCtrlMethodMask;
    
    if (config_method == kTcpipCfgCtrlDhcp) {
        OPENER_TRACE_INFO("IfaceApplyConfiguration: Switching to DHCP mode\n");
        bool dhcp_success = EthernetMgr.DhcpBegin();
        if (!dhcp_success) {
            OPENER_TRACE_ERR("IfaceApplyConfiguration: DHCP failed\n");
            return kEipStatusError;
        }
    } else if (config_method == kTcpipCfgCtrlStaticIp) {
        OPENER_TRACE_INFO("IfaceApplyConfiguration: Setting static IP configuration\n");
        
        CipUdint ip_addr = ntohl(tcpip->interface_configuration.ip_address);
        CipUdint netmask = ntohl(tcpip->interface_configuration.network_mask);
        CipUdint gateway = ntohl(tcpip->interface_configuration.gateway);
        
        IpAddress ip = IpAddress((ip_addr >> 24) & 0xFF, 
                                 (ip_addr >> 16) & 0xFF,
                                 (ip_addr >> 8) & 0xFF,
                                 ip_addr & 0xFF);
        IpAddress nm = IpAddress((netmask >> 24) & 0xFF,
                                 (netmask >> 16) & 0xFF,
                                 (netmask >> 8) & 0xFF,
                                 netmask & 0xFF);
        IpAddress gw = IpAddress((gateway >> 24) & 0xFF,
                                 (gateway >> 16) & 0xFF,
                                 (gateway >> 8) & 0xFF,
                                 gateway & 0xFF);
        
        EthernetMgr.LocalIp(ip);
        EthernetMgr.NetmaskIp(nm);
        EthernetMgr.GatewayIp(gw);
        EthernetMgr.Setup();
        
        ip4_addr_t ip4_addr, ip4_netmask, ip4_gateway;
        IP4_ADDR(&ip4_addr, (ip_addr >> 24) & 0xFF, (ip_addr >> 16) & 0xFF, 
                 (ip_addr >> 8) & 0xFF, ip_addr & 0xFF);
        IP4_ADDR(&ip4_netmask, (netmask >> 24) & 0xFF, (netmask >> 16) & 0xFF,
                 (netmask >> 8) & 0xFF, netmask & 0xFF);
        IP4_ADDR(&ip4_gateway, (gateway >> 24) & 0xFF, (gateway >> 16) & 0xFF,
                 (gateway >> 8) & 0xFF, gateway & 0xFF);
        
#if LWIP_NETIF_API && LWIP_IPV4
        netifapi_netif_set_addr(iface, &ip4_addr, &ip4_netmask, &ip4_gateway);
#else
        netif_set_addr(iface, &ip4_addr, &ip4_netmask, &ip4_gateway);
#endif
        
        OPENER_TRACE_INFO("IfaceApplyConfiguration: Static IP set to %d.%d.%d.%d\n",
                          (ip_addr >> 24) & 0xFF, (ip_addr >> 16) & 0xFF,
                          (ip_addr >> 8) & 0xFF, ip_addr & 0xFF);
    }
    
    if (tcpip->hostname.string != NULL && tcpip->hostname.length > 0) {
#if LWIP_NETIF_HOSTNAME
        if (tcpip->hostname.string[tcpip->hostname.length] == '\0') {
            netif_set_hostname(iface, (const char *)tcpip->hostname.string);
            OPENER_TRACE_INFO("IfaceApplyConfiguration: Hostname set to '%s'\n", tcpip->hostname.string);
        } else {
            static char hostname_buffer[65];
            size_t copy_len = (tcpip->hostname.length < 64) ? tcpip->hostname.length : 64;
            memcpy(hostname_buffer, tcpip->hostname.string, copy_len);
            hostname_buffer[copy_len] = '\0';
            netif_set_hostname(iface, hostname_buffer);
            OPENER_TRACE_INFO("IfaceApplyConfiguration: Hostname set to '%s'\n", hostname_buffer);
        }
#endif
    }
    
    tcpip->status &= ~kTcpipStatusIfaceCfgPend;
    
    return kEipStatusOk;
}

void ClearCoreRebootDevice(void) {
    OPENER_TRACE_INFO("ClearCoreRebootDevice: Rebooting device...\n");
    ConnectorUsb.Flush();
    Delay_ms(100);
    NVIC_SystemReset();
}

void ClearCoreClearNvram(void) {
    OPENER_TRACE_INFO("ClearCoreClearNvram: Clearing NVRAM...\n");
    
    uint8_t zero_magic[4] = {0, 0, 0, 0};
    ClearCore::NvmManager &nvm = ClearCore::NvmManager::Instance();
    
    bool success = nvm.BlockWrite((ClearCore::NvmManager::NvmLocations)0x0100, 4, zero_magic);
    if (success) {
        OPENER_TRACE_INFO("ClearCoreClearNvram: NVRAM cleared successfully\n");
    } else {
        OPENER_TRACE_ERR("ClearCoreClearNvram: Failed to clear NVRAM\n");
    }
    
    ConnectorUsb.Flush();
}
}

#endif

