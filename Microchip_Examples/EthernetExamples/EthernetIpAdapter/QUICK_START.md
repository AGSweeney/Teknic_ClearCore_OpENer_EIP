# Quick Start: Creating Microchip Studio Project

## Fastest Method: Copy and Modify Existing Project

### Step 1: Copy Project Template
1. In File Explorer, copy the entire `EthernetTcpClientHelloWorld` folder
2. Rename it to `EthernetIpAdapter`
3. Delete `EthernetTcpClientHelloWorld.cpp`
4. Rename `EthernetTcpClientHelloWorld.cppproj` to `EthernetIpAdapter.cppproj`
5. Rename `EthernetTcpClientHelloWorld.componentinfo.xml` to `EthernetIpAdapter.componentinfo.xml` (if exists)

### Step 2: Edit Project File
1. Open `EthernetIpAdapter.cppproj` in a text editor
2. Replace all occurrences of:
   - `EthernetTcpClientHelloWorld` → `EthernetIpAdapter`
   - `{eb373d13-2ec0-42da-bde8-eb9f90421805}` → `{7939f930-0e6a-420a-8b47-3bcce557cc98}`
   - `<Name>EthernetTcpHelloWorld</Name>` → `<Name>EthernetIpAdapter</Name>`
3. Find `<Compile Include="EthernetTcpClientHelloWorld.cpp">` and change to:
   ```xml
   <Compile Include="EthernetIpAdapter.cpp">
   ```

### Step 3: Add Main Source File
1. Copy `EthernetIpAdapter.cpp` into the `EthernetIpAdapter` folder
2. In Microchip Studio, right-click project → **Add → Existing Item**
3. Select `EthernetIpAdapter.cpp`

### Step 4: Add opENer Source Files
Use the file list in `list_source_files.txt` or follow these steps:

**In Microchip Studio:**
1. Right-click project → **Add → Existing Item**
2. Navigate to each directory and add files:

**Add CIP files (38 files):**
- Go to `opener/src/cip/`
- Select ALL `.c` files (Ctrl+A, filter for .c)
- Click **Add**

**Add Ethernet Encapsulation files (3 files):**
- Go to `opener/src/enet_encap/`
- Select ALL `.c` files
- Click **Add**

**Add Utility files (4 files):**
- Go to `opener/src/utils/`
- Select ALL `.c` files
- Click **Add**

**Add Generic Network Handler:**
- Add `opener/src/ports/generic_networkhandler.c`

**Add Socket Timer:**
- Add `opener/src/ports/socket_timer.c`

**Add NVData files (4 files):**
- Go to `opener/src/ports/nvdata/`
- Select ALL `.c` files
- Click **Add**

**Add ClearCore Port files:**
- Add `opener/src/ports/ClearCore/opener.c`
- Add `opener/src/ports/ClearCore/networkhandler.c`
- Add `opener/src/ports/ClearCore/networkconfig.c`
- Add `opener/src/ports/ClearCore/opener_error.c`

**Add Sample Application:**
- Add `opener/src/ports/ClearCore/sample_application/sampleapplication.c`

### Step 5: Configure Include Paths
1. Right-click project → **Properties**
2. **C/C++ Build → Settings → Tool Settings → ARM GCC C++ Compiler → Directories**
3. Click **Add** and add these paths:

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

4. Repeat for **ARM GCC C Compiler → Directories**

### Step 6: Add Preprocessor Definitions
1. In **Properties → C/C++ Build → Settings → Tool Settings**
2. **ARM GCC C++ Compiler → Symbols**
3. Click **Add**: `CLEARCORE`
4. Repeat for **ARM GCC C Compiler → Symbols**

### Step 7: Add Project to Solution
1. In Microchip Studio, open `EthernetExamples.atsln`
2. Right-click **Solution** → **Add → Existing Project**
3. Select `EthernetIpAdapter/EthernetIpAdapter.cppproj`
4. Click **Open**

### Step 8: Build and Test
1. Select **Debug** or **Release** configuration
2. Right-click project → **Build**
3. Fix any compilation errors
4. Connect ClearCore
5. Right-click project → **Debug → Start Without Debugging** (or F5)

---

## Troubleshooting

### If files won't add:
- Make sure you're selecting `.c` files, not headers
- Check that paths are relative to project directory
- Try adding files one directory at a time

### If build fails:
- Verify all include paths are correct
- Check that `CLEARCORE` is defined
- Ensure project references (ClearCore, LwIP) are added

### If linker errors:
- Check that all source files are added
- Verify linker script paths
- Check library paths and libraries (libm, arm_cortexM4lf_math)

---

## Expected File Count

After adding all files, you should have approximately:
- 1 Main application file
- ~18 CIP source files
- ~3 Ethernet encapsulation files
- ~4 Utility files
- ~5 Port-specific files
- ~4 NVData files
- 1 Device startup file
- 2 Linker scripts

**Total: ~38 source files**

---

## Verification Checklist

- [ ] Project compiles without errors
- [ ] All opENer source files added
- [ ] Include paths configured
- [ ] `CLEARCORE` defined
- [ ] Project references added (ClearCore, LwIP)
- [ ] Linker settings configured
- [ ] Project added to solution

