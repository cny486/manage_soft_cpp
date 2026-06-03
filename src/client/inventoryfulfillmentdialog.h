#pragma once

#include "appservice.h"

#include <QDialog>
#include <QList>

class QLabel;
class QPushButton;
class QTableWidget;

class InventoryFulfillmentDialog : public QDialog {
public:
    explicit InventoryFulfillmentDialog(AppService *storageService,
                                        QWidget *parent = nullptr);

    bool inventoryChanged() const;

private:
    void importDemandFile();
    void exportResults();
    void selectSufficientRows();
    void clearSelection();
    void applySelectedStockOut();
    void refreshTable();
    QList<InventoryFulfillmentResult> selectedResults() const;

    AppService *m_storageService = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QLabel *m_sourceLabel = nullptr;
    QTableWidget *m_table = nullptr;
    QPushButton *m_exportButton = nullptr;
    QPushButton *m_applyButton = nullptr;
    QList<InventoryFulfillmentResult> m_results;
    QString m_sourceFilePath;
    bool m_inventoryChanged = false;
};