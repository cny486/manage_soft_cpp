#pragma once

#include "appschema.h"

#include <QDialog>
#include <QList>
#include <QVariantMap>

class QTableWidget;
class AppService;

struct ManualStockInEntry {
    QVariantMap itemData;
    QString note;
    QString summary;
    bool existingItem = false;
};

class ManualStockInDialog : public QDialog {
public:
    ManualStockInDialog(const QList<FieldDefinition> &fields,
                        const QList<QVariantMap> &inventoryRecords,
                        AppService *service,
                        QWidget *parent = nullptr);

    QList<ManualStockInEntry> entries() const;

protected:
    void accept() override;

private:
    void addExistingItem();
    void addNewItem();
    void removeSelectedEntry();
    void refreshTable();

    QList<FieldDefinition> m_fields;
    QList<QVariantMap> m_inventoryRecords;
    AppService *m_service = nullptr;
    QList<ManualStockInEntry> m_entries;
    QTableWidget *m_table = nullptr;
};
