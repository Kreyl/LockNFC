/*
 * ChunkTypes.h
 *
 *  Created on: 08 ���. 2015 �.
 *      Author: Kreyl
 */

#ifndef KL_LIB_CHUNKTYPES_H_
#define KL_LIB_CHUNKTYPES_H_

#include "color.h"
#include "ch.h"

enum ChunkSort_t {csSetup, csWait, csGoto, csEnd};

// The beginning of any sort of chunk. Everyone must contain it.
#define BaseChunk_t \
    ChunkSort_t ChunkSort;          \
    union {                         \
        uint32_t Value;             \
        uint32_t Time_ms;           \
        uint32_t ChunkToJumpTo;     \
    }


// RGB LED chunk
struct LedChunk_t {
    BaseChunk_t;
    Color_t Color;
} __attribute__((packed));
#define LED_CHUNK_SZ    sizeof(LedChunk_t)  // 8 bytes


#if 1 // ====================== Base sequencer class ===========================
enum SequencerLoopTask_t {sltProceed, sltBreak};

class BaseSequenceProcess_t {
protected:
    virtual SequencerLoopTask_t ISetup() = 0; // BaseSequencer_t::IProcessSequence() can call this
    virtual void ISwitchOff() = 0;  // BaseSequencer_t::Stop() can call this
public:
    virtual void IProcessSequenceI() = 0;   // Common timer callback can call this
};

// Common Timer callback
static void GeneralSequencerTmrCallback(void *p) {
    chSysLockFromIsr();
    ((BaseSequenceProcess_t*)p)->IProcessSequenceI();
    chSysUnlockFromIsr();
}

template <class TChunk>
class BaseSequencer_t : public BaseSequenceProcess_t {
private:
    VirtualTimer ITmr;
protected:
    const TChunk *IPStartChunk, *IPCurrentChunk;
    BaseSequencer_t() : IPStartChunk(nullptr), IPCurrentChunk(nullptr) {}
    void SetupDelay(uint32_t ms) { chVTSetI(&ITmr, MS2ST(ms), GeneralSequencerTmrCallback, this); }
public:
    void StartSequence(const TChunk *PLedChunk) {
        chSysLock();
        IPStartChunk = PLedChunk;   // Save first chunk
        IPCurrentChunk = PLedChunk;
        IProcessSequenceI();
        chSysUnlock();
    }
    void Stop() {
        chSysLock();
        if(chVTIsArmedI(&ITmr)) chVTResetI(&ITmr);
        ISwitchOff();
        chSysUnlock();
    }
    const TChunk* GetCurrentSequence() { return IPStartChunk; }

    void IProcessSequenceI() {
        if(chVTIsArmedI(&ITmr)) chVTResetI(&ITmr);  // Reset timer
        while(true) {   // Process the sequence
            switch(IPCurrentChunk->ChunkSort) {
                case csSetup: // setup now and exit if required
                    if(ISetup() == sltBreak) return;
                    break;

                case csWait: { // Start timer, pointing to next chunk
                        uint32_t Delay = IPCurrentChunk->Time_ms;
                        IPCurrentChunk++;
                        if(Delay != 0) {
                            SetupDelay(Delay);
                            return;
                        }
                    }
                    break;

                case csGoto:
                    IPCurrentChunk = IPStartChunk + IPCurrentChunk->ChunkToJumpTo;
                    SetupDelay(1);
                    return;
                    break;

                case csEnd:
                    return;
                    break;
            } // switch
        } // while
    } // IProcessSequenceI
};
#endif

#endif /* KL_LIB_CHUNKTYPES_H_ */
