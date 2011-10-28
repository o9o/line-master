#-------------------------------------------------
#
# Project created by QtCreator 2011-06-24T17:38:34
#
#-------------------------------------------------

QT       += core

QT       -= gui

TARGET = tomo
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

LIBS += -lglpk

SOURCES += main.cpp \
    tomodata.cpp \
    ../util/util.cpp \
    netquest.cpp \
    binarytomo.cpp

HEADERS += \
    tomodata.h \
    ../util/debug.h \
    ../util/util.h \
    netquest.h \
    binarytomo.h
