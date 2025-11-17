# EtherNet/IP Adapter Build Instructions

## Project Status

The Microchip Studio project has been scaffolded and configured for building the opENer EtherNet/IP adapter for Teknic ClearCore.

## What Has Been Configured

### 1. Source Files Added
All opENer source files have been added to the project:
- **18 CIP files** from `opener/src/cip/`
- **3 Ethernet encapsulation files** from `opener/src/enet_encap/`
- **4 Utility files** from `opener/src/utils/`
- **2 Generic network files** from `opener/src/ports/`
- **4 NVData files** from `opener/src/ports/nvdata/`
- **4 ClearCore port files** from `opener/src/ports/ClearCore/`
- **1 Sample application** from `opener/src/ports/ClearCore/sample_application/`

### 2. Include Paths Configured
The following include paths have been added to both C and C++ compiler settings:
- `../../libClearCore/inc`
- `../../LwIP/LwIP/src/include`
- `../../LwIP/LwIP/port/include`
- `../opener/src`
- `../opener/src/cip`
- `../opener/src/enet_encap`
- `../opener/src/utils`
- `../opener/src/ports`
- `../opener/src/ports/ClearCore`
- `../opener/src/ports/ClearCore/sample_application`
- `../opener/src/ports/nvdata`

### 3. Preprocessor Definitions
- `CLEARCORE` has been added to both Debug and Release configurations for both C and C++ compilers

### 4. Main Application
- `EthernetIpAdapter.cpp` has been updated to use the correct include path (`opener.h`)

## Next Steps

### 1. Open Project in Microchip Studio
1. Open Microchip Studio
2. File → Open → Project/Solution
3. Navigate to `EthernetIpAdapter.atsln` and open it

### 2. Verify Project References
Ensure the following project references are added:
- `ClearCore` library project (from `../../libClearCore/`)
- `LwIP` library project (from `../../LwIP/`)

To add references:
1. Right-click project → **References**
2. Click **Add New Reference**
3. Browse and add the ClearCore and LwIP projects

### 3. Build the Project
1. Select **Debug** or **Release** configuration
2. Right-click project → **Build**
3. Fix any compilation errors if they occur

### 4. Common Build Issues

**Missing include files:**
- Verify all include paths are correct
- Check that project references (ClearCore, LwIP) are added

**Undefined references:**
- Ensure ClearCore and LwIP projects are built first
- Check that project references are properly linked

**CLEARCORE not defined:**
- Verify preprocessor definitions in Project Properties → C/C++ Build → Settings

### 5. Flash and Test
1. Connect ClearCore via USB
2. Right-click project → **Debug → Start Without Debugging** (or press F5)
3. Open serial terminal (9600 baud) to see status messages
4. Verify Ethernet link is active
5. Check for IP address assignment
6. Test with EtherNet/IP scanner software (RSLinx, etc.)

## Project Structure

```
EthernetIpAdapter/
├── EthernetIpAdapter.cppproj    (Updated with all source files)
├── EthernetIpAdapter.cpp        (Main application)
├── Device_Startup/              (Device initialization files)
└── opener/                      (opENer library)
    └── src/
        ├── cip/                 (CIP object implementations)
        ├── enet_encap/          (Ethernet encapsulation)
        ├── utils/               (Utility functions)
        └── ports/
            ├── ClearCore/       (ClearCore port implementation)
            └── nvdata/          (Non-volatile data)
```

## Testing with EtherNet/IP Scanner

1. **RSLinx Classic:**
   - Add EtherNet/IP driver
   - Browse to device IP address
   - Configure I/O connection:
     - Output Assembly: 150
     - Input Assembly: 100
     - Config Assembly: 151 (optional)

2. **Expected Behavior:**
   - Write data to Output Assembly (150)
   - Read from Input Assembly (100)
   - Data should be mirrored (identical)

## Notes

- opENer requires periodic calls to `opener_process()` every ~10ms
- `EthernetMgr.Refresh()` must be called regularly in main loop
- Single-threaded operation (no RTOS required)
- Uses ClearCore's LwIP integration via `EthernetMgr.MacInterface()`

