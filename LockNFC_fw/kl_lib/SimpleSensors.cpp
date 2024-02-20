/*
 * SimpleSensors.cpp
 *
 *  Created on: 17 џэт. 2015 у.
 *      Author: Kreyl
 */

#include "SimpleSensors.h"

Sensors_t Sensors;

// ==== Sensors Thread ====
static WORKING_AREA(waSensorsThread, 128);
__attribute__((noreturn))
static void SensorsThread(void *arg) {
    chRegSetThreadName("Sensors");
    Sensors.ITask();
}

void Sensors_t::Init() {
    // Init pins
    for(uint32_t i=0; i < SNSGROUP_CNT; i++) SnsGroups[i]->Init();
    // Create and start thread
    chThdCreateStatic(waSensorsThread, sizeof(waSensorsThread), LOWPRIO, (tfunc_t)SensorsThread, NULL);
}

__attribute__((noreturn))
void Sensors_t::ITask() {
    while(true) {
        chThdSleepMilliseconds(SNS_POLL_PERIOD_MS);
        // Iterate groups checking edge
        for(uint32_t i=0; i < SNSGROUP_CNT; i++) SnsGroups[i]->Check();
    } // while true
}
