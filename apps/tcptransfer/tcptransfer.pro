HEADERS       = dialog.h
SOURCES       = dialog.cpp \
                main.cpp
QT           -= gui
QT           += network

TARGET = tcptransfer

target.path = /usr/bin
INSTALLS += target

OTHER_FILES += \
    make-remote.sh
