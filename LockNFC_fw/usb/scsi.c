/*
 * scsi.c
 *
 *  Created on: Oct 9, 2013
 *      Author: g.kruglov
 */

#include "scsi.h"

const SCSI_InquiryResponse_t InquiryData = {
        Peripheral:         0x00,   // direct access block device, connected
        Removable:          0x80,   // device is removable
        Version:            0x04,   // SPC-2 compliance
        ResponseDataFormat: 0x02,   // Other values are either obsolete or reserved
        AdditionalLength:   0x1F,   // == 36-5: depends on data length, i.e. string length here
        Sccstp:             0x80,   // SCCS = 1: Storage Controller Component (Keil)
        bqueetc:            0x00,
        CmdQue:             0x00,
        VendorID:           "Ostranna",
        ProductID:          "MassStorage ",
        ProductRev:         "0001"
};

// USB Mass storage Page 0 Inquiry Data
const uint8_t Page00InquiryData[PAGE0_INQUIRY_DATA_SZ] = {
    0x00,
    0x00,
    0x00,
    (PAGE0_INQUIRY_DATA_SZ - 4),
    0x00,
    0x80,
    0x83
};

// USB Mass storage sense 6  Data
const uint8_t Mode_Sense6_data[MODE_SENSE6_DATA_SZ] = {
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00
};
