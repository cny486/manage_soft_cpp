#pragma once

#include <functional>

#include <QVariantMap>
#include <QWidget>

class AppService;
class QLabel;
class QTableWidget;

class DemandLibraryDetailPage : public QWidget {
public:
    explicit DemandLibraryDetailPage(AppService *storageService,
                                     std::function<void()> backToOverview,
                                     QWidget *parent = nullptr);

    void setRecord(const QVariantMap &record);
    void reloadData();
    bool hasRecord() const;

private:
    void buildUi();
    void updateSummaryLabels();
    void exportDemandList();
    void analyzeDemandList();

    AppService *m_storageService = nullptr;
    std::function<void()> m_backToOverview;
    QVariantMap m_record;
    QLabel *m_nameValueLabel = nullptr;
    QLabel *m_sourceFileValueLabel = nullptr;
    QLabel *m_itemCountValueLabel = nullptr;
    QLabel *m_updatedAtValueLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTableWidget *m_table = nullptr;
};