# LWIP Modifications and Configuration Changes Documentation

## Overview

This document provides a comprehensive record of all modifications made to the LWIP stack and configuration changes for the ESP32-P4 OpENer EtherNet/IP project. These changes include RFC 5227 compliance implementation, performance optimizations, and task affinity configurations.

**ESP-IDF Version**: v5.5.1  
**LWIP Component Path**: `C:\Users\agswe\esp\v5.5.1\esp-idf\components\lwip\`

---

## Table of Contents

1. [LWIP Source Code Modifications](#lwip-source-code-modifications)
2. [Configuration Changes (sdkconfig)](#configuration-changes-sdkconfig)
3. [Build Configuration Changes](#build-configuration-changes)
4. [ACD Timing Configuration](#acd-timing-configuration)
5. [Task Affinity Configuration](#task-affinity-configuration)
6. [Summary of Changes](#summary-of-changes)

---

## LWIP Source Code Modifications

### 1. New Files Created

#### `lwip/src/include/lwip/netif_pending_ip.h` (NEW FILE)

**Purpose**: Defines structure to hold pending IP configuration during RFC 5227 compliant deferred IP assignment.

**Location**: `C:\Users\agswe\esp\v5.5.1\esp-idf\components\lwip\lwip\src\include\lwip\netif_pending_ip.h`

**Key Contents**:
- `struct netif_pending_ip_config` - Stores IP, netmask, gateway, ACD state, and callback
- Forward declarations to avoid circular dependencies
- Conditional compilation based on `LWIP_ACD_RFC5227_COMPLIANT_STATIC`

**Rationale**: Required for RFC 5227 compliance to defer IP assignment until ACD confirms safety.

---

### 2. Modified Files

#### `lwip/src/include/lwip/opt.h`

**File Path**: `C:\Users\agswe\esp\v5.5.1\esp-idf\components\lwip\lwip\src\include\lwip\opt.h`

**Changes Made**:

1. **Added RFC 5227 Compliance Option** (around line 1058-1073):
   ```c
   /**
    * LWIP_ACD_RFC5227_COMPLIANT_STATIC==1: Enable RFC 5227 compliant static IP assignment.
    * When enabled, netif_set_addr() will defer IP assignment until ACD confirms safety.
    * IP address is NOT assigned until ACD_IP_OK callback is received.
    * If conflict is detected, IP is not assigned (or removed if already assigned).
    * 
    * This requires LWIP_ACD to be enabled.
    * Default: 1 (enabled) for RFC 5227 compliance
    */
   #ifndef LWIP_ACD_RFC5227_COMPLIANT_STATIC
   #define LWIP_ACD_RFC5227_COMPLIANT_STATIC   1
   #endif
   #if !LWIP_ACD
   #undef LWIP_ACD_RFC5227_COMPLIANT_STATIC
   #define LWIP_ACD_RFC5227_COMPLIANT_STATIC   0
   #endif
   ```

**Rationale**: Enables RFC 5227 compliant static IP assignment by default, ensuring IP addresses are not assigned until ACD confirms they are safe.

---

#### `lwip/src/include/lwip/netif.h`

**File Path**: `C:\Users\agswe\esp\v5.5.1\esp-idf\components\lwip\lwip\src\include\lwip\netif.h`

**Changes Made**:

1. **Added Forward Declaration** (around line 52-66):
   ```c
   #if LWIP_ACD && LWIP_ACD_RFC5227_COMPLIANT_STATIC
   /* Forward declare to avoid circular dependency */
   struct netif_pending_ip_config;
   /* Forward declare callback type needed for netif_set_addr_with_acd */
   #include "lwip/prot/acd.h"
   typedef void (*acd_conflict_callback_t)(struct netif *netif, acd_callback_enum_t state);
   #endif
   ```

2. **Added Field to struct netif** (around line 401-406):
   ```c
   #if LWIP_ACD && LWIP_ACD_RFC5227_COMPLIANT_STATIC
     struct netif_pending_ip_config *pending_ip_config;
   #endif /* LWIP_ACD && LWIP_ACD_RFC5227_COMPLIANT_STATIC */
   ```

3. **Added Function Declaration** (around line 458-463):
   ```c
   #if LWIP_ACD && LWIP_ACD_RFC5227_COMPLIANT_STATIC
   err_t netif_set_addr_with_acd(struct netif *netif,
                                  const ip4_addr_t *ipaddr,
                                  const ip4_addr_t *netmask,
                                  const ip4_addr_t *gw,
                                  acd_conflict_callback_t callback);
   #endif
   ```

**Rationale**: Provides API for RFC 5227 compliant static IP assignment and stores pending configuration.

---

#### `lwip/src/include/lwip/acd.h`

**File Path**: `C:\Users\agswe\esp\v5.5.1\esp-idf\components\lwip\lwip\src\include\lwip\acd.h`

**Changes Made**:

1. **Added Forward Declaration for etharp_hdr** (around line 49-51):
   ```c
   /* Forward declare struct etharp_hdr to avoid circular dependency */
   struct etharp_hdr;
   ```

2. **Added RFC 5227 Callback Declaration** (around line 58-59):
   ```c
   #if LWIP_ACD_RFC5227_COMPLIANT_STATIC
   void acd_static_ip_rfc5227_callback(struct netif *netif, acd_callback_enum_t state);
   #endif
   ```

3. **Removed Direct Include of etharp.h**: Replaced with forward declaration to break circular dependency.

**Rationale**: Breaks circular dependencies and exposes RFC 5227 callback function.

---

#### `lwip/src/core/ipv4/acd.c`

**File Path**: `C:\Users\agswe\esp\v5.5.1\esp-idf\components\lwip\lwip\src\core\ipv4\acd.c`

**Changes Made**:

1. **Added Includes** (around line 73-78):
   ```c
   #include "lwip/etharp.h"  /* Need full definition of struct etharp_hdr for acd_arp_reply */
   #if LWIP_ACD_RFC5227_COMPLIANT_STATIC
   #include "lwip/netif_pending_ip.h"
   #include "lwip/netif.h"
   #include "lwip/mem.h"
   #endif
   ```

2. **Implemented RFC 5227 Callback Function** (around line 112-181):
   - `acd_static_ip_rfc5227_callback()` - Assigns IP only after `ACD_IP_OK`, removes IP on conflict
   - Made function non-static and declared in `acd.h`

3. **Modified Conflict Handling** (in `acd_handle_arp_conflict()`):
   - Added RFC 5227 compliant conflict detection
   - Removes IP address when conflict detected for static IPs
   - Calls user callback with conflict state

**Rationale**: Implements RFC 5227 compliant behavior - IP assigned only after ACD confirms safety, removed on conflict.

---

#### `lwip/src/core/netif.c`

**File Path**: `C:\Users\agswe\esp\v5.5.1\esp-idf\components\lwip\lwip\src\core\netif.c`

**Changes Made**:

1. **Added Includes** (around line 70-75):
   ```c
   #if LWIP_ACD
   #include "lwip/acd.h"
   #if LWIP_ACD_RFC5227_COMPLIANT_STATIC
   #include "lwip/netif_pending_ip.h"
   #endif
   #endif /* LWIP_ACD */
   ```

2. **Implemented `netif_set_addr_with_acd()` Function** (around line 500-600):
   - New API function for RFC 5227 compliant static IP assignment
   - Starts ACD BEFORE assigning IP (RFC 5227 compliant)
   - Defers IP assignment until `ACD_IP_OK` callback

3. **Added Cleanup in `netif_remove()`** (around line 200-400):
   ```c
   #if LWIP_ACD && LWIP_ACD_RFC5227_COMPLIANT_STATIC
     /* Clean up pending IP configuration if present */
     if (netif->pending_ip_config != NULL) {
       acd_remove(netif, &netif->pending_ip_config->acd);
       mem_free(netif->pending_ip_config);
       netif->pending_ip_config = NULL;
     }
   #endif
   ```

**Rationale**: Provides RFC 5227 compliant API and ensures proper cleanup of pending configurations.

---

#### `lwip/src/include/lwip/prot/acd.h`

**File Path**: `C:\Users\agswe\esp\v5.5.1\esp-idf\components\lwip\lwip\src\include\lwip\prot\acd.h`

**Changes Made**:

1. **Made ACD Timing Constants Configurable** (around line 45-90):
   - Wrapped all timing constants with `#ifndef` guards
   - Allows override in `lwipopts.h` for protocol-specific requirements (e.g., EtherNet/IP)
   - Constants made configurable:
     - `PROBE_WAIT` (default: 1 second)
     - `PROBE_MIN` (default: 1 second)
     - `PROBE_MAX` (default: 2 seconds)
     - `PROBE_NUM` (default: 3 packets)
     - `ANNOUNCE_NUM` (default: 2 packets)
     - `ANNOUNCE_INTERVAL` (default: 2 seconds)
     - `ANNOUNCE_WAIT` (default: 2 seconds)
     - `DEFEND_INTERVAL` (default: 10 seconds)

**Rationale**: Enables EtherNet/IP and other protocols to override RFC 5227 default timings as needed.

---

#### `port/include/lwipopts.h`

**File Path**: `C:\Users\agswe\esp\v5.5.1\esp-idf\components\lwip\port\include\lwipopts.h`

**Changes Made**:

1. **Added Documentation for ACD Timing Override** (around line 310-324):
   ```c
   /**
    * Note: ACD timing constants (PROBE_WAIT, PROBE_MIN, PROBE_MAX, PROBE_NUM,
    * ANNOUNCE_NUM, ANNOUNCE_INTERVAL, ANNOUNCE_WAIT, DEFEND_INTERVAL) can be
    * overridden in lwipopts.h for protocol-specific requirements. For example,
    * EtherNet/IP uses different timings than RFC 5227. Define these constants
    * before including this file to override the defaults.
    */
   ```

**Rationale**: Documents how to override ACD timings for protocol-specific requirements.

---

## Configuration Changes (sdkconfig)

### Performance Optimizations

#### Socket and Connection Limits

| Configuration | Default | Modified | Rationale |
|--------------|---------|----------|-----------|
| `CONFIG_LWIP_MAX_SOCKETS` | 32 | **64** | Increased for EtherNet/IP multiple connections |
| `CONFIG_LWIP_MAX_ACTIVE_TCP` | 32 | **64** | Support more concurrent TCP connections |
| `CONFIG_LWIP_MAX_LISTENING_TCP` | 16 | **32** | More listening sockets for EtherNet/IP services |
| `CONFIG_LWIP_MAX_UDP_PCBS` | 64 | **128** | Increased UDP support for EtherNet/IP messaging |

#### Buffer and Reassembly Settings

| Configuration | Default | Modified | Rationale |
|--------------|---------|----------|-----------|
| `CONFIG_LWIP_IP_REASS_MAX_PBUFS` | 10 | **20** | More IP fragment reassembly buffers |
| `CONFIG_LWIP_LOOPBACK_MAX_PBUFS` | 8 | **16** | Increased loopback buffer capacity |

#### TCP Buffer Sizes

| Configuration | Default | Modified | Rationale |
|--------------|---------|----------|-----------|
| `CONFIG_LWIP_TCP_SND_BUF_DEFAULT` | 16384 | **32768** | Larger send buffer for better throughput |
| `CONFIG_LWIP_TCP_WND_DEFAULT` | 16384 | **32768** | Larger receive window for better performance |

#### Mailbox Sizes

| Configuration | Default | Modified | Rationale |
|--------------|---------|----------|-----------|
| `CONFIG_LWIP_TCPIP_RECVMBOX_SIZE` | 32 | **64** | Larger TCP/IP task mailbox for message handling |
| `CONFIG_LWIP_TCP_RECVMBOX_SIZE` | 32 | **64** | TCP receive mailbox for better concurrency |
| `CONFIG_LWIP_TCP_ACCEPTMBOX_SIZE` | 6 | **16** | More pending TCP accept connections |
| `CONFIG_LWIP_UDP_RECVMBOX_SIZE` | 128 | **128** | Maintained (already at reasonable level) |
| `CONFIG_LWIP_TCP_OOSEQ_MAX_PBUFS` | 4 | **12** | More out-of-sequence TCP packet buffers |

#### Task Configuration

| Configuration | Default | Modified | Rationale |
|--------------|---------|----------|-----------|
| `CONFIG_LWIP_TCPIP_TASK_STACK_SIZE` | 3072 → 6144 | **8192** | Increased stack for OpENer and ACD processing |
| `CONFIG_LWIP_TCPIP_TASK_PRIO` | 18 | **12** | Adjusted priority for EtherNet/IP requirements |

### Task Affinity Configuration

| Configuration | Default | Modified | Rationale |
|--------------|---------|----------|-----------|
| `CONFIG_LWIP_TCPIP_TASK_AFFINITY_NO_AFFINITY` | y | **n** | Disabled no-affinity mode |
| `CONFIG_LWIP_TCPIP_TASK_AFFINITY_CPU0` | n | **y** | Pin LWIP TCP/IP task to Core 0 |
| `CONFIG_LWIP_TCPIP_TASK_AFFINITY` | 0x7FFFFFFF | **0x0** | Explicit Core 0 affinity |

**Rationale**: Pins LWIP TCP/IP task to Core 0, leaving Core 1 available for other hardware interfaces and OpENer processing.

### IRAM Optimization

| Configuration | Default | Modified | Rationale |
|--------------|---------|----------|-----------|
| `CONFIG_LWIP_IRAM_OPTIMIZATION` | n | **y** | Places RX/TX functions in IRAM (~10KB) |
| `CONFIG_LWIP_EXTRA_IRAM_OPTIMIZATION` | n | **y** | Places TCP functions in IRAM (~17KB) |

**Total IRAM Usage**: ~27KB for LWIP optimizations

**Rationale**: Improves network performance by avoiding flash cache misses, especially important for real-time EtherNet/IP communication.

### ACD and DHCP Configuration

| Configuration | Default | Modified | Rationale |
|--------------|---------|----------|-----------|
| `CONFIG_LWIP_DHCP_DOES_ACD_CHECK` | n | **y** | Enable ACD checking for DHCP-assigned addresses |
| `CONFIG_LWIP_AUTOIP` | y | **y** | Required for ACD support |
| `CONFIG_LWIP_AUTOIP_MAX_CONFLICTS` | 10 | **9** | Slightly reduced for faster conflict detection |

### Other Configuration

| Configuration | Default | Modified | Rationale |
|--------------|---------|----------|-----------|
| `CONFIG_LWIP_SO_REUSE` | n | **y** | Allow socket reuse for EtherNet/IP |
| `CONFIG_LWIP_SO_REUSE_RXTOALL` | n | **y** | Receive broadcast/multicast on all sockets |
| `CONFIG_LWIP_STATS` | n | **y** | Enable statistics for debugging |
| `CONFIG_LWIP_ESP_GRATUITOUS_ARP` | n | **y** | Send gratuitous ARP for better network compatibility |
| `CONFIG_LWIP_NETIF_LOOPBACK` | y | **y** | Enable loopback interface |

---

## Build Configuration Changes

### CMakeLists.txt

**File Path**: `CMakeLists.txt` (project root)

**Changes Made**:

1. **Added FD_SETSIZE Definition** (around line 30-34):
   ```cmake
   # Define FD_SETSIZE for LWIP_MAX_SOCKETS=64
   # LWIP_SOCKET_OFFSET = FD_SETSIZE - MAX_SOCKETS, and we need LWIP_SOCKET_OFFSET >= 6
   # So FD_SETSIZE >= MAX_SOCKETS + 6 = 64 + 6 = 70
   # Also need room for stdout, stderr, stdin (+3), so use 73 for safety
   add_compile_definitions(FD_SETSIZE=73)
   ```

**Rationale**: Required when `LWIP_MAX_SOCKETS > 61`. Ensures sufficient file descriptor space for sockets and console I/O. Without this, build fails with static assertion error.

**Impact**: 
- `LWIP_SOCKET_OFFSET = 73 - 64 = 9` (meets >= 6 requirement)
- File descriptors 0-8 reserved for non-socket descriptors
- File descriptors 9-72 available for LWIP sockets

---

## ACD Timing Configuration

### Default RFC 5227 Timings

All ACD timing constants are now configurable via `lwipopts.h`. Default values (RFC 5227 compliant):

| Constant | Value | Unit | Description |
|----------|-------|------|-------------|
| `PROBE_WAIT` | 1 | second | Initial random delay before probing |
| `PROBE_MIN` | 1 | second | Minimum delay between probe packets |
| `PROBE_MAX` | 2 | seconds | Maximum delay between probe packets |
| `PROBE_NUM` | 3 | packets | Number of probe packets to send |
| `ANNOUNCE_WAIT` | 2 | seconds | Delay before announcing |
| `ANNOUNCE_NUM` | 2 | packets | Number of announcement packets |
| `ANNOUNCE_INTERVAL` | 2 | seconds | Time between announcement packets |
| `DEFEND_INTERVAL` | 10 | seconds | Minimum interval between defensive ARPs |

### Override Example for EtherNet/IP

To override timings in `lwipopts.h`:

```c
/* EtherNet/IP specific ACD timings */
#define PROBE_WAIT           0   /* Override RFC 5227 default */
#define PROBE_MIN            0
#define PROBE_MAX            1
#define PROBE_NUM            2
/* ... etc */
```

**Rationale**: EtherNet/IP may require different timing values than RFC 5227 defaults. Making these configurable allows protocol-specific optimization without modifying core LWIP code.

---

## Task Affinity Configuration

### LWIP TCP/IP Task

**Configuration**: Pinned to Core 0

**Settings**:
- `CONFIG_LWIP_TCPIP_TASK_AFFINITY_CPU0=y`
- `CONFIG_LWIP_TCPIP_TASK_AFFINITY=0x0`

**Rationale**: 
- Centralizes network processing on one core
- Reduces inter-core communication overhead
- Leaves Core 1 available for other tasks

### OpENer Task

**Configuration**: Pinned to Core 0 (via code modification)

**File**: `components/opener/src/ports/ESP32/opener.c`

**Change**: Modified task creation to use `xTaskCreatePinnedToCore()`:

```c
BaseType_t result = xTaskCreatePinnedToCore(opener_thread,
                                             "OpENer",
                                             OPENER_STACK_SIZE,
                                             netif,
                                             OPENER_THREAD_PRIO,
                                             &opener_task_handle,
                                             0);  // Core 0
```

**Rationale**: 
- Keeps EtherNet/IP stack and OpENer on same core
- Reduces context switching overhead
- Improves cache locality for network operations

---

## Summary of Changes

### Files Modified in LWIP Source Tree

1. ✅ **NEW**: `lwip/src/include/lwip/netif_pending_ip.h`
2. ✅ **MODIFIED**: `lwip/src/include/lwip/opt.h`
3. ✅ **MODIFIED**: `lwip/src/include/lwip/netif.h`
4. ✅ **MODIFIED**: `lwip/src/include/lwip/acd.h`
5. ✅ **MODIFIED**: `lwip/src/core/ipv4/acd.c`
6. ✅ **MODIFIED**: `lwip/src/core/netif.c`
7. ✅ **MODIFIED**: `lwip/src/include/lwip/prot/acd.h`
8. ✅ **MODIFIED**: `port/include/lwipopts.h`

### Configuration Summary

#### Performance Increases

- **Sockets**: 32 → 64 (+100%)
- **Active TCP**: 32 → 64 (+100%)
- **Listening TCP**: 16 → 32 (+100%)
- **UDP PCBs**: 64 → 128 (+100%)
- **TCP Send Buffer**: 16KB → 32KB (+100%)
- **TCP Window**: 16KB → 32KB (+100%)
- **TCP/IP Stack**: 3072 → 8192 bytes (+167%)

#### Mailbox Increases

- **TCP/IP Recv**: 32 → 64 (+100%)
- **TCP Recv**: 32 → 64 (+100%)
- **TCP Accept**: 6 → 16 (+167%)
- **TCP OOSEQ**: 4 → 12 (+200%)

#### New Features

- ✅ RFC 5227 compliant static IP assignment
- ✅ Configurable ACD timings
- ✅ Task affinity control (Core 0)
- ✅ IRAM optimization enabled

### Build Requirements

- **FD_SETSIZE**: Must be >= 70 when `LWIP_MAX_SOCKETS=64`
- **Current Setting**: `FD_SETSIZE=73` (defined in `CMakeLists.txt`)

### Backward Compatibility

- ✅ All changes are backward compatible
- ✅ RFC 5227 mode can be disabled via `LWIP_ACD_RFC5227_COMPLIANT_STATIC=0`
- ✅ Legacy `netif_set_addr()` still works when RFC 5227 mode disabled
- ✅ ACD timing overrides are optional

---

## Verification

### Build Verification

1. **FD_SETSIZE Check**: Build should complete without static assertion errors
2. **ACD Compilation**: Verify `LWIP_ACD=1` in build output
3. **RFC 5227 Mode**: Verify `LWIP_ACD_RFC5227_COMPLIANT_STATIC=1` in build

### Runtime Verification

1. **Task Affinity**: Check that LWIP and OpENer tasks run on Core 0
2. **ACD Functionality**: Test static IP assignment with ACD enabled
3. **Performance**: Monitor network throughput and latency

### Configuration Verification

```bash
# Check sdkconfig values
grep "CONFIG_LWIP" sdkconfig | grep -E "(MAX_|SIZE|STACK)"

# Check build configuration
grep "LWIP_ACD" build/config/sdkconfig.h
grep "FD_SETSIZE" build/config/sdkconfig.h
```

---

## References

- **RFC 5227**: IPv4 Address Conflict Detection - https://tools.ietf.org/html/rfc5227
- **Implementation Guide**: `dependency_modifications/lwIP/RFC5227_IMPLEMENTATION_GUIDE.md`
- **ACD Testing Guide**: `ReadmeACD.md`
- **ACD Static IP Issue**: `dependency_modifications/lwIP/acd-static-ip-issue.md`
- **RFC 5227 Requirements**: `dependency_modifications/Analysis_of_ACD_Issue/RFC5227_COMPLIANCE_REQUIREMENTS.md`

---

## Maintenance Notes

### Upgrading ESP-IDF

When upgrading ESP-IDF, these modifications will need to be reapplied:

1. **Source Code Changes**: All files in `components/lwip/` directory
2. **Configuration**: `sdkconfig` file (can be merged/updated)
3. **Build Config**: `CMakeLists.txt` (FD_SETSIZE definition)

### Recommended Approach

1. Keep a patch file or script to reapply LWIP modifications
2. Document any ESP-IDF version-specific changes
3. Test thoroughly after ESP-IDF upgrades

---

**Document Version**: 1.0  
**Last Updated**: 2024  
**Project**: ESP32-P4 OpENer EtherNet/IP Stack

