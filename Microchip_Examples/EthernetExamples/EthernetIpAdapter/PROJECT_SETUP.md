# Microchip Studio Project Setup Guide

## Step-by-Step Instructions

### Method 1: Create New Project in Microchip Studio (Recommended)

#### Step 1: Create New Project
1. Open **Microchip Studio**
2. Go to **File → New → Project**
3. Select **GCC C++ Executable Project**
4. Click **Next**
5. **Project Name**: `EthernetIpAdapter`
6. **Location**: `C:\Users\Adam\Desktop\ClearCore-library-master\Microchip_Examples\EthernetExamples\`
7. Click **Next**
8. Select device: **ATSAME53N19A**
9. Click **Finish**

#### Step 2: Remove Default Files
1. Delete `main.cpp` (if it exists)
2. Keep `Device_Startup` folder (or copy from another project)

#### Step 3: Add Main Source File
1. Right-click project → **Add → Existing Item**
2. Navigate to `EthernetIpAdapter\EthernetIpAdapter.cpp`
3. Select and click **Add**

#### Step 4: Add Device Startup Files
1. Right-click project → **Add → Existing Item**
2. Navigate to `Device_Startup\startup_same53.c`
3. Click **Add**
4. In Solution Explorer, right-click `startup_same53.c` → **Properties**
5. Set **Link** to: `Device_Startup\startup_same53.c`

#### Step 5: Add Linker Scripts
1. Right-click project → **Add → Existing Item**
2. Add both:
   - `Device_Startup\flash_with_bootloader.ld`
   - `Device_Startup\flash_without_bootloader.ld`
3. For each, set **Link** to: `Device_Startup\<filename>`

#### Step 6: Add opENer Core Source Files

**Add CIP Files:**
1. Right-click project → **Add → Existing Item**
2. Navigate to `opener\src\cip\`
3. Select ALL `.c` files (Ctrl+A, then filter for .c files)
4. Click **Add**

**Add Ethernet Encapsulation Files:**
1. Right-click project → **Add → Existing Item**
2. Navigate to `opener\src\enet_encap\`
3. Select ALL `.c` files
4. Click **Add**

**Add Utility Files:**
1. Right-click project → **Add → Existing Item**
2. Navigate to `opener\src\utils\`
3. Select ALL `.c` files
4. Click **Add**

**Add Generic Network Handler:**
1. Add: `opener\src\ports\generic_networkhandler.c`

**Add Socket Timer:**
1. Add: `opener\src\ports\socket_timer.c`

**Add NVData Files:**
1. Navigate to `opener\src\ports\nvdata\`
2. Add ALL `.c` files

#### Step 7: Add ClearCore Port Files

1. Right-click project → **Add → Existing Item**
2. Navigate to `opener\src\ports\ClearCore\`
3. Add these files:
   - `opener.c`
   - `networkhandler.c`
   - `networkconfig.c`
   - `opener_error.c`

4. Navigate to `opener\src\ports\ClearCore\sample_application\`
5. Add: `sampleapplication.c`

#### Step 8: Add Project References

1. Right-click project → **References**
2. Click **Add New Reference**
3. Add these projects:
   - `ClearCore` (from `..\..\libClearCore\`)
   - `LwIP` (from `..\..\LwIP\`)

#### Step 9: Configure Include Paths

1. Right-click project → **Properties**
2. Go to **C/C++ Build → Settings → Tool Settings → ARM GCC C++ Compiler → Directories**
3. Click **Add** and add these paths (use relative paths from project directory):

```
../../libClearCore/inc
../../LwIP/LwIP/src/include
../../LwIP/LwIP/port/include
../opener/src
../opener/src/cip
../opener/src/enet_encap
../opener/src/utils
../opener/src/ports
../opener/src/ports/ClearCore
../opener/src/ports/ClearCore/sample_application
../opener/src/ports/nvdata
```

4. Also go to **ARM GCC C Compiler → Directories** and add the same paths

#### Step 10: Add Preprocessor Definitions

1. In **Properties**, go to **C/C++ Build → Settings → Tool Settings**
2. Select **ARM GCC C++ Compiler → Symbols**
3. Click **Add** and add:
   - `CLEARCORE`
4. Do the same for **ARM GCC C Compiler → Symbols**

#### Step 11: Configure Project Properties

1. In **Properties → C/C++ Build → Settings → Tool Settings → ARM GCC C++ Linker → General**
2. Set **Additional Specs**: `Use rdimon (semihosting) library (--specs=rdimon.specs)`

3. Go to **ARM GCC C++ Linker → Libraries**
4. Add libraries:
   - `libm`
   - `arm_cortexM4lf_math`

5. Go to **ARM GCC C++ Linker → Library Search Paths**
6. Add:
   - `../../Device_Startup`
   - `$(ProjectDir)Device_Startup`

7. Go to **ARM GCC C++ Linker → Miscellaneous → Linker Flags**
8. Add: `-Tflash_with_bootloader.ld -mfloat-abi=hard -mfpu=fpv4-sp-d16`

#### Step 12: Add Project to Solution

1. Right-click **Solution** → **Add → Existing Project**
2. Navigate to `EthernetIpAdapter\EthernetIpAdapter.cppproj`
3. Click **Open**

OR manually edit `EthernetExamples.atsln` and add:

```
Project("{E66E83B9-2572-4076-B26E-6BE79FF3018A}") = "EthernetIpAdapter", "EthernetIpAdapter\EthernetIpAdapter.cppproj", "{NEW_GUID_HERE}"
EndProject
```

And in the configuration section, add build configurations for Debug|ARM and Release|ARM.

#### Step 13: Build and Test

1. Select **Debug** or **Release** configuration
2. Right-click project → **Build**
3. Fix any compilation errors
4. Connect ClearCore via USB
5. Right-click project → **Debug → Start Without Debugging** (or F5)

---

## Method 2: Copy and Modify Existing Project

### Quick Steps:
1. Copy entire `EthernetTcpClientHelloWorld` folder and rename to `EthernetIpAdapter`
2. Delete old `.cpp` file and add `EthernetIpAdapter.cpp`
3. Update `.cppproj` file name references
4. Add all opENer source files (Steps 6-7 from Method 1)
5. Update include paths and preprocessor definitions (Steps 9-10)

---

## Troubleshooting

### Common Issues:

**"Cannot find include file"**
- Verify all include paths are added correctly
- Check path separators (use forward slashes or backslashes consistently)
- Ensure relative paths are correct from project directory

**"Undefined reference to..."**
- Verify project references (ClearCore, LwIP) are added
- Check that all source files are added to project
- Verify linker library paths

**"CLEARCORE not defined"**
- Check preprocessor definitions in both C and C++ compiler settings

**"Multiple definition" errors**
- Ensure each source file is only added once
- Check for duplicate file entries in project

**Linker errors**
- Verify linker script path is correct
- Check that libraries (libm, arm_cortexM4lf_math) are linked
- Verify library search paths

---

## File Count Summary

You should have approximately:
- ~40 CIP source files
- ~7 Ethernet encapsulation files  
- ~9 Utility files
- ~6 NVData files
- 4 ClearCore port files
- 1 Sample application file
- 1 Main application file

**Total: ~60-70 source files**

---

## Next Steps After Building

1. **Test Network Initialization**
   - Flash the firmware
   - Open serial terminal (9600 baud)
   - Verify IP address is assigned
   - Check for "OpENer initialized successfully" message

2. **Test EtherNet/IP Communication**
   - Use RSLinx or similar scanner
   - Browse to device IP
   - Test explicit messaging
   - Configure I/O connection (Output 150, Input 100)

3. **Verify Mirror Functionality**
   - Write data to Output Assembly (150)
   - Read from Input Assembly (100)
   - Verify data matches

