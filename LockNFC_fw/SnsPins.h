/*
 * SnsPins.h
 *
 *  Created on: 17 џэт. 2015 у.
 *      Author: Kreyl
 */

#ifndef SNSPINS_H_
#define SNSPINS_H_

#include "ch.h"
#include "hal.h"
#include "evt_mask.h"
#ifdef STM32F2XX
#include "kl_lib_f2xx.h"
#elif defined STM32L1XX_MD || defined STM32L1XX_HD
#include "kl_lib_L15x.h"
#endif
// Here Threads defined
#include "main.h"

#if 1 // ============================ Classes ==================================
// Single pin setup data
struct SnsData_t {
    GPIO_TypeDef *PGpio;
    uint16_t Pin;
    PinPullUpDown_t Pud;
    void Init() const { PinSetupIn(PGpio, Pin, Pud); }
    void Off()  const { PinSetupAnalog(PGpio, Pin);  }
    bool IsHi() const { return PinIsSet(PGpio, Pin); }
};

struct SnsGroup_t {
    uint32_t Cnt;
    eventmask_t EvtMskRising, EvtMskFalling;    // } Events to send to
    Thread **PThd;                              // } the Thread
    ftVoidPVoid pFunction;                      // Function to call after check
    const SnsData_t *Pins;
    bool *WasHi;
    // Methods
    void Init() const { for(uint32_t i=0; i < Cnt; i++) Pins[i].Init(); }
    void Off()  const { for(uint32_t i=0; i < Cnt; i++) Pins[i].Off();  }
    void SignalEvtRising () const {
        chSysLock();
        chEvtSignalI(*PThd, EvtMskRising);
        chSysUnlock();
    }
    void SignalEvtFalling() const {
        chSysLock();
        chEvtSignalI(*PThd, EvtMskFalling);
        chSysUnlock();
    }
    void Check() const {
        for(uint32_t i=0; i < Cnt; i++) {
            if     (EvtMskRising  and  Pins[i].IsHi() and !WasHi[i]) SignalEvtRising();
            else if(EvtMskFalling and !Pins[i].IsHi() and  WasHi[i]) SignalEvtFalling();
            WasHi[i] = Pins[i].IsHi();
        } // for
        if(pFunction != nullptr) pFunction((void*)this);
    } // Check
};
#endif

// ================================= Settings ==================================

// ==== Keys group ====
const SnsData_t SnsKeyGroupData[] = {
        {GPIOC,  3, pudPullUp},  // A
        {GPIOC,  2, pudPullUp},  // B
        {GPIOC, 13, pudPullUp},  // C
};
#define KEYS_CNT    countof(SnsKeyGroupData)
static bool SnsKeysWasHi[KEYS_CNT];         // Required for any group to detect edges
// Pseudo function defining what is "Key Pressed"
#define KeyIsPressed(PinIsHi)   (!PinIsHi)  // Pin is Low == Key is pressed

extern void ProcessKeyState(void *p);       // Keys handler

const SnsGroup_t SnsGroupKey = {
        KEYS_CNT,
        EVTMSK_NO_MASK, EVTMSK_NO_MASK,     // signal nothing
        nullptr,                            // No thread
        &ProcessKeyState,                   // Keys handler
        SnsKeyGroupData,
        SnsKeysWasHi
};


// ==== USB group ====
#define SNS_USB_CNT     1
const SnsData_t SnsUsbData = {GPIOA,  9, pudPullDown};
static bool UsbKeysWasHi[SNS_USB_CNT];

const SnsGroup_t SnsGroupUsb = {
        SNS_USB_CNT,
        EVTMSK_USB_CONNECTED, EVTMSK_USB_DISCONNECTED, // Signal both
        &App.PThd,
        nullptr,
        &SnsUsbData,
        UsbKeysWasHi
};

// ============================== Common data ==================================
#define SNS_POLL_PERIOD_MS  72
// Group array
const SnsGroup_t* const SnsGroups[] = {
        &SnsGroupKey,
        &SnsGroupUsb,
};
#define SNSGROUP_CNT        countof(SnsGroups)

#endif /* SNSPINS_H_ */
