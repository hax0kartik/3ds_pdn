#include "ipc.hpp"
extern "C" {
    void spinwait(uint32_t);
}
static void HandlePDNSCommands(void)
{
    u32 *cmdbuf = getThreadCommandBuffer();
    switch(cmdbuf[0] >> 16)
    {
        case 1:
        {
            cmdbuf[0] = 0x100C0;
            cmdbuf[1] = 0;
            cmdbuf[2] = *(vu32*)0x1EC41008; //  PDN_WAKE_ENABLE
            cmdbuf[3] = *(vu32*)0x1EC4100C; //  PDN_WAKE_REASON
            break;
        }

        case 2:
        {
            *(vu32*)0x1EC4100C = cmdbuf[2] & cmdbuf[1];
            *(vu32*)0x1EC41008 = cmdbuf[1];
            *(vu32*)0x1EC4100C = (cmdbuf[2] & ~cmdbuf[1]);
            cmdbuf[0] = 0x20040;
            cmdbuf[1] = 0;
            break;
        }

        case 3:
        {
            *(vu32*)0x1EC4100C = cmdbuf[1];
            cmdbuf[0] = 0x30040;
            cmdbuf[1] = 0;
            break;
        }

        default:
        {
            cmdbuf[0] = 64;
            cmdbuf[1] = 0xD900182F;
            break;
        }
    }
}

static Result ControlDSPCNT(bool enable, bool resetengines, bool resetregisters)
{
    if ( (resetregisters & ~(u8)enable) != 0 )
        return 0;
    *(vu32*)0x1EC41230 = (resetengines ^ 1) | (2 * enable);
    if((resetengines & resetregisters) != 0)
    {
        spinwait(0x30);
        *(vu32*)0x1EC41230 = (2 * resetregisters) | 1;
    }
    return 1;
}

static void HandlePDNDCommands(void) 
{
    u32 *cmdbuf = getThreadCommandBuffer();
    switch(cmdbuf[0] >> 16)
    {
        case 1:
        {
            Result ret = ControlDSPCNT(cmdbuf[1], cmdbuf[2], cmdbuf[3]);
            if(ret) cmdbuf[1] = 0;
            else cmdbuf[1] = 0xE0E02401;
            cmdbuf[0] = 0x10040;
            break;
        }

        default:
        {
            cmdbuf[0] = 64;
            cmdbuf[1] = 0xD900182F;
            break;
        }
    }
}

static void HandlePDNICommands(void)
{
    u32 *cmdbuf = getThreadCommandBuffer();
    switch(cmdbuf[0] >> 16)
    {
        case 1:
        {
            *(vu8*)0x1EC41220 = ((u8)cmdbuf[1] | (*(vu8*)0x1EC41220 & ~1));
            cmdbuf[0] = 0x10040;
            cmdbuf[1] = 0;
            break;
        }

        case 2:
        {
            *(vu8*)0x1EC41220 = ((*(vu8*)0x1EC41220 & ~2) | (2 * (u8)cmdbuf[1]));
            cmdbuf[0] = 0x20040;
            cmdbuf[1] = 0;
            break;
        }

        default:
        {
            cmdbuf[0] = 64;
            cmdbuf[1] = 0xD900182F;
            break;
        }
    }
}

static Result ControlGPUCNT(bool enable, bool resetengines, bool resetregisters)
{
    if(((resetengines | resetregisters) & ~(u8)enable) != 0)
        return 0;
    *(vu32*)0x1EC41200 = (((resetregisters == 0) ? 126 : 0) | (enable ? 0x10000 : 0) | (resetengines ^ 1));
    if(resetengines | resetregisters)
    {
        spinwait(12);
        *(vu32*)0x1EC41200 = ((enable) ? 0x10000 : 0) | 127;
    }
    return 1;
}

static void HandlePDNGCommands(void)
{
    u32 *cmdbuf = getThreadCommandBuffer();
    switch(cmdbuf[0] >> 16)
    {
        case 1:
        {
            Result res = ControlGPUCNT(cmdbuf[1], cmdbuf[2], cmdbuf[3]);
            if(res) cmdbuf[1] = 0;
            else cmdbuf[1] = 0xE0E02401;
            cmdbuf[0] = 0x10040;
            break;
        }

        default:
        {
            cmdbuf[0] = 64;
            cmdbuf[1] = 0xD900182F;
            break;
        }
    }
}

static void HandlePDNCCommands(void)
{
    u32 *cmdbuf = getThreadCommandBuffer();
    switch(cmdbuf[0] >> 16)
    {
        case 1:
        {
            *(vu8*)0x1EC41224 = (u8)cmdbuf[1] | (*(vu8*)0x1EC41224 & 0xFE);
            cmdbuf[0] = 0x10040;
            cmdbuf[1] = 0;
            break;
        }

        case 2:
        {
            cmdbuf[0] = 0x20080;
            cmdbuf[1] = 0;
            cmdbuf[2] = *(vu8*)0x1EC41224 & 0x1;
            break;
        }

        default:
        {
            cmdbuf[0] = 64;
            cmdbuf[1] = 0xD900182F;
            break;
        }
    }
}

void IPC::HandleCommands(uint32_t index)
{
    typedef void (*functions)(void);
    const functions sessionhandlers[5] = {HandlePDNSCommands, HandlePDNDCommands, HandlePDNICommands, HandlePDNGCommands, HandlePDNCCommands};
    sessionhandlers[index]();
}