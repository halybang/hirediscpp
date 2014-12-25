TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -std=c++11
QMAKE_LFLAGS +=  -std=c++11

#Get hiredis.pri at https://github.com/halybang/hiredis-mingw
include (../../hiredis/hiredis.pri)
include(hirediscpp.pri)
LIBS += -levent
win32:LIBS += -LC:/Qt/Qt5.4.0/Tools/mingw491_32/lib
win32:LIBS += -lkernel32 -ladvapi32 -luser32 -lws2_32 -lIphlpapi -lPsapi -lsetupapi
win32:INCLUDEPATH += "C:/Qt/Qt5.4.0/Tools/mingw491_32/include/"
win32:INCLUDEPATH += "C:/Qt/Qt5.4.0/Tools/mingw491_32/include/event2"

SOURCES     += $$PWD/async_example.cpp

OTHER_FILES += README\



