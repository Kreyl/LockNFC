/*
 * pn.cpp
 *
 *  Created on: 09 џэт. 2015 у.
 *      Author: Kreyl
 */

#include "pn.h"
#include "ch.h"
#include "cmd_uart.h"
#include "evt_mask.h"
#include "main.h"

//#define DBG_PINS

#ifdef DBG_PINS
#define DBG_GPIO1   GPIOC
#define DBG_PIN1    5
#define DBG1_SET()  PinSet(DBG_GPIO1, DBG_PIN1)
#define DBG1_CLR()  PinClear(DBG_GPIO1, DBG_PIN1)
#endif

//#define PRINT_IO
//#define PRINT_DATA
//#define PRINT_TAGS

PN532_t Pn;

extern "C" {
    void PnDmaTxCompIrq(void *p, uint32_t flags);
    void PnDmaRxCompIrq(void *p, uint32_t flags);
}

// Thread
static WORKING_AREA(waPnThread, 256);
__attribute__ ((__noreturn__))
static void PnThread(void *arg) {
    chRegSetThreadName("PnTask");
    Pn.ITask();
}

void PN532_t::Init() {
#ifdef DBG_PINS
    PinSetupOut(DBG_GPIO1, DBG_PIN1, omPushPull);
    DBG1_CLR();
#endif
    // ==== GPIO ====
    PinSetupOut(PN_GPIO, PN_RST_PIN, omPushPull, pudNone);
    PinSetupOut(PN_NSS_GPIO, PN_NSS_PIN, omPushPull, pudNone);
    PinSetupIn(PN_IRQ_GPIO, PN_IRQ_PIN, pudNone);
    PinSetupAlterFunc(PN_GPIO, PN_SCK_PIN, omPushPull, pudNone, AF5);
    PinSetupAlterFunc(PN_GPIO, PN_MISO_PIN, omPushPull, pudPullDown, AF5);
    PinSetupAlterFunc(PN_GPIO, PN_MOSI_PIN, omPushPull, pudNone, AF5);
    IRstLo();
    INssHi();
    // ==== SPI ====    LSB first, master, ClkLowIdle, FirstEdge, Baudrate=f/2
    ISpi.Setup(PN_SPI, boLSB, cpolIdleLow, cphaFirstEdge, sbFdiv8);
    ISpi.Enable();
    // ==== DMA ====
    // Tx
    dmaStreamAllocate(PN_TX_DMA, IRQ_PRIO_MEDIUM, PnDmaTxCompIrq, NULL);
    dmaStreamSetPeripheral(PN_TX_DMA, &PN_SPI->DR);
    dmaStreamSetMode      (PN_TX_DMA, PN_TX_DMA_MODE);
    // Rx
    dmaStreamAllocate(PN_RX_DMA, IRQ_PRIO_MEDIUM, PnDmaRxCompIrq, NULL);
    dmaStreamSetPeripheral(PN_RX_DMA, &PN_SPI->DR);
    dmaStreamSetMode      (PN_RX_DMA, PN_RX_DMA_MODE);
    // ==== IRQ ====
    IIrqPin.Setup(PN_IRQ_GPIO, PN_IRQ_PIN, ttFalling);
    // ==== Variables ====
    State = psSetup;
    PThd = chThdCreateStatic(waPnThread, sizeof(waPnThread), NORMALPRIO, (tfunc_t)PnThread, NULL);
}

void PN532_t::IReset() {
    IRstLo();
    chThdSleepMilliseconds(9);
    IRstHi();
    chThdSleepMilliseconds(9);
}

__attribute__ ((__noreturn__))
void PN532_t::ITask() {
    while(true) {
        switch (State) {
            case psConfigured:
                chThdSleepMilliseconds(PN_POLL_INTERVAL);
                if(!CardOk) {
                    if(CardAppeared()) {
                        if(MifareRead(0) == OK) {
                            CardOk = true;
//                            Uart.Printf("\rCard Appeared");
                            App.CurrentID.ConstructOfBuf(PReply->Buf);
                            App.SendEvt(EVTMSK_CARD_APPEARS);
                        }
                    } // if appeared
                }
                else {
                    if(!CardIsStillNear()) {
//                        Uart.Printf("\rCard Lost");
                        CardOk = false;
                        App.SendEvt(EVTMSK_CARD_DISAPPEARS);
                    }
                } // if Card is ok
                break;

            case psSetup:
                IReset();
                Cmd(PN_CMD_GET_FIRMWARE_VERSION, WITHOUT_DATA);  // First Cmd will be discarded
                Cmd(PN_CMD_GET_FIRMWARE_VERSION, WITHOUT_DATA);
                Cmd(PN_CMD_SAM_CONFIGURATION, 1, 0x01);          // Disable SAM to calm PN: Normal mode, the SAM is not used
                Cmd(PN_CMD_RF_CONFIGURATION, 4, 0x05, 0x02, 0x01, 0x05);
                Cmd(PN_CMD_RF_CONFIGURATION, 4, 0x02, 0x00, 0x0B, 0x10);
                State = psConfigured;
                break;

            case psOff:
                chSysLock();
                chSchGoSleepS(THD_STATE_SUSPENDED);
                chSysUnlock();
                break;
        } // switch
    } // while true
}

bool PN532_t::CardAppeared() {
    FieldOn();
    if(Cmd(PN_CMD_IN_LIST_PASSIVE_TARGET, 2, 0x01, 0x00) == OK) {
        if(PReply->RplCode != PN_RPL_IN_LIST_PASSIVE_TARGET or PReply->NbTg == 0) { // Incorrect reply or Nothing found
            FieldOff();
            return false;
        }
        // ==== Tag is found ====
#ifdef PRINT_TAGS
        Uart.Printf("\rTag1: %A", PReply->Buf, (RxDataSz-3), ' '); // without TFI, Rpl code and NbTg
#endif
        return true;
    }
    else return false;
}

bool PN532_t::CardIsStillNear() {
    // Try to read data from Mifare to determine if it still near
    //if(MifareRead(nullptr, 0) == OK) return true;
    if(Cmd(PN_CMD_IN_DESELECT, 1, 0x01) == OK) {
        if(Cmd(PN_CMD_IN_SELECT, 1, 0x01) == OK) {
            if(PReply->Err == 0) return true;
        }
    }
    FieldOff();
    return false;
}

// Read 16 bytes starting from address AAddr into ABuf.
uint8_t PN532_t::MifareRead(uint32_t AAddr) {
    if(Cmd(PN_CMD_IN_DATA_EXCHANGE, 3, 0x01, MIFARE_CMD_READ, AAddr) == OK) {
        //klPrintf("PN reply: %H\r", Buf, Length);
        if ((PReply->RplCode == PN_CMD_IN_DATA_EXCHANGE+1) and (PReply->Err == 0x00)) { // Correct reply & errorcode == 0
            return OK;
        }
    }
    return FAILURE;
}

#if 1 // ========================== Data exchange ==============================
uint8_t PN532_t::Cmd(uint8_t CmdID, uint32_t ADataLength, ...) {
    uint8_t Rslt = OK;
    uint32_t FLength;
    IBuf[0] = PN_PRE_DATA_WRITE;
    // Prologue
    PrologueExt->Preamble = 0x00;     // Always
    PrologueExt->SoP0     = 0x00;     // Always
    PrologueExt->SoP1     = 0xFF;     // Always
    PrologueExt->NPLC     = 0xFF;     // Always
    PrologueExt->NPL      = 0xFF;     // Always
    // Length = 1 (TFI) + 1 (CMD == PD0) + ADataLength
    FLength = 1 + 1 + ADataLength;
    PrologueExt->LengthHi = (uint8_t)((FLength >> 8) & 0xFF);
    PrologueExt->LengthLo = (uint8_t)( FLength       & 0xFF);
    PrologueExt->CalcLCS();           // LCS + LENGTH == 0
    // Data
    uint8_t *pd = &IBuf[PN_DATA_EXT_INDX];  // Beginning of TFI+Data
    *pd++ = PN_FRAME_TFI_TRANSMIT;
    *pd++ = CmdID;
    if(ADataLength != 0) {              // If data present copy it to ComboData Buffer
        va_list Arg;                    // Init Agr
        va_start(Arg, ADataLength);     // Set pointer to last argument
        for(uint32_t i=0; i<ADataLength; i++) *pd++ = (uint8_t)va_arg(Arg, int);
        va_end(Arg);
    }
    // Epilogue
    WriteEpilogue(FLength);
    // ==== Transmit frame ====
#ifdef PRINT_IO
    Uart.Printf("\r>> %A   ", IBuf, PN_TX_SZ(FLength), ' ');
#endif
#ifdef PRINT_DATA
    Uart.Printf("\r>> %A   ", &IBuf[PN_DATA_EXT_INDX], 2+ADataLength, ' ');
#endif
    INssLo();
    ITxRx(IBuf, nullptr, PN_TX_SZ(FLength));
    INssHi();
    // ======= Receive =======
    if((Rslt = ReceiveAck()) != OK) return Rslt;
    return ReceiveData();
}

uint8_t PN532_t::ReceiveAck() {
    uint8_t Rslt = OK;
    if((Rslt = WaitReplyReady(PN_ACK_TIMEOUT)) != OK) return Rslt;
    IBuf[0] = PN_PRE_DATA_READ;
    INssLo();
    ITxRx(IBuf, IBuf, PN_ACK_NACK_SZ+1);    // First byte is sequence "read"
    INssHi();
    PnAckNack_t *PAckNack = (PnAckNack_t*)&IBuf[1]; // First byte is reply to sequence "read"
    Rslt = (*PAckNack == PnPktAck)? OK : FAILURE;
#ifdef PRINT_IO
    Uart.Printf("\r<< %A   ", PAckNack, PN_ACK_NACK_SZ, ' ');
#endif
    return Rslt;
}

uint8_t PN532_t::ReceiveData() {
    uint8_t Rslt;
    uint8_t* PRxData;
    PReply = nullptr;
    if((Rslt = WaitReplyReady(PN_DATA_TIMEOUT)) != OK) return Rslt;
    IBuf[0] = PN_PRE_DATA_READ;
    INssLo();
    // Receive reply's prologue
    ITxRx(IBuf, IBuf, PROLOGUE_SZ+1);    // First byte is sequence "read"
    // Check if wrong beginning
    if(!Prologue->IsStartOk()) {
        Uart.Printf("\rBad start");
        INssHi();
        return FAILURE;
    }
    // Check if extended frame
    if(Prologue->IsExtended()) {
        ITxRx(IBuf, &PrologueExt->LengthHi, (PROLOGUE_EXT_SZ - PROLOGUE_SZ)); // Receive remainder of prologueExt
        // Check length crc
        if(!PrologueExt->IsLcsOk()) {
            Uart.Printf("\rBad Ext LCS");
            INssHi();
            return FAILURE;
        }
        PRxData = &IBuf[PN_DATA_EXT_INDX];
        RxDataSz = BuildUint16(PrologueExt->LengthLo, PrologueExt->LengthHi);
    }
    // Normal frame
    else {
        // Check length crc
        if(!Prologue->IsLcsOk()) {
            Uart.Printf("\rBad LCS");
            INssHi();
            return FAILURE;
        }
        PRxData = &IBuf[PN_DATA_NORMAL_INDX];
        RxDataSz = Prologue->Len;
    }
    ITxRx(IBuf, PRxData, (RxDataSz + EPILOGUE_SZ));  // Receive data and epilogue
    INssHi();
    // Check DCS
    uint8_t DCS = 0;
    for(uint32_t i=0; i < RxDataSz+1; i++) DCS += PRxData[i]; // TFI + D0 + D1 + ... + DCS
    if(DCS != 0) {
        Uart.Printf("\rBad DCS");
        return FAILURE;
    }
    // All ok
    PReply = (PnReply_t*)PRxData;
#ifdef PRINT_IO
    uint32_t TotalLength = DataSz + (Prologue->IsExtended()? PROLOGUE_EXT_SZ : PROLOGUE_SZ) + EPILOGUE_SZ;
    Uart.Printf("\r<< %A", Prologue, TotalLength, ' ');
#endif
#ifdef PRINT_DATA
    Uart.Printf("\r<< %A", PRxData, RxDataSz, ' ');
#endif
    return OK;
}

void PN532_t::ITxRx(void *PTx, void *PRx, uint32_t ALength) {
    ISpi.ClearOVR();
    chSysLock();
    uint32_t Msk = EVTMSK_PN_TX_COMPLETED;
    // RX
    if(PRx != nullptr) {
        dmaStreamSetMemory0(PN_RX_DMA, PRx);
        dmaStreamSetTransactionSize(PN_RX_DMA, ALength);
        dmaStreamSetMode(PN_RX_DMA, PN_RX_DMA_MODE);
        dmaStreamEnable(PN_RX_DMA);
        Msk = EVTMSK_PN_RX_COMPLETED;
        ISpi.EnableRxDma();
    }
    // TX
    dmaStreamSetMemory0(PN_TX_DMA, PTx);
    dmaStreamSetTransactionSize(PN_TX_DMA, ALength);
    dmaStreamSetMode(PN_TX_DMA, PN_TX_DMA_MODE);
    dmaStreamEnable(PN_TX_DMA);
    chSysUnlock();
    chEvtGetAndClearEvents(EVTMSK_PN_RX_COMPLETED | EVTMSK_PN_TX_COMPLETED);
    ISpi.EnableTxDma();
    if(chEvtWaitOneTimeout(Msk, MS2ST(PN_ACK_TIMEOUT)) == 0) Uart.Printf("\rTxRxTimeout");
    ISpi.WaitBsyLo();
}

uint8_t PN532_t::WaitReplyReady(uint32_t ATimeout) {
    // Enable IRQ and wait event
    IIrqPin.CleanIrqFlag();
    IIrqPin.EnableIrq(IRQ_PRIO_MEDIUM);
    if(chEvtWaitOneTimeout(EVTMSK_PN_NEW_PKT, MS2ST(ATimeout)) == 0) {
        chSysLock();
        IIrqPin.DisableIrq();
        chSysUnlock();
//        Uart.Printf("\rTimeout");
        return TIMEOUT;
    }
    else return OK;
}
#endif

#if 1 // ========================= IRQs ========================================
void PN532_t::IrqPinHandler() { // Interrupt caused by Low level on IRQ_Pin
    IIrqPin.CleanIrqFlag();     // Clear IRQ Pending Bit
    IIrqPin.DisableIrq();       // Disable IRQ
    chEvtSignalI(PThd, EVTMSK_PN_NEW_PKT);
}

extern "C" {
CH_IRQ_HANDLER(PN_IRQ_HANDLER) {
    CH_IRQ_PROLOGUE();
    chSysLockFromIsr();
    Pn.IrqPinHandler();
    chSysUnlockFromIsr();
    CH_IRQ_EPILOGUE();
}

// DMA transmission complete
void PnDmaTxCompIrq(void *p, uint32_t flags) {
    dmaStreamDisable(PN_TX_DMA);    // Disable DMA
    Pn.ISpi.DisableTxDma();         // Disable SPI DMA
    chSysLockFromIsr();
    chEvtSignalI(Pn.PThd, EVTMSK_PN_TX_COMPLETED);
    chSysUnlockFromIsr();
}
// DMA reception complete
void PnDmaRxCompIrq(void *p, uint32_t flags) {
    dmaStreamDisable(PN_RX_DMA); // Disable DMA
    Pn.ISpi.DisableRxDma();
    chSysLockFromIsr();
    chEvtSignalI(Pn.PThd, EVTMSK_PN_RX_COMPLETED);
    chSysUnlockFromIsr();
}
} // extern C
#endif
