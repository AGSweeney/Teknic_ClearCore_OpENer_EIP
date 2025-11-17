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
#include "cipstring.h"
#include "ciptypes.h"
#include "typedefs.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvtcpip.h"
#include "cipethernetlink.h"
#include "generic_networkhandler.h"
#include "vl53l1x.h"
#include "VL53L1X_api.h"
#include "sdkconfig.h"
#include "system_config.h"

struct netif;

static void ScheduleRestart(void);
static void RestoreTcpIpDefaults(void);
static void ConfigureStatusLed(void);

#define DEMO_APP_INPUT_ASSEMBLY_NUM                100
#define DEMO_APP_OUTPUT_ASSEMBLY_NUM               150
#define DEMO_APP_CONFIG_ASSEMBLY_NUM               151
EipUint8 g_assembly_data064[32];
EipUint8 g_assembly_data096[32];
EipUint8 g_assembly_data097[10];

static const gpio_num_t kStatusLedGpio = GPIO_NUM_33;
static bool restart_pending = false;
static EipUint32 s_active_io_connections = 0;
static bool s_io_activity_seen = false;

/* Mutexes for thread-safe access */
static SemaphoreHandle_t s_assembly_mutex = NULL;  // Protects assembly data arrays
static SemaphoreHandle_t s_sensor_state_mutex = NULL;  // Protects sensor state variables

/* VL53L1x sensor handles */
static vl53l1x_handle_t s_vl53l1x_handle = VL53L1X_INIT;
static vl53l1x_device_handle_t s_vl53l1x_device = VL53L1X_DEVICE_INIT;
static TaskHandle_t s_vl53l1x_task_handle = NULL;
static bool s_sensor_enabled = true;  // Track sensor enabled state
static uint8_t s_sensor_start_byte = 0;  // Track sensor data start byte offset

// Export device handle for webui_api.c
void *g_vl53l1x_device_handle = NULL;

// Export mutex for webui_api.c and modbus_register_map.c
SemaphoreHandle_t sample_application_get_assembly_mutex(void)
{
    return s_assembly_mutex;
}

// Function to set sensor enabled state (called from API)
void sample_application_set_sensor_enabled(bool enabled)
{
    if (s_sensor_state_mutex == NULL) {
        s_sensor_state_mutex = xSemaphoreCreateMutex();
        if (s_sensor_state_mutex == NULL) {
            OPENER_TRACE_ERR("Failed to create sensor state mutex\n");
            return;
        }
    }
    
    xSemaphoreTake(s_sensor_state_mutex, portMAX_DELAY);
    s_sensor_enabled = enabled;
    uint8_t current_offset = s_sensor_start_byte;  // Copy while holding mutex
    xSemaphoreGive(s_sensor_state_mutex);
    
    if (!enabled) {
        // Zero out configured byte range when disabled (need assembly mutex)
        if (s_assembly_mutex != NULL) {
            xSemaphoreTake(s_assembly_mutex, portMAX_DELAY);
            memset(&g_assembly_data064[current_offset], 0, 9);
            xSemaphoreGive(s_assembly_mutex);
        } else {
            memset(&g_assembly_data064[current_offset], 0, 9);
        }
    }
}

// Function to set sensor data start byte offset (called from API)
void sample_application_set_sensor_byte_offset(uint8_t start_byte)
{
    // Validate: must be 0, 9, or 18
    if (start_byte != 0 && start_byte != 9 && start_byte != 18) {
        OPENER_TRACE_ERR("Invalid sensor byte offset: %d (must be 0, 9, or 18)\n", start_byte);
        return;
    }
    
    if (s_sensor_state_mutex == NULL) {
        s_sensor_state_mutex = xSemaphoreCreateMutex();
        if (s_sensor_state_mutex == NULL) {
            OPENER_TRACE_ERR("Failed to create sensor state mutex\n");
            return;
        }
    }
    
    xSemaphoreTake(s_sensor_state_mutex, portMAX_DELAY);
    uint8_t old_offset = s_sensor_start_byte;
    
    // Zero out the old byte range before changing to the new one
    if (s_sensor_start_byte != start_byte) {
        OPENER_TRACE_INFO("Changing sensor byte offset from %d to %d, zeroing old range\n", 
                         s_sensor_start_byte, start_byte);
        xSemaphoreGive(s_sensor_state_mutex);  // Release before assembly access
        
        // Zero old range with assembly mutex
        if (s_assembly_mutex != NULL) {
            xSemaphoreTake(s_assembly_mutex, portMAX_DELAY);
            memset(&g_assembly_data064[old_offset], 0, 9);
            xSemaphoreGive(s_assembly_mutex);
        } else {
            memset(&g_assembly_data064[old_offset], 0, 9);
        }
        
        xSemaphoreTake(s_sensor_state_mutex, portMAX_DELAY);
    }
    
    s_sensor_start_byte = start_byte;
    xSemaphoreGive(s_sensor_state_mutex);
    
    OPENER_TRACE_INFO("Sensor data start byte offset set to %d (bytes %d-%d)\n", 
                     start_byte, start_byte, start_byte + 8);
}

// Function to get sensor data start byte offset (called from API)
uint8_t sample_application_get_sensor_byte_offset(void)
{
    if (s_sensor_state_mutex == NULL) {
        return s_sensor_start_byte;  // Return current value if mutex not initialized
    }
    
    xSemaphoreTake(s_sensor_state_mutex, portMAX_DELAY);
    uint8_t offset = s_sensor_start_byte;
    xSemaphoreGive(s_sensor_state_mutex);
    return offset;
}

static void IdentityEnter(CipIdentityState state,
                          CipIdentityExtendedStatus ext_status) {
  if (g_identity.state != (CipUsint)state) {
    OPENER_TRACE_INFO("Identity state -> %u\n", (unsigned)state);
    g_identity.state = (CipUsint)state;
  }
  CipIdentitySetExtendedDeviceStatus(ext_status);
}

static void IdentityFlagFault(bool fatal) {
  CipWord flag = fatal ? kMajorUnrecoverableFault : kMajorRecoverableFault;
  CipIdentitySetStatusFlags(flag);
  IdentityEnter(fatal ? kStateMajorUnrecoverableFault
                      : kStateMajorRecoverableFault,
                kMajorFault);
}

static void IdentityNoteIoActivity(void) {
  if (s_active_io_connections > 0) {
    s_io_activity_seen = true;
    IdentityEnter(kStateOperational, kAtLeastOneIoConnectionInRunMode);
  }
}

void SampleApplicationNotifyLinkUp(void) {
  CipIdentityClearStatusFlags(kMajorRecoverableFault | kMajorUnrecoverableFault);
  CipEthernetLinkSetInterfaceState(1, kEthLinkInterfaceStateEnabled);
  IdentityEnter(kStateStandby,
                s_active_io_connections > 0 ?
                kAtLeastOneIoConnectionEstablishedAllInIdleMode :
                kNoIoConnectionsEstablished);
}

void SampleApplicationNotifyLinkDown(void) {
  s_active_io_connections = 0;
  CipEthernetLinkSetInterfaceState(1, kEthLinkInterfaceStateDisabled);
  s_io_activity_seen = false;
  IdentityFlagFault(false);
}

void SampleApplicationSetActiveNetif(struct netif *netif) {
  (void)netif;
}

#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
EipStatus EthLnkPreGetCallback(CipInstance *instance,
                               CipAttributeStruct *attribute,
                               CipByte service);
EipStatus EthLnkPostGetCallback(CipInstance *instance,
                                CipAttributeStruct *attribute,
                                CipByte service);
#endif

static void RestartTask(void *param) {
  (void)param;
  vTaskDelay(pdMS_TO_TICKS(100));
  esp_restart();
}

static void ScheduleRestart(void) {
  if (restart_pending) {
    return;
  }
  restart_pending = true;
  xTaskCreate(RestartTask, "restart", 2048, NULL, configMAX_PRIORITIES - 1, NULL);
}

static void RestoreTcpIpDefaults(void) {
  g_tcpip.config_control &= ~kTcpipCfgCtrlMethodMask;
  g_tcpip.config_control |= kTcpipCfgCtrlDhcp;
  g_tcpip.interface_configuration.ip_address = 0;
  g_tcpip.interface_configuration.network_mask = 0;
  g_tcpip.interface_configuration.gateway = 0;
  g_tcpip.interface_configuration.name_server = 0;
  g_tcpip.interface_configuration.name_server_2 = 0;
  ClearCipString(&g_tcpip.interface_configuration.domain_name);
  ClearCipString(&g_tcpip.hostname);
  g_tcpip.status |= kTcpipStatusIfaceCfgPend;
  (void)NvTcpipStore(&g_tcpip);
}

static void ConfigureStatusLed(void) {
  gpio_config_t led_config = {
    .pin_bit_mask = 1ULL << kStatusLedGpio,
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
  };
  gpio_config(&led_config);
  gpio_set_level(kStatusLedGpio, 0);
}

static void vl53l1x_sensor_task(void *pvParameters) {
  (void)pvParameters;
  
  OPENER_TRACE_INFO("VL53L1x sensor task started on Core 1\n");
  
  /* Initialize I2C handle with configured GPIO pins */
  vl53l1x_i2c_handle_t vl53l1x_i2c_handle = VL53L1X_I2C_INIT;
  vl53l1x_i2c_handle.scl_gpio = CONFIG_OPENER_I2C_SCL_GPIO;
  vl53l1x_i2c_handle.sda_gpio = CONFIG_OPENER_I2C_SDA_GPIO;
  
  s_vl53l1x_handle.i2c_handle = &vl53l1x_i2c_handle;
  
  /* Initialize VL53L1x */
  if (!vl53l1x_init(&s_vl53l1x_handle)) {
    OPENER_TRACE_ERR("VL53L1x initialization failed\n");
    vTaskDelete(NULL);
    return;
  }
  OPENER_TRACE_INFO("VL53L1x handle initialized\n");
  
  /* Add device */
  s_vl53l1x_device.vl53l1x_handle = &s_vl53l1x_handle;
  s_vl53l1x_device.i2c_address = VL53L1X_DEFAULT_I2C_ADDRESS;
  if (!vl53l1x_add_device(&s_vl53l1x_device)) {
    OPENER_TRACE_ERR("Failed to add VL53L1x device\n");
    vTaskDelete(NULL);
    return;
  }
  g_vl53l1x_device_handle = &s_vl53l1x_device;
  OPENER_TRACE_INFO("VL53L1x device added successfully\n");
  
  /* Wait a bit for sensor to stabilize */
  vTaskDelay(pdMS_TO_TICKS(100));
  
  /* Main sensor reading loop */
  const TickType_t loop_delay_ticks = pdMS_TO_TICKS(100); /* 10 Hz update rate (100ms) */
  
  while (1) {
    // Read sensor state with mutex protection
    bool sensor_enabled;
    uint8_t sensor_offset;
    if (s_sensor_state_mutex != NULL) {
      xSemaphoreTake(s_sensor_state_mutex, portMAX_DELAY);
      sensor_enabled = s_sensor_enabled;
      sensor_offset = s_sensor_start_byte;
      xSemaphoreGive(s_sensor_state_mutex);
    } else {
      sensor_enabled = s_sensor_enabled;
      sensor_offset = s_sensor_start_byte;
    }
    
    if (sensor_enabled) {
      if (s_vl53l1x_handle.initialized) {
        /* Read complete result structure */
        VL53L1X_Result_t result;
        VL53L1X_ERROR api_status = VL53L1X_GetResult(s_vl53l1x_device.dev, &result);
        
        if (api_status == VL53L1X_ERROR_NONE) {
          /* Write sensor data to assembly with mutex protection */
          if (s_assembly_mutex != NULL) {
            xSemaphoreTake(s_assembly_mutex, portMAX_DELAY);
          }
          
          /* Write distance to bytes offset+0 and offset+1 of input assembly (little-endian) */
          g_assembly_data064[sensor_offset + 0] = (uint8_t)(result.Distance & 0xFF);
          g_assembly_data064[sensor_offset + 1] = (uint8_t)((result.Distance >> 8) & 0xFF);
          
          /* Write Status, Ambient, SigPerSPAD, NumSPADs starting at offset+2 */
          g_assembly_data064[sensor_offset + 2] = result.Status;  /* Status */
          
          /* Bytes offset+3 to offset+4: Ambient (little-endian) */
          g_assembly_data064[sensor_offset + 3] = (uint8_t)(result.Ambient & 0xFF);
          g_assembly_data064[sensor_offset + 4] = (uint8_t)((result.Ambient >> 8) & 0xFF);
          
          /* Bytes offset+5 to offset+6: SigPerSPAD (little-endian) */
          g_assembly_data064[sensor_offset + 5] = (uint8_t)(result.SigPerSPAD & 0xFF);
          g_assembly_data064[sensor_offset + 6] = (uint8_t)((result.SigPerSPAD >> 8) & 0xFF);
          
          /* Bytes offset+7 to offset+8: NumSPADs (little-endian) */
          g_assembly_data064[sensor_offset + 7] = (uint8_t)(result.NumSPADs & 0xFF);
          g_assembly_data064[sensor_offset + 8] = (uint8_t)((result.NumSPADs >> 8) & 0xFF);
          
          if (s_assembly_mutex != NULL) {
            xSemaphoreGive(s_assembly_mutex);
          }
          
          /* Clear interrupt after reading */
          VL53L1X_ClearInterrupt(s_vl53l1x_device.dev);
          
          /* Optional: Log every 10 readings (1 second at 10Hz) */
          static uint32_t log_counter = 0;
          if (++log_counter >= 10) {
            log_counter = 0;
            OPENER_TRACE_INFO("VL53L1x: Distance=%d mm, Status=%d, Ambient=%d, SigPerSPAD=%d, NumSPADs=%d\n",
                             result.Distance, result.Status, result.Ambient, result.SigPerSPAD, result.NumSPADs);
          }
        } else {
          OPENER_TRACE_WARN("VL53L1x GetResult failed: %d\n", api_status);
        }
      } else {
        OPENER_TRACE_WARN("VL53L1x not initialized\n");
      }
    } else {
      /* Sensor is disabled - zero out configured byte range */
      if (s_assembly_mutex != NULL) {
        xSemaphoreTake(s_assembly_mutex, portMAX_DELAY);
        memset(&g_assembly_data064[sensor_offset], 0, 9);
        xSemaphoreGive(s_assembly_mutex);
      } else {
        memset(&g_assembly_data064[sensor_offset], 0, 9);
      }
    }
    
    vTaskDelay(loop_delay_ticks);
  }
}

EipStatus ApplicationInitialization(void) {
  CreateAssemblyObject( DEMO_APP_OUTPUT_ASSEMBLY_NUM, g_assembly_data096,
                       sizeof(g_assembly_data096));

  CreateAssemblyObject( DEMO_APP_INPUT_ASSEMBLY_NUM, g_assembly_data064,
                       sizeof(g_assembly_data064));

  CreateAssemblyObject( DEMO_APP_CONFIG_ASSEMBLY_NUM, g_assembly_data097,
                       sizeof(g_assembly_data097));

  ConfigureExclusiveOwnerConnectionPoint(0, DEMO_APP_OUTPUT_ASSEMBLY_NUM,
  DEMO_APP_INPUT_ASSEMBLY_NUM,
                                         DEMO_APP_CONFIG_ASSEMBLY_NUM);
  ConfigureInputOnlyConnectionPoint(0, DEMO_APP_OUTPUT_ASSEMBLY_NUM,
                                    DEMO_APP_INPUT_ASSEMBLY_NUM,
                                    DEMO_APP_CONFIG_ASSEMBLY_NUM);
  ConfigureListenOnlyConnectionPoint(0, DEMO_APP_OUTPUT_ASSEMBLY_NUM,
                                     DEMO_APP_INPUT_ASSEMBLY_NUM,
                                     DEMO_APP_CONFIG_ASSEMBLY_NUM);
  CipRunIdleHeaderSetO2T(false);
  CipRunIdleHeaderSetT2O(false);
  ConfigureStatusLed();

  /* Initialize mutexes */
  s_assembly_mutex = xSemaphoreCreateMutex();
  if (s_assembly_mutex == NULL) {
    OPENER_TRACE_ERR("Failed to create assembly mutex\n");
  }
  
  s_sensor_state_mutex = xSemaphoreCreateMutex();
  if (s_sensor_state_mutex == NULL) {
    OPENER_TRACE_ERR("Failed to create sensor state mutex\n");
  }

  /* Load sensor enabled state from NVS and initialize */
  s_sensor_enabled = system_sensor_enabled_load();
  sample_application_set_sensor_enabled(s_sensor_enabled);
  
  /* Load sensor data start byte offset from NVS */
  s_sensor_start_byte = system_sensor_byte_offset_load();
  sample_application_set_sensor_byte_offset(s_sensor_start_byte);
  
  /* Create VL53L1x sensor task on Core 1 (always create, task checks enabled state) */
  BaseType_t sensor_task_result = xTaskCreatePinnedToCore(
    vl53l1x_sensor_task,
    "VL53L1x_Sensor",
    4096,  /* Stack size */
    NULL,
    5,     /* Priority (lower than OpENer) */
    &s_vl53l1x_task_handle,
    1);    /* Core 1 */
  
  if (sensor_task_result == pdPASS) {
    OPENER_TRACE_INFO("VL53L1x sensor task created on Core 1 (enabled=%s)\n", 
                     s_sensor_enabled ? "yes" : "no");
  } else {
    OPENER_TRACE_ERR("Failed to create VL53L1x sensor task\n");
  }

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

  s_active_io_connections = 0;
  CipIdentityClearStatusFlags(kMajorRecoverableFault | kMajorUnrecoverableFault);
  IdentityEnter(kStateStandby, kNoIoConnectionsEstablished);
  s_io_activity_seen = false;
  CipEthernetLinkSetInterfaceState(1, kEthLinkInterfaceStateDisabled);

  return kEipStatusOk;
}

void HandleApplication(void) {
}

void CheckIoConnectionEvent(unsigned int output_assembly_id,
                            unsigned int input_assembly_id,
                            IoConnectionEvent io_connection_event) {

  (void) output_assembly_id;
  (void) input_assembly_id;

  switch (io_connection_event) {
    case kIoConnectionEventOpened:
      if (s_active_io_connections++ == 0) {
        IdentityEnter(kStateStandby,
                      kAtLeastOneIoConnectionEstablishedAllInIdleMode);
      }
      break;
    case kIoConnectionEventTimedOut:
    case kIoConnectionEventClosed:
      if (s_active_io_connections > 0) {
        s_active_io_connections--;
      }
      if (s_active_io_connections == 0) {
        s_io_activity_seen = false;
        IdentityEnter(kStateStandby, kNoIoConnectionsEstablished);
      }
      break;
    default:
      break;
  }
}

EipStatus AfterAssemblyDataReceived(CipInstance *instance) {
  EipStatus status = kEipStatusOk;

  switch (instance->instance_number) {
    case DEMO_APP_OUTPUT_ASSEMBLY_NUM:
      /* Process output assembly data (LED control only) */
      /* Note: Input assembly bytes 0-8 are now reserved for VL53L1x sensor data:
       *   Bytes 0-1: Distance (mm)
       *   Byte 2: Status
       *   Bytes 3-4: Ambient (kcps)
       *   Bytes 5-6: SigPerSPAD (kcps/SPAD)
       *   Bytes 7-8: NumSPADs
       * Output data is no longer mirrored to input assembly */
      gpio_set_level(kStatusLedGpio,
                     (g_assembly_data096[0] & 0x01) ? 1 : 0);
      IdentityNoteIoActivity();
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

EipBool8 BeforeAssemblyDataSend(CipInstance *instance) {
  (void) instance;
  IdentityNoteIoActivity();
  return true;
}

EipStatus ResetDevice(void) {
  CloseAllConnections();
  CipQosUpdateUsedSetQosValues();
  s_active_io_connections = 0;
  CipIdentityClearStatusFlags(kMajorRecoverableFault | kMajorUnrecoverableFault);
  IdentityEnter(kStateSelfTesting, kSelftestingUnknown);
  s_io_activity_seen = false;
  CipEthernetLinkSetInterfaceState(1, kEthLinkInterfaceStateDisabled);
  ScheduleRestart();
  return kEipStatusOk;
}

EipStatus ResetDeviceToInitialConfiguration(void) {
  g_tcpip.encapsulation_inactivity_timeout = 120;
  CipQosResetAttributesToDefaultValues();
  RestoreTcpIpDefaults();
  s_active_io_connections = 0;
  CipIdentityClearStatusFlags(kMajorRecoverableFault | kMajorUnrecoverableFault);
  IdentityEnter(kStateSelfTesting, kSelftestingUnknown);
  s_io_activity_seen = false;
  CipEthernetLinkSetInterfaceState(1, kEthLinkInterfaceStateDisabled);
  ScheduleRestart();
  return kEipStatusOk;
}

#if defined(OPENER_ETHLINK_CNTRS_ENABLE) && 0 != OPENER_ETHLINK_CNTRS_ENABLE
static void ZeroInterfaceCounters(CipEthernetLinkInterfaceCounters *counters) {
  memset(counters->cntr32, 0, sizeof(counters->cntr32));
}

static void ZeroMediaCounters(CipEthernetLinkMediaCounters *counters) {
  memset(counters->cntr32, 0, sizeof(counters->cntr32));
}

EipStatus EthLnkPreGetCallback(CipInstance *instance,
                               CipAttributeStruct *attribute,
                               CipByte service) {
  (void)service;
  if (instance == NULL || attribute == NULL) {
    return kEipStatusOk;
  }

  if (instance->instance_number == 0 ||
      instance->instance_number > OPENER_ETHLINK_INSTANCE_CNT) {
    return kEipStatusOk;
  }

  size_t idx = instance->instance_number - 1U;
  switch (attribute->attribute_number) {
    case 4: {
      const NetworkInterfaceCounters *src = NetworkGetInterfaceCounters();
      CipEthernetLinkInterfaceCounters *dst = &g_ethernet_link[idx].interface_cntrs;
      dst->ul.in_octets         = src->in_octets;
      dst->ul.in_ucast          = src->in_ucast_packets;
      dst->ul.in_nucast         = src->in_nucast_packets;
      dst->ul.in_discards       = src->in_discards;
      dst->ul.in_errors         = src->in_errors;
      dst->ul.in_unknown_protos = src->in_unknown_protos;
      dst->ul.out_octets        = src->out_octets;
      dst->ul.out_ucast         = src->out_ucast_packets;
      dst->ul.out_nucast        = src->out_nucast_packets;
      dst->ul.out_discards      = src->out_discards;
      dst->ul.out_errors        = src->out_errors;
      OPENER_TRACE_INFO("EthCntr Pre: inst=%u in_oct=%" PRIu32 " in_ucast=%" PRIu32 " out_ucast=%" PRIu32 " out_oct=%" PRIu32 "\n",
                        (unsigned)instance->instance_number,
                        src->in_octets,
                        src->in_ucast_packets,
                        src->out_ucast_packets,
                        src->out_octets);
      break;
    }
    case 5: {
      CipEthernetLinkMediaCounters *dst = &g_ethernet_link[idx].media_cntrs;
      ZeroMediaCounters(dst);
      break;
    }
    default:
      break;
  }

  return kEipStatusOk;
}

EipStatus EthLnkPostGetCallback(CipInstance *instance,
                                CipAttributeStruct *attribute,
                                CipByte service) {
  if (instance == NULL || attribute == NULL) {
    return kEipStatusOk;
  }

  if ((service & 0x7FU) != kEthLinkGetAndClear) {
    return kEipStatusOk;
  }

  if (instance->instance_number == 0 ||
      instance->instance_number > OPENER_ETHLINK_INSTANCE_CNT) {
    return kEipStatusOk;
  }

  size_t idx = instance->instance_number - 1U;
  switch (attribute->attribute_number) {
    case 4:
      ZeroInterfaceCounters(&g_ethernet_link[idx].interface_cntrs);
      NetworkResetInterfaceCounters();
      break;
    case 5:
      ZeroMediaCounters(&g_ethernet_link[idx].media_cntrs);
      break;
    default:
      break;
  }

  return kEipStatusOk;
}
#else
EipStatus EthLnkPreGetCallback(CipInstance *instance,
                               CipAttributeStruct *attribute,
                               CipByte service) {
  (void)instance;
  (void)attribute;
  (void)service;
  return kEipStatusOk;
}

EipStatus EthLnkPostGetCallback(CipInstance *instance,
                                CipAttributeStruct *attribute,
                                CipByte service) {
  (void)instance;
  (void)attribute;
  (void)service;
  return kEipStatusOk;
}
#endif /* OPENER_ETHLINK_CNTRS_ENABLE */

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
    IdentityNoteIoActivity();
  } else if (s_active_io_connections == 0) {
    IdentityEnter(kStateStandby, kNoIoConnectionsEstablished);
  } else if (!s_io_activity_seen) {
    IdentityEnter(kStateStandby,
                  kAtLeastOneIoConnectionEstablishedAllInIdleMode);
  }
  (void) run_idle_value;
}

