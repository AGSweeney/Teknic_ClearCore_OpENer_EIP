# EtherNet/IP Adapter for ClearCore

This project integrates the opENer EtherNet/IP stack with ClearCore for a simple test application that mirrors Output Assembly data to Input Assembly.

## Features

- Simple output-to-input mirroring (Assembly 150 -> Assembly 100)
- No hardware dependencies - pure data mirroring test
- EtherNet/IP adapter functionality
- Supports explicit messaging and I/O connections

## Assembly Objects

- **Output Assembly (150)**: Data received from scanner
- **Input Assembly (100)**: Mirrored output data (sent to scanner)
- **Config Assembly (151)**: Configuration data (not used in simple test)

## Project Setup

### 1. Add opENer Source Files to Project

In Microchip Studio, add the following source files to your project:

#### Core opENer Files:
- `opener/src/cip/*.c` - All CIP object implementations
- `opener/src/enet_encap/*.c` - Ethernet encapsulation layer
- `opener/src/utils/*.c` - Utility functions
- `opener/src/ports/generic_networkhandler.c` - Generic network handler
- `opener/src/ports/socket_timer.c` - Socket timer functions
- `opener/src/ports/nvdata/*.c` - Non-volatile data handling

#### ClearCore Port Files:
- `opener/src/ports/ClearCore/opener.c`
- `opener/src/ports/ClearCore/networkhandler.c`
- `opener/src/ports/ClearCore/networkconfig.c`
- `opener/src/ports/ClearCore/opener_error.c`
- `opener/src/ports/ClearCore/sample_application/sampleapplication.c`

### 2. Add Include Paths

Add these include directories to your project:

```
opener/src
opener/src/cip
opener/src/enet_encap
opener/src/utils
opener/src/ports
opener/src/ports/ClearCore
opener/src/ports/ClearCore/sample_application
opener/src/ports/nvdata
../../LwIP/LwIP/src/include
../../LwIP/LwIP/port/include
../../libClearCore/inc
```

### 3. Define Platform

In your project settings, add the preprocessor definition:
```
CLEARCORE
```

This tells opENer to use the ClearCore port.

### 4. Linker Settings

Ensure you have sufficient heap memory allocated. opENer requires:
- Minimum 64KB heap recommended
- Check linker script memory settings

## Testing

### Using EtherNet/IP Scanner Software

1. **RSLinx Classic** (Rockwell Automation)
   - Add EtherNet/IP driver
   - Browse to your device IP address
   - Configure I/O connection:
     - Output Assembly: 150
     - Input Assembly: 100
     - Config Assembly: 151

2. **Wireshark**
   - Filter: `enip`
   - Monitor EtherNet/IP traffic on port 44818 (TCP) and 2222 (UDP)

3. **Simple Test**
   - Write data to Output Assembly (150)
   - Read from Input Assembly (100)
   - Data should be mirrored

## Serial Output

The application outputs status information via USB serial:
- Network initialization status
- IP address assignment
- OpENer initialization status
- Periodic status messages

## Troubleshooting

### Network Issues
- Ensure Ethernet cable is connected
- Check IP address assignment (DHCP or static)
- Verify network interface is up

### Compilation Issues
- Verify all include paths are correct
- Check that `CLEARCORE` is defined
- Ensure LwIP headers are accessible

### Runtime Issues
- Check serial output for error messages
- Verify `EthernetMgr.Refresh()` is called regularly
- Ensure `opener_process()` is called every 10ms

## Next Steps

To add hardware functionality:
1. Modify `AfterAssemblyDataReceived()` in `sampleapplication.c`
2. Add hardware-specific code to process output assembly data
3. Update `BeforeAssemblyDataSend()` to read hardware and populate input assembly

