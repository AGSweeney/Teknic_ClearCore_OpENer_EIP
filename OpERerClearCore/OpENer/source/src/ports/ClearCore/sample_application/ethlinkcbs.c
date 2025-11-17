/******************************************************************************
 * Copyright (c) 2019, Rockwell Automation, Inc.
 * All rights reserved.
 *
 *****************************************************************************/

#include "ethlinkcbs.h"

#include "cipethernetlink.h"
#include "trace.h"
#include "lwip/netif.h"
#include "lwip/snmp.h"
#include "ports/ClearCore/opener.h"

#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE

typedef struct {
  CipUdint in_octets;
  CipUdint in_ucast;
  CipUdint in_nucast;
  CipUdint in_discards;
  CipUdint in_errors;
  CipUdint in_unknown_protos;
  CipUdint out_octets;
  CipUdint out_ucast;
  CipUdint out_nucast;
  CipUdint out_discards;
  CipUdint out_errors;
} ClearCoreInterfaceCounters;

static ClearCoreInterfaceCounters g_interface_counters = {0};

void ClearCoreUpdateInStats(CipUdint bytes, bool is_broadcast) {
  g_interface_counters.in_octets += bytes;
  if (is_broadcast) {
    g_interface_counters.in_nucast++;
  } else {
    g_interface_counters.in_ucast++;
  }
}

void ClearCoreUpdateOutStats(CipUdint bytes, bool is_broadcast) {
  g_interface_counters.out_octets += bytes;
  if (is_broadcast) {
    g_interface_counters.out_nucast++;
  } else {
    g_interface_counters.out_ucast++;
  }
}

void ClearCoreIncrementInErrors(void) {
  g_interface_counters.in_errors++;
}

void ClearCoreIncrementOutErrors(void) {
  g_interface_counters.out_errors++;
}

EipStatus EthLnkPreGetCallback
(
  CipInstance *const instance,
  CipAttributeStruct *const attribute,
  CipByte service
)
{
  bool hadAction = true;
  EipStatus status = kEipStatusOk;

  CipUint attr_no = attribute->attribute_number;
  CipUdint inst_no  = instance->instance_number;
  unsigned idx = inst_no-1;
  switch (attr_no) {
  case 4: {
    CipEthernetLinkInterfaceCounters *p_iface_cntrs = &g_ethernet_link[idx].interface_cntrs;

    p_iface_cntrs->ul.in_octets = g_interface_counters.in_octets;
    p_iface_cntrs->ul.in_ucast = g_interface_counters.in_ucast;
    p_iface_cntrs->ul.in_nucast = g_interface_counters.in_nucast;
    p_iface_cntrs->ul.in_discards = g_interface_counters.in_discards;
    p_iface_cntrs->ul.in_errors = g_interface_counters.in_errors;
    p_iface_cntrs->ul.in_unknown_protos = g_interface_counters.in_unknown_protos;
    p_iface_cntrs->ul.out_octets = g_interface_counters.out_octets;
    p_iface_cntrs->ul.out_ucast = g_interface_counters.out_ucast;
    p_iface_cntrs->ul.out_nucast = g_interface_counters.out_nucast;
    p_iface_cntrs->ul.out_discards = g_interface_counters.out_discards;
    p_iface_cntrs->ul.out_errors = g_interface_counters.out_errors;
    break;
  }
  case 5: {
    CipEthernetLinkMediaCounters *p_media_cntrs = &g_ethernet_link[idx].media_cntrs;

    p_media_cntrs->ul.align_errs = 0;
    p_media_cntrs->ul.fcs_errs = 0;
    p_media_cntrs->ul.single_coll = 0;
    p_media_cntrs->ul.multi_coll = 0;
    p_media_cntrs->ul.sqe_test_errs = 0;
    p_media_cntrs->ul.def_trans = 0;
    p_media_cntrs->ul.late_coll = 0;
    p_media_cntrs->ul.exc_coll = 0;
    p_media_cntrs->ul.mac_tx_errs = 0;
    p_media_cntrs->ul.crs_errs = 0;
    p_media_cntrs->ul.frame_too_long = 0;
    p_media_cntrs->ul.mac_rx_errs = 0;
    break;
  }
  default:
    hadAction = false;
    break;
  }

  if (hadAction) {
    OPENER_TRACE_INFO(
      "Eth Link PreCallback: %s, i %" PRIu32 ", a %" PRIu16 ", s %" PRIu8 "\n",
      instance->cip_class->class_name,
      instance->instance_number,
      attribute->attribute_number,
      service);
  }
  return status;
}

EipStatus EthLnkPostGetCallback
(
  CipInstance *const instance,
  CipAttributeStruct *const attribute,
  CipByte service
)
{
  CipUdint  inst_no = instance->instance_number;
  EipStatus status = kEipStatusOk;

  if (kEthLinkGetAndClear == (service & 0x7f)) {
    OPENER_TRACE_INFO(
      "Eth Link PostCallback: %s, i %" PRIu32 ", a %" PRIu16 ", s %" PRIu8 "\n",
      instance->cip_class->class_name,
      inst_no,
      attribute->attribute_number,
      service);
    switch (attribute->attribute_number) {
    case 4:
      g_interface_counters.in_octets = 0;
      g_interface_counters.in_ucast = 0;
      g_interface_counters.in_nucast = 0;
      g_interface_counters.in_discards = 0;
      g_interface_counters.in_errors = 0;
      g_interface_counters.in_unknown_protos = 0;
      g_interface_counters.out_octets = 0;
      g_interface_counters.out_ucast = 0;
      g_interface_counters.out_nucast = 0;
      g_interface_counters.out_discards = 0;
      g_interface_counters.out_errors = 0;
      break;
    case 5:
      for (int idx = 0; idx < 12; ++idx) {
        g_ethernet_link[inst_no-1].media_cntrs.cntr32[idx] = 0U;
      }
      break;
    default:
      OPENER_TRACE_INFO(
        "Wrong attribute number %" PRIu16 " in GetAndClear callback\n",
        attribute->attribute_number);
      break;
    }
  }
  return status;
}
#endif

