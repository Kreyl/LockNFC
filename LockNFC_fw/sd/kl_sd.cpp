/*
 * kl_sd.cpp
 *
 *  Created on: 13.02.2013
 *      Author: kreyl
 */

#include "kl_sd.h"
#include "sdc.h"
#include "sdc_lld.h"
#include <string.h>
#include <stdlib.h>
#include "kl_lib_f2xx.h"

sd_t SD;
extern Semaphore semSDRW;

void sd_t::Init() {
    IsReady = FALSE;
#if INI_FILES_ENABLED
    iniFile.PFile = &File;
#endif
    // Bus pins
    PinSetupAlterFunc(GPIOC,  8, omPushPull, pudPullUp, AF12, ps50MHz); // DAT0
    PinSetupAlterFunc(GPIOC,  9, omPushPull, pudPullUp, AF12, ps50MHz); // DAT1
    PinSetupAlterFunc(GPIOC, 10, omPushPull, pudPullUp, AF12, ps50MHz); // DAT2
    PinSetupAlterFunc(GPIOC, 11, omPushPull, pudPullUp, AF12, ps50MHz); // DAT3
    PinSetupAlterFunc(GPIOC, 12, omPushPull, pudNone,   AF12, ps50MHz); // CLK
    PinSetupAlterFunc(GPIOD,  2, omPushPull, pudPullUp, AF12, ps50MHz); // CMD
    // Power pin
    PinSetupOut(GPIOC, 4, omPushPull, pudNone);
    PinClear(GPIOC, 4); // Power on
    chThdSleepMilliseconds(270);    // Let power to stabilize

    FRESULT err;
    sdcInit();
    sdcStart(&SDCD1, NULL);
    if(sdcConnect(&SDCD1)) {
        Uart.Printf("SD connect error\r");
        return;
    }
    else {
        Uart.Printf("SD capacity: %u\r", SDCD1.capacity);
    }
    err = f_mount(0, &SDC_FS);
    if(err != FR_OK) {
        Uart.Printf("SD mount error\r");
        sdcDisconnect(&SDCD1);
        return;
    }
    // Init RW semaphore
    chSemInit(&semSDRW, 1);
    IsReady = TRUE;
}

#if INI_FILES_ENABLED // ================ ini file operations ==================
// ==== Inner use ====
static inline char* skipleading(char *S) {
    while (*S != '\0' && *S <= ' ') S++;
    return S;
}
static inline char* skiptrailing(char *S, const char *base) {
    while ((S > base) && (*(S-1) <= ' ')) S--;
    return S;
}
static inline char* striptrailing(char *S) {
    char *ptr = skiptrailing(strchr(S, '\0'), S);
    *ptr='\0';
    return S;
}

uint8_t iniFile_t::ReadString(const char *ASection, const char *AKey, char **PPOutput) {
//    Uart.Printf("\rReadString: %S %S", ASection, AKey);
    f_lseek(PFile, 0); // Move to start of file
    // Move through file one line at a time until a section is matched or EOF.
    char *StartP, *EndP = nullptr;
    int32_t len = strlen(ASection);
    do {
        if(f_gets(IStr, SD_STRING_SZ, PFile) == nullptr) {
            Uart.Printf("\riniNoSection %S", ASection);
            return FAILURE;
        }
        StartP = skipleading(IStr);
        if((*StartP != '[') or (*StartP == ';') or (*StartP == '#')) continue;
        EndP = strchr(StartP, ']');
        if((EndP == NULL) or ((int32_t)(EndP-StartP-1) != len)) continue;
    } while (strncmp(StartP+1, ASection, len) != 0);

    // Section found, find the key
    len = strlen(AKey);
    do {
        if(!f_gets(IStr, SD_STRING_SZ, PFile) or *(StartP = skipleading(IStr)) == '[') {
//            Uart.Printf("\riniNoKey");
            return FAILURE;
        }
        StartP = skipleading(IStr);
        if((*StartP == ';') or (*StartP == '#')) continue;
        EndP = strchr(StartP, '=');
        if(EndP == NULL) continue;
    } while(((int32_t)(skiptrailing(EndP, StartP)-StartP) != len or strncmp(StartP, AKey, len) != 0));

    // Process Key's value
    StartP = skipleading(EndP + 1);
    // Remove a trailing comment
    uint8_t isstring = 0;
    for(EndP = StartP; (*EndP != '\0') and (((*EndP != ';') and (*EndP != '#')) or isstring) and ((uint32_t)(EndP - StartP) < SD_STRING_SZ); EndP++) {
        if (*EndP == '"') {
            if (*(EndP + 1) == '"') EndP++;     // skip "" (both quotes)
            else isstring = !isstring; // single quote, toggle isstring
        }
        else if (*EndP == '\\' and *(EndP + 1) == '"') EndP++; // skip \" (both quotes)
    } // for
    *EndP = '\0';   // Terminate at a comment
    striptrailing(StartP);
    *PPOutput = StartP;
    return OK;
}

uint8_t iniFile_t::ReadInt32(const char *ASection, const char *AKey, int32_t *POutput) {
    char *S = nullptr;
    if(ReadString(ASection, AKey, &S) == OK) {
        *POutput = strtol(S, NULL, 10);
        return OK;
    }
    else return FAILURE;
}

static inline uint8_t CharToByte(char c, uint8_t *Rslt) {
    if     (c >= '0' and c <= '9') *Rslt = c - '0';
    else if(c >= 'a' and c <= 'f') *Rslt = 10 + c - 'a';
    else if(c >= 'A' and c <= 'F') *Rslt = 10 + c - 'A';
    else return FAILURE;
    return OK;
}

uint8_t iniFile_t::ReadArray(const char *ASection, const char *AKey, uint8_t *p, uint32_t Sz) {
    char *S = nullptr;
    if(p == nullptr or Sz == 0) return FAILURE;
    for(uint32_t i=0; i<Sz; i++) p[i] = 0;
    if(ReadString(ASection, AKey, &S) == OK) {
        for(uint32_t i=0; i<Sz; i++) {
            if(*S == 0) return OK;
            uint8_t bHi, bLo;
            if(CharToByte(*S++, &bHi) != OK) return FAILURE;
            if(CharToByte(*S++, &bLo) != OK) return FAILURE;
            p[i] = (bHi << 4) | bLo;
        } // for i
        return OK;
    } // if read string
    else return FAILURE;
}


#endif

// ============================== Hardware =====================================
#ifdef __cplusplus
extern "C" {
#endif

bool_t sdc_lld_is_card_inserted(SDCDriver *sdcp) { return TRUE; }

bool_t sdc_lld_is_write_protected(SDCDriver *sdcp) { return FALSE; }

#ifdef __cplusplus
}
#endif
