#pragma once

#include <QHostAddress>
#include <QHash>
#include <QJsonObject>
#include <QObject>

class QTcpServer;
class QTcpSocket;

class JsonStorageService;

class TcpBackendServer : public QObject {
public:
    explicit TcpBackendServer(JsonStorageService *storage, QObject *parent = nullptr);

    bool listen(const QHostAddress &address, quint16 port, QString *errorMessage = nullptr);

private:
    void handleNewConnection();
    void handleSocketReadyRead(QTcpSocket *socket);
    QJsonObject processRequest(const QJsonObject &request) const;
    QString authenticatedUserName(const QJsonObject &payload) const;
    QJsonObject makeResponse(const QString &requestId,
                             bool success,
                             const QJsonObject &payload = {},
                             const QString &message = QString()) const;
    bool writeTempFile(const QString &suffix, const QByteArray &content, QString *filePath, QString *errorMessage) const;
    bool readFileBytes(const QString &filePath, QByteArray *content, QString *errorMessage) const;

    JsonStorageService *m_storage = nullptr;
    QTcpServer *m_server = nullptr;
    mutable QHash<QString, QString> m_sessionUsers;
};