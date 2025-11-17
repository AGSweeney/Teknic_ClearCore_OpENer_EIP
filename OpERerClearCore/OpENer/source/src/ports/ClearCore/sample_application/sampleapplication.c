/*******************************************************************************
 * Copyright (c) 2012, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "opener_api.h"
#include "appcontype.h"
#include "trace.h"
#include "cipidentity.h"
#include "ciptcpipinterface.h"
#include "cipqos.h"
#include "ports/ClearCore/clearcore_wrapper.h"
#include "cipconnectionmanager.h"
#include "cipethernetlink.h"
#include "ports/ClearCore/sample_application/ethlinkcbs.h"

#define DEMO_APP_INPUT_ASSEMBLY_NUM                100
#define DEMO_APP_OUTPUT_ASSEMBLY_NUM               150
#define DEMO_APP_CONFIG_ASSEMBLY_NUM               151

EipUint8 g_assembly_data064[32];
EipUint8 g_assembly_data096[32];
EipUint8 g_assembly_data097[10];

EipStatus ApplicationInitialization(void) {
  CipRunIdleHeaderSetO2T(true);
  CipRunIdleHeaderSetT2O(false);
  OPENER_TRACE_INFO("ApplicationInitialization: Initializing hardware...\n");
  ConnectorIO0_Initialize();
  ConnectorIO1_Initialize();
  ConnectorIO2_Initialize();
  ConnectorIO3_Initialize();
  ConnectorIO4_Initialize();
  ConnectorIO5_Initialize();
  OPENER_TRACE_INFO("ApplicationInitialization: IO-0 through IO-5 initialized as digital outputs\n");
  ConnectorDI6_Initialize();
  ConnectorDI7_Initialize();
  ConnectorDI8_Initialize();
  OPENER_TRACE_INFO("ApplicationInitialization: DI-6, DI-7, DI-8 initialized as digital inputs\n");
  ConnectorA9_Initialize();
  ConnectorA10_Initialize();
  ConnectorA11_Initialize();
  ConnectorA12_Initialize();
  OPENER_TRACE_INFO("ApplicationInitialization: A-9, A-10, A-11, A-12 initialized as digital inputs\n");
  
  OPENER_TRACE_INFO("ApplicationInitialization: Creating assembly objects...\n");
  
  CreateAssemblyObject( DEMO_APP_INPUT_ASSEMBLY_NUM, g_assembly_data064,
                       sizeof(g_assembly_data064));
  OPENER_TRACE_INFO("ApplicationInitialization: Created input assembly %d\n", DEMO_APP_INPUT_ASSEMBLY_NUM);

  CreateAssemblyObject( DEMO_APP_OUTPUT_ASSEMBLY_NUM, g_assembly_data096,
                       sizeof(g_assembly_data096));
  OPENER_TRACE_INFO("ApplicationInitialization: Created output assembly %d\n", DEMO_APP_OUTPUT_ASSEMBLY_NUM);

  CreateAssemblyObject( DEMO_APP_CONFIG_ASSEMBLY_NUM, g_assembly_data097,
                       sizeof(g_assembly_data097));
  OPENER_TRACE_INFO("ApplicationInitialization: Created config assembly %d\n", DEMO_APP_CONFIG_ASSEMBLY_NUM);

  OPENER_TRACE_INFO("ApplicationInitialization: Configuring connection points...\n");
  
  ConfigureExclusiveOwnerConnectionPoint(0, DEMO_APP_OUTPUT_ASSEMBLY_NUM,
  DEMO_APP_INPUT_ASSEMBLY_NUM,
                                         DEMO_APP_CONFIG_ASSEMBLY_NUM);
  OPENER_TRACE_INFO("ApplicationInitialization: Configured exclusive owner connection point\n");

#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
  {
    CipClass *p_eth_link_class = GetCipClass(kCipEthernetLinkClassCode);
    InsertGetSetCallback(p_eth_link_class,
                         EthLnkPreGetCallback,
                         kPreGetFunc);
    InsertGetSetCallback(p_eth_link_class,
                         EthLnkPostGetCallback,
                         kPostGetFunc);
    for (int idx = 0; idx < OPENER_ETHLINK_INSTANCE_CNT; ++idx)
    {
      CipAttributeStruct *p_eth_link_attr;
      CipInstance *p_eth_link_inst =
        GetCipInstance(p_eth_link_class, idx + 1);
      OPENER_ASSERT(p_eth_link_inst);

      p_eth_link_attr = GetCipAttribute(p_eth_link_inst, 4);
      p_eth_link_attr->attribute_flags |= (kPreGetFunc | kPostGetFunc);
      p_eth_link_attr = GetCipAttribute(p_eth_link_inst, 5);
      p_eth_link_attr->attribute_flags |= (kPreGetFunc | kPostGetFunc);
    }
  }
#endif

  OPENER_TRACE_INFO("ApplicationInitialization: Sample application setup complete\n");
  return kEipStatusOk;
}

void HandleApplication(void) {
}

void CheckIoConnectionEvent(unsigned int output_assembly_id,
                            unsigned int input_assembly_id,
                            IoConnectionEvent io_connection_event) {

  (void) output_assembly_id;
  (void) input_assembly_id;
  (void) io_connection_event;
}

EipStatus AfterAssemblyDataReceived(CipInstance *instance) {
  EipStatus status = kEipStatusOk;

  switch (instance->instance_number) {
    case DEMO_APP_OUTPUT_ASSEMBLY_NUM:
      memcpy(&g_assembly_data064[0], &g_assembly_data096[0],
             sizeof(g_assembly_data064));
      if (g_assembly_data096[0] & 0x01) {
        ConnectorIO0_SetState(1);
        OPENER_TRACE_INFO("AfterAssemblyDataReceived: IO-0 ON (bit 0 set)\n");
      } else {
        ConnectorIO0_SetState(0);
      }
      if (g_assembly_data096[0] & 0x02) {
        ConnectorIO1_SetState(1);
        OPENER_TRACE_INFO("AfterAssemblyDataReceived: IO-1 ON (bit 1 set)\n");
      } else {
        ConnectorIO1_SetState(0);
      }
      if (g_assembly_data096[0] & 0x04) {
        ConnectorIO2_SetState(1);
        OPENER_TRACE_INFO("AfterAssemblyDataReceived: IO-2 ON (bit 2 set)\n");
      } else {
        ConnectorIO2_SetState(0);
      }
      if (g_assembly_data096[0] & 0x08) {
        ConnectorIO3_SetState(1);
        OPENER_TRACE_INFO("AfterAssemblyDataReceived: IO-3 ON (bit 3 set)\n");
      } else {
        ConnectorIO3_SetState(0);
      }
      if (g_assembly_data096[0] & 0x10) {
        ConnectorIO4_SetState(1);
        OPENER_TRACE_INFO("AfterAssemblyDataReceived: IO-4 ON (bit 4 set)\n");
      } else {
        ConnectorIO4_SetState(0);
      }
      if (g_assembly_data096[0] & 0x20) {
        ConnectorIO5_SetState(1);
        OPENER_TRACE_INFO("AfterAssemblyDataReceived: IO-5 ON (bit 5 set)\n");
      } else {
        ConnectorIO5_SetState(0);
      }
      break;
    case DEMO_APP_CONFIG_ASSEMBLY_NUM:
      status = kEipStatusOk;
      break;
    default:
      OPENER_TRACE_INFO(
          "Unknown assembly instance ind AfterAssemblyDataReceived");
      break;
  }
  return status;
}

EipBool8 BeforeAssemblyDataSend(CipInstance *pa_pstInstance) {
  if (pa_pstInstance->instance_number == DEMO_APP_INPUT_ASSEMBLY_NUM) {
    g_assembly_data064[0] = 0;
    if (ConnectorDI6_GetState()) {
      g_assembly_data064[0] |= 0x01;
    }
    if (ConnectorDI7_GetState()) {
      g_assembly_data064[0] |= 0x02;
    }
    if (ConnectorDI8_GetState()) {
      g_assembly_data064[0] |= 0x04;
    }
    if (ConnectorA9_GetState()) {
      g_assembly_data064[0] |= 0x08;
    }
    if (ConnectorA10_GetState()) {
      g_assembly_data064[0] |= 0x10;
    }
    if (ConnectorA11_GetState()) {
      g_assembly_data064[0] |= 0x20;
    }
    if (ConnectorA12_GetState()) {
      g_assembly_data064[0] |= 0x40;
    }
  }
  return true;
}

EipStatus ResetDevice(void) {
  OPENER_TRACE_INFO("ResetDevice: Closing connections and updating QoS...\n");
  CloseAllConnections();
  CipQosUpdateUsedSetQosValues();
  OPENER_TRACE_INFO("ResetDevice: Rebooting device...\n");
  ClearCoreRebootDevice();
  return kEipStatusOk;
}

EipStatus ResetDeviceToInitialConfiguration(void) {
  OPENER_TRACE_INFO("ResetDeviceToInitialConfiguration: Resetting to factory defaults...\n");
  g_tcpip.encapsulation_inactivity_timeout = 120;
  CipQosResetAttributesToDefaultValues();
  OPENER_TRACE_INFO("ResetDeviceToInitialConfiguration: Clearing NVRAM...\n");
  ClearCoreClearNvram();
  OPENER_TRACE_INFO("ResetDeviceToInitialConfiguration: Rebooting device...\n");
  ClearCoreRebootDevice();
  return kEipStatusOk;
}

void*
CipCalloc(size_t number_of_elements,
          size_t size_of_element) {
  return calloc(number_of_elements, size_of_element);
}

void CipFree(void *data) {
  free(data);
}

void RunIdleChanged(EipUint32 run_idle_value) {
  OPENER_TRACE_INFO("Run/Idle handler triggered\n");
  if ((0x0001 & run_idle_value) == 1) {
    CipIdentitySetExtendedDeviceStatus(kAtLeastOneIoConnectionInRunMode);
  } else {
    CipIdentitySetExtendedDeviceStatus(
        kAtLeastOneIoConnectionEstablishedAllInIdleMode);
  }
  (void) run_idle_value;
}

