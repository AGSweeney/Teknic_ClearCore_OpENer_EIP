#include "ClearCore.h"
#include "lwip/netif.h"
#include "opener.h"

bool usingDhcp = true;

int main(void) {
    ConnectorUsb.Mode(Connector::USB_CDC);
    ConnectorUsb.Speed(9600);
    ConnectorUsb.PortOpen();
    
    uint32_t timeout = 5000;
    uint32_t startTime = Milliseconds();
    while (!ConnectorUsb && Milliseconds() - startTime < timeout) {
        continue;
    }
    
    ConnectorUsb.SendLine("Initializing EtherNet/IP Adapter...");
    
    while (!EthernetMgr.PhyLinkActive()) {
        ConnectorUsb.SendLine("Waiting for Ethernet link...");
        Delay_ms(1000);
    }
    
    EthernetMgr.Setup();
    
    if (usingDhcp) {
        bool dhcpSuccess = EthernetMgr.DhcpBegin();
        if (!dhcpSuccess) {
            ConnectorUsb.SendLine("DHCP failed, using static IP");
            IpAddress ip = IpAddress(192, 168, 1, 100);
            EthernetMgr.LocalIp(ip);
        }
    } else {
        IpAddress ip = IpAddress(192, 168, 1, 100);
        EthernetMgr.LocalIp(ip);
    }
    
    ConnectorUsb.Send("EtherNet/IP Adapter IP: ");
    ConnectorUsb.SendLine(EthernetMgr.LocalIp().StringValue());
    
    struct netif *netif = EthernetMgr.MacInterface();
    if (netif == nullptr) {
        ConnectorUsb.SendLine("Failed to get network interface!");
        while (true) {
            continue;
        }
    }
    
    opener_init(netif);
    
    ConnectorUsb.SendLine("EtherNet/IP Adapter ready");
    ConnectorUsb.SendLine("Output Assembly (150) -> Input Assembly (100) mirroring enabled");
    
    uint32_t lastOpenerCall = 0;
    uint32_t lastStatusPrint = 0;
    
    while (true) {
        EthernetMgr.Refresh();
        
        uint32_t currentTime = Milliseconds();
        if (currentTime - lastOpenerCall >= 10) {
            opener_process();
            lastOpenerCall = currentTime;
        }
        
        if (currentTime - lastStatusPrint >= 5000) {
            ConnectorUsb.SendLine("EtherNet/IP running...");
            lastStatusPrint = currentTime;
        }
        
        Delay_ms(1);
    }
}

