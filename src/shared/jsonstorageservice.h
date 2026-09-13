#pragma once

#include "appservice.h"

#include <QDateTime>
#include <QHash>

class JsonStorageService : public AppService {
public:
    JsonStorageService();

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
                                     int fulfillmentSetCount,
                                     QList<InventoryFulfillmentResult> *results,
                                     QString *errorMessage = nullptr) const override;
    bool enrichInventoryRecord(const QString &manufacturerPart,
                               const QVariantMap &currentRecord,
                               const QStringList &desiredFieldKeys,
                               InventoryEnrichmentResult *result,
                               QString *errorMessage = nullptr) const override;
    bool importDemandList(const QString &name,
                          const QString &filePath,
                          QString *errorMessage = nullptr) const override;
    bool exportDemandList(const QString &recordId,
                          const QString &filePath,
                          QString *errorMessage = nullptr) const override;
    bool loadDemandListItems(const QString &recordId,
                             QList<DemandListItem> *items,
                             QString *errorMessage = nullptr) const override;
    bool analyzeDemandListFulfillment(const QString &recordId,
                                      int buildCount,
                                      QList<InventoryFulfillmentResult> *results,
                                      QString *errorMessage = nullptr) const override;
    bool exportInventoryFulfillment(const QList<InventoryFulfillmentResult> &results,
                                    const QString &sourceFilePath,
                                    const QString &filePath,
                                    QString *errorMessage = nullptr) const override;
    bool applyInventoryFulfillment(const QList<InventoryFulfillmentResult> &results,
                                   QString *errorMessage = nullptr) const override;
    bool loadReimbursementAttachments(const QVariantMap &record,
                                      QList<ReimbursementAttachmentContent> *attachments,
                                      QString *errorMessage = nullptr) const override;

private:
    struct VerificationCodeState {
        QString code;
        QString email;
        VerificationPurpose purpose = VerificationPurpose::BindEmail;
        QDateTime expiresAt;
    };

    QString pageFilePath(const QString &pageId) const;
    QString usersFilePath() const;
    QString attachmentsRootDirectory() const;
    QString recordAttachmentRelativeDirectory(const QString &pageId, const QString &recordId) const;
    QString recordAttachmentDirectory(const QString &pageId, const QString &recordId) const;
    QString demandListArchivedFilePath(const QVariantMap &record) const;
    QString inventoryHistoryFilePath() const;
    bool ensureStorageReady(QString *errorMessage = nullptr) const;
    bool ensureUserStoreReady(QString *errorMessage = nullptr) const;
    bool loadUsers(QList<QVariantMap> *users, QString *errorMessage = nullptr) const;
    bool saveUsers(const QList<QVariantMap> &users, QString *errorMessage = nullptr) const;
    int indexOfUserByName(const QList<QVariantMap> &users, const QString &userName) const;
    bool rateLimitAllowsSend(const QString &userName, QString *errorMessage = nullptr) const;
    QString verificationStateKey(const QString &userName, VerificationPurpose purpose) const;
    bool archiveReimbursementAttachments(QVariantMap *record,
                                        const QVariantMap &existingRecord,
                                        QStringList *obsoleteFiles,
                                        QStringList *createdFiles,
                                        QString *errorMessage = nullptr) const;
    bool removeDemandListArchive(const QVariantMap &record) const;
    bool savePageRecords(const QString &pageId,
                         const QList<QVariantMap> &records,
                         QString *errorMessage = nullptr) const;
    bool saveInventoryHistory(const QList<QVariantMap> &records,
                              QString *errorMessage = nullptr) const;

    QString m_currentUserName;
    mutable QHash<QString, VerificationCodeState> m_verificationCodes;
    mutable QHash<QString, QList<QDateTime>> m_verificationSendHistory;
};
