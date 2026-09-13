#pragma once

#include <QDialog>

#include <QList>
#include <QVariantMap>

class QComboBox;
class QDateEdit;
class QLineEdit;
class QTableWidget;

class InventoryHistoryDialog : public QDialog {
public:
    explicit InventoryHistoryDialog(const QList<QVariantMap> &historyRecords,
                                    QWidget *parent = nullptr);

private:
    void refreshTable();
    void showHistoryDetails(int row);

    QList<QVariantMap> m_historyRecords;
    QList<QVariantMap> m_filteredRecords;
    QDateEdit *m_startDateEdit = nullptr;
    QDateEdit *m_endDateEdit = nullptr;
    QComboBox *m_operationTypeComboBox = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QTableWidget *m_table = nullptr;
};