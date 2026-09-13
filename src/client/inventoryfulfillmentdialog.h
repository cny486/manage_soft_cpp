#pragma once

#include "appservice.h"

#include <QDialog>
#include <QList>

class QLabel;
class QPushButton;
class SearchHighlightDelegate;
class QTableWidget;

class InventoryFulfillmentDialog : public QDialog {
public:
    explicit InventoryFulfillmentDialog(AppService *storageService,
                                        QWidget *parent = nullptr);

    bool inventoryChanged() const;
    void loadResults(const QList<InventoryFulfillmentResult> &results,
                     const QString &sourceLabel);

private:
    void importDemandFile();
    void exportResults();
    void handleResultCellClicked(int row, int column);
    void showStatusMenu(int resultRow);
    void showCandidateMenu(int resultRow);
    void browseAllInventoryItems(int resultRow);
    void confirmCandidateForCurrentRow(int candidateRow);
    void confirmCandidateForRow(int resultRow, int candidateRow);
    void markCurrentRowMissing();
    void markRowMissing(int resultRow);
    void clearCurrentConfirmation();
    void clearRowConfirmation(int resultRow);
    void applySelectedStockOut();
    void refreshTable();
    void updateActionState();
    QStringList displayHeaders() const;
    QList<InventoryFulfillmentResult> confirmedStockOutResults() const;

    AppService *m_storageService = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QLabel *m_sourceLabel = nullptr;
    QTableWidget *m_resultsTable = nullptr;
    QPushButton *m_exportButton = nullptr;
    QPushButton *m_applyButton = nullptr;
    QList<InventoryFulfillmentResult> m_results;
    QString m_sourceFilePath;
    int m_fulfillmentSetCount = 0;
    bool m_inventoryChanged = false;
};
