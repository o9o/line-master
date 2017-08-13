#-------------------------------------------------
#
# Project created by QtCreator 2011-06-07T14:43:49
#
#-------------------------------------------------

QT       += core xml

QT       -= gui

TARGET   = line-router
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

DEFINES += LINE_EMULATOR

bundle.path = /usr/bin
bundle.files = $$TARGET deploycore.pl
INSTALLS += bundle

INCLUDEPATH += ../util/
#INCLUDEPATH += ../PF_RING-4.6.5/userland/c++ ../PF_RING-4.6.5/kernel ../PF_RING-4.6.5/kernel/plugins ../PF_RING-4.6.5/userland/libpcap-1.1.1-ring ../PF_RING-4.6.5/userland/lib
#QMAKE_LIBS += ../PF_RING-4.6.5/userland/c++/libpfring_cpp.a ../PF_RING-4.6.5/userland/lib/libpfring.a ../PF_RING-4.6.5/userland/libpcap-1.1.1-ring/libpcap.a
QMAKE_LIBS += /usr/local/lib/libpfring.a /usr/local/lib/libpcap.a -lunwind -lpcap


  QMAKE_CFLAGS += -std=gnu99 -fno-strict-overflow -fno-strict-aliasing -Wno-unused-local-typedefs -gdwarf-2
  QMAKE_CXXFLAGS += -std=c++11 -fno-strict-overflow -fno-strict-aliasing -Wno-unused-local-typedefs -gdwarf-2
  QMAKE_LFLAGS += -flto -fno-strict-overflow -fno-strict-aliasing

  QMAKE_CFLAGS += -fstack-protector-all --param ssp-buffer-size=4
  QMAKE_CXXFLAGS += -fstack-protector-all --param ssp-buffer-size=4
  QMAKE_LFLAGS += -fstack-protector-all --param ssp-buffer-size=4

  QMAKE_CFLAGS += -fPIE -pie -rdynamic
  QMAKE_CXXFLAGS += -fPIE -pie -rdynamic
  QMAKE_LFLAGS += -fPIE -pie -rdynamic

  QMAKE_CFLAGS += -Wl,-z,relro,-z,now
  QMAKE_CXXFLAGS += -Wl,-z,relro,-z,now
  QMAKE_LFLAGS += -Wl,-z,relro,-z,now

  QMAKE_CFLAGS_DEBUG += -g -fno-omit-frame-pointer -rdynamic
  QMAKE_CXXFLAGS_DEBUG += -g -fno-omit-frame-pointer -rdynamic
  QMAKE_LFLAGS_DEBUG += -g -fno-omit-frame-pointer -rdynamic

  QMAKE_CFLAGS_DEBUG += -fsanitize=address -fno-omit-frame-pointer
  QMAKE_CXXFLAGS_DEBUG += -fsanitize=address -fno-omit-frame-pointer
  QMAKE_LFLAGS_DEBUG += -fsanitize=address -fno-omit-frame-pointer -fuse-ld=gold

SOURCES += main.cpp \
    pfcount.cpp \
    qpairingheap.cpp \
    pconsumer.cpp \
    pscheduler.cpp \
    psender.cpp \
    bitarray.cpp \
    ../line-gui/netgraphpath.cpp \
    ../line-gui/netgraphnode.cpp \
    ../line-gui/netgraphedge.cpp \
    ../line-gui/netgraphconnection.cpp \
    ../line-gui/netgraphas.cpp \
    ../line-gui/netgraph.cpp \
    ../util/util.cpp \
    ../line-gui/route.cpp \
    ../tomo/tomodata.cpp

OTHER_FILES += \
    make-remote.sh \
    deploycore-template.pl \
    ../remote-config.sh

HEADERS += \
    qpairingheap.h \
    pscheduler.h \
    pconsumer.h \
    spinlockedqueue.h \
    psender.h \
    bitarray.h \
    ../line-gui/netgraphpath.h \
    ../line-gui/netgraphnode.h \
    ../line-gui/netgraphedge.h \
    ../line-gui/netgraphconnection.h \
    ../line-gui/netgraphas.h \
    ../line-gui/netgraph.h \
    ../util/util.h \
    ../util/debug.h \
    ../line-gui/route.h \
    ../tomo/tomodata.h









