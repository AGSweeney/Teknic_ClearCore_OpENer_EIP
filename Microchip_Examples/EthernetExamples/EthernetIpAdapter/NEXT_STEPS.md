# Next Steps: Adding Source Files to Project

The solution file and project file have been created. Now you need to add the opENer source files to the project.

## What's Already Done ✅

- ✅ Solution file updated (`EthernetExamples.atsln`)
- ✅ Project file created (`EthernetIpAdapter.cppproj`)
- ✅ Include paths configured for opENer
- ✅ Preprocessor definition `CLEARCORE` added
- ✅ Project references to ClearCore and LwIP added
- ✅ Device startup files copied
- ✅ Main application file exists

## What You Need to Do Next

### Step 1: Open Solution in Microchip Studio

1. Double-click `EthernetExamples.atsln` or open it in Microchip Studio
2. You should see `EthernetIpAdapter` project in Solution Explorer

### Step 2: Add opENer Source Files

In Microchip Studio Solution Explorer:

**Add CIP Core Files (18 files):**
1. Right-click `EthernetIpAdapter` project → **Add → Existing Item**
2. Navigate to `opener\src\cip\`
3. Select ALL `.c` files (use Ctrl+A, then filter for `.c` extension)
4. Click **Add**

**Add Ethernet Encapsulation Files (3 files):**
1. Right-click project → **Add → Existing Item**
2. Navigate to `opener\src\enet_encap\`
3. Select ALL `.c` files
4. Click **Add**

**Add Utility Files (4 files):**
1. Right-click project → **Add → Existing Item**
2. Navigate to `opener\src\utils\`
3. Select ALL `.c` files
4. Click **Add**

**Add Generic Network Handler:**
1. Add: `opener\src\ports\generic_networkhandler.c`

**Add Socket Timer:**
1. Add: `opener\src\ports\socket_timer.c`

**Add NVData Files (4 files):**
1. Right-click project → **Add → Existing Item**
2. Navigate to `opener\src\ports\nvdata\`
3. Select ALL `.c` files
4. Click **Add**

**Add ClearCore Port Files:**
1. Add: `opener\src\ports\ClearCore\opener.c`
2. Add: `opener\src\ports\ClearCore\networkhandler.c`
3. Add: `opener\src\ports\ClearCore\networkconfig.c`
4. Add: `opener\src\ports\ClearCore\opener_error.c`

**Add Sample Application:**
1. Add: `opener\src\ports\ClearCore\sample_application\sampleapplication.c`

### Step 3: Verify Project Structure

Your project should now have:
- `EthernetIpAdapter.cpp` (main file)
- `Device_Startup/startup_same53.c`
- All opENer `.c` files listed above
- Linker scripts in `Device_Startup/`

### Step 4: Build the Project

1. Select **Debug** or **Release** configuration
2. Right-click `EthernetIpAdapter` project → **Build**
3. Fix any compilation errors

## Expected File Count

After adding all files:
- ~36 opENer source files
- 1 main application file
- 1 device startup file
- 2 linker scripts

**Total: ~40 files**

## Troubleshooting

### If project doesn't appear in solution:
- Close and reopen the solution file
- Or manually add: Right-click Solution → **Add → Existing Project** → select `EthernetIpAdapter.cppproj`

### If build fails with include errors:
- Verify include paths in project properties
- Check that paths use forward slashes or relative paths
- Ensure `CLEARCORE` is defined (it should already be in the .cppproj file)

### If linker errors:
- Verify project references (ClearCore, LwIP) are present
- Check that linker script path is correct
- Verify libraries are linked (libm, arm_cortexM4lf_math)

## Quick File List

Use this list as a checklist when adding files:

```
opener/src/cip/*.c (18 files)
opener/src/enet_encap/*.c (3 files)
opener/src/utils/*.c (4 files)
opener/src/ports/generic_networkhandler.c
opener/src/ports/socket_timer.c
opener/src/ports/nvdata/*.c (4 files)
opener/src/ports/ClearCore/opener.c
opener/src/ports/ClearCore/networkhandler.c
opener/src/ports/ClearCore/networkconfig.c
opener/src/ports/ClearCore/opener_error.c
opener/src/ports/ClearCore/sample_application/sampleapplication.c
```

## Once Building Successfully

1. Connect ClearCore via USB
2. Right-click project → **Debug → Start Without Debugging**
3. Open serial terminal (9600 baud) to see status messages
4. Test with EtherNet/IP scanner software

