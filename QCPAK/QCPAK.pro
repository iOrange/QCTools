#-------------------------------------------------
#
# Project created by QtCreator 2017-04-10T23:56:27
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = QCPAK
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += src

SOURCES += src/main.cpp\
    src/mainwindow.cpp \
    src/aboutdlg.cpp \
    src/qc/PAKArchive.cpp \
    src/zlib/adler32.c \
    src/zlib/crc32.c \
    src/zlib/infback.c \
    src/zlib/inffast.c \
    src/zlib/inflate.c \
    src/zlib/inftrees.c \
    src/zlib/zutil.c

HEADERS  += src/mainwindow.h \
    src/aboutdlg.h \
    src/qc/PAKArchive.h \
    src/qc/qccommon.h

FORMS    += ui/mainwindow.ui \
    ui/aboutdlg.ui

RESOURCES += resources.qrc
