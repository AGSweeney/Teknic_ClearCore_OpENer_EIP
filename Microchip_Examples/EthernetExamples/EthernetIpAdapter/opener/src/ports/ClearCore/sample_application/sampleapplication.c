#include "ClearCore.h"
#include "opener_api.h"
#include "appcontype.h"
#include "cipidentity.h"
#include "ciptcpipinterface.h"
#include "cipqos.h"
#include "cipstring.h"
#include "ciptypes.h"
#include "typedefs.h"
#include "cipassembly.h"
#include <string.h>

// Redefine LOG_TRACE to use ClearCore's ConnectorUsb after including ClearCore.h
#undef LOG_TRACE
#define LOG_TRACE(...) do { \
    char trace_buf[256]; \
    snprintf(trace_buf, sizeof(trace_buf), __VA_ARGS__); \
    ConnectorUsb.SendLine(trace_buf); \
} while(0)

#include "trace.h"

#define DEMO_APP_INPUT_ASSEMBLY_NUM                100
#define DEMO_APP_OUTPUT_ASSEMBLY_NUM               150
#define DEMO_APP_CONFIG_ASSEMBLY_NUM               151

#define ASSEMBLY_DATA_SIZE 32

EipUint8 g_assembly_data_input[ASSEMBLY_DATA_SIZE];
EipUint8 g_assembly_data_output[ASSEMBLY_DATA_SIZE];
EipUint8 g_assembly_data_config[10];

static EipUint32 s_active_io_connections = 0;
static bool s_io_activity_seen = false;

EipStatus ApplicationInitialization(void) {
    CreateAssemblyObject(DEMO_APP_OUTPUT_ASSEMBLY_NUM, g_assembly_data_output,
                         sizeof(g_assembly_data_output));

    CreateAssemblyObject(DEMO_APP_INPUT_ASSEMBLY_NUM, g_assembly_data_input,
                         sizeof(g_assembly_data_input));

    CreateAssemblyObject(DEMO_APP_CONFIG_ASSEMBLY_NUM, g_assembly_data_config,
                         sizeof(g_assembly_data_config));

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

    memset(g_assembly_data_input, 0, sizeof(g_assembly_data_input));
    memset(g_assembly_data_output, 0, sizeof(g_assembly_data_output));
    memset(g_assembly_data_config, 0, sizeof(g_assembly_data_config));

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
            memcpy(g_assembly_data_input, g_assembly_data_output, 
                   sizeof(g_assembly_data_input));
            s_io_activity_seen = true;
            break;
        case DEMO_APP_CONFIG_ASSEMBLY_NUM:
            break;
        default:
            status = kEipStatusError;
            break;
    }

    return status;
}

EipBool8 BeforeAssemblyDataSend(CipInstance *instance) {
    (void) instance;
    return s_io_activity_seen;
}

EipStatus ResetDevice(void) {
    return kEipStatusError;
}

EipStatus ResetDeviceToInitialConfiguration(void) {
    return kEipStatusError;
}

void *CipCalloc(size_t number_of_elements, size_t size_of_element) {
    return calloc(number_of_elements, size_of_element);
}

void CipFree(void *data) {
    free(data);
}

void RunIdleChanged(EipUint32 run_idle_value) {
    (void) run_idle_value;
}

