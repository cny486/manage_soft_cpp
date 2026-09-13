#pragma once

#include <QWidget>

class AppService;
class QLabel;
class QTableWidget;

class ReimbursementOverviewPage : public QWidget {
public:
    explicit ReimbursementOverviewPage(AppService *storageService,
                                       QWidget *parent = nullptr);

    void reloadData();

private:
    void buildUi();
    void refreshOwnerTable();

    AppService *m_storageService = nullptr;
    QLabel *m_totalAmountLabel = nullptr;
    QLabel *m_uninvoicedAmountLabel = nullptr;
    QLabel *m_invoicedAmountLabel = nullptr;
    QLabel *m_unreimbursedAmountLabel = nullptr;
    QLabel *m_reimbursedAmountLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTableWidget *m_ownerSummaryTable = nullptr;
};