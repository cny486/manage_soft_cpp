#pragma once

#include "appschema.h"

#include <QList>
#include <QPoint>
#include <QVariantMap>
#include <QWidget>

class AppService;
class QLabel;
class QLineEdit;
class QPushButton;
class QComboBox;
class QTableWidget;

class ManagementPage : public QWidget {
public:
    ManagementPage(const PageConfig &config,
                   AppService *storageService,
                   QWidget *parent = nullptr);

    void reloadRecords();

private:
    bool isInventoryPage() const;
    bool isReimbursementPage() const;
    void buildUi();
    void refreshTable(const QList<QVariantMap> &records);
    void configureTableColumns();
    QList<FieldDefinition> listFields() const;
    QList<QVariantMap> filteredRecords(const QList<QVariantMap> &allRecords,
                                       const QString &keyword,
                                       const QString &category) const;
    void updateCategoryFilterOptions(const QList<QVariantMap> &allRecords);
    void updateReimbursementFilterOptions(const QList<QVariantMap> &allRecords);
    QVariantMap selectedRecord() const;
    QList<QVariantMap> selectedRecords() const;
    void showInventoryContextMenu(const QPoint &position);
    void viewInventoryHistory(const QString &inputTypeFilter = QString());
    void directUpdateRecord();
    void stockInRecord();
    void scanInventoryRecord(InventoryOperationType operationType);
    void manualStockInRecord();
    void excelStockInRecord();
    void fulfillDemandRecord();
    void stockOutRecord();
    void manualStockOutRecord();
    void bomStockOutRecord();
    void importInventoryRecords(InventoryOperationType operationType);
    void addRecord();
    void editRecord();
    void selectAllRecords();
    void deleteRecord();
    void deleteSelectedRecords();
    void exportSelectedAttachments();
    void exportPendingInvoiceAttachments();
    void markSelectedReimbursed();
    void downloadInvoiceAttachment(int row);
    void importRecords();
    void exportRecords();

    PageConfig m_config;
    AppService *m_storageService = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_categoryFilterCombo = nullptr;
    QComboBox *m_ownerFilterCombo = nullptr;
    QComboBox *m_invoiceStatusFilterCombo = nullptr;
    QComboBox *m_reimbursedStatusFilterCombo = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_statusLabel = nullptr;
    QList<QVariantMap> m_visibleRecords;
};
