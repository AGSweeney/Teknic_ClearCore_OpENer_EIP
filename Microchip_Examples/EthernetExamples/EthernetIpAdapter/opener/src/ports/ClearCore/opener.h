#ifndef OPENER_CLEARCORE_H_
#define OPENER_CLEARCORE_H_

#include "lwip/netif.h"

void opener_init(struct netif *netif);
void opener_process(void);

#endif

