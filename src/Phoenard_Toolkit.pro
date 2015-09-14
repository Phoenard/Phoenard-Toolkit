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

INCLUDEPATH += $$PWD/controls \
               $$PWD/dialogs \
               $$PWD/imaging \
               $$PWD/stk500

SOURCES += main.cpp\
        mainwindow.cpp \
    stk500/stk500.cpp \
    stk500/stk500task.cpp \
    stk500/stk500serial.cpp \
    stk500/stk500settings.cpp \
    stk500/longfilenamegen.cpp \
    stk500/sketchinfo.cpp \
    stk500/programdata.cpp \
    stk500/stk500sd.cpp \
    stk500/stk500registers.cpp \
    stk500/stk500service.cpp \
    stk500/tasks/stk500deletefiles.cpp \
    stk500/tasks/stk500importfiles.cpp \
    stk500/tasks/stk500launchsketch.cpp \
    stk500/tasks/stk500listsketches.cpp \
    stk500/tasks/stk500listsubdirs.cpp \
    stk500/tasks/stk500loadicon.cpp \
    stk500/tasks/stk500renamefiles.cpp \
    stk500/tasks/stk500renamevolume.cpp \
    stk500/tasks/stk500savefiles.cpp \
    stk500/tasks/stk500updateregisters.cpp \
    stk500/tasks/stk500updatefirmware.cpp \
    stk500/tasks/stk500upload.cpp \
    imaging/quantize.cpp \
    controls/colorselect.cpp \
    controls/menubutton.cpp \
    controls/imageviewer.cpp \
    controls/sdbrowserwidget.cpp \
    controls/serialmonitorwidget.cpp \
    controls/sketchlistwidget.cpp \
    dialogs/progressdialog.cpp \
    dialogs/qmultifiledialog.cpp \
    dialogs/formatselectdialog.cpp \
    dialogs/codeselectdialog.cpp \
    dialogs/iconeditdialog.cpp \
    imaging/phnimage.cpp \
    imaging/imageformat.cpp \
    dialogs/asknamedialog.cpp \
    imaging/phniconprovider.cpp \
    controls/chipcontrolwidget.cpp \
    controls/portselectbox.cpp \
    stk500/stk500port.cpp

HEADERS  += mainwindow.h \
    stk500/stk500.h \
    stk500/stk500_fat.h \
    stk500/stk500task.h \
    stk500/stk500serial.h \
    stk500/longfilenamegen.h \
    stk500/stk500command.h \
    imaging/quantize.h \
    controls/colorselect.h \
    controls/mainmenutab.h \
    controls/menubutton.h \
    controls/imageviewer.h \
    controls/sdbrowserwidget.h \
    controls/serialmonitorwidget.h \
    controls/sketchlistwidget.h \
    dialogs/progressdialog.h \
    dialogs/qmultifiledialog.h \
    dialogs/formatselectdialog.h \
    dialogs/codeselectdialog.h \
    dialogs/iconeditdialog.h \
    stk500/sketchinfo.h \
    stk500/stk500settings.h \
    imaging/phnimage.h \
    imaging/imageformat.h \
    dialogs/asknamedialog.h \
    imaging/phniconprovider.h \
    stk500/stk500sd.h \
    stk500/stk500registers.h \
    controls/chipcontrolwidget.h \
    stk500/stk500service.h \
    stk500/programdata.h \
    controls/portselectbox.h \
    stk500/stk500port.h

FORMS    += mainwindow.ui \
    dialogs/progressdialog.ui \
    dialogs/formatselectdialog.ui \
    dialogs/codeselectdialog.ui \
    dialogs/iconeditdialog.ui \
    controls/serialmonitorwidget.ui \
    controls/sketchlistwidget.ui \
    dialogs/asknamedialog.ui \
    controls/chipcontrolwidget.ui

OTHER_FILES +=

RESOURCES += \
    resources.qrc
