#include <3ds/types.h>
#include <3ds/result.h>
#include <3ds/srv.h>
#include <3ds/svc.h>
#include <ipc_handler.h>
#include <err.h>
#include <pdn.h>
#include <memset.h>

#define OS_REMOTE_SESSION_CLOSED MAKERESULT(RL_STATUS, RS_CANCELED, RM_OS, 26)

static __attribute__((section(".data.TerminationFlag"))) bool TerminationFlag = false;

inline static void HandleSRVNotification()
{
    u32 id;
    Err_FailedThrow(srvReceiveNotification(&id));
    if (id == 0x100)
        TerminationFlag = true;
}

static const char* const service_names[] = {"pdn:s", "pdn:d", "pdn:i", "pdn:g", "pdn:c"};

static inline void initBSS()
{
    extern void* __bss_start__;
    extern void* __bss_end__;
    _memset32_aligned(__bss_start__, 0, (size_t)__bss_end__ - (size_t)__bss_start__);
}

void PDNMain()
{
    initBSS();
    const s32 SERVICECOUNT = 5;
    const s32 INDEXMAX = SERVICECOUNT * 2 + 1;
    const s32 REMOTESESSIONINDEX = SERVICECOUNT + 1;

    Handle sessionhandles[INDEXMAX];

    int serviceindexes[SERVICECOUNT];

    s32 handlecount = SERVICECOUNT + 1;

    Err_FailedThrow(srvInit());

    for (int i = 1; i <= SERVICECOUNT; i++)
        Err_FailedThrow(srvRegisterService(&sessionhandles[i], service_names[i - 1], 1));

    Err_FailedThrow(srvEnableNotification(&sessionhandles[0]));

    Handle target = 0;
    s32 targetindex = -1;
    for (;;) {
        s32 index;

        if (!target) {
            if (TerminationFlag && handlecount == REMOTESESSIONINDEX)
                break;
            else
                *getThreadCommandBuffer() = 0xFFFF0000;
        }

        Result res = svcReplyAndReceive(&index, sessionhandles, handlecount, target);
        s32 lasttargetindex = targetindex;
        target = 0;
        targetindex = -1;

        if (R_FAILED(res)) {

            if (res != OS_REMOTE_SESSION_CLOSED) {
                Err_Throw(res);
            }
            else if (index == -1) {
                if (lasttargetindex == -1) {
                    Err_Throw(PDN_CANCELED_RANGE);
                }
                else
                    index = lasttargetindex;
            }

            else if (index >= handlecount)
                Err_Throw(PDN_CANCELED_RANGE);

            svcCloseHandle(sessionhandles[index]);
            handlecount--;
            for (s32 i = index - REMOTESESSIONINDEX; i < handlecount - REMOTESESSIONINDEX; i++) {
                sessionhandles[REMOTESESSIONINDEX + i] = sessionhandles[REMOTESESSIONINDEX + i + 1];
                serviceindexes[i] = serviceindexes[i + 1];
            }

            continue;
        }

        if (index == 0)
            HandleSRVNotification();

        else if (index >= 1 && index < REMOTESESSIONINDEX) {
            Handle newsession = 0;
            Err_FailedThrow(svcAcceptSession(&newsession, sessionhandles[index]));

            if (handlecount >= INDEXMAX) {
                svcCloseHandle(newsession);
                continue;
            }

            sessionhandles[handlecount] = newsession;
            serviceindexes[handlecount - REMOTESESSIONINDEX] = index - 1;
            handlecount++;

        } else if (index >= REMOTESESSIONINDEX && index < INDEXMAX) {
            IPC_HandleCommands(serviceindexes[index -  REMOTESESSIONINDEX]);
            target = sessionhandles[index];
            targetindex = index;

        } else {
            Err_Throw(PDN_INTERNAL_RANGE);
        }
    }

    for (int i = 0; i < SERVICECOUNT; i++) {
        srvUnregisterService(service_names[i]);
        svcCloseHandle(sessionhandles[i + 1]);
    }

    svcCloseHandle(sessionhandles[0]);

    srvExit();
}