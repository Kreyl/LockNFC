/*
 * led_rgb.h
 *
 *  Created on: 31 рту. 2014 у.
 *      Author: Kreyl
 */

#ifndef LED_RGB_H_
#define LED_RGB_H_

#include "hal.h"
#include "color.h"
#include "ChunkTypes.h"
#include "cmd_uart.h"

#ifdef STM32F2XX
#include "kl_lib_f2xx.h"
#endif
//#include "kl_lib_f100.h"


#if 1 // ===================== Lite version: blinker only ======================
#define LED_RGB_BLINKER
class LedChnl_t {
public:
    GPIO_TypeDef *PGpio;
    uint16_t Pin;
    void Init() const { PinSetupOut(PGpio, Pin, omPushPull); }
    void Set(uint8_t AValue) { if(AValue != 0) PinSet(PGpio, Pin); else PinClear(PGpio, Pin); }
    LedChnl_t(GPIO_TypeDef *APGpio, uint16_t APin) : PGpio(APGpio), Pin(APin) {}
};

class LedRgbBlinker_t : public BaseSequencer_t<LedChunk_t> {
protected:
    LedChnl_t R, G, B;
    void ISwitchOff() { SetColor(clBlack); }
    SequencerLoopTask_t ISetup() {
        SetColor(IPCurrentChunk->Color);
        IPCurrentChunk++;   // Always increase
        return sltProceed;  // Always proceed
    }
public:
    LedRgbBlinker_t(const LedChnl_t ARed, const LedChnl_t AGreen, const LedChnl_t ABlue) :
        BaseSequencer_t(), R(ARed), G(AGreen), B(ABlue) {}
    void Init() {
        R.Init();
        G.Init();
        B.Init();
        SetColor(clBlack);
    }
    void SetColor(Color_t AColor) {
        R.Set(AColor.R);
        G.Set(AColor.G);
        B.Set(AColor.B);
    }
};
#endif

#if 1 // ============================== LedRGB =================================
#define LED_RGB
#define LED_RGB_TOP_VALUE   255 // Intencity 0...255
#define LED_RGB_INVERTED    invInverted
class LedChnlTmr_t : public LedChnl_t {
public:
    TIM_TypeDef *PTimer;
    uint32_t TmrChnl;
    void Set(const uint8_t AValue) const { *TMR_PCCR(PTimer, TmrChnl) = AValue; }    // CCR[N] = AValue
    void Enable()  const { TMR_ENABLE (PTimer); }
    void Disable() const { TMR_DISABLE(PTimer); }
    void Init() const {
        Timer_t::InitClock(PTimer);
        Timer_t::InitPwm(PTimer, PGpio, Pin, TmrChnl, LED_RGB_TOP_VALUE, LED_RGB_INVERTED);
        Enable();
    }
    LedChnlTmr_t(GPIO_TypeDef *APGpio, uint16_t APin, TIM_TypeDef *APTimer, uint32_t ATmrChnl) : LedChnl_t(APGpio, APin), PTimer(APTimer), TmrChnl(ATmrChnl) {}
};

#define LED_TOP_VALUE       255
#define LED_INVERTED_PWM    FALSE
// TimeToWaitBeforeNextAdjustment = SmoothVar / (N+4) + 1, where N - current LED brightness.

class LedRGB_t : public BaseSequencer_t<LedChunk_t> {
private:
    LedChnlTmr_t R, G, B;
    uint32_t ICalcDelay(uint32_t CurrentBrightness, uint32_t SmoothVar) { return (uint32_t)((SmoothVar / (CurrentBrightness+4)) + 1); }
    Color_t ICurrColor;
    void ISwitchOff() { SetColor(clBlack); }
    SequencerLoopTask_t ISetup() {
        if(ICurrColor != IPCurrentChunk->Color) {
            if(IPCurrentChunk->Value == 0) {     // If smooth time is zero,
                SetColor(IPCurrentChunk->Color); // set color now,
                ICurrColor = IPCurrentChunk->Color;
                IPCurrentChunk++;                // and goto next chunk
            }
            else {
                ICurrColor.Adjust(&IPCurrentChunk->Color);
                SetColor(ICurrColor);
                // Check if completed now
                if(ICurrColor == IPCurrentChunk->Color) IPCurrentChunk++;
                else { // Not completed
                    // Calculate time to next adjustment
                    uint32_t DelayR = (ICurrColor.R == IPCurrentChunk->Color.R)? 0 : ICalcDelay(ICurrColor.R, IPCurrentChunk->Value);
                    uint32_t DelayG = (ICurrColor.G == IPCurrentChunk->Color.G)? 0 : ICalcDelay(ICurrColor.G, IPCurrentChunk->Value);
                    uint32_t DelayB = (ICurrColor.B == IPCurrentChunk->Color.B)? 0 : ICalcDelay(ICurrColor.B, IPCurrentChunk->Value);
                    uint32_t Delay = DelayR;
                    if(DelayG > Delay) Delay = DelayG;
                    if(DelayB > Delay) Delay = DelayB;
                    SetupDelay(Delay);
                    return sltBreak;
                } // Not completed
            } // if time > 256
        } // if color is different
        else IPCurrentChunk++; // Color is the same, goto next chunk
        return sltProceed;
    }
public:
    LedRGB_t(const LedChnlTmr_t ARed, const LedChnlTmr_t AGreen, const LedChnlTmr_t ABlue) :
        BaseSequencer_t(), R(ARed), G(AGreen), B(ABlue) {}
    void Init() {
        R.Init();
        G.Init();
        B.Init();
        SetColor(clBlack);
    }
    void SetColor(Color_t AColor) {
        R.Set(AColor.R);
        G.Set(AColor.G);
        B.Set(AColor.B);
    }
};
#endif

#endif /* LED_RGB_H_ */
