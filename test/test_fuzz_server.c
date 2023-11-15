#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "iso14229.h"
#include "tp/mock.h"

#ifdef __cplusplus
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *, size_t);
#else
int LLVMFuzzerTestOneInput(const uint8_t *, size_t);
#endif

typedef struct {
    uint8_t srv_retval;
    uint16_t client_sa;
    uint16_t client_ta;
    uint8_t client_func_req;
    uint8_t msg[UDS_BUFSIZE];
} StuffToFuzz_t;

static StuffToFuzz_t fuzz;
static uint8_t client_recv_buf[UDS_BUFSIZE];

static uint8_t fn(UDSServer_t *srv, UDSServerEvent_t ev, const void *arg) { 
    printf("Whoah, got event %d\n", ev);
    return fuzz.srv_retval;
}

static uint32_t g_ms = 0;
uint32_t UDSMillis() { return g_ms; }
static UDSServer_t srv;
static UDSTpHandle_t *mock_client = NULL;

void DoInitialization() {
    UDSServerConfig_t cfg = {
        .fn = fn,
        .tp = TPMockCreate("server"),
        .target_addr = 0x7E0,
        .source_addr = 0x7E8,
        .source_addr_func = 0x7DF,
    };
    UDSServerInit(&srv, &cfg);
    mock_client = TPMockCreate("client");
}


int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static bool initialized = false;
    if (!initialized) {
        DoInitialization();
        initialized = true;
    }
    memset(&fuzz, 0, sizeof(fuzz));
    memmove(&fuzz, data, size);
    srv.tp = TPMockCreate("server");
    mock_client = TPMockCreate("client");

    UDSSDU_t msg = {
        .A_Mtype = UDS_A_MTYPE_DIAG,
        .A_SA = fuzz.client_sa,
        .A_TA = fuzz.client_ta,
        .A_TA_Type = fuzz.client_func_req ? UDS_A_TA_TYPE_FUNCTIONAL : UDS_A_TA_TYPE_PHYSICAL,
        .A_Length = size > offsetof(StuffToFuzz_t, msg) ? size - offsetof(StuffToFuzz_t, msg) : 0,
        .A_Data = (uint8_t *)data + offsetof(StuffToFuzz_t, msg),
        .A_DataBufSize = sizeof(fuzz.msg),
    };
    mock_client->send(mock_client, &msg);

    for (g_ms = 0; g_ms < 100; g_ms++) {
        UDSServerPoll(&srv);
    }

    {
        UDSSDU_t msg2 = {
            .A_Data = client_recv_buf,    
            .A_DataBufSize = sizeof(client_recv_buf),
        };
        mock_client->recv(mock_client, &msg2);
    }
    TPMockReset();
    return 0;
}
