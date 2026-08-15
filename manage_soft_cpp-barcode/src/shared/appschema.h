#pragma once

#include <QList>
#include <QString>
#include <QStringList>

enum class FieldType {
    Text,
    Integer,
    Double,
    Date,
    Multiline,
    Choice,
    Boolean,
    File,
    Files
};

struct FieldDefinition {
    QString key;
    QString label;
    FieldType type = FieldType::Text;
    bool required = false;
    QStringList options;
};

enum class InventoryOperationType {
    DirectUpdate,
    StockIn,
    StockOut,
    Delete
};

enum class InventoryInputType {
    Manual,
    Excel,
    Scanner
};

struct PageConfig {
    QString pageId;
    QString title;
    QList<FieldDefinition> fields;
    QStringList searchableKeys;
    QStringList listFieldKeys;
    bool showUpdatedAt = true;
};

inline QList<FieldDefinition> prioritizedFields(const QList<FieldDefinition> &fields,
                                               const QStringList &leadingKeys,
                                               const QStringList &requiredKeys = {})
{
    QList<FieldDefinition> orderedFields;
    QStringList appendedKeys;

    auto appendField = [&](const FieldDefinition &field) {
        if (appendedKeys.contains(field.key)) {
            return;
        }

        FieldDefinition adjustedField = field;
        if (requiredKeys.contains(adjustedField.key)) {
            adjustedField.required = true;
        }
        orderedFields.append(adjustedField);
        appendedKeys.append(adjustedField.key);
    };

    for (const QString &key : leadingKeys) {
        for (const FieldDefinition &field : fields) {
            if (field.key == key) {
                appendField(field);
                break;
            }
        }
    }

    for (const FieldDefinition &field : fields) {
        appendField(field);
    }

    return orderedFields;
}
