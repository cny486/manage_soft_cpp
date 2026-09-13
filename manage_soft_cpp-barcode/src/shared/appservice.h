#pragma once

#include "appschema.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVariantMap>

enum class InventoryFulfillmentStatus {
    Sufficient,
    Insufficient,
    Missing
};

struct InventoryFulfillmentResult {
    QString itemId;
    QString manufacturerPart;
    QString manufacturer;
    QString name;
    QString uniqueId;
    QString unit;
    QString location;
    QString sourceFile;
    QList<int> sourceRows;
    int requiredQuantity = 0;
    int availableQuantity = 0;
    InventoryFulfillmentStatus status = InventoryFulfillmentStatus::Missing;
};

struct InventoryEnrichmentField {
    QString key;
    QString value;
    QString sourceTitle;
    QString sourceUrl;
};

struct InventoryEnrichmentResult {
    QString manufacturerPart;
    QString provider;
    QList<InventoryEnrichmentField> fields;
};

struct ReimbursementAttachmentContent {
    QString reference;
    QString fileName;
    QByteArray content;
};

struct UserSecurityInfo {
    QString userName;
    QString email;
};

enum class VerificationPurpose {
    BindEmail,
    ChangePassword
};

enum class AuthenticationStatus {
    Success,
    UserNotFound,
    WrongPassword,
    Failed
};

class AppService {
public:
    virtual ~AppService() = default;

    virtual AuthenticationStatus authenticate(const QString &username,
                                              const QString &password,
                                              QString *errorMessage = nullptr) = 0;
    virtual void setCurrentUserName(const QString &userName) = 0;
    virtual QString currentUserName() const = 0;
    virtual bool loadCurrentUserSecurityInfo(UserSecurityInfo *info,
                                             QString *errorMessage = nullptr) const = 0;
    virtual bool sendCurrentUserVerificationCode(VerificationPurpose purpose,
                                                 const QString &email,
                                                 QString *errorMessage = nullptr) = 0;
    virtual bool bindCurrentUserEmail(const QString &email,
                                      const QString &verificationCode,
                                      QString *errorMessage = nullptr) = 0;
    virtual bool changeCurrentUserPassword(const QString &newPassword,
                                           const QString &verificationCode,
                                           QString *errorMessage = nullptr) = 0;
    virtual QString storageRoot() const = 0;
    virtual QList<QVariantMap> loadPageRecords(const QString &pageId) const = 0;
    virtual bool upsertRecord(const QString &pageId, QVariantMap record, QString *errorMessage = nullptr) const = 0;
    virtual bool removeRecord(const QString &pageId, const QString &id, QString *errorMessage = nullptr) const = 0;
    virtual bool deleteInventoryRecord(const QString &id,
                                       const QString &note = QString(),
                                       QString *errorMessage = nullptr) const = 0;
    virtual bool importExcel(const QString &pageId,
                             const QList<FieldDefinition> &fields,
                             const QString &filePath,
                             QString *errorMessage = nullptr) const = 0;
    virtual bool exportExcel(const QString &pageId,
                             const QList<FieldDefinition> &fields,
                             const QString &sheetName,
                             const QString &filePath,
                             QString *errorMessage = nullptr) const = 0;
    virtual QList<QVariantMap> loadInventoryHistory() const = 0;
    virtual bool applyInventoryChange(const QVariantMap &itemData,
                                      InventoryOperationType operationType,
                                      InventoryInputType inputType,
                                      const QString &note = QString(),
                                      const QString &sourceFile = QString(),
                                      int sourceRow = 0,
                                      QString *errorMessage = nullptr) const = 0;
    virtual bool importInventoryExcel(const QList<FieldDefinition> &fields,
                                      const QString &filePath,
                                      InventoryOperationType operationType,
                                                                            const QString &note = QString(),
                                      QString *errorMessage = nullptr) const = 0;
    virtual bool importInventoryBom(const QString &filePath,
                                    InventoryOperationType operationType,
                                                                        const QString &note = QString(),
                                    QString *errorMessage = nullptr) const = 0;
    virtual bool analyzeInventoryFulfillment(const QString &filePath,
                                             QList<InventoryFulfillmentResult> *results,
                                             QString *errorMessage = nullptr) const = 0;
    virtual bool enrichInventoryRecord(const QString &manufacturerPart,
                                       const QVariantMap &currentRecord,
                                       const QStringList &desiredFieldKeys,
                                       InventoryEnrichmentResult *result,
                                       QString *errorMessage = nullptr) const = 0;
    virtual bool exportInventoryFulfillment(const QList<InventoryFulfillmentResult> &results,
                                            const QString &filePath,
                                            QString *errorMessage = nullptr) const = 0;
    virtual bool applyInventoryFulfillment(const QList<InventoryFulfillmentResult> &results,
                                           QString *errorMessage = nullptr) const = 0;
    virtual bool loadReimbursementAttachments(const QVariantMap &record,
                                              QList<ReimbursementAttachmentContent> *attachments,
                                              QString *errorMessage = nullptr) const = 0;
};