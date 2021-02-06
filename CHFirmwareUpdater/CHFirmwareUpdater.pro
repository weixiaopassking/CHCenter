QT       += core gui serialport
RC_FILE = exe_ico.rc

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    kptl/kboot_protocol.cpp \
    kptl/kptl.c \
    main.cpp \
    mainwindow.cpp \
    utilities/hex2bin.cpp \
    utilities/serial.cpp

HEADERS += \
    kptl/kboot_protocol.h \
    kptl/kptl.h \
    mainwindow.h \
    utilities/hex2bin.h \
    utilities/serial.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target