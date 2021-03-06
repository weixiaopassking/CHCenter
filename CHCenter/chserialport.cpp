#include "chserialport.h"

CHSerialport::CHSerialport(QObject *parent) : QObject(parent)
{
    //this->setParent(nullptr); this is unnecessary
    m_thread = new QThread();

    CH_serial = new QSerialPort();
    timer_framerate = new QTimer();

    //move those to second thread
    this->moveToThread(m_thread);
    CH_serial->moveToThread(m_thread);
    timer_framerate->moveToThread(m_thread);

    //cross threads signals
    connect(m_thread, SIGNAL(started()), this, SLOT(on_thread_started()),Qt::QueuedConnection);
    connect(m_thread, SIGNAL(finished()), this, SLOT(on_thread_stopped()),Qt::QueuedConnection);

    //be able to write data from baseform to serial port in second thread
    connect(this, SIGNAL(sigWriteData(QString)), this, SLOT(getsigWriteData(QString)));

    //be able to close port and thread from baseform
    connect(this, SIGNAL(sigCloseThreadAndPort()), this, SLOT(closeThreadAndPort()));

    timer_framerate->setInterval(1000);

}

CHSerialport::~CHSerialport()
{
    sigCloseThreadAndPort();
}


void CHSerialport::initThreadReading()
{
    if(m_thread->isRunning()){
        m_thread->quit();
        m_thread->wait();
    }
    m_thread->start();

}

int CHSerialport::openSerialport(QString port_name, int baudrate)
{

    CH_serial->setPortName(port_name);
    CH_serial->setBaudRate(baudrate);
    CH_serial->setDataBits(QSerialPort::Data8);
    CH_serial->setParity(QSerialPort::NoParity);
    CH_serial->setStopBits(QSerialPort::OneStop);
    CH_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!CH_serial->open(QIODevice::ReadWrite)) {
        return -1;
    }
    return 0;
}

void CHSerialport::closePort(){
    emit sigCloseThreadAndPort();
}

void CHSerialport::closeThreadAndPort()
{
    while(1){
        if(CH_serial->isOpen()) {
            CH_serial->disconnect();
            CH_serial->close();
        }
        else {
            emit sigPortClosed();
            break;
        }
    }
}
void CHSerialport::quitmThread(){
    m_thread->quit();
    m_thread->wait();
    qDebug()<<"Port and thread are closed";
}



void CHSerialport::linkCHdevices(QString port_name, int baudrate)
{
    imu_data_decode_init();

    m_port_name=port_name;
    m_baudrate=baudrate;
    initThreadReading();
}

void CHSerialport::countFrameRate()
{

    mutex_writing.lock();
    Frame_rate=frame_count;
    frame_count=0;
    mutex_writing.unlock();
    if(Frame_rate==0){
        checkPortStatus();
    }

}

void CHSerialport::checkPortStatus()
{
    QSerialPortInfo *info = new QSerialPortInfo;
    QList<QSerialPortInfo> list = info->availablePorts();
    bool port_still_available = false;

    for (QSerialPortInfo &port : list) {
        if (port.portName() == CH_serial->portName())
            port_still_available = true;
    }
    if (!port_still_available) {
        qDebug() << "Selected COM-port," << CH_serial->portName() << ", no longer available!";
        closePort();
        emit errorOpenPort();
    }
}



void CHSerialport::on_thread_started()
{

    connect(CH_serial, SIGNAL(readyRead()), this, SLOT(handleData()), Qt::QueuedConnection);
    connect(timer_framerate, SIGNAL(timeout()), this, SLOT(countFrameRate()),Qt::QueuedConnection);

    timer_framerate->start();
    int ret=openSerialport(m_port_name, m_baudrate);
    if(ret==-1){
        closePort();
        emit errorOpenPort();
    }
    else{
        emit sigPortOpened();
        //qDebug() << "serial port thread is:" << QThread::currentThreadId();
    }
}

void CHSerialport::on_thread_stopped()
{
    timer_framerate->stop();
    timer_framerate->disconnect();
    receive_gwsol.tag=0;
    receive_gwsol.n=0;
    m_number_of_node=0;
    m_is_gwsol=0;
}

void CHSerialport::getsigWriteData(QString str)
{
    QByteArray ba = str.toLocal8Bit();
    const char *c_str2 = ba.data();
    CH_serial->write(c_str2,100);
}


void CHSerialport::handleData()
{

    if(CH_serial->bytesAvailable() > 0 && CH_serial->isReadable())
    {
        QByteArray raw_data = CH_serial->readAll();

        mutex_writing.lock();

        protocol_ASC2(raw_data);
        protocol_0x5A(raw_data);

        mutex_writing.unlock();


        //        if(Is_msgMode){
        //            bool is_hexdata=0;

        //            for (int i=0;i<NumberOfBytesToRead;i++) {
        //                uint8_t c=raw_data[i];
        //                if(c == 0x5A){
        //                    is_hexdata=1;
        //                }
        //            }
        //            if(is_hexdata==1){
        //                m_IMUmsg=QString(raw_data.toHex()).toUpper();
        //                emit sigSendIMUmsg(m_IMUmsg);
        //                m_IMUmsg="";
        //            }
        //            else{

        //                m_IMUmsg+=arr;
        //                QStringList list1 = m_IMUmsg.split("\r\n");
        //                if(list1.lastIndexOf("OK")>=0){
        //                    emit sigSendIMUmsg(m_IMUmsg);
        //                    m_IMUmsg="";
        //                }
        //                else if(list1.lastIndexOf("ERR")>=0){
        //                    emit sigSendIMUmsg(m_IMUmsg);
        //                    m_IMUmsg="";
        //                }
        //                if(m_IMUmsg.size()>300){
        //                    emit sigSendIMUmsg(m_IMUmsg);
        //                    m_IMUmsg="";
        //                }
        //            }
        //        }

        //        if(m_frame_received!=frame_count){


        //            if(receive_gwsol.tag == KItemDongle || receive_gwsol.tag == KItemDongleRaw)
        //            {
        //                //if switch from single IMU module
        //                if(m_is_gwsol==0){
        //                    m_is_gwsol=1;
        //                    emit sigUpdateDongleNodeList(true,receive_gwsol);
        //                }

        //                //if the new numbers of nodes isn't equal to the list
        //                if(!(m_number_of_node==receive_gwsol.n)){
        //                    m_number_of_node=receive_gwsol.n;
        //                    emit sigUpdateDongleNodeList(true,receive_gwsol);
        //                }

        //                emit sigSendDongle(receive_gwsol);

        //            }
        //            else{
        //                if(m_is_gwsol==1){
        //                    m_is_gwsol=0;
        //                    emit sigUpdateDongleNodeList(false,receive_gwsol);
        //                }

        //                emit sigSendIMU(receive_imusol);

        //            }


        //            if(Content_bits!=bitmap){
        //                Content_bits=bitmap;
        //                sigSendBitmap(Content_bits);
        //            }
        //            m_frame_received=frame_count;

        //        }

        //        mutex_writing.unlock();
    }


}

void CHSerialport::protocol_0x5A(QByteArray binary_data)
{
    for (int i=0;i<binary_data.length();i++) {
        uint8_t c=binary_data[i];
        packet_decode(c);
    }

    //if there is new frame, send signal with the new frame to baseform and other classes.
    if(m_frame_received!=frame_count){


        if(receive_gwsol.tag == KItemDongle || receive_gwsol.tag == KItemDongleRaw)
        {
            //if switch from single IMU module
            if(m_is_gwsol==0){
                m_is_gwsol=1;
                emit sigUpdateDongleNodeList(true,receive_gwsol);
            }

            //if the new numbers of nodes isn't equal to the list
            if(!(m_number_of_node==receive_gwsol.n)){
                m_number_of_node=receive_gwsol.n;
                emit sigUpdateDongleNodeList(true,receive_gwsol);
            }

            emit sigSendDongle(receive_gwsol);

        }
        else{
            if(m_is_gwsol==1){
                m_is_gwsol=0;
                emit sigUpdateDongleNodeList(false,receive_gwsol);
            }

            emit sigSendIMU(receive_imusol);

        }


        if(Content_bits!=bitmap){
            Content_bits=bitmap;
            sigSendBitmap(Content_bits);
        }
        m_frame_received=frame_count;

    }
}

void CHSerialport::protocol_ASC2(QByteArray asc2_data)
{
    int rst=asc2_data.indexOf(0x5A);

    //has found 0x5A
    if(rst!=-1){
//        m_IMUmsg=QString(asc2_data.toHex()).toUpper();
//        emit sigSendIMUmsg(m_IMUmsg);
//        m_IMUmsg="";
    }

    //no found of 0x5A, meaning that the device sent ACS2
    else{
        m_IMUmsg+=asc2_data;
        if(m_IMUmsg.indexOf("OK")!=-1)
        {
            emit sigSendIMUmsg(m_IMUmsg);
            m_IMUmsg="";
        }
        else if(m_IMUmsg.indexOf("ERR")!=-1){
            emit sigSendIMUmsg(m_IMUmsg);
            m_IMUmsg="";
        }
        else{
            if(m_IMUmsg.size()>300){
                emit sigSendIMUmsg("Data decoded error.");
                m_IMUmsg="";
            }

        }
        //        QStringList list1 = m_IMUmsg.split("\r\n");
        //        if(list1.lastIndexOf("OK")>=0){
        //            emit sigSendIMUmsg(m_IMUmsg);
        //            m_IMUmsg="";
        //        }
        //        else if(list1.lastIndexOf("ERR")>=0){
        //            emit sigSendIMUmsg(m_IMUmsg);
        //            m_IMUmsg="";
        //        }
        //        if(m_IMUmsg.size()>300){
        //            emit sigSendIMUmsg(m_IMUmsg);
        //            m_IMUmsg="";
        //        }

    }
}

void CHSerialport::writeData(QString ATcmd)
{
    emit sigWriteData(ATcmd);
}
