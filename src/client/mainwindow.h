#pragma once

#include <functional>
#include <QString>
#include <QMainWindow>

class AppService;
class DemandLibraryDetailPage;
class DemandLibraryOverviewPage;
class QLabel;
class ManagementPage;
class QPushButton;
class QStackedWidget;
class ReimbursementOverviewPage;
class QWidget;

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(AppService *storageService,
                        const QString &statusMessage,
                        std::function<void(QWidget *)> openConnectionSettings,
                        QWidget *parent = nullptr);
    ~MainWindow() override;

    AppService *storageService() const;

private:
    void applyTheme();
    void buildUi();
    void setCurrentPage(int index);
    void updateNavigationState();

    AppService *m_storageService = nullptr;
    QLabel *m_pageTitleLabel = nullptr;
    QPushButton *m_inventoryButton = nullptr;
    QPushButton *m_demandLibraryButton = nullptr;
    QPushButton *m_reimbursementButton = nullptr;
    QWidget *m_demandLibraryTabsWidget = nullptr;
    QPushButton *m_demandLibraryOverviewTabButton = nullptr;
    QPushButton *m_demandLibraryDetailTabButton = nullptr;
    QWidget *m_reimbursementTabsWidget = nullptr;
    QPushButton *m_reimbursementOverviewTabButton = nullptr;
    QPushButton *m_reimbursementDetailTabButton = nullptr;
    QStackedWidget *m_stack = nullptr;
    ManagementPage *m_inventoryPage = nullptr;
    DemandLibraryOverviewPage *m_demandLibraryOverviewPage = nullptr;
    DemandLibraryDetailPage *m_demandLibraryDetailPage = nullptr;
    ReimbursementOverviewPage *m_reimbursementOverviewPage = nullptr;
    ManagementPage *m_reimbursementDetailPage = nullptr;
    QString m_statusMessage;
    std::function<void(QWidget *)> m_openConnectionSettings;
};
