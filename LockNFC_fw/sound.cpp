#include "sound.h"
#include <string.h>
#include "evt_mask.h"
#include "clocking.h"

Sound_t Sound;

// Mode register
//#define VS_MODE_REG_VALUE   0x0802  // Native SDI mode, Layer I + II enabled
#define VS_MODE_REG_VALUE   0x0803  // Native SDI mode, Layer I + II enabled, differential output

static const uint8_t SZero = 0;

static uint8_t ReadWriteByte(uint8_t AByte);

// ================================= IRQ =======================================
extern "C" {
// Dreq IRQ
CH_IRQ_HANDLER(EXTI2_IRQHandler) {
    CH_IRQ_PROLOGUE();
    chSysLockFromIsr();
    EXTI->PR = (1 << 2);  // Clean irq flag
//    Uart.Printf("Irq ");
    Sound.IDreq.DisableIrq();
    chEvtSignalI(Sound.PThread, VS_EVT_DREQ_IRQ);
    chSysUnlockFromIsr();
    CH_IRQ_EPILOGUE();
}
// DMA irq
void SIrqDmaHandler(void *p, uint32_t flags) {
    chSysLockFromIsr();
    chEvtSignalI(Sound.PThread, VS_EVT_DMA_DONE);
    chSysUnlockFromIsr();
}
} // extern c

// =========================== Implementation ==================================
static WORKING_AREA(waSoundThread, 512);
__attribute__((noreturn))
static void SoundThread(void *arg) {
    chRegSetThreadName("Sound");
    Sound.ITask();
}

__attribute__((noreturn))
void Sound_t::ITask() {
    while(true) {
        eventmask_t EvtMsk = chEvtWaitAny(ALL_EVENTS);
#if 1 // ==== DMA done ====
        if(EvtMsk & VS_EVT_DMA_DONE) {
            ISpi.WaitBsyLo();                   // Wait SPI transaction end
            if(Clk.AHBFreqHz > 12000000) Loop(450); // Make a solemn pause
            XCS_Hi();                           // }
            XDCS_Hi();                          // } Stop SPI
            // Send next data if VS is ready
            if(IDreq.IsHi()) ISendNextData();   // More data allowed, send it now
            else IDreq.EnableIrq(IRQ_PRIO_MEDIUM); // Enable dreq irq
        }
#endif

        if(EvtMsk & VS_EVT_DREQ_IRQ) {
            chThdSleepMilliseconds(1);  // Make a pause after IRQ rise
            ISendNextData();
        }

        // Play new request
        if(EvtMsk & VS_EVT_COMPLETED) {
//        	Uart.Printf("\rComp");
            AddCmd(VS_REG_MODE, 0x0004);    // Soft reset
            if(IFilename != NULL) IPlayNew();
            else {
//                AmpfOff();    // switch off the amplifier to save energy
                if(IPAppThd != nullptr) chEvtSignal(IPAppThd, EVTMSK_PLAY_ENDS);  // Raise event if nothing to play
            }
        }
        // Stop request
        else if(EvtMsk & VS_EVT_STOP) {
//            Uart.Printf("\rStop");
            PrepareToStop();
        }

#if 1 // ==== Read next ====
        else if(EvtMsk & VS_EVT_READ_NEXT) {
//            Uart.Printf("\rreadNext; L= %u %u", Buf1.DataSz, Buf2.DataSz);
            FRESULT rslt = FR_OK;
            bool Eof = f_eof(&IFile);
            // Read next if not EOF
            if(!Eof) {
                if     (Buf1.DataSz == 0) { /*Uart.Printf(" r1");*/ rslt = Buf1.ReadFromFile(&IFile); }
                else if(Buf2.DataSz == 0) { /*Uart.Printf(" r2");*/ rslt = Buf2.ReadFromFile(&IFile); }
            }
            if(rslt != FR_OK) Uart.Printf("sndReadErr=%u\r", rslt);
            if(rslt == FR_OK and !Eof) StartTransmissionIfNotBusy();
        }
#endif
    } // while true
}

void Sound_t::Init() {
    // ==== GPIO init ====
    PinSetupOut(VS_GPIO, VS_RST, omPushPull);
    PinSetupOut(VS_GPIO, VS_XCS, omPushPull);
    PinSetupOut(VS_GPIO, VS_XDCS, omPushPull);
    Rst_Lo();
    XCS_Hi();
    XDCS_Hi();
    chThdSleepMilliseconds(45);
    PinSetupIn(VS_GPIO, VS_DREQ, pudPullDown);
    PinSetupAlterFunc(VS_GPIO, VS_XCLK, omPushPull, pudNone, VS_AF);
    PinSetupAlterFunc(VS_GPIO, VS_SO,   omPushPull, pudNone, VS_AF);
    PinSetupAlterFunc(VS_GPIO, VS_SI,   omPushPull, pudNone, VS_AF);

    // ==== SPI init ====
    ISpi.Setup(VS_SPI, boMSB, cpolIdleLow, cphaFirstEdge, sbFdiv8);
    ISpi.Enable();
    ISpi.EnableTxDma();

    // ==== DMA ====
    // Here only unchanged parameters of the DMA are configured.
    dmaStreamAllocate     (VS_DMA, IRQ_PRIO_MEDIUM, SIrqDmaHandler, NULL);
    dmaStreamSetPeripheral(VS_DMA, &VS_SPI->DR);
    dmaStreamSetMode      (VS_DMA, VS_DMA_MODE);

    // ==== Variables ====
    State = sndStopped;
    IDmaIdle = true;
    PBuf = &Buf1;
    IAttenuation = VS_INITIAL_ATTENUATION;
    chMBInit(&CmdBox, CmdBuf, VS_CMD_BUF_SZ);

    // ==== Init VS ====
    Rst_Hi();
    chThdSleepMilliseconds(7);
    Clk.MCO1Enable(mco1HSE, mcoDiv1);   // Only after reset, as pins are grounded when Rst is Lo
    chThdSleepMilliseconds(7);
    // ==== DREQ IRQ ====
    IDreq.Setup(VS_GPIO, VS_DREQ, ttRising);
    // ==== Thread ====
    PThread = chThdCreateStatic(waSoundThread, sizeof(waSoundThread), NORMALPRIO, (tfunc_t)SoundThread, NULL);
#if VS_AMPF_EXISTS
    PinSetupOut(VS_AMPF_GPIO, VS_AMPF_PIN, omPushPull);
    AmpfOff();
#endif
}

void Sound_t::Shutdown() {
#if VS_AMPF_EXISTS
    AmpfOff();
#endif
    Clk.MCO1Disable();  // Switch clk off as XTALI & XTALO grounded in reset
    Rst_Lo();           // enter shutdown mode
}

void Sound_t::IPlayNew() {
    AmpfOn();
    AddCmd(VS_REG_MODE, VS_MODE_REG_VALUE);
    AddCmd(VS_REG_CLOCKF, (0x8000 + (12000000/2000)));
    AddCmd(VS_REG_VOL, ((IAttenuation * 256) + IAttenuation));

    FRESULT rslt;
    // Open new file
    Uart.Printf("Play %S at %u\r", IFilename, IStartPosition);
    rslt = f_open(&IFile, IFilename, FA_READ+FA_OPEN_EXISTING);
    IFilename = NULL;
    if (rslt != FR_OK) {
        if (rslt == FR_NO_FILE) Uart.Printf("%S: not found\r", IFilename);
        else Uart.Printf("OpenFile error: %u\r", rslt);
        Stop();
        return;
    }
    // Check if zero file
    if (IFile.fsize == 0) {
        f_close(&IFile);
        Uart.Printf("Empty file\r");
        Stop();
        return;
    }
    // Fast forward to start position if not zero
    if(IStartPosition != 0) {
        if(IStartPosition < IFile.fsize) f_lseek(&IFile, IStartPosition);
    }

    // Initially, fill both buffers
    if(Buf1.ReadFromFile(&IFile) != OK) { Stop(); return; }
    // Fill second buffer if needed
    if(Buf1.DataSz == VS_DATA_BUF_SZ) Buf2.ReadFromFile(&IFile);

    PBuf = &Buf1;
    State = sndPlaying;
    StartTransmissionIfNotBusy();
}

// ================================ Inner use ==================================
void Sound_t::AddCmd(uint8_t AAddr, uint16_t AData) {
    VsCmd_t FCmd;
    FCmd.OpCode = VS_WRITE_OPCODE;
    FCmd.Address = AAddr;
    FCmd.Data = __REV16(AData);
    // Add cmd to queue
    chMBPost(&CmdBox, FCmd.Msg, TIME_INFINITE);
    StartTransmissionIfNotBusy();
}

void Sound_t::ISendNextData() {
//    Uart.Printf("\rSN");
    dmaStreamDisable(VS_DMA);
    IDmaIdle = false;
    // ==== If command queue is not empty, send command ====
    msg_t msg = chMBFetch(&CmdBox, &ICmd.Msg, TIME_IMMEDIATE);
    if(msg == RDY_OK) {
//        Uart.PrintfI("\rvCmd: %A", &ICmd, 4, ' ');
        XCS_Lo();   // Start Cmd transmission
        dmaStreamSetMemory0(VS_DMA, &ICmd);
        dmaStreamSetTransactionSize(VS_DMA, sizeof(VsCmd_t));
        dmaStreamSetMode(VS_DMA, VS_DMA_MODE | STM32_DMA_CR_MINC);  // Memory pointer increase
        dmaStreamEnable(VS_DMA);
    }
    // ==== Send next chunk of data if any ====
    else switch(State) {
        case sndPlaying: {
//            Uart.PrintfI("\rD");
            // Switch buffer if required
            if(PBuf->DataSz == 0) {
                PBuf = (PBuf == &Buf1)? &Buf2 : &Buf1;      // Switch to next buf
//                Uart.Printf("\rB=%u; Sz=%u", ((PBuf == &Buf1)? 1 : 2), PBuf->DataSz);
                if(PBuf->DataSz == 0) { // Previous attempt to read the file failed
                    IDmaIdle = true;
                    PrepareToStop();
                    break;
                }
                else {
                    chSysLock();
                    chEvtSignalI(PThread, VS_EVT_READ_NEXT);    // Read next chunk of file
                    chSysUnlock();
                }
            }
            // Send next piece of data
            XDCS_Lo();  // Start data transmission
            uint32_t FLength = (PBuf->DataSz > 32)? 32 : PBuf->DataSz;
            dmaStreamSetMemory0(VS_DMA, PBuf->PData);
            dmaStreamSetTransactionSize(VS_DMA, FLength);
            dmaStreamSetMode(VS_DMA, VS_DMA_MODE | STM32_DMA_CR_MINC);  // Memory pointer increase
            dmaStreamEnable(VS_DMA);
//            if(PBuf == &Buf1) Uart.Printf("*"); else Uart.Printf("#");
            // Process pointers and lengths
            PBuf->DataSz -= FLength;
            PBuf->PData += FLength;
        } break;

        case sndWritingZeroes:
//            Uart.Printf("\rZ");
            if(ZeroesCount == 0) { // Was writing zeroes, now all over
                State = sndStopped;
                IDmaIdle = true;
//                Uart.Printf("\rvEnd");
                chSysLock();
                chEvtSignalI(PThread, VS_EVT_COMPLETED);
                chSysUnlock();
            }
            else SendZeroes();
            break;

        case sndStopped:
//            Uart.PrintfI("\rI");
            if(!IDreq.IsHi()) IDreq.EnableIrq(IRQ_PRIO_MEDIUM);
            else IDmaIdle = true;
    } // switch
}

void Sound_t::PrepareToStop() {
//    Uart.Printf("\rPrepare");
    State = sndWritingZeroes;
    ZeroesCount = ZERO_SEQ_LEN;
    if(IFile.fs != 0) f_close(&IFile);
    StartTransmissionIfNotBusy();
}

void Sound_t::SendZeroes() {
//    Uart.Printf("sz\r");
    XDCS_Lo();  // Start data transmission
    uint32_t FLength = (ZeroesCount > 32)? 32 : ZeroesCount;
    dmaStreamSetMemory0(VS_DMA, &SZero);
    dmaStreamSetTransactionSize(VS_DMA, FLength);
    dmaStreamSetMode(VS_DMA, VS_DMA_MODE);  // Do not increase memory pointer
    dmaStreamEnable(VS_DMA);
    ZeroesCount -= FLength;
}

uint8_t ReadWriteByte(uint8_t AByte) {
    VS_SPI->DR = AByte;
    while(!(VS_SPI->SR & SPI_SR_RXNE));
//    while(!(VS_SPI->SR & SPI_SR_BSY));
    return (uint8_t)(VS_SPI->DR);
}

// ==== Commands ====
uint8_t Sound_t::CmdRead(uint8_t AAddr, uint16_t* AData) {
//    uint8_t IReply;
    uint16_t IData;
    // Wait until ready
    //if ((IReply = BusyWait()) != OK) return IReply; // Get out in case of timeout
    XCS_Lo();   // Start transmission
    ReadWriteByte(VS_READ_OPCODE);  // Send operation code
    ReadWriteByte(AAddr);           // Send addr
    *AData = ReadWriteByte(0);      // Read upper byte
    *AData <<= 8;
    IData = ReadWriteByte(0);       // Read lower byte
    *AData += IData;
    XCS_Hi();   // End transmission
    return OK;
}
uint8_t Sound_t::CmdWrite(uint8_t AAddr, uint16_t AData) {
//    uint8_t IReply;
    // Wait until ready
//    if ((IReply = BusyWait()) != OK) return IReply; // Get out in case of timeout
    XCS_Lo();                       // Start transmission
    ReadWriteByte(VS_WRITE_OPCODE); // Send operation code
    ReadWriteByte(AAddr);           // Send addr
    ReadWriteByte(AData >> 8);      // Send upper byte
    ReadWriteByte(0x00FF & AData);  // Send lower byte
    XCS_Hi();                       // End transmission
    return OK;
}
