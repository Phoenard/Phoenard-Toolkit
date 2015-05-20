#-------------------------------------------------
#
# Project created by QtCreator 2014-11-10T10:37:13
#
#-------------------------------------------------

QT       += core gui
QT       += serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Phoenard_Toolkit
TEMPLATE = app

win32:RC_ICONS += App_Icon.ico

SOURCES += main.cpp\
        mainwindow.cpp \
    sdbrowserwidget.cpp \
    menubutton.cpp \
    qmultifiledialog.cpp \
    stk500/stk500.cpp \
    stk500/stk500session.cpp \
    stk500/stk500task.cpp \
    stk500/longfilenamegen.cpp \
    stk500/progressdialog.cpp \
    imageeditor.cpp \
    quantize.cpp \
    formatselectdialog.cpp \
    colorselect.cpp \
    serialmonitorwidget.cpp

HEADERS  += mainwindow.h \
    sdbrowserwidget.h \
    menubutton.h \
    qmultifiledialog.h \
    stk500/stk500.h \
    stk500/stk500_fat.h \
    stk500/stk500session.h \
    stk500/stk500task.h \
    stk500/longfilenamegen.h \
    stk500/stk500command.h \
    stk500/progressdialog.h \
    imageeditor.h \
    quantize.h \
    formatselectdialog.h \
    colorselect.h \
    serialmonitorwidget.h

FORMS    += mainwindow.ui \
    progressdialog.ui \
    formatselectdialog.ui \
    serialmonitorwidget.ui

OTHER_FILES +=

RESOURCES += \
    icons/icons.qrc
