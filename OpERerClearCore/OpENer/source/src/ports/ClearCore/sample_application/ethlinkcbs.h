/******************************************************************************
 * Copyright (c) 2019, Rockwell Automation, Inc.
 * All rights reserved.
 *
 *****************************************************************************/
#ifndef OPENER_ETHLINKCBS_H_
#define OPENER_ETHLINKCBS_H_

#include "typedefs.h"
#include "ciptypes.h"

EipStatus EthLnkPreGetCallback
(
    CipInstance *const instance,
    CipAttributeStruct *const attribute,
    CipByte service
);

EipStatus EthLnkPostGetCallback
(
    CipInstance *const instance,
    CipAttributeStruct *const attribute,
    CipByte service
);

#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
#ifdef CLEARCORE
void ClearCoreUpdateInStats(CipUdint bytes, bool is_broadcast);
void ClearCoreUpdateOutStats(CipUdint bytes, bool is_broadcast);
void ClearCoreIncrementInErrors(void);
void ClearCoreIncrementOutErrors(void);
#endif
#endif

#endif

