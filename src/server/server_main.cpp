#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>

#include "datainitializer.h"
#include "jsonstorageservice.h"
#include "tcpbackendserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("ManageSoftServer");
    QCoreApplication::setOrganizationName("ManageSoftCpp");
    QCoreApplication::setOrganizationDomain("local.manage.soft");

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("ManageSoftCpp TCP backend server"));
    parser.addHelpOption();

    QCommandLineOption hostOption(QStringList() << QStringLiteral("listen-host"),
                                  QStringLiteral("Address to listen on."),
                                  QStringLiteral("host"),
                                  qEnvironmentVariable("MANAGE_SOFT_SERVER_LISTEN_HOST", "0.0.0.0"));
    QCommandLineOption portOption(QStringList() << QStringLiteral("listen-port"),
                                  QStringLiteral("Port to listen on."),
                                  QStringLiteral("port"),
                                  qEnvironmentVariable("MANAGE_SOFT_SERVER_LISTEN_PORT", "45454"));

    parser.addOption(hostOption);
    parser.addOption(portOption);
    parser.process(app);

    bool portOk = false;
    const quint16 port = parser.value(portOption).toUShort(&portOk);
    if (!portOk) {
        qCritical() << "Invalid listen port:" << parser.value(portOption);
        return 1;
    }

    QHostAddress address;
    if (!address.setAddress(parser.value(hostOption))) {
        qCritical() << "Invalid listen host:" << parser.value(hostOption);
        return 1;
    }

    JsonStorageService storage;
    ensureInventorySampleData(&storage);

    TcpBackendServer server(&storage);
    QString errorMessage;
    if (!server.listen(address, port, &errorMessage)) {
        qCritical() << "Failed to start backend server:" << errorMessage;
        return 1;
    }

    qInfo() << "ManageSoft backend listening on" << address.toString() << ":" << port;
    qInfo() << "Storage root:" << storage.storageRoot();
    return app.exec();
}