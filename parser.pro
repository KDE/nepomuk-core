######################################################################
# Automatically generated by qmake (2.01a) Thu Jun 13 13:23:31 2013
######################################################################

CONFIG += debug
TEMPLATE = app
TARGET = parser
DEPENDPATH += .
INCLUDEPATH += .
QT -= gui
LIBS += -lnepomukcore -lkdecore -lsoprano

# Input
HEADERS += parser.h \
           pass_splitunits.h \
           pass_numbers.h \
           pass_filesize.h \
           pass_typehints.h \
           pass_sentby.h

SOURCES += main.cpp \
           parser.cpp \
           pass_splitunits.cpp \
           pass_numbers.cpp \
           pass_filesize.cpp \
           pass_typehints.cpp \
           pass_sentby.cpp
