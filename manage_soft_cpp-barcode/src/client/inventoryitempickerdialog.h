#pragma once

#include <QDialog>
#include <QList>
#include <QVariantMap>

class QLineEdit;
class QTableWidget;

class InventoryItemPickerDialog : public QDialog {
public:
    explicit InventoryItemPickerDialog(const QList<QVariantMap> &inventoryRecords,
                                       QWidget *parent = nullptr);

    QVariantMap selectedRecord() const;

protected:
    void accept() override;

private:
    void refreshTable();
    QList<QVariantMap> filteredRecords() const;

    QList<QVariantMap> m_inventoryRecords;
    QList<QVariantMap> m_visibleRecords;
    QLineEdit *m_searchEdit = nullptr;
    QTableWidget *m_table = nullptr;
};