#ifndef OPENER_USER_CONF_H_
#define OPENER_USER_CONF_H_

#include <assert.h>
#include "typedefs.h"
#include "lwip/opt.h"
#include "lwip/arch.h"

#if defined(CLEARCORE)
/* For ClearCore, ensure socket structures are fully defined before api.h */
#include "lwip/ip_addr.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#endif

#include "lwip/api.h"

#if !defined(CLEARCORE)
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#endif

#ifndef RESTRICT
#define RESTRICT
#endif

#ifndef CIP_FILE_OBJECT
#define CIP_FILE_OBJECT 0
#endif

#ifndef CIP_SECURITY_OBJECTS
#define CIP_SECURITY_OBJECTS 0
#endif

#ifndef OPENER_IS_DLR_DEVICE
#define OPENER_IS_DLR_DEVICE  0
#endif

#define OPENER_TCPIP_IFACE_CFG_SETTABLE 1
#define OPENER_ETHLINK_INSTANCE_CNT  1
#define OPENER_ETHLINK_LABEL_ENABLE  0
#define OPENER_ETHLINK_CNTRS_ENABLE 1
#define OPENER_ETHLINK_IFACE_CTRL_ENABLE 0

#define OPENER_CIP_NUM_APPLICATION_SPECIFIC_CONNECTABLE_OBJECTS 1
#define OPENER_CIP_NUM_EXPLICIT_CONNS 6
#define OPENER_CIP_NUM_EXLUSIVE_OWNER_CONNS 1
#define OPENER_CIP_NUM_INPUT_ONLY_CONNS 1
#define OPENER_CIP_NUM_INPUT_ONLY_CONNS_PER_CON_PATH 3
#define OPENER_CIP_NUM_LISTEN_ONLY_CONNS 1
#define OPENER_CIP_NUM_LISTEN_ONLY_CONNS_PER_CON_PATH 3

#define OPENER_NUMBER_OF_SUPPORTED_SESSIONS 20
#define PC_OPENER_ETHERNET_BUFFER_SIZE 512

static const MilliSeconds kOpenerTimerTickInMilliSeconds = 10;

#define OPENER_WITH_TRACES
#define OPENER_TRACE_LEVEL (OPENER_TRACE_LEVEL_ERROR | OPENER_TRACE_LEVEL_WARNING)

#include <stdio.h>
#include <string.h>

// LOG_TRACE definition: For C files, it's a no-op. 
// C++ files that include ClearCore.h will redefine it.
// Note: ClearCore port C++ files (opener.c, networkhandler.c, etc.) 
// include ClearCore.h before opener_user_conf.h and redefine LOG_TRACE after.
#ifndef LOG_TRACE
// Default: no-op for C files or when ClearCore.h hasn't been included yet
#define LOG_TRACE(...) ((void)0)
#endif

#define OPENER_ASSERT(assertion) assert(assertion)

#endif

