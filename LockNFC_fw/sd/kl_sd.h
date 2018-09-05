/*
 * sd.h
 *
 *  Created on: 13.02.2013
 *      Author: kreyl
 */

#ifndef SD_H_
#define SD_H_

#include "ff.h"
#include "ch.h"
#include "hal.h"
#include "kl_lib_f2xx.h"
#include "cmd_uart.h"
#include "diskio.h"

#define MAX_NAME_LEN        128
#define INI_FILES_ENABLED   TRUE
#define USE_SD_INNER_FILE   TRUE

// See SDIO clock divider in halconf.h

#if INI_FILES_ENABLED // =============== ini file operations ===================
/*
 * ini file has the following structure:
 *
 * # This is Comment: comment uses either '#' or ';' symbol
 * ; This is Comment too
 *
 * [Section]    ; This is name of section
 * Count=6      ; This is key with value of int32
 * Volume=-1    ; int32
 * SoundFileName=phrase01.wav   ; string
 *
 * [Section2]
 * Key1=1
 * ...
 */

#define SD_STRING_SZ    256 // for operations with strings

class iniFile_t {
private:
    char IStr[SD_STRING_SZ];
    FIL *PFile;
public:
    uint8_t ReadString(const char *ASection, const char *AKey, char **PPOutput);
    uint8_t ReadInt32 (const char *ASection, const char *AKey, int32_t *POutput);
    uint8_t ReadArray(const char *ASection, const char *AKey, uint8_t *p, uint32_t Sz);

    void WriteSection(const char *ASection) { f_printf(PFile, "[%S]\r\n", ASection); }
    void WriteInt32(const char *AKey, const int32_t AValue) { f_printf(PFile, "%S=%D\r\n", AKey, AValue); }
    void WriteArray(const char *AKey, uint8_t *p, uint32_t Sz) {
        f_printf(PFile, "%S=", AKey);
        for(uint32_t i=0; i<Sz; i++) f_printf(PFile, "%02X", *p++);
        f_putc('\r', PFile);
        f_putc('\n', PFile);
    }
    void WriteNewline() {
        f_putc('\r', PFile);
        f_putc('\n', PFile);
    }
    friend class sd_t;
};
#endif

class sd_t {
private:
    FATFS SDC_FS;
    char LongFileName[MAX_NAME_LEN];
    FILINFO FileInfo;
public:
    DIR Directory;
#if USE_SD_INNER_FILE || INI_FILES_ENABLED
    FIL File;  // Do not leave it opened
    uint8_t OpenRead(const char *AFileName) {
//        Uart.Printf("\rOpenR %S", AFileName);
        FRESULT Rslt = f_open(&File, AFileName, FA_READ+FA_OPEN_EXISTING);
        if(Rslt != FR_OK) {
            if (Rslt == FR_NO_FILE) Uart.Printf("%S: not found\r", AFileName);
            else Uart.Printf("%S: openFile error: %u\r", AFileName, Rslt);
            return FAILURE;
        }
        // Check if zero file
        if(File.fsize == 0) {
            f_close(&File);
            Uart.Printf("Empty file\r");
            return FAILURE;
        }
        return OK;
    }
    uint8_t OpenRewrite(const char *AFileName) {
//        Uart.Printf("\rOpenW %S", AFileName);
        FRESULT Rslt = f_open(&File, AFileName, FA_WRITE+FA_CREATE_ALWAYS);
        if(Rslt != FR_OK) Uart.Printf("%S: openFile error: %u\r", AFileName, Rslt);
        return OK;
    }
    void Close() { f_close(&File); }
#endif
    bool IsReady;
    void Init();
#if INI_FILES_ENABLED
    iniFile_t iniFile;
#endif
};

extern sd_t SD;


#endif /* SD_H_ */
