# ClearCore Port Integration Guide

## Quick Start for Simple Test

This guide helps you integrate opENer with ClearCore for a simple output-to-input mirroring test.

## Files Created

### Platform Files:
- `opener.h` / `opener.c` - Main initialization and processing
- `networkhandler.c` - Platform-specific network functions
- `networkconfig.c` / `networkconfig.h` - Network interface configuration
- `opener_error.c` - Error handling
- `platform_network_includes.h` - Platform includes
- `devicedata.h` - Device data definitions

### Application Files:
- `sample_application/sampleapplication.c` - Simple mirror application
- `sample_application/opener_user_conf.h` - opENer configuration

## Key Features

### Simple Mirror Test
- **Output Assembly (150)**: Receives data from EtherNet/IP scanner
- **Input Assembly (100)**: Mirrors output data back to scanner
- **Config Assembly (151)**: Reserved for configuration (not used in simple test)

### How It Works
1. Scanner sends data to Output Assembly (150)
2. `AfterAssemblyDataReceived()` copies output to input buffer
3. Scanner reads Input Assembly (100) and receives mirrored data

## Integration Steps

### 1. Add Source Files to Microchip Studio Project

Add these files to your project:

**Core opENer:**
- All `.c` files from `opener/src/cip/`
- All `.c` files from `opener/src/enet_encap/`
- All `.c` files from `opener/src/utils/`
- `opener/src/ports/generic_networkhandler.c`
- `opener/src/ports/socket_timer.c`
- All `.c` files from `opener/src/ports/nvdata/`

**ClearCore Port:**
- `opener/src/ports/ClearCore/opener.c`
- `opener/src/ports/ClearCore/networkhandler.c`
- `opener/src/ports/ClearCore/networkconfig.c`
- `opener/src/ports/ClearCore/opener_error.c`
- `opener/src/ports/ClearCore/sample_application/sampleapplication.c`

### 2. Add Include Paths

In Project Properties → C/C++ Build → Settings → Tool Settings → ARM GCC C++ Compiler → Directories:

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

### 3. Add Preprocessor Definitions

In Project Properties → C/C++ Build → Settings → Tool Settings → ARM GCC C++ Compiler → Symbols:

```
CLEARCORE
```

### 4. Main Application

Use `EthernetIpAdapter/EthernetIpAdapter.cpp` as your main file, or integrate the code into your existing project.

## Testing

### Using RSLinx Classic

1. Add EtherNet/IP driver
2. Browse to your device IP address
3. Configure I/O connection:
   - Output Assembly: 150
   - Input Assembly: 100
   - Config Assembly: 151 (optional)

### Expected Behavior

- Write data to Output Assembly (150)
- Read from Input Assembly (100)
- Data should be identical (mirrored)

## Troubleshooting

### Compilation Errors

- **Missing includes**: Verify all include paths are added
- **Undefined CLEARCORE**: Add `CLEARCORE` preprocessor definition
- **LwIP errors**: Ensure LwIP include paths are correct

### Runtime Issues

- **No network**: Check Ethernet cable and link status
- **No IP**: Verify DHCP or static IP configuration
- **No response**: Ensure `EthernetMgr.Refresh()` and `opener_process()` are called regularly

## Next Steps

To add hardware functionality:

1. Modify `AfterAssemblyDataReceived()` in `sampleapplication.c`
2. Process output assembly data for your hardware
3. Update `BeforeAssemblyDataSend()` to read hardware state
4. Populate input assembly with hardware data

## Notes

- opENer requires periodic calls to `opener_process()` every ~10ms
- `EthernetMgr.Refresh()` must be called regularly in main loop
- Single-threaded operation (no RTOS required)
- Uses ClearCore's LwIP integration via `EthernetMgr.MacInterface()`

