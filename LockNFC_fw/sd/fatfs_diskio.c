/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2007        */
/*-----------------------------------------------------------------------*/
/* This is a stub disk I/O module that acts as front end of the existing */
/* disk I/O modules and attach it to FatFs module with common interface. */
/*-----------------------------------------------------------------------*/

#include "ch.h"
#include "hal.h"
#include "ffconf.h"
#include "diskio.h"

#if HAL_USE_MMC_SPI && HAL_USE_SDC
#error "cannot specify both MMC_SPI and SDC drivers"
#endif

#if HAL_USE_MMC_SPI
extern MMCDriver MMCD1;
#elif HAL_USE_SDC
extern SDCDriver SDCD1;
#else
#error "MMC_SPI or SDC driver must be specified"
#endif

// KL
#undef HAL_USE_RTC

extern void PrintfC(const char *format, ...);

#if HAL_USE_RTC
#include "chrtclib.h"
extern RTCDriver RTCD1;
#endif

/*-----------------------------------------------------------------------*/
/* Correspondence between physical drive number and physical drive.      */

#define MMC     0
#define SDC     0


// ====================== KL's semaphored read/write ===========================
Semaphore semSDRW;

bool SDRead(uint32_t startblk, uint8_t *buffer, uint32_t n) {
//    PrintfC("\r*%S ", chThdSelf()->p_name);
    msg_t msg = chSemWaitTimeout(&semSDRW, MS2ST(3600));
    if(msg == RDY_OK) {
//        PrintfC("%u %u\r", startblk, n);
//        PrintfC(" +%S ", chThdSelf()->p_name);
        bool rslt = sdcRead(&SDCD1, startblk, buffer, n);
        chSemSignal(&semSDRW);
//        PrintfC(" =%S ", chThdSelf()->p_name);
        return rslt;
    }
    else {
        PrintfC("\rSD sem=%d Thd=%S", msg, chThdSelf()->p_name);
        return false;
    }
}

bool SDWrite(uint32_t startblk, const uint8_t *buffer, uint32_t n) {
    msg_t msg = chSemWaitTimeout(&semSDRW, MS2ST(3600));
    if(msg == RDY_OK) {
        bool rslt = sdcWrite(&SDCD1, startblk, buffer, n);
        chSemSignal(&semSDRW);
        return rslt;
    }
    else return false;
}

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */

DSTATUS disk_initialize (
    BYTE drv                /* Physical drive nmuber (0..) */
)
{
  DSTATUS stat;

  switch (drv) {
#if HAL_USE_MMC_SPI
  case MMC:
    stat = 0;
    /* It is initialized externally, just reads the status.*/
    if (blkGetDriverState(&MMCD1) != BLK_READY)
      stat |= STA_NOINIT;
    if (mmcIsWriteProtected(&MMCD1))
      stat |=  STA_PROTECT;
    return stat;
#else
  case SDC:
    stat = 0;
    /* It is initialized externally, just reads the status.*/
    if (blkGetDriverState(&SDCD1) != BLK_READY)
      stat |= STA_NOINIT;
//    if (sdcIsWriteProtected(&SDCD1))
//      stat |=  STA_PROTECT;
    return stat;
#endif
  }
  return STA_NODISK;
}



/*-----------------------------------------------------------------------*/
/* Return Disk Status                                                    */

DSTATUS disk_status (
    BYTE drv        /* Physical drive nmuber (0..) */
)
{
  DSTATUS stat;

//  switch (drv) {
#if HAL_USE_MMC_SPI
  case MMC:
    stat = 0;
    /* It is initialized externally, just reads the status.*/
    if (blkGetDriverState(&MMCD1) != BLK_READY)
      stat |= STA_NOINIT;
    if (mmcIsWriteProtected(&MMCD1))
      stat |= STA_PROTECT;
    return stat;
#else
//  case SDC:
    stat = 0;
    /* It is initialized externally, just reads the status.*/
//    if (blkGetDriverState(&SDCD1) != BLK_READY)
//      stat |= STA_NOINIT;
//    if (sdcIsWriteProtected(&SDCD1))
//      stat |= STA_PROTECT;
    return stat;
#endif
//  }
//  return STA_NODISK;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */

DRESULT disk_read (
    BYTE drv,        /* Physical drive nmuber (0..) */
    BYTE *buff,        /* Data buffer to store read data */
    DWORD sector,    /* Sector address (LBA) */
    BYTE count        /* Number of sectors to read (1..255) */
)
{
//  switch (drv) {
#if HAL_USE_MMC_SPI
  case MMC:
    if (blkGetDriverState(&MMCD1) != BLK_READY)
      return RES_NOTRDY;
    if (mmcStartSequentialRead(&MMCD1, sector))
      return RES_ERROR;
    while (count > 0) {
      if (mmcSequentialRead(&MMCD1, buff))
        return RES_ERROR;
      buff += MMC_SECTOR_SIZE;
      count--;
    }
    if (mmcStopSequentialRead(&MMCD1))
        return RES_ERROR;
    return RES_OK;
#else
//  case SDC:
//    if (blkGetDriverState(&SDCD1) != BLK_READY) return RES_NOTRDY;
    if (SDRead(sector, buff, count)) return RES_ERROR;
    return RES_OK;
#endif
//  }
//  return RES_PARERR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */

#if _READONLY == 0
DRESULT disk_write (
    BYTE drv,            /* Physical drive nmuber (0..) */
    const BYTE *buff,    /* Data to be written */
    DWORD sector,        /* Sector address (LBA) */
    BYTE count            /* Number of sectors to write (1..255) */
)
{
#if HAL_USE_MMC_SPI
    switch (drv) {
  case MMC:
    if (blkGetDriverState(&MMCD1) != BLK_READY)
        return RES_NOTRDY;
    if (mmcIsWriteProtected(&MMCD1))
        return RES_WRPRT;
    if (mmcStartSequentialWrite(&MMCD1, sector))
        return RES_ERROR;
    while (count > 0) {
        if (mmcSequentialWrite(&MMCD1, buff))
            return RES_ERROR;
        buff += MMC_SECTOR_SIZE;
        count--;
    }
    if (mmcStopSequentialWrite(&MMCD1))
        return RES_ERROR;
    return RES_OK;
  case SDC:
#else
//    PrintfC("\r__DiskW");
//    if (blkGetDriverState(&SDCD1) != BLK_READY) return RES_NOTRDY;
    if (SDWrite(sector, buff, count)) return RES_ERROR;
    return RES_OK;
#endif
//  }
//  return RES_PARERR;
}
#endif /* _READONLY */



/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */

DRESULT disk_ioctl (
    BYTE drv,        /* Physical drive nmuber (0..) */
    BYTE ctrl,        /* Control code */
    void *buff        /* Buffer to send/receive control data */
)
{
  switch (drv) {
#if HAL_USE_MMC_SPI
  case MMC:
    switch (ctrl) {
    case CTRL_SYNC:
        return RES_OK;
    case GET_SECTOR_SIZE:
        *((WORD *)buff) = MMC_SECTOR_SIZE;
        return RES_OK;
#if _USE_ERASE
    case CTRL_ERASE_SECTOR:
        mmcErase(&MMCD1, *((DWORD *)buff), *((DWORD *)buff + 1));
        return RES_OK;
#endif
    default:
        return RES_PARERR;
    }
#else
  case SDC:
    switch (ctrl) {
    case CTRL_SYNC:
        return RES_OK;
    case GET_SECTOR_COUNT:
        *((DWORD *)buff) = mmcsdGetCardCapacity(&SDCD1);
        return RES_OK;
    case GET_SECTOR_SIZE:
        *((WORD *)buff) = MMCSD_BLOCK_SIZE;
        return RES_OK;
    case GET_BLOCK_SIZE:
        *((DWORD *)buff) = 256; /* 512b blocks in one erase block */
        return RES_OK;
#if _USE_ERASE
    case CTRL_ERASE_SECTOR:
        sdcErase(&SDCD1, *((DWORD *)buff), *((DWORD *)buff + 1));
        return RES_OK;
#endif
    default:
        return RES_PARERR;
    }
#endif
  }
  return RES_PARERR;
}

/*
bit31:25  Year origin from the 1980 (0..127)
bit24:21  Month (1..12)
bit20:16  Day of the month(1..31)
bit15:11  Hour (0..23)
bit10:5   Minute (0..59)
bit4:0    Second / 2 (0..29)
*/
DWORD get_fattime(void) {
#if HAL_USE_RTC
    return rtcGetTimeFat(&RTCD1);
#else
#define FAT_YEAR    2015
#define FAT_MONTH   1
#define FAT_DAY     1
    uint32_t tics = chTimeNow();
    uint32_t sec = (tics / 1000) % 60;
    uint32_t min = (tics / (1000 * 60)) % 60;
    uint32_t hour = (tics / (1000 * 60 * 60)) % 24;
    uint32_t year  = FAT_YEAR;
    uint32_t month = FAT_MONTH;
    uint32_t day   = FAT_DAY;
    return ((year - 1980) << 25)|(month << 21)|(day << 16)|(hour << 11)|(min << 5)|(sec >> 1);
    //return ((uint32_t)0 | (1 << 16)) | (1 << 21); /* wrong but valid time */
#endif
}
