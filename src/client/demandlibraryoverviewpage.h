#pragma once

#include <functional>

#include <QVariantMap>
#include <QWidget>

class AppService;
class QLabel;
class QTableWidget;

class DemandLibraryOverviewPage : public QWidget {
public:
    explicit DemandLibraryOverviewPage(AppService *storageService,
                                       std::function<void(const QVariantMap &)> openDetail,
                                       QWidget *parent = nullptr);

    void reloadData();

private:
    void buildUi();
    void importDemandList();
    QVariantMap recordAtRow(int row) const;

    AppService *m_storageService = nullptr;
    std::function<void(const QVariantMap &)> m_openDetail;
    QTableWidget *m_table = nullptr;
    QLabel *m_statusLabel = nullptr;
    QList<QVariantMap> m_records;
};