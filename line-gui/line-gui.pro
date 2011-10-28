#-------------------------------------------------
#
# Project created by QtCreator 2011-03-28T15:57:20
#
#-------------------------------------------------

QT += core gui xml svg opengl

TARGET = line-gui
TEMPLATE = app

SOURCES += main.cpp\
        mainwindow.cpp \
    netgraph.cpp \
    netgraphnode.cpp \
    netgraphedge.cpp \
    netgraphscene.cpp \
    netgraphscenenode.cpp \
    netgraphsceneedge.cpp \
    ../util/util.cpp \
    netgraphpath.cpp \
    briteimporter.cpp \
    netgraphconnection.cpp \
    bgp.cpp \
    gephilayout.cpp \
    convexhull.cpp \
    netgraphas.cpp \
    netgraphsceneas.cpp \
    remoteprocessssh.cpp \
    netgraphsceneconnection.cpp \
    qdisclosure.cpp \
    qaccordion.cpp \
    qoplot.cpp \
    nicelabel.cpp \
    route.cpp \
    mainwindow_topologyeditor.cpp \
    mainwindow_newopensave.cpp \
    mainwindow_validation.cpp \
    mainwindow_scalability.cpp \
    mainwindow_batch.cpp \
    mainwindow_deployment.cpp \
    mainwindow_BRITE.cpp \
    mainwindow_remote.cpp \
    mainwindow_results.cpp \
    ../tomo/tomodata.cpp \
    qgraphicstooltip.cpp \
    flowlayout.cpp \
    qcoloredtabwidget.cpp

HEADERS  += mainwindow.h \
    netgraph.h \
    netgraphnode.h \
    netgraphedge.h \
    netgraphscene.h \
    netgraphscenenode.h \
    netgraphsceneedge.h \
    ../util/util.h \
    ../util/debug.h \
    netgraphpath.h \
    briteimporter.h \
    netgraphconnection.h \
    bgp.h \
    gephilayout.h \
    convexhull.h \
    netgraphas.h \
    netgraphsceneas.h \
    remoteprocessssh.h \
    netgraphsceneconnection.h \
    qdisclosure.h \
    qaccordion.h \
    qoplot.h \
    nicelabel.h \
    route.h \
    ../tomo/tomodata.h \
    qgraphicstooltip.h \
    flowlayout.h \
    qcoloredtabwidget.h

FORMS    += mainwindow.ui

INCLUDEPATH += ../util/

OTHER_FILES += \
    todo.txt \
    ../remote-config.sh \
    style.css \
    ../credits.txt

RESOURCES += \
    resources.qrc

#QMAKE_LFLAGS += -Wl,-rpath -Wl,/usr/local/lib












