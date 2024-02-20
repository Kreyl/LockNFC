/*
 * SimpleSensors.h
 *
 *  Created on: 17 џэт. 2015 у.
 *      Author: Kreyl
 */

#ifndef KL_LIB_SIMPLESENSORS_H_
#define KL_LIB_SIMPLESENSORS_H_

/*
 * Simple sensors are sensors with two logic states: Low and High.
 * Every time state changes (edge occures) new event generated.
 * Single event generated per pin change.
 */

#include "kl_lib_f2xx.h"
#include "SnsPins.h"

class Sensors_t {
private:
public:
    void Init();
    void Shutdown() { for(uint8_t i=0; i<SNSGROUP_CNT; i++) SnsGroups[i]->Off(); }
    // Inner use
    void ITask();
};

extern Sensors_t Sensors;

#endif /* KL_LIB_SIMPLESENSORS_H_ */
