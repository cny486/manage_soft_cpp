#pragma once

#include "appschema.h"

#include <QDialog>
#include <QHash>
#include <QVariantMap>

class QWidget;

class RecordDialog : public QDialog {
public:
    RecordDialog(const QString &title,
                 const QList<FieldDefinition> &fields,
                 QWidget *parent = nullptr);

    void setRecordData(const QVariantMap &record);
    QVariantMap recordData() const;

protected:
    void accept() override;

private:
    QWidget *createEditor(const FieldDefinition &field);
    QVariant editorValue(const FieldDefinition &field, QWidget *editor) const;
    void setEditorValue(const FieldDefinition &field, QWidget *editor, const QVariant &value);

    QList<FieldDefinition> m_fields;
    QHash<QString, QWidget *> m_editors;
    QVariantMap m_originalRecord;
};