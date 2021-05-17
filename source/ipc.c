#include <ipc_handler.h>
#include <3ds/types.h>
#include <3ds/result.h>
#include <3ds/ipc.h>
#include <3ds/svc.h>
#include <pdn.h>

void spinwait(uint32_t wait);

#define OS_INVALID_HEADER        MAKERESULT(RL_PERMANENT, RS_WRONGARG, RM_OS, 47)
#define OS_INVALID_IPC_PARAMATER MAKERESULT(RL_PERMANENT, RS_WRONGARG, RM_OS, 48)

#define PDN_WAKE_ENABLE ((vu32*)0x1EC41008)
#define PDN_WAKE_REASON ((vu32*)0x1EC4100C)

#define PDN_GPU_CNT ((vu32*)0x1EC41200)
#define PDN_I2S_CNT ((vu8*)0x1EC41220)
#define PDN_CAMERA_CNT ((vu8*)0x1EC41224)
#define PDN_DSP_CNT ((vu8*)0x1EC41230)

static void HandlePDNSCommands(void)
{
    u32 *cmdbuf = getThreadCommandBuffer();
    switch(cmdbuf[0] >> 16)
    {
    case 1:
        cmdbuf[0] = IPC_MakeHeader(0x1, 3, 0);
        cmdbuf[1] = 0;
        cmdbuf[2] = *PDN_WAKE_ENABLE;
        cmdbuf[3] = *PDN_WAKE_REASON;
        break;

    case 2:
        *PDN_WAKE_REASON = cmdbuf[2] & cmdbuf[1];
        *PDN_WAKE_REASON = cmdbuf[1];
        *PDN_WAKE_REASON = (cmdbuf[2] & ~cmdbuf[1]);
        cmdbuf[0] = IPC_MakeHeader(0x2, 1, 0);
        cmdbuf[1] = 0;
        break;

    case 3:
        *PDN_WAKE_REASON = cmdbuf[1];
        cmdbuf[0] = IPC_MakeHeader(0x3, 1, 0);
        cmdbuf[1] = 0;
        break;

    default:
        cmdbuf[0] = IPC_MakeHeader(0x0, 1, 0);
        cmdbuf[1] = OS_INVALID_HEADER;
    }
}

static int ControlDSPCNT(bool enable, bool resetengines, bool resetregisters)
{
    if ( (resetregisters & ~(u8)enable) != 0 )
        return 0;
    *PDN_DSP_CNT = (resetengines ^ 1) | (2 * enable);
    if((resetengines & resetregisters) != 0)
    {
        spinwait(48);
        *PDN_DSP_CNT = (2 * resetregisters) | 1;
    }
    return 1;
}

static void HandlePDNDCommands(void) 
{
    u32 *cmdbuf = getThreadCommandBuffer();
    switch(cmdbuf[0] >> 16)
    {
    case 1:
        if(ControlDSPCNT(cmdbuf[1], cmdbuf[2], cmdbuf[3]))
            cmdbuf[1] = 0;
        else
            cmdbuf[1] = PDN_INVALID_ARG;
        cmdbuf[0] = IPC_MakeHeader(0x1, 1, 0);
        break;

    default:
        cmdbuf[0] = IPC_MakeHeader(0x0, 1, 0);
        cmdbuf[1] = OS_INVALID_HEADER;
    }
}

static void HandlePDNICommands(void)
{
    u32 *cmdbuf = getThreadCommandBuffer();
    switch(cmdbuf[0] >> 16)
    {
    case 1:
        *PDN_I2S_CNT = ((u8)cmdbuf[1] | (*PDN_I2S_CNT & ~1));
        cmdbuf[0] = IPC_MakeHeader(0x1, 1, 0);
        cmdbuf[1] = 0;
        break;

    case 2:
        *PDN_I2S_CNT = ((*PDN_I2S_CNT & ~2) | ((u8)cmdbuf[1]) << 1);
        cmdbuf[0] = IPC_MakeHeader(0x2, 1, 0);
        cmdbuf[1] = 0;
        break;

    default:
        cmdbuf[0] = IPC_MakeHeader(0x0, 1, 0);
        cmdbuf[1] = OS_INVALID_HEADER;
    }
}

static int ControlGPUCNT(bool enable, bool resetengines, bool resetregisters)
{
    if(((resetengines | resetregisters) & ~(u8)enable) != 0)
        return 0;
    *PDN_GPU_CNT = ((!resetregisters ? 126 : 0) | (enable ? BIT(16) : 0) | (resetengines ^ 1));
    if(resetengines | resetregisters)
    {
        spinwait(12);
        *PDN_GPU_CNT = (enable ? BIT(16) : 0) | 127;
    }
    return 1;
}

static void HandlePDNGCommands(void)
{
    u32 *cmdbuf = getThreadCommandBuffer();
    switch(cmdbuf[0] >> 16)
    {
    case 1:
        if(ControlGPUCNT(cmdbuf[1], cmdbuf[2], cmdbuf[3]))
            cmdbuf[1] = 0;
        else
            cmdbuf[1] = PDN_INVALID_ARG;
        cmdbuf[0] = IPC_MakeHeader(0x1, 1, 0);
        break;

    default:
        cmdbuf[0] = IPC_MakeHeader(0x0, 1, 0);
        cmdbuf[1] = OS_INVALID_HEADER;
    }
}

static void HandlePDNCCommands(void)
{
    u32 *cmdbuf = getThreadCommandBuffer();
    switch(cmdbuf[0] >> 16)
    {
    case 1:
        *PDN_CAMERA_CNT = (u8)cmdbuf[1] | (*PDN_CAMERA_CNT & 0xFE);
        cmdbuf[0] = IPC_MakeHeader(0x1, 1, 0);
        cmdbuf[1] = 0;
        break;

    case 2:
        cmdbuf[0] = IPC_MakeHeader(0x2, 2, 0);
        cmdbuf[1] = 0;
        cmdbuf[2] = *PDN_CAMERA_CNT & 0x1;
        break;

    default:
        cmdbuf[0] = IPC_MakeHeader(0x0, 1, 0);
        cmdbuf[1] = OS_INVALID_HEADER;
    }
}

void IPC_HandleCommands(int index)
{
    if (index == 0)
        HandlePDNSCommands();
    else if (index == 1)
        HandlePDNDCommands();
    else if (index == 2)
        HandlePDNICommands();
    else if (index == 3)
        HandlePDNGCommands();
    else if (index == 4)
        HandlePDNCCommands();
}
