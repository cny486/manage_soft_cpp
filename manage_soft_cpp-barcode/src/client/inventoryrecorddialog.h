#pragma once

#include "appschema.h"
#include "appservice.h"

#include <QDialog>
#include <QHash>
#include <QVariantMap>

class QLabel;
class QPushButton;
class QWidget;

class InventoryRecordDialog : public QDialog {
public:
    InventoryRecordDialog(const QString &title,
                          const QList<FieldDefinition> &fields,
                          AppService *service,
                          const QStringList &allowedEnrichmentKeys = {},
                          QWidget *parent = nullptr);

    void setRecordData(const QVariantMap &record);
    QVariantMap recordData() const;

protected:
    void accept() override;

private:
    QWidget *createEditor(const FieldDefinition &field);
    QVariant editorValue(const FieldDefinition &field, QWidget *editor) const;
    void setEditorValue(const FieldDefinition &field, QWidget *editor, const QVariant &value);
    void runAiEnrichment();
    void showSourceDetails();
    QString fieldLabel(const QString &fieldKey) const;

    QList<FieldDefinition> m_fields;
    AppService *m_service = nullptr;
    QStringList m_allowedEnrichmentKeys;
    QHash<QString, QWidget *> m_editors;
    QVariantMap m_originalRecord;
    InventoryEnrichmentResult m_lastEnrichment;
    QLabel *m_enrichmentStatusLabel = nullptr;
    QPushButton *m_showSourcesButton = nullptr;
};