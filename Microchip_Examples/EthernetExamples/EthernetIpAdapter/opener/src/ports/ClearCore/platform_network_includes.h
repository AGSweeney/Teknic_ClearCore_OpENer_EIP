#ifndef PLATFORM_NETWORK_INCLUDES_H_
#define PLATFORM_NETWORK_INCLUDES_H_

#include "lwip/netif.h"
#if defined(CLEARCORE)
#include "lwip/ip_addr.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#else
#include "lwip/sockets.h"
#endif
#include "lwip/errno.h"

#endif

