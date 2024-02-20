/*
 * IDStore.h
 *
 *  Created on: 25.11.2011
 *      Author: Kreyl
 *
 *      Beware: number of stored IDs and Save function are made for data to fit
 *      to (2 pages = 2048 bytes), and for page size of 1024 bytes. Change this if needed.
 *      Payload data size is (252 * 8 bytes = 2016 bytes) + (4 bytes of count) + (4 bytes of EraseCounter) = 2024 bytes
 */

#ifndef IDSTORE_H_
#define IDSTORE_H_

#include <inttypes.h>
#include "cmd_uart.h"
#include "ch.h"
#include "kl_sd.h"

#define ID_SZ_BYTES         8   // 7 bytes of Mifare Ultralite's ID
// Group types: Access, MasterAdder, MasterRemover, Secret
// Group sizes
#define ID_ACCESS_CNT       99
#define ID_ADDER_CNT        9
#define ID_REMOVER_CNT      9
#define ID_SECRET_CNT       9

#if 1 // ========== Files ==========
#define IDSTORE_FILENAME        "ID_Store.ini"

#define ID_GROUP_NAME_ACCESS    "AccessID"
#define ID_GROUP_NAME_ADDER     "MasterAdderID"
#define ID_GROUP_NAME_REMOVER   "MasterRemoverID"
#define ID_GROUP_NAME_SECRET    "SecretID"

#endif

#if 1 // ========================== Auxilary classes ===========================
// Struct of single ID. ID is 8-byte wide.
struct ID_t {
    union {
        uint32_t ID32[2];
        uint8_t ID8[8];
    } __attribute__ ((__packed__));
    void ConstructOfBuf(uint8_t *p) {
        chSysLock();
        memcpy(ID8, p, 8);
        chSysUnlock();
    }
    void Print() { Uart.Printf("\r%04X %04X\r", ID32[0], ID32[1]); }
    bool operator == (const ID_t &AID) { return (ID32[0] == AID.ID32[0]) and (ID32[1] == AID.ID32[1]); }
    bool operator != (const ID_t &AID) { return (ID32[0] != AID.ID32[0]) or  (ID32[1] != AID.ID32[1]); }
    ID_t& operator = (const ID_t &AID) { ID32[0] = AID.ID32[0]; ID32[1] = AID.ID32[1]; return *this; }
} __attribute__ ((__packed__));

// Array of IDs. Length is templated.
template <int32_t TCnt>
struct ID_Array_t {
    int32_t Cnt;        // Number of IDs stored
    ID_t ID[TCnt];      // IDs themselves
    void Erase() { Cnt = 0; }
    bool ContainsID(ID_t &sID, int32_t *PIndx = nullptr) {
        for(int32_t i=0; i<Cnt; i++) {
            if(ID[i] == sID) {
                if(PIndx != nullptr) *PIndx = i;
                return true;
            }
        }
        return false;
    }
    uint8_t Add(ID_t &sID) {
        if(ContainsID(sID)) {
            Uart.Printf("\rAlready in base");
            return OK;
        }
        else if(Cnt == TCnt) {
            Uart.Printf("\rBase overflow");
            return FAILURE;
        }
        else {
            ID[Cnt] = sID;
            Cnt++;
            return OK;
        }
    }
    uint8_t Remove(ID_t &sID) {
        int32_t indx=0;
        if(ContainsID(sID, &indx)) {
            if(indx != (Cnt-1)) {  // Check if not last
                for(int32_t i=indx; i<Cnt-1; i++) ID[i] = ID[i+1];
            }
            Cnt--;
            Uart.Printf("\rID removed, count = %u\r", Cnt);
            return OK;
        }
        else {
            Uart.Printf("\rNo such ID");
            return FAILURE;
        }
    }
    // Save/Load to SD card
    void Save(const char *GroupName) {
        SD.iniFile.WriteSection(GroupName);
        char *p, IDKey[11] = "ID";
        for(int32_t i=0; i<Cnt; i++) {
            p = Convert::Int32ToStr(i, &IDKey[2]);
            *p = 0; // End of string
            SD.iniFile.WriteArray(IDKey, ID[i].ID8, 8);
        }
        SD.iniFile.WriteNewline();
    }
    void Load(const char *GroupName) {
        Cnt = 0;
        char *p, IDKey[11] = "ID";
        while(Cnt < TCnt) {
            p = Convert::Int32ToStr(Cnt, &IDKey[2]);
            *p = 0; // End of string
            if(SD.iniFile.ReadArray(GroupName, IDKey, ID[Cnt].ID8, 8) == OK) Cnt++;
            else break; // Get out if failed to read array
        }
//        Uart.Printf("\rCnt=%u; %04X %04X; Id0=%A", Cnt, ID[0].ID32[0], ID[0].ID32[1], ID[0].ID8, 8, ' ');
    }
} __attribute__ ((__packed__));

#endif

enum IdKind_t {ikNone, ikAccess, ikAdder, ikRemover, ikSecret};

class IDStore_t {
private:
    ID_Array_t<ID_ACCESS_CNT>  IDAccess;
    ID_Array_t<ID_ADDER_CNT>   IDAdder;
    ID_Array_t<ID_REMOVER_CNT> IDRemover;
    ID_Array_t<ID_SECRET_CNT>  IDSecret;
public:
    bool HasChanged;
    // ID operations
    IdKind_t Check(ID_t &sID, int32_t *PIndx = nullptr);
    uint8_t Add(ID_t &sID, IdKind_t Kind) {
        uint8_t Rslt = FAILURE;
        switch(Kind) {
            case ikAccess:  Rslt = IDAccess.Add(sID);  break;
            case ikAdder:   Rslt = IDAdder.Add(sID);   break;
            case ikRemover: Rslt = IDRemover.Add(sID); break;
            default: break;
        }
        if(Rslt == OK) HasChanged = true;   // Failure means absence in base
        return Rslt;
    }
    uint8_t Remove(ID_t &sID, IdKind_t Kind) {
        uint8_t Rslt = FAILURE;
        switch(Kind) {
            case ikAccess:  Rslt = IDAccess.Remove(sID);  break;
            case ikAdder:   Rslt = IDAdder.Remove(sID);   break;
            case ikRemover: Rslt = IDRemover.Remove(sID); break;
            default: break;
        }
        if(Rslt == OK) HasChanged = true;   // Failure means absence in base
        return Rslt;
    }
    // Load/save
    void Load();
    void Save();
    void EraseAll() {
        IDAccess.Erase();
        IDAdder.Erase();
        IDRemover.Erase();
        HasChanged = true;
    }
};

#endif /* IDSTORE_H_ */
