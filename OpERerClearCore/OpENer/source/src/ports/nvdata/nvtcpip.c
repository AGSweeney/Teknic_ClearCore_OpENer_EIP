/*******************************************************************************
 * Copyright (c) 2019, Rockwell Automation, Inc.
 * All rights reserved.
 *
 ******************************************************************************/

/** @file nvtcpip.c
 *  @brief This file implements the functions to handle TCP/IP object's NV data.
 */
#include "nvtcpip.h"

#include <string.h>
#include <stdint.h>

#include "trace.h"
#include "opener_api.h"
#include "typedefs.h"

#ifdef CLEARCORE
#include "ports/ClearCore/socket_types.h"
#else
#include "conffile.h"
#define TCPIP_CFG_NAME  "tcpip.cfg"
#endif

#ifdef CLEARCORE
#define TCPIP_EEPROM_BASE_ADDR  0x0100
#define TCPIP_EEPROM_MAGIC      0x54435049
#define TCPIP_EEPROM_VERSION    1

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t crc;
  CipDword config_control;
  CipUdint ip_address;
  CipUdint network_mask;
  CipUdint gateway;
  CipUdint name_server;
  CipUdint name_server_2;
  uint16_t domain_name_length;
  char domain_name[48];
  uint16_t hostname_length;
  char hostname[64];
} TcpIpEepromData;

static uint32_t CalculateCrc32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0xEDB88320;
      } else {
        crc >>= 1;
      }
    }
  }
  return ~crc;
}

static int EepromRead(uint16_t address, uint8_t *data, size_t length) {
  extern int ClearCoreEepromRead(uint16_t address, uint8_t *data, size_t length);
  return ClearCoreEepromRead(address, data, length);
}

static int EepromWrite(uint16_t address, const uint8_t *data, size_t length) {
  extern int ClearCoreEepromWrite(uint16_t address, const uint8_t *data, size_t length);
  return ClearCoreEepromWrite(address, data, length);
}
#endif

/** @brief Load NV data of the TCP/IP object from EEPROM
 *
 *  @param  p_tcp_ip pointer to the TCP/IP object's data structure
 *  @return kEipStatusOk: success; kEipStatusError: failure
 */
EipStatus NvTcpipLoad(CipTcpIpObject *p_tcp_ip) {
#ifdef CLEARCORE
  TcpIpEepromData eeprom_data;
  
  if (EepromRead(TCPIP_EEPROM_BASE_ADDR, (uint8_t *)&eeprom_data, sizeof(eeprom_data)) != 0) {
    OPENER_TRACE_INFO("NvTcpipLoad: EEPROM read failed\n");
    return kEipStatusError;
  }
  
  if (eeprom_data.magic != TCPIP_EEPROM_MAGIC) {
    OPENER_TRACE_INFO("NvTcpipLoad: Invalid magic number (0x%08X)\n", eeprom_data.magic);
    return kEipStatusError;
  }
  
  if (eeprom_data.version != TCPIP_EEPROM_VERSION) {
    OPENER_TRACE_INFO("NvTcpipLoad: Unsupported version (%d)\n", eeprom_data.version);
    return kEipStatusError;
  }
  
  uint32_t calculated_crc = CalculateCrc32((uint8_t *)&eeprom_data + 16, sizeof(eeprom_data) - 16);
  if (calculated_crc != eeprom_data.crc) {
    OPENER_TRACE_ERR("NvTcpipLoad: CRC mismatch (stored=0x%08X, calculated=0x%08X)\n", 
                     eeprom_data.crc, calculated_crc);
    return kEipStatusError;
  }
  
  p_tcp_ip->config_control = eeprom_data.config_control;
  
  p_tcp_ip->interface_configuration.ip_address = eeprom_data.ip_address;
  p_tcp_ip->interface_configuration.network_mask = eeprom_data.network_mask;
  p_tcp_ip->interface_configuration.gateway = eeprom_data.gateway;
  p_tcp_ip->interface_configuration.name_server = eeprom_data.name_server;
  p_tcp_ip->interface_configuration.name_server_2 = eeprom_data.name_server_2;
  
  if (eeprom_data.domain_name_length > 0 && eeprom_data.domain_name_length <= 48) {
    if (p_tcp_ip->interface_configuration.domain_name.string != NULL) {
      CipFree(p_tcp_ip->interface_configuration.domain_name.string);
    }
    p_tcp_ip->interface_configuration.domain_name.length = eeprom_data.domain_name_length;
    p_tcp_ip->interface_configuration.domain_name.string = (CipByte *)CipCalloc(eeprom_data.domain_name_length + 1, 1);
    if (p_tcp_ip->interface_configuration.domain_name.string != NULL) {
      memcpy(p_tcp_ip->interface_configuration.domain_name.string, eeprom_data.domain_name, eeprom_data.domain_name_length);
      p_tcp_ip->interface_configuration.domain_name.string[eeprom_data.domain_name_length] = '\0';
    }
  } else {
    p_tcp_ip->interface_configuration.domain_name.length = 0;
    p_tcp_ip->interface_configuration.domain_name.string = NULL;
  }
  
  if (eeprom_data.hostname_length > 0 && eeprom_data.hostname_length <= 64) {
    if (p_tcp_ip->hostname.string != NULL) {
      CipFree(p_tcp_ip->hostname.string);
    }
    p_tcp_ip->hostname.length = eeprom_data.hostname_length;
    p_tcp_ip->hostname.string = (CipByte *)CipCalloc(eeprom_data.hostname_length + 1, 1);
    if (p_tcp_ip->hostname.string != NULL) {
      memcpy(p_tcp_ip->hostname.string, eeprom_data.hostname, eeprom_data.hostname_length);
      p_tcp_ip->hostname.string[eeprom_data.hostname_length] = '\0';
    }
  } else {
    p_tcp_ip->hostname.length = 0;
    p_tcp_ip->hostname.string = NULL;
  }
  
  OPENER_TRACE_INFO("NvTcpipLoad: Successfully loaded config from EEPROM\n");
  return kEipStatusOk;
#else
  FILE *p_file = ConfFileOpen(false, TCPIP_CFG_NAME);
  if (NULL != p_file) {
    OPENER_TRACE_ERR("ERROR: Loading of TCP/IP object's NV data not implemented yet\n");
    EipStatus eip_status = kEipStatusError;
    eip_status = (kEipStatusError == ConfFileClose(&p_file)) ? kEipStatusError : eip_status;
    return eip_status;
  }
  return kEipStatusError;
#endif
}

/** @brief Store NV data of the TCP/IP object to EEPROM
 *
 *  @param  p_tcp_ip pointer to the TCP/IP object's data structure
 *  @return kEipStatusOk: success; kEipStatusError: failure
 */
EipStatus NvTcpipStore(const CipTcpIpObject *p_tcp_ip) {
#ifdef CLEARCORE
  TcpIpEepromData eeprom_data;
  
  memset(&eeprom_data, 0, sizeof(eeprom_data));
  eeprom_data.magic = TCPIP_EEPROM_MAGIC;
  eeprom_data.version = TCPIP_EEPROM_VERSION;
  eeprom_data.config_control = p_tcp_ip->config_control;
  
  eeprom_data.ip_address = p_tcp_ip->interface_configuration.ip_address;
  eeprom_data.network_mask = p_tcp_ip->interface_configuration.network_mask;
  eeprom_data.gateway = p_tcp_ip->interface_configuration.gateway;
  eeprom_data.name_server = p_tcp_ip->interface_configuration.name_server;
  eeprom_data.name_server_2 = p_tcp_ip->interface_configuration.name_server_2;
  
  if (p_tcp_ip->interface_configuration.domain_name.string != NULL && 
      p_tcp_ip->interface_configuration.domain_name.length > 0) {
    eeprom_data.domain_name_length = (uint16_t)((p_tcp_ip->interface_configuration.domain_name.length > 48) ? 48 : p_tcp_ip->interface_configuration.domain_name.length);
    memcpy(eeprom_data.domain_name, p_tcp_ip->interface_configuration.domain_name.string, eeprom_data.domain_name_length);
  } else {
    eeprom_data.domain_name_length = 0;
  }
  
  if (p_tcp_ip->hostname.string != NULL && p_tcp_ip->hostname.length > 0) {
    eeprom_data.hostname_length = (uint16_t)((p_tcp_ip->hostname.length > 64) ? 64 : p_tcp_ip->hostname.length);
    memcpy(eeprom_data.hostname, p_tcp_ip->hostname.string, eeprom_data.hostname_length);
  } else {
    eeprom_data.hostname_length = 0;
  }
  
  eeprom_data.crc = CalculateCrc32((uint8_t *)&eeprom_data + 16, sizeof(eeprom_data) - 16);
  
  if (EepromWrite(TCPIP_EEPROM_BASE_ADDR, (uint8_t *)&eeprom_data, sizeof(eeprom_data)) != 0) {
    OPENER_TRACE_ERR("NvTcpipStore: EEPROM write failed\n");
    return kEipStatusError;
  }
  
  OPENER_TRACE_INFO("NvTcpipStore: Successfully stored config to EEPROM\n");
  return kEipStatusOk;
#else
  FILE *p_file = ConfFileOpen(true, TCPIP_CFG_NAME);
  if (NULL != p_file) {
    OPENER_TRACE_ERR("ERROR: Storing of TCP/IP object's NV data not implemented yet\n");
    EipStatus eip_status = kEipStatusError;
    return (kEipStatusError == ConfFileClose(&p_file)) ? kEipStatusError : eip_status;
  } else {
    return kEipStatusError;
  }
#endif
}
