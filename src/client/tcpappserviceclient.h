#pragma once

#include "aiapisettings.h"
#include "appservice.h"

#include <QJsonObject>

class TcpAppServiceClient : public AppService {
public:
    TcpAppServiceClient(const QString &host, quint16 port, int timeoutMs = 10000);

    AuthenticationStatus authenticate(const QString &username,
                                      const QString &password,
                                      QString *errorMessage = nullptr) override;
    void setCurrentUserName(const QString &userName) override;
    QString currentUserName() const override;
    bool loadCurrentUserSecurityInfo(UserSecurityInfo *info,
                                     QString *errorMessage = nullptr) const override;
    bool sendCurrentUserVerificationCode(VerificationPurpose purpose,
                                         const QString &email,
                                         QString *errorMessage = nullptr) override;
    bool bindCurrentUserEmail(const QString &email,
                              const QString &verificationCode,
                              QString *errorMessage = nullptr) override;
    bool changeCurrentUserPassword(const QString &newPassword,
                                   const QString &verificationCode,
                                   QString *errorMessage = nullptr) override;
    QString storageRoot() const override;
    QList<QVariantMap> loadPageRecords(const QString &pageId) const override;
    bool upsertRecord(const QString &pageId, QVariantMap record, QString *errorMessage = nullptr) const override;
    bool removeRecord(const QString &pageId, const QString &id, QString *errorMessage = nullptr) const override;
    bool deleteInventoryRecord(const QString &id,
                               const QString &note = QString(),
                               QString *errorMessage = nullptr) const override;
    bool importExcel(const QString &pageId,
                     const QList<FieldDefinition> &fields,
                     const QString &filePath,
                     QString *errorMessage = nullptr) const override;
    bool exportExcel(const QString &pageId,
                     const QList<FieldDefinition> &fields,
                     const QString &sheetName,
                     const QString &filePath,
                     QString *errorMessage = nullptr) const override;
    QList<QVariantMap> loadInventoryHistory() const override;
    bool applyInventoryChange(const QVariantMap &itemData,
                              InventoryOperationType operationType,
                              InventoryInputType inputType,
                              const QString &note = QString(),
                              const QString &sourceFile = QString(),
                              int sourceRow = 0,
                              QString *errorMessage = nullptr) const override;
    bool importInventoryExcel(const QList<FieldDefinition> &fields,
                              const QString &filePath,
                              InventoryOperationType operationType,
                                                            const QString &note = QString(),
                              QString *errorMessage = nullptr) const override;
    bool importInventoryBom(const QString &filePath,
                            InventoryOperationType operationType,
                                                        const QString &note = QString(),
                            QString *errorMessage = nullptr) const override;
    bool analyzeInventoryFulfillment(const QString &filePath,
                                     QList<InventoryFulfillmentResult> *results,
                                     QString *errorMessage = nullptr) const override;
    bool enrichInventoryRecord(const QString &manufacturerPart,
                               const QVariantMap &currentRecord,
                               const QStringList &desiredFieldKeys,
                               InventoryEnrichmentResult *result,
                               QString *errorMessage = nullptr) const override;
    bool exportInventoryFulfillment(const QList<InventoryFulfillmentResult> &results,
                                    const QString &filePath,
                                    QString *errorMessage = nullptr) const override;
    bool applyInventoryFulfillment(const QList<InventoryFulfillmentResult> &results,
                                   QString *errorMessage = nullptr) const override;
    bool loadReimbursementAttachments(const QVariantMap &record,
                                      QList<ReimbursementAttachmentContent> *attachments,
                                      QString *errorMessage = nullptr) const override;

    bool ping(QString *errorMessage = nullptr) const;
    bool testAiConnection(const AiApiSettings &settings,
                          QString *responsePreview = nullptr,
                          QString *errorMessage = nullptr) const;

private:
    bool sendRequest(const QString &action,
                     const QJsonObject &payload,
                     QJsonObject *response,
                     QString *errorMessage,
                     int timeoutMs = -1) const;
    bool readFileBytes(const QString &filePath, QByteArray *content, QString *errorMessage) const;
    bool writeFileBytes(const QString &filePath, const QByteArray &content, QString *errorMessage) const;
    bool responseSucceeded(const QJsonObject &response, QString *message) const;
    int aiRequestTimeoutMs(int suggestedTimeoutMs = -1) const;

    QString m_host;
    quint16 m_port = 0;
    int m_timeoutMs = 10000;
    QString m_sessionToken;
    QString m_currentUserName;
};