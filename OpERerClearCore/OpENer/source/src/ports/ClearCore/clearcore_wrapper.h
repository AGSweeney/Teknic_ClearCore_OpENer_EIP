#ifndef CLEARCORE_WRAPPER_H_
#define CLEARCORE_WRAPPER_H_

#ifdef CLEARCORE

#ifdef __cplusplus
extern "C" {
#endif

unsigned long GetMillis(void);
void ConnectorLed_SetState(int state);
void ConnectorIO0_Initialize(void);
void ConnectorIO0_SetState(int state);
void ConnectorIO1_Initialize(void);
void ConnectorIO1_SetState(int state);
void ConnectorIO2_Initialize(void);
void ConnectorIO2_SetState(int state);
void ConnectorIO3_Initialize(void);
void ConnectorIO3_SetState(int state);
void ConnectorIO4_Initialize(void);
void ConnectorIO4_SetState(int state);
void ConnectorIO5_Initialize(void);
void ConnectorIO5_SetState(int state);
void ConnectorDI6_Initialize(void);
int ConnectorDI6_GetState(void);
void ConnectorDI7_Initialize(void);
int ConnectorDI7_GetState(void);
void ConnectorDI8_Initialize(void);
int ConnectorDI8_GetState(void);
void ConnectorA9_Initialize(void);
int ConnectorA9_GetState(void);
void ConnectorA10_Initialize(void);
int ConnectorA10_GetState(void);
void ConnectorA11_Initialize(void);
int ConnectorA11_GetState(void);
void ConnectorA12_Initialize(void);
int ConnectorA12_GetState(void);
int ClearCoreEepromRead(uint16_t address, uint8_t *data, size_t length);
int ClearCoreEepromWrite(uint16_t address, const uint8_t *data, size_t length);
void ClearCoreRebootDevice(void);
void ClearCoreClearNvram(void);

#ifdef __cplusplus
}
#endif

#endif

#endif

