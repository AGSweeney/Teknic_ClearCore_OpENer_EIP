#include "ClearCore.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"
#include "ports/ClearCore/opener.h"
#include "ciptcpipinterface.h"
#include <stdio.h>

int main(void) {
    ConnectorUsb.PortOpen();
    Delay_ms(100);
    
    ConnectorUsb.SendLine("\r\n=== OpENer ClearCore Debug ===");
    ConnectorUsb.SendLine("Waiting for Ethernet link...");
    ConnectorUsb.Flush();
    
    uint32_t linkWaitStart = Milliseconds();
    while (!EthernetMgr.PhyLinkActive()) {
        if (Milliseconds() - linkWaitStart > 5000) {
            ConnectorUsb.SendLine("ERROR: Ethernet link timeout!");
            while (true) {
                Delay_ms(1000);
            }
        }
        Delay_ms(100);
    }
    
    ConnectorUsb.SendLine("Ethernet link detected!\r\n");
    ConnectorUsb.Flush();
    
    EthernetMgr.Setup();
    Delay_ms(100);
    
    ConnectorUsb.SendLine("Initializing network (OpENer will load saved config or use defaults)...");
    bool dhcpSuccess = EthernetMgr.DhcpBegin();
    if (!dhcpSuccess) {
        ConnectorUsb.SendLine("DHCP failed, using default static IP");
        IpAddress ip = IpAddress(192, 168, 1, 100);
        EthernetMgr.LocalIp(ip);
        EthernetMgr.NetmaskIp(IpAddress(255, 255, 255, 0));
        EthernetMgr.GatewayIp(IpAddress(192, 168, 1, 1));
    } else {
        ConnectorUsb.SendLine("DHCP successful (default, may be overridden by saved config)");
    }
    
    Delay_ms(500);
    
    struct netif *netif = EthernetMgr.MacInterface();
    if (netif == nullptr) {
        ConnectorUsb.SendLine("ERROR: Failed to get netif pointer!");
        while (true) {
            Delay_ms(1000);
        }
    }
    
    if (netif->ip_addr.addr != 0) {
        char ipStr[20];
        snprintf(ipStr, sizeof(ipStr), "IP Address: %d.%d.%d.%d",
                 ip4_addr1(&netif->ip_addr),
                 ip4_addr2(&netif->ip_addr),
                 ip4_addr3(&netif->ip_addr),
                 ip4_addr4(&netif->ip_addr));
        ConnectorUsb.SendLine(ipStr);
    } else {
        ConnectorUsb.SendLine("WARNING: IP address not assigned yet");
    }
    
    ConnectorUsb.SendLine("\r\n--- Initializing OpENer ---\r\n");
    ConnectorUsb.Flush();
    
    opener_init(netif);
    
    Delay_ms(500);
    ConnectorUsb.Flush();
    
    int opener_status = opener_get_status();
    if (opener_status == 0) {
        ConnectorUsb.SendLine("OpENer init: SUCCESS (g_end_stack=0)");
    } else {
        char statusMsg[50];
        snprintf(statusMsg, sizeof(statusMsg), "OpENer init: FAILED (g_end_stack=%d)", opener_status);
        ConnectorUsb.SendLine(statusMsg);
    }
    ConnectorUsb.SendLine("\r\n--- OpENer initialization complete ---\r\n");
    ConnectorUsb.Flush();
    
    uint32_t lastOpenerCall = 0;
    uint32_t lastStatusPrint = 0;
    uint32_t lastLedBlink = 0;
    bool ledState = false;
    
    while (true) {
        uint32_t currentTime = Milliseconds();
        
        EthernetMgr.Refresh();
        
        if (currentTime - lastOpenerCall >= 10) {
            opener_cyclic();
            lastOpenerCall = currentTime;
        }
        
        if ((g_tcpip.status & kTcpipStatusIfaceCfgPend) != 0) {
            if (currentTime - lastLedBlink >= 250) {
                ledState = !ledState;
                ConnectorLed.State(ledState);
                lastLedBlink = currentTime;
            }
        } else {
            if (ledState) {
                ConnectorLed.State(false);
                ledState = false;
            }
        }
        
        if (currentTime - lastStatusPrint >= 5000) {
            if (netif->ip_addr.addr != 0) {
                char statusStr[50];
                snprintf(statusStr, sizeof(statusStr), "Status: IP=%d.%d.%d.%d Link=%s",
                         ip4_addr1(&netif->ip_addr),
                         ip4_addr2(&netif->ip_addr),
                         ip4_addr3(&netif->ip_addr),
                         ip4_addr4(&netif->ip_addr),
                         EthernetMgr.PhyLinkActive() ? "UP" : "DOWN");
                ConnectorUsb.SendLine(statusStr);
            }
            lastStatusPrint = currentTime;
        }
    }
}