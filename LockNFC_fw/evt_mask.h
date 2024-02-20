/*
 * evt_mask.h
 *
 *  Created on: Apr 12, 2013
 *      Author: g.kruglov
 */

#ifndef EVT_MASK_H_
#define EVT_MASK_H_

// Event masks
#define EVTMSK_NO_MASK          0

#define EVTMSK_UART_RX_POLL     EVENT_MASK(1)
#define EVTMSK_USB_READY        EVENT_MASK(2)
#define EVTMSK_PLAY_ENDS        EVENT_MASK(3)
#define EVTMSK_CARD_APPEARS     EVENT_MASK(4)
#define EVTMSK_CARD_DISAPPEARS  EVENT_MASK(5)
#define EVTMSK_KEYS             EVENT_MASK(6)
#define EVTMSK_STATE_TIMEOUT    EVENT_MASK(7)
// Indication
#define EVTMSK_DOOR_OPEN        EVENT_MASK(10)
#define EVTMSK_DOOR_SHUT        EVENT_MASK(11)
#define EVTMSK_BAD_KEY          EVENT_MASK(12)
#define EVTMSK_SECRET_KEY       EVENT_MASK(18)
// Sensors
#define EVTMSK_USB_CONNECTED    EVENT_MASK(21)
#define EVTMSK_USB_DISCONNECTED EVENT_MASK(22)

// Inner use
#define EVTMSK_PN_NEW_PKT       EVENT_MASK(27)
#define EVTMSK_PN_TX_COMPLETED  EVENT_MASK(28)
#define EVTMSK_PN_RX_COMPLETED  EVENT_MASK(29)

#endif /* EVT_MASK_H_ */
