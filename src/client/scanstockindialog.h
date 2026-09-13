#pragma once

#include "appschema.h"
#include "manualstockindialog.h"

#include <QDialog>
#include <QList>
#include <QVariantMap>

class QLineEdit;
class QTableWidget;
class AppService;

class ScanStockInDialog : public QDialog {
public:
    ScanStockInDialog(const QList<FieldDefinition> &fields,
                      const QList<QVariantMap> &inventoryRecords,
                      AppService *service,
                      QWidget *parent = nullptr);

    QList<ManualStockInEntry> entries() const;

protected:
    void accept() override;
    void showEvent(QShowEvent *event) override;

private:
    void processScanResult();
    void removeSelectedEntry();
    void refreshTable();

    QList<FieldDefinition> m_fields;
    QList<QVariantMap> m_inventoryRecords;
    AppService *m_service = nullptr;
    QList<ManualStockInEntry> m_entries;
    QLineEdit *m_scanInput = nullptr;
    QTableWidget *m_table = nullptr;
};
