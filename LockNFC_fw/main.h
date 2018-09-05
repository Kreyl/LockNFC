/*
 * main.h
 *
 *  Created on: 30.03.2013
 *      Author: kreyl
 */

#ifndef MAIN_H_
#define MAIN_H_

#include "kl_sd.h"
#include "IDStore.h"
#include "evt_mask.h"
#include "led_rgb.h"
#include "Sequences.h"

#define USB_ENABLED TRUE

// Timings
#define STATE_TIMEOUT           18000   // ms; switch to waiting state
#define DOOR_CLOSE_TIMEOUT      9999    // ms

// Saving last ID to file
#define SAVE_LAST_ID            FALSE
#if SAVE_LAST_ID
#define LAST_ID_FILENAME        "Last_ID.txt"
#endif

// File folders
#define DIRNAME_GOOD_KEY        "GoodKey"
#define DIRNAME_BAD_KEY         "BadKey"
#define DIRNAME_DOOR_CLOSING    "Closing"
#define DIRNAME_SECRET          "Secret"

enum DoorState_t {dsClosed, dsOpen};
enum AppState_t  {
    asIdle,
    asAddingAccess, asRemovingAccess,
    asAddingAdder, asRemovingAdder,
    asAddingRemover, asRemovingRemover,
};
enum AddRemoveResult_t {arrAddingOk, arrAddingErr, arrRemovingOk};

void TmrGeneralCallback(void *p);

class App_t {
private:
    DoorState_t DoorState = dsClosed;
    VirtualTimer ITmr, IDoorTmr;
    void ProcessCardAppearance();
    void RestartStateTimer() {
        chSysLock();
        if(chVTIsArmedI(&ITmr)) chVTResetI(&ITmr);
        chVTSetI(&ITmr, MS2ST(STATE_TIMEOUT), TmrGeneralCallback, (void*)EVTMSK_STATE_TIMEOUT);
        chSysUnlock();
    }
    // States and indication
    void EnterState(AppState_t NewState);
    void ShowAddRemoveResult(AddRemoveResult_t Rslt);

#if SAVE_LAST_ID
    ID_t LastID;
#endif
public:
    Thread *PThd; // Main thread
    AppState_t State = asIdle;
    ID_t CurrentID;
    IDStore_t IDStore;
    void SendEvt(uint32_t EvtMsk) {
        chSysLock();
        chEvtSignalI(PThd, EvtMsk);
        chSysUnlock();
    }
    // Inner use
    void ITask();
    uint8_t ReadConfig();
};

extern App_t App;
extern LedRgbBlinker_t LedService;

#endif /* MAIN_H_ */
