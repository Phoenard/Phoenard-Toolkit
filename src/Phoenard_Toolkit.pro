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
    menubutton.cpp \
    stk500/stk500.cpp \
    stk500/stk500task.cpp \
    stk500/stk500serial.cpp \
    stk500/longfilenamegen.cpp \
    imageeditor.cpp \
    quantize.cpp \
    colorselect.cpp \
    controls/sdbrowserwidget.cpp \
    controls/serialmonitorwidget.cpp \
    controls/sketchlistwidget.cpp \
    dialogs/progressdialog.cpp \
    dialogs/qmultifiledialog.cpp \
    dialogs/formatselectdialog.cpp \
    dialogs/codeselectdialog.cpp

HEADERS  += mainwindow.h \
    menubutton.h \
    stk500/stk500.h \
    stk500/stk500_fat.h \
    stk500/stk500task.h \
    stk500/stk500serial.h \
    stk500/longfilenamegen.h \
    stk500/stk500command.h \
    imageeditor.h \
    quantize.h \
    colorselect.h \
    controls/sdbrowserwidget.h \
    controls/serialmonitorwidget.h \
    controls/sketchlistwidget.h \
    dialogs/progressdialog.h \
    dialogs/qmultifiledialog.h \
    dialogs/formatselectdialog.h \
    dialogs/codeselectdialog.h \
    stk500/sketchinfo.h \
    mainmenutab.h

FORMS    += mainwindow.ui \
    progressdialog.ui \
    dialogs/formatselectdialog.ui \
    dialogs/codeselectdialog.ui \
    controls/serialmonitorwidget.ui \
    controls/sketchlistwidget.ui

OTHER_FILES +=

RESOURCES += \
    icons/icons.qrc