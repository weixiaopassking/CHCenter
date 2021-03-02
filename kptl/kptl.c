#include <string.h>
#include <stdio.h>

#include "kptl.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))
#endif

#ifndef CH_OK
#define CH_OK   (0)
#endif

#ifndef CH_ERR
#define CH_ERR  (1)
#endif

/* get the patyload len in a frame packet */
uint32_t kptl_get_payload_len(kptl_t *p)
{
    return ARRAY2INT16(p->len);
}

/*  generate CRC16
    @param  currectCrc:     previous buffer pointer, if is a new start, pointer should refer a zero uint16_t
    @param  src:            current buffer pointer
    @param  lengthInBytes:  length of current buf
*/
void crc16_update(uint16_t *currectCrc, const uint8_t *src, uint32_t lengthInBytes)
{
    uint32_t crc = *currectCrc;
    uint32_t j;
    for (j=0; j < lengthInBytes; ++j)
    {
        uint32_t i;
        uint32_t byte = src[j];
        crc ^= byte << 8;
        for (i = 0; i < 8; ++i)
        {
            uint32_t temp = crc << 1;
            if (crc & 0x8000)
            {
                temp ^= 0x1021;
            }
            crc = temp;
        }
    } 
    *currectCrc = crc;
}


enum status
{
    kStatus_Idle,
    kStatus_Cmd,
    kStatus_LenLow,
    kStatus_LenHigh,
    kStatus_CRCLow,
    kStatus_CRCHigh,
    kStatus_Data,
};

 /**
 * @brief  initalize packet decoder
 * @param  packet decoder struct
 * @retval 0 succ    1 fail
 */
int kptl_decode_init(pkt_dec_t *d)
{
    d->cnt = 0;
    d->status = kStatus_Idle;
    if(!d->pkt)
    {
        return 1;
    }
    memset(d->pkt, 0, sizeof(kptl_t));
    return 0;
}

#define SAFE_CALL_CB    if(d->cb) d->cb(p)
    
 /**
 * @brief  decoder process
 * @note   call this function when received a byte
 * @param  d: decode handle, c: received byte
 * @retval CH_OK
 */

uint32_t kptl_decode(pkt_dec_t *d, uint8_t c)
{
    int ret = CH_ERR;
    uint16_t crc_calculated = 0;          /* CRC value caluated from a frame */
    kptl_t *p = d->pkt;
    uint8_t *payload_buf = (uint8_t*)d->pkt->payload;
    
    switch(d->status)
    {
        case kStatus_Idle:
            if(c == kFramingPacketStartByte)
            {
                d->status = kStatus_Cmd;
                p->hr.start_byte = c;
            }
            break;
        case kStatus_Cmd:
            p->hr.packet_type = c;
            switch(c)
            {
                case kFramingPacketType_Command:
                    d->status = kStatus_LenLow;
                    break;
                case kFramingPacketType_Data:
                    d->status = kStatus_LenLow;
                    break;
                case kFramingPacketType_Ping:
                    SAFE_CALL_CB;
                    ret = CH_OK;
                    d->status = kStatus_Idle;
                    break;
                case kFramingPacketType_PingResponse:
                    p->len[0] = 0;
                    p->len[1] = 0;
                    d->cnt = 0;
                    d->status = kStatus_Data;
                    break;
                case kFramingPacketType_Ack:
                case kFramingPacketType_Nak:
                    d->status = kStatus_Idle;
                    SAFE_CALL_CB;
                    return CH_OK;
            }
            break;
        case kStatus_LenLow:
            p->len[0] = c;
            d->status = kStatus_LenHigh;
            break;
        case kStatus_LenHigh:
            p->len[1] = c;
            if(kptl_get_payload_len(p) <= MAX_PACKET_LEN)
            {
                d->status = kStatus_CRCLow;
            }
            else
            {
                d->status = kStatus_Idle;
            }
            break;
        case kStatus_CRCLow:
            p->crc16[0] = c;
            d->status = kStatus_CRCHigh;
            break;
        case kStatus_CRCHigh:
            p->crc16[1] = c;
            d->cnt = 0;
            d->status = kStatus_Data;
            break;
        case kStatus_Data:
            payload_buf[d->cnt++] = c;
                   
            if((p->hr.packet_type == kFramingPacketType_Command || p->hr.packet_type == kFramingPacketType_Data) && d->cnt >= kptl_get_payload_len(p))
            {
                /* calculate CRC */
                crc_calculated = 0;
                crc16_update(&crc_calculated, (uint8_t*)&p->hr, 2);
                crc16_update(&crc_calculated, p->len, 2);
                crc16_update(&crc_calculated, payload_buf, d->cnt);
                
                /* CRC match */
                if(crc_calculated == ARRAY2INT16(p->crc16))
                {
                    SAFE_CALL_CB;
                    ret = CH_OK;
                }
                d->status = kStatus_Idle;
            }
            
            if(p->hr.packet_type == kFramingPacketType_PingResponse && d->cnt >= 8) /* ping response */
            {
                p->len[0] = 8;
                p->len[1] = 0;
                SAFE_CALL_CB;
                d->status = kStatus_Idle;
            }
            
            break;
        default:
            d->status = kStatus_Idle;
            break;
    }
    return ret;
}

