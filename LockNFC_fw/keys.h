/*
 * keys.h
 *
 *  Created on: 07.02.2013
 *      Author: kreyl
 */

#ifndef KEYS_H_
#define KEYS_H_

#include "hal.h"
#include "kl_lib_f2xx.h"
#include "kl_buf.h"

#include "SnsPins.h"

// Select required events. KeyPress is a must.
#define KEY_RELEASE     FALSE
#define KEY_LONGPRESS   FALSE
#define KEY_COMBO       TRUE

#define KEY_REPEAT_PERIOD_MS        306
#define KEY_LONGPRESS_DELAY_MS      801
#define KEYS_DELAY_BEFORE_REPEAT_MS (KEY_REPEAT_PERIOD_MS + KEY_LONGPRESS_DELAY_MS)

#define KEYS_EVT_Q_LEN              7

enum KeyName_t {keyA=0, keyB=1, keyC=2};

// Key status
struct Key_t {
    bool IsPressed
#if KEY_LONGPRESS
    , IsRepeating, IsLongPress
#endif
    ;
};

// KeyEvent: contains info about event type, count of participating keys and array with key IDs
enum KeyEvt_t {kePress, keLongPress, keRelease, keCancel, keRepeat, keCombo};
struct KeyEvtInfo_t {
    KeyEvt_t Type;
    uint8_t KeysCnt;
    uint8_t KeyID[KEYS_CNT];
};

class Keys_t {
private:
    Key_t Key[KEYS_CNT];
    systime_t RepeatTimer, LongPressTimer;
    bool IsCombo;
    void AddEvtToQueue(KeyEvtInfo_t Evt);
    void AddEvtToQueue(KeyEvt_t AType, uint8_t KeyIndx);
public:
    void ProcessKeysState(bool *PCurrentState);
    // Events
    CircBuf_t<KeyEvtInfo_t, KEYS_EVT_Q_LEN> EvtBuf;
};

extern Keys_t Keys;


#endif /* KEYS_H_ */
