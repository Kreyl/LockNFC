/*
 * Soundlist.cpp
 *
 *  Created on: 18 џэт. 2015 у.
 *      Author: Kreyl
 */

#include "Soundlist.h"
#include "cmd_uart.h"
#include "sound.h"

void SndList_t::Init() {

}

FRESULT SndList_t::CountFilesInDir(const char* DirName, uint32_t *PCnt) {
    FRESULT Rslt = f_opendir(&Dir, DirName);
    if(Rslt != FR_OK) return Rslt;
    *PCnt = 0;
    while(true) {
        Rslt = f_readdir(&Dir, &FileInfo);
        if(Rslt != FR_OK) return Rslt;
        if((FileInfo.fname[0] == 0) and (FileInfo.lfname[0] == 0)) return FR_OK;   // No files left
        else { // Filename ok, check if not dir
            if(!(FileInfo.fattrib & AM_DIR)) {
                // Check if wav or mp3
                char *FName = (FileInfo.lfname[0] == 0)? FileInfo.fname : FileInfo.lfname;
//                Uart.Printf("\r%S", FName);
                uint32_t Len = strlen(FName);
                if(Len > 4) {
                    if((strcasecmp(&FName[Len-3], "mp3") == 0) or (strcasecmp(&FName[Len-3], "wav") == 0)) (*PCnt)++;
                } // if Len>4
            } // if not dir
        } // Filename ok
    }
    return FR_OK;
}

void SndList_t::PlayRandomFileFromDir(const char* DirName) {
    uint32_t Cnt=0;
    FRESULT Rslt = CountFilesInDir(DirName, &Cnt);
    if(Rslt != FR_OK or Cnt == 0) return;       // Get out if nothing to play
//    Uart.Printf("\rR=%u; Cnt=%u", Rslt, Cnt);
    // Select number of file
    uint32_t N = 0;
    if(Cnt > 1) {   // Get random number if count > 1
        do {
            N = Random(Cnt-1);      // [0; Cnt-1]
        } while(N == PreviousN);    // skip same as previous
    }
//    Uart.Printf("; Random=%u", N);
    PreviousN = N;
    // Iterate files in dir until success
    uint32_t Counter = 0;
    Rslt = f_opendir(&Dir, DirName);
    if(Rslt != FR_OK) return;
    while(true) {
        Rslt = f_readdir(&Dir, &FileInfo);
        if(Rslt != FR_OK) return;
        if((FileInfo.fname[0] == 0) and (FileInfo.lfname[0] == 0)) return;  // somehow no files left
        else { // Filename ok, check if not dir
            if(!(FileInfo.fattrib & AM_DIR)) {
                // Check if wav or mp3
                char *FName = (FileInfo.lfname[0] == 0)? FileInfo.fname : FileInfo.lfname;
//                Uart.Printf("\r%S  Cnt=%u", FName, Counter);
                uint32_t Len = strlen(FName);
                if(Len > 4) {
                    if((strcasecmp(&FName[Len-3], "mp3") == 0) or (strcasecmp(&FName[Len-3], "wav") == 0)) {
                        if(N == Counter) {
                            // Build full filename with path
                            // Check if root dir. Empty string allowed, too
                            int Len = strlen(DirName);
                            if((Len > 1) or (Len == 1 and *DirName != '/' and *DirName != '\\')) {
                                strcpy(Filename, DirName);
                                Filename[Len] = '/';
                            }
                            strcpy(&Filename[Len+1], FName);
                            Sound.Play(Filename);
                            return;
                        }
                        else Counter++;
                    }
                } // if Len>4
            } // if not dir
        } // Filename o
    } // while true
}



