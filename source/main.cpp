#include <3ds.h>
#include <cstdlib>
#include <cstring>
#include "ipc.hpp"
#include <mpg123.h>
extern "C"{
    #include "services.h"
    #include "csvc.h"
    #include "ifile.h"
    #include "mythread.h"
}

#define ONERRSVCBREAK(ret) if(R_FAILED(ret)) svcBreak(USERBREAK_ASSERT);
#define OS_REMOTE_SESSION_CLOSED MAKERESULT(RL_STATUS,    RS_CANCELED, RM_OS, 26)
#define OS_INVALID_HEADER        MAKERESULT(RL_PERMANENT, RS_WRONGARG, RM_OS, 47)
#define OS_INVALID_IPC_PARAMATER MAKERESULT(RL_PERMANENT, RS_WRONGARG, RM_OS, 48)

static Result HandleNotifications(int *exit)
{
    uint32_t notid = 0;
    Result ret = srvReceiveNotification(&notid);
    if(R_FAILED(ret)) return ret;

    switch(notid)
    {
        case 0x100: // Exit
        {
            *exit = 1;
            break;
        }

    }
    return 0;
}

char *m_filedata;

uint32_t VATOPA(const void *addr){
    return svcConvertVAToPA(addr, false);
}

/* Two Data allocations for double buffering */
typedef struct{
    u32 sampleRate;
    u32 dataSize;	
    u16 bitsPerSample;
    u16 ndspFormat;
    u16 numChannels;
    u8* data;
    u8* data2;
    u8 number;
    bool decode;
    mpg123_handle* mh;
} PHL_Sound;

/* This is different from the ctrulib variant as you can use linear as well normal buffers with this. */
static Result csndPlaySound_(int chn, u32 flags, u32 sampleRate, float vol, float pan, void* data0, void* data1, u32 size)
{
    if (!(csndChannels & BIT(chn)))
        return 1;

    u32 paddr0 = 0, paddr1 = 0;

    int encoding = (flags >> 12) & 3;
    int loopMode = (flags >> 10) & 3;

    if (!loopMode) flags |= SOUND_ONE_SHOT;

    if (encoding != CSND_ENCODING_PSG)
    {
        if (data0) paddr0 = VATOPA(data0);
        if (data1) paddr1 = VATOPA(data1);

        if (data0 && encoding == CSND_ENCODING_ADPCM)
        {
            int adpcmSample = ((s16*)data0)[-2];
            int adpcmIndex = ((u8*)data0)[-2];
            CSND_SetAdpcmState(chn, 0, adpcmSample, adpcmIndex);
        }
    }

    u32 timer = CSND_TIMER(sampleRate);
    if (timer < 0x0042) timer = 0x0042;
    else if (timer > 0xFFFF) timer = 0xFFFF;
    flags &= ~0xFFFF001F;
    flags |= SOUND_ENABLE | SOUND_CHANNEL(chn) | (timer << 16);

    u32 volumes = CSND_VOL(vol, pan);
    CSND_SetChnRegs(flags, paddr0, paddr1, size, volumes, volumes);

    if (loopMode == CSND_LOOPMODE_NORMAL && paddr1 > paddr0)
    {
        // Now that the first block is playing, configure the size of the subsequent blocks
        size -= paddr1 - paddr0;
        CSND_SetBlock(chn, 1, paddr1, size);
    }

    return csndExecCmds(true);
}

/*
PHL_Sound loadWav(char* fname) {
    char *fullPath = fname;
    
    IFile f;
    if (IFile_Open(&f,ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, NULL), fsMakePath(PATH_ASCII, fname), FS_OPEN_READ) == 0) {

        // Check for valid fileName
        u32 sig;
        IFile_Read(&f, NULL, &sig, 4);

        if (sig == 0x46464952) { // RIFF
            u32 chunkSize;
            u32 format;
            
            IFile_Read(&f, NULL, &chunkSize, 4);
            IFile_Read(&f, NULL, &format, 4);
            
            if (format == 0x45564157) { // WAVE
                u32 subchunk1ID;
                IFile_Read(&f, NULL, &subchunk1ID, 4);
                if (subchunk1ID == 0x20746D66) { // fmt
                    u32 subchunk1Size;
                    u16 audioFormat;
                    u16 numChannels;
                    u32 sampleRate;
                    u32 byteRate;
                    u16 blockAlign;
                    u16 bitsPerSample;
                    
                    IFile_Read(&f, NULL, &subchunk1Size, 4);
                    IFile_Read(&f, NULL, &audioFormat, 2);
                    IFile_Read(&f, NULL, &numChannels, 2);
                    IFile_Read(&f, NULL, &sampleRate, 4);
                    IFile_Read(&f, NULL, &byteRate, 4);
                    IFile_Read(&f, NULL, &blockAlign, 2);
                    IFile_Read(&f, NULL, &bitsPerSample, 2);
                    
                    // Search for 'data'
                    for (int i = 0; i < 100; i++) {
                        u8 c;
                        IFile_Read(&f, NULL, &c, 1);
                        if (c == 0x64) { // 'd'
                            IFile_Read(&f, NULL, &c, 1);
                            if (c == 0x61) { // 'a'
                                IFile_Read(&f, NULL, &c, 1);
                                if (c == 0x74) { // 't'
                                    IFile_Read(&f, NULL, &c, 1);
                                    if (c == 0x61) { // 'a'
                                        i = 100;
                                    }
                                }
                            }
                        }
                    }
                    
                    u32 subchunk2Size;
                    IFile_Read(&f, NULL, &subchunk2Size, 4);
                    
                    snd.numChannels = numChannels;
                    
                    if(bitsPerSample == 8) {
                        snd.ndspFormat = (numChannels == 1) ?
                            NDSP_FORMAT_MONO_PCM8 :
                            NDSP_FORMAT_STEREO_PCM8;
                    } else {
                        snd.ndspFormat = (numChannels == 1) ?
                            NDSP_FORMAT_MONO_PCM16 :
                            NDSP_FORMAT_STEREO_PCM16;
                    }
                    
                    snd.sampleRate = sampleRate;
                    snd.dataSize = subchunk2Size;
                    snd.bitsPerSample = bitsPerSample;
                    
                    snd.data = (u8*)(malloc(subchunk2Size));
                    IFile_Read(&f, NULL, snd.data, subchunk2Size);
                }
            }
        }
        
        IFile_Close(&f);
    }
    return snd;
}
*/

static ssize_t replace_read(void *file, void * buffer, size_t length)
{
    u64 total = 0;
    IFile_Read((IFile*)file, &total, buffer, length);
    return length;
}

off_t replace_lseek(void * file, off_t to, int whence)
{
    IFile_Seek((IFile*)file, to);
    IFile *f = (IFile*)file;
    return f->pos;
}

static inline bool isServiceUsable(const char *name)
{
    bool r;
    return R_SUCCEEDED(srvIsServiceRegistered(&r, name)) && r;
}

PHL_Sound snd;

void SoundThreadFunc(void *p)
{
    svcSleepThread(5e+9);
    snd.number = 1;
    snd.decode = true;
    Result ret = 0;

    while(isServiceUsable("csnd:SND") != true) svcSleepThread(1e+9);
    ret = csndInit();
    if(ret) *(u32*)ret = 0x128;
    bool decode = true;
    size_t done = 0;
    u8 *buffs[] = {snd.data, snd.data2};
    while(1)
    {
        /* Set DSP I2S Sound to 0 */
        *(vu16*)0x1EC45000 = ((*(vu16*)0x1EC45000 >> 6) << 6);
        u8 playing;
        csndIsPlaying(8, &playing);
        if(decode)
        {
            mpg123_read(snd.mh, (u8*)buffs[!snd.number], snd.dataSize, &done);
            done = done / sizeof(int16_t);
            decode = false;
        }

        if(!playing)
        {
            u8 *playbuf = buffs[!snd.number];
            svcFlushDataCacheRange(playbuf, snd.dataSize);
            csndPlaySound_(8, SOUND_FORMAT_16BIT, snd.sampleRate, 1.0f, 0, playbuf, NULL, snd.dataSize);
            decode = true;
            snd.number = !snd.number;
        }
        svcSleepThread(10);
    }
}

extern "C"
{
    extern u32 __ctru_heap, __ctru_heap_size, __ctru_linear_heap, __ctru_linear_heap_size;
    extern char *fake_heap_start;
    extern char *fake_heap_end;

    // this is called before main
    void __system_allocateHeaps(void)
    {
        u32 tmp=0;
        __ctru_heap_size = 1024 * 1024;
        // Allocate the application heap
        __ctru_heap = 0x08000000;
        svcControlMemoryEx(&tmp, __ctru_heap, 0x0, __ctru_heap_size, MEMOP_ALLOC, (MemPerm)(MEMPERM_READWRITE | MEMREGION_BASE), false);
        // Set up newlib heap
        fake_heap_start = (char*)__ctru_heap;
        fake_heap_end = fake_heap_start + __ctru_heap_size;
    }

    void __appInit() {
        srvSysInit();
        mappableInit(0x10000000, 0x14000000);
        fsSysInit();
    }

    // this is called after main exits
    void __appExit() {
        srvSysExit();
    }

    // stubs for non-needed pre-main functions
    void __sync_init();
    void __sync_fini();
    void __system_initSyscalls();
    void __libc_init_array(void);
    void __libc_fini_array(void);

    void initSystem(void (*retAddr)(void)) {
        __libc_init_array();
        __sync_init();
        __system_initSyscalls();
        __system_allocateHeaps();
        __appInit();
    }

    void __ctru_exit(int rc) {
        __appExit();
        __sync_fini();
        __libc_fini_array();
        svcExitProcess();
    }
}

struct handler {
    const char *name;
    int noofsessions;
};

const handler handlers[] = {
    {"pdn:s", 1},
    {"pdn:d", 1},
    {"pdn:i", 1},
    {"pdn:g", 1},
    {"pdn:c", 1}
};

static uint8_t ALIGN(8) threadstack[0x1000];

int main()
{   
    IPC ipc;

    const s32 SERVICECOUNT = 5;
    const s32 INDEXMAX = SERVICECOUNT * 2 + 1;
    const s32 REMOTESESSIONINDEX = SERVICECOUNT + 1;

    Handle sessionhandles[INDEXMAX];

    u32 serviceindexes[SERVICECOUNT];

    s32 handlecount = SERVICECOUNT + 1;

    for (int i = 1; i <= SERVICECOUNT; i++)
        ONERRSVCBREAK(srvRegisterService(&sessionhandles[i], handlers[i - 1].name, handlers[i -1].noofsessions));

    ONERRSVCBREAK(srvEnableNotification(&sessionhandles[0]));

    Handle target = 0;
    s32 targetindex = -1;
    int terminationflag = 0;
    int err = 0;
    int encoding = 0;
    int channels = 0;
    long int rate = 0;
    IFile f;
    if((err = mpg123_init()) != MPG123_OK)
        ;

    if((snd.mh = mpg123_new(NULL, &err)) == NULL)
    {
        ;//printf("Error: %s\n", mpg123_plain_strerror(err));
    }
    Result ret = IFile_Open(&f, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, NULL), fsMakePath(PATH_ASCII, "/sound.mp3"), FS_OPEN_READ);
    //printf("IFILE_Open: %08X\n", ret);

    mpg123_replace_reader_handle(snd.mh, replace_read, replace_lseek, NULL);
    if(mpg123_open_handle(snd.mh, &f) != MPG123_OK || mpg123_getformat(snd.mh, &rate, &channels, &encoding) != MPG123_OK)
    {
        ;//printf("Trouble with mpg123: %s\n", mpg123_strerror(mh));
    }
    mpg123_format_none(snd.mh);
    mpg123_format(snd.mh, rate * 2, channels, encoding);
    size_t buffSize = mpg123_outblock(snd.mh) * 32;
    snd.sampleRate = rate*2;
    snd.numChannels = channels;
    snd.dataSize = buffSize;
    snd.data = (u8*)malloc(buffSize * sizeof(uint16_t));
    snd.data2 = (u8*)malloc(buffSize * sizeof(uint16_t));

    MyThread t;

    s32 pio = 0;
    svcGetThreadPriority(&pio, CUR_THREAD_HANDLE );
    ret = MyThread_Create(&t, SoundThreadFunc, nullptr, threadstack, 0x1000, pio + 1, 0);
    if(ret != 0) *(u32*)0x987 = 123;
    for (;;) {
        s32 index;

        if (!target) {
            if (terminationflag && handlecount == REMOTESESSIONINDEX)
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
                ONERRSVCBREAK(0xd9875);
            }
            else if (index == -1) {
                if (lasttargetindex == -1) {
                    ONERRSVCBREAK(0xd9874);
                }
                else
                    index = lasttargetindex;
            }

            else if (index >= handlecount)
                ONERRSVCBREAK(-1);

            svcCloseHandle(sessionhandles[index]);
            handlecount--;
            for (s32 i = index - REMOTESESSIONINDEX; i < handlecount - REMOTESESSIONINDEX; i++) {
                sessionhandles[REMOTESESSIONINDEX + i] = sessionhandles[REMOTESESSIONINDEX + i + 1];
                serviceindexes[i] = serviceindexes[i + 1];
            }

            continue;
        }

        if (index == 0)
            HandleNotifications(&terminationflag);

        else if (index >= 1 && index < REMOTESESSIONINDEX) {
            Handle newsession = 0;
            ONERRSVCBREAK(svcAcceptSession(&newsession, sessionhandles[index]));

            if (handlecount >= INDEXMAX) {
                svcCloseHandle(newsession);
                continue;
            }

            sessionhandles[handlecount] = newsession;
            serviceindexes[handlecount - REMOTESESSIONINDEX] = index - 1;
            handlecount++;

        } else if (index >= REMOTESESSIONINDEX && index < INDEXMAX) {
            ipc.HandleCommands(serviceindexes[index -  REMOTESESSIONINDEX]);
            target = sessionhandles[index];
            targetindex = index;

        } else {
            ONERRSVCBREAK(-4);
        }
    }

    for (int i = 0; i < SERVICECOUNT; i++) {
        srvUnregisterService(handlers[i].name);
        svcCloseHandle(sessionhandles[i + 1]);
    }

    svcCloseHandle(sessionhandles[0]);
}