#-------------------------------------------------
#
# Project created by QtCreator 2011-03-25T14:38:58
#
#-------------------------------------------------

QT       -= core
QT       -= gui

TARGET = udpping
CONFIG   -= console
CONFIG   -= app_bundle

TEMPLATE = app

target.path = /usr/bin
INSTALLS += target

SOURCES += main.cpp \
    server.cpp \
    client.cpp \
    util.cpp

HEADERS += \
    server.h \
    client.h \
    util.h \
    messages.h

OTHER_FILES += \
    make-remote.sh
