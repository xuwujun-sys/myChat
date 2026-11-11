QT += widgets network
CONFIG += c++11
TEMPLATE = app
TARGET = chatroomclient
SOURCES += main.cpp \
           chatwindow.cpp \
           logindialog.cpp \
           tcpmanger.cpp
HEADERS += chatwindow.h \
           logindialog.h \
           tcpmanger.h
FORMS += chatwindow.ui \
         logindialog.ui
