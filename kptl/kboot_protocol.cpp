#include "kboot_protocol.h"

/* processing bootloder protocol */

static uint8_t resp_cnt = 0;

void kptl_callback(kptl_t *pkt)
{
    Q_UNUSED(pkt);
    resp_cnt++;
}

kboot_protocol::kboot_protocol()
{
    static kptl_t rx_pkt;
    this->dec.pkt = &rx_pkt;
    this->dec.cb = kptl_callback;
    kptl_decode_init(&this->dec);

}

kboot_protocol::~kboot_protocol()
{

}

void kboot_protocol::slt_serial_read(QByteArray &ba)
{
    this->brx.append(ba);
}

void kboot_protocol::delay(uint32_t ms)
{
    QEventLoop eventloop;
    QTimer::singleShot(ms, &eventloop, SLOT(quit()));
    eventloop.exec();
}

/* expected_len: if recv data count within timeout == expected_len, return:true */
bool kboot_protocol::serial_send_then_recv(QByteArray &tx, QByteArray &rx, int expected_len, int timeout)
{
    emit sig_serial_send(tx);

    rx.clear();
    brx.clear();

    while(this->brx.size() < expected_len && timeout)
    {
        delay(1);
        timeout--;
    }

    rx.append(brx);
    return (rx.size() == expected_len);
}



QByteArray kboot_protocol::cmd_packet(uint8_t tag, uint8_t param_cnt, uint32_t *param, int expected_len)
{
    kptl_t fp;
    cmd_hdr_t cp;

    cp.tag = tag;
    cp.param_cnt = param_cnt;
    cp.reserved = 0;
    cp.flags = 0;

    kptl_create_cmd_packet(&fp, &cp, param);

    QByteArray btx((const char*)&fp, kptl_get_frame_size(&fp));
    QByteArray brx;
    brx.clear();

    serial_send_then_recv(btx, brx, expected_len);

    /* check if data recv ok */
    resp_cnt = 0;
    for(int i=0; i<brx.size(); i++)
    {
        kptl_decode(&this->dec, brx.at(i));
    }

    if(resp_cnt == 0)
    {
        brx.clear();
    }

    return brx;
}


QByteArray kboot_protocol::cmd_get_property(uint8_t property_code)
{
    QByteArray brx;
    uint32_t tx_param = property_code;
    uint8_t param_cnt = 1;
    brx = cmd_packet(kCommandTag_GetProperty, param_cnt, &tx_param, 20);
    if(brx.size() == 20)
    {
        brx.remove(0, 8);
        brx.remove(0, 8);
        return brx;
    }

    brx.clear();
    return brx;
}

bool kboot_protocol::cmd_flash_erase_region(uint32_t addr, uint32_t len)
{
    QByteArray ba;

    uint32_t param[2];

    param[0] = addr;
    param[1] = len;
    ba = cmd_packet(kCommandTag_FlashEraseRegion, 2, param, 20);
    return (ba.size() == 20);

}

bool kboot_protocol::cmd_flash_write_memory(uint32_t addr, uint32_t len)
{
    QByteArray ba;
    uint32_t param[2];

    param[0] = addr;
    param[1] = len;
    ba = cmd_packet(kCommandTag_WriteMemory, 2, param, 20);
    return (ba.size() == 20);

}

bool kboot_protocol::cmd_reset()
{
    cmd_packet(kCommandTag_Reset, 0, NULL, 2);
    return true;
}


bool kboot_protocol::cmd_send_data_packet(QByteArray &buf, bool is_last)
{
    kptl_t fp;

    kptl_frame_packet_begin(&fp, kFramingPacketType_Data);
    kptl_frame_packet_add(&fp, buf.data(), buf.size());
    kptl_frame_packet_final(&fp);

    QByteArray ba;
    ba.append((const char*)&fp, kptl_get_frame_size(&fp));

    //qDebug(ba.toHex(','));
    if(is_last)
    {
        serial_send_then_recv(ba, ba, 20);
        if(ba.size() == 20)
        {
            return true;
        }
    }
    else
    {
        serial_send_then_recv(ba, ba, 2);
        if(ba.size() == 2)
        {
            return true;
        }
    }

    return false;
}




bool kboot_protocol::connect()
{
    QByteArray btx, brx;

    /* 5A A6 PING */
    packet_ping_t ping;
    kptl_create_ping(&ping);

    btx.clear();
    btx.append(reinterpret_cast<char *>(&ping), sizeof(packet_ping_t));

    serial_send_then_recv(btx, brx, 10);

    /* check if data recv ok */
    resp_cnt = 0;
    for(int i=0; i<brx.size(); i++)
    {
        kptl_decode(&this->dec, brx.at(i));
    }

    if(resp_cnt)
    {
        uint8_t * buf = reinterpret_cast<uint8_t*>(brx.data());
        if(buf[0] == 0x5A && buf[1] == 0xA7)
        {
            _bugfix = buf[2];
            _minor  = buf[3];
            _major  = buf[4];
        }
        else
        {
            return false;
        }
    }

    /* version */
    brx = cmd_get_property(0x01);
    if(brx.size() == 4)
    {
        this->_major = brx.at(2);
        this->_minor = brx.at(1);
        this->_bugfix = brx.at(0);
    }
    else
        return false;

    /* get max packet size */
    brx = cmd_get_property(0x0B);
    if(brx.size() == 4)
    {
        this->_max_packet_size = (brx.at(0) | (brx.at(1)<<8) | brx.at(2)<<16 | brx.at(3)<<24);
    }
    else return false;

    /* flash size */
    brx = this->cmd_get_property(0x04);
    if(brx.size() == 4)
    {
        this->_flash_size = (brx.at(0) | (brx.at(1)<<8) | brx.at(2)<<16 | brx.at(3)<<24) / 1024;
    }
    else return false;

    //SDID
    brx = this->cmd_get_property(0x10);
    if(brx.size() == 4)
    {
        this->_sdid = brx.toHex();
    }
    else return false;

    //Flash Sector Size
    brx = this->cmd_get_property(0x05);
    if(brx.size() == 4)
    {
        this->_flash_sec_size = (brx.at(0) | (brx.at(1)<<8) | brx.at(2)<<16 | brx.at(3)<<24);
    }
    else return false;

    return true;
}



bool kboot_protocol::download(QByteArray image, int start_addr, int retry)
{

    if(!cmd_flash_erase_region(start_addr, image.size()))
    {
        return false;
    }

    int sz = image.size();
    int i = 0;

    if(!cmd_flash_write_memory(start_addr, image.size()))
    {
        return false;
    }

    while(i < sz)
    {
        int pkt_len = (sz - i) > _max_packet_size?(this->_max_packet_size):(sz - 1);

        QByteArray slice = image.mid(i, pkt_len);

        while(retry)
        {
            if(cmd_send_data_packet(slice, (slice.size() == this->_max_packet_size)?(false):(true)))
            {
                i += slice.size();
                break;
            }
            else
            {
                retry--;
            }
        }

        /* retry many times, return failed */
        if(retry == 0)
        {
            return false;
        }
        emit sig_download_progress(i*100 / sz);
    }

    cmd_reset();
    return true;
}



