#############################################################################
# Makefile for building: udpping
# Generated by qmake (2.01a) (Qt 4.7.0) on: Fri Mar 25 16:40:42 2011
# Project:  ../udpping/udpping.pro
# Template: app
# Command: /home/ovi/qtsdk-2010.05/qt/bin/qmake -spec ../../../qtsdk-2010.05/qt/mkspecs/linux-g++ CONFIG+=debug -o Makefile ../udpping/udpping.pro
#############################################################################

####### Compiler, tools and options

CC            = gcc
CXX           = g++
DEFINES       = 
CFLAGS        = -pipe -g -Wall -W -D_REENTRANT $(DEFINES)
CXXFLAGS      = -pipe -g -Wall -W -D_REENTRANT $(DEFINES)
INCPATH       = -I../../../qtsdk-2010.05/qt/mkspecs/linux-g++ -I../udpping -I../../../qtsdk-2010.05/qt/include -I. -I../udpping -I.
LINK          = g++
LFLAGS        = -Wl,-rpath,/home/ovi/qtsdk-2010.05/qt/lib
LIBS          = $(SUBLIBS)  -L/home/ovi/qtsdk-2010.05/qt/lib -lpthread 
AR            = ar cqs
RANLIB        = 
QMAKE         = /home/ovi/qtsdk-2010.05/qt/bin/qmake
TAR           = tar -cf
COMPRESS      = gzip -9f
COPY          = cp -f
SED           = sed
COPY_FILE     = $(COPY)
COPY_DIR      = $(COPY) -r
STRIP         = strip
INSTALL_FILE  = install -m 644 -p
INSTALL_DIR   = $(COPY_DIR)
INSTALL_PROGRAM = install -m 755 -p
DEL_FILE      = rm -f
SYMLINK       = ln -f -s
DEL_DIR       = rmdir
MOVE          = mv -f
CHK_DIR_EXISTS= test -d
MKDIR         = mkdir -p

####### Output directory

OBJECTS_DIR   = ./

####### Files

SOURCES       = ../udpping/main.cpp \
		../udpping/server.cpp \
		../udpping/client.cpp \
		../udpping/util.cpp 
OBJECTS       = main.o \
		server.o \
		client.o \
		util.o
DIST          = ../../../qtsdk-2010.05/qt/mkspecs/common/g++.conf \
		../../../qtsdk-2010.05/qt/mkspecs/common/unix.conf \
		../../../qtsdk-2010.05/qt/mkspecs/common/linux.conf \
		../../../qtsdk-2010.05/qt/mkspecs/qconfig.pri \
		../../../qtsdk-2010.05/qt/mkspecs/modules/qt_webkit_version.pri \
		../../../qtsdk-2010.05/qt/mkspecs/features/qt_functions.prf \
		../../../qtsdk-2010.05/qt/mkspecs/features/qt_config.prf \
		../../../qtsdk-2010.05/qt/mkspecs/features/exclusive_builds.prf \
		../../../qtsdk-2010.05/qt/mkspecs/features/default_pre.prf \
		../../../qtsdk-2010.05/qt/mkspecs/features/debug.prf \
		../../../qtsdk-2010.05/qt/mkspecs/features/default_post.prf \
		../../../qtsdk-2010.05/qt/mkspecs/features/warn_on.prf \
		../../../qtsdk-2010.05/qt/mkspecs/features/qt.prf \
		../../../qtsdk-2010.05/qt/mkspecs/features/unix/thread.prf \
		../../../qtsdk-2010.05/qt/mkspecs/features/moc.prf \
		../../../qtsdk-2010.05/qt/mkspecs/features/resources.prf \
		../../../qtsdk-2010.05/qt/mkspecs/features/uic.prf \
		../../../qtsdk-2010.05/qt/mkspecs/features/yacc.prf \
		../../../qtsdk-2010.05/qt/mkspecs/features/lex.prf \
		../../../qtsdk-2010.05/qt/mkspecs/features/include_source_dir.prf \
		../udpping/udpping.pro
QMAKE_TARGET  = udpping
DESTDIR       = 
TARGET        = udpping

first: all
####### Implicit rules

.SUFFIXES: .o .c .cpp .cc .cxx .C

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o "$@" "$<"

.cc.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o "$@" "$<"

.cxx.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o "$@" "$<"

.C.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o "$@" "$<"

.c.o:
	$(CC) -c $(CFLAGS) $(INCPATH) -o "$@" "$<"

####### Build rules

all: Makefile $(TARGET)

$(TARGET):  $(OBJECTS)  
	$(LINK) $(LFLAGS) -o $(TARGET) $(OBJECTS) $(OBJCOMP) $(LIBS)


dist: 
	@$(CHK_DIR_EXISTS) ../udpping/.tmp/udpping1.0.0 || $(MKDIR) ../udpping/.tmp/udpping1.0.0 
	$(COPY_FILE) --parents $(SOURCES) $(DIST) ../udpping/.tmp/udpping1.0.0/ && $(COPY_FILE) --parents ../udpping/server.h ../udpping/client.h ../udpping/util.h ../udpping/.tmp/udpping1.0.0/ && $(COPY_FILE) --parents ../udpping/main.cpp ../udpping/server.cpp ../udpping/client.cpp ../udpping/util.cpp ../udpping/.tmp/udpping1.0.0/ && (cd `dirname ../udpping/.tmp/udpping1.0.0` && $(TAR) udpping1.0.0.tar udpping1.0.0 && $(COMPRESS) udpping1.0.0.tar) && $(MOVE) `dirname ../udpping/.tmp/udpping1.0.0`/udpping1.0.0.tar.gz . && $(DEL_FILE) -r ../udpping/.tmp/udpping1.0.0


clean:compiler_clean 
	-$(DEL_FILE) $(OBJECTS)
	-$(DEL_FILE) *~ core *.core


####### Sub-libraries

distclean: clean
	-$(DEL_FILE) $(TARGET) 
	-$(DEL_FILE) Makefile


check: first

mocclean: compiler_moc_header_clean compiler_moc_source_clean

mocables: compiler_moc_header_make_all compiler_moc_source_make_all

compiler_moc_header_make_all:
compiler_moc_header_clean:
compiler_rcc_make_all:
compiler_rcc_clean:
compiler_image_collection_make_all: qmake_image_collection.cpp
compiler_image_collection_clean:
	-$(DEL_FILE) qmake_image_collection.cpp
compiler_moc_source_make_all:
compiler_moc_source_clean:
compiler_uic_make_all:
compiler_uic_clean:
compiler_yacc_decl_make_all:
compiler_yacc_decl_clean:
compiler_yacc_impl_make_all:
compiler_yacc_impl_clean:
compiler_lex_make_all:
compiler_lex_clean:
compiler_clean: 

####### Compile

main.o: ../udpping/main.cpp ../udpping/client.h \
		../udpping/server.h \
		../udpping/util.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o main.o ../udpping/main.cpp

server.o: ../udpping/server.cpp ../udpping/server.h \
		../udpping/util.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o server.o ../udpping/server.cpp

client.o: ../udpping/client.cpp ../udpping/client.h \
		../udpping/util.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o client.o ../udpping/client.cpp

util.o: ../udpping/util.cpp ../udpping/util.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o util.o ../udpping/util.cpp

####### Install

install:   FORCE
	$(INSTALL_PROGRAM) udpping /usr/bin/udpping

uninstall:   FORCE
	$(DEL_FILE) /usr/bin/udpping

FORCE:

