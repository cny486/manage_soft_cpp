QT += widgets network

CONFIG += c++17 console
CONFIG -= app_bundle

TARGET = ManageSoftCpp
TEMPLATE = app

INCLUDEPATH += src
INCLUDEPATH += src/client
INCLUDEPATH += src/shared

SOURCES += \
    src/shared/aiinventoryenricher.cpp \
    src/client/main.cpp \
    src/client/accountsecuritydialog.cpp \
    src/client/connectionsettings.cpp \
    src/client/connectionsettingsdialog.cpp \
    src/shared/datainitializer.cpp \
    src/shared/emailsettings.cpp \
    src/client/inventoryhistorydialog.cpp \
    src/client/inventoryfulfillmentdialog.cpp \
    src/client/inventoryitempickerdialog.cpp \
    src/client/inventorytransactiondialog.cpp \
    src/shared/jsonstorageservice.cpp \
    src/client/inventoryrecorddialog.cpp \
    src/client/manualstockindialog.cpp \
    src/client/logindialog.cpp \
    src/shared/smtpemailclient.cpp \
    src/shared/simplexlsxdocument.cpp \
    src/client/recorddialog.cpp \
    src/client/managementpage.cpp \
    src/client/mainwindow.cpp \
    src/client/tcpappserviceclient.cpp \
    src/shared/tcpmessagecodec.cpp

HEADERS += \
    src/shared/aiinventoryenricher.h \
    src/shared/appservice.h \
    src/shared/appschema.h \
    src/client/accountsecuritydialog.h \
    src/client/connectionsettings.h \
    src/client/connectionsettingsdialog.h \
    src/shared/datainitializer.h \
    src/shared/emailsettings.h \
    src/client/inventoryhistorydialog.h \
    src/client/inventoryfulfillmentdialog.h \
    src/client/inventoryitempickerdialog.h \
    src/client/inventorytransactiondialog.h \
    src/shared/jsonstorageservice.h \
    src/client/inventoryrecorddialog.h \
    src/client/manualstockindialog.h \
    src/client/logindialog.h \
    src/shared/smtpemailclient.h \
    src/shared/simplexlsxdocument.h \
    src/client/recorddialog.h \
    src/client/managementpage.h \
    src/client/mainwindow.h \
    src/client/tcpappserviceclient.h \
    src/shared/tcpmessagecodec.h

LIBS += -lz