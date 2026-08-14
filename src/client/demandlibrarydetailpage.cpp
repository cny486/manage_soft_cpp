#include "demandlibrarydetailpage.h"

#include "appservice.h"
#include "inventoryfulfillmentdialog.h"

#include <QFileDialog>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {
QString sectionPanelStyle()
{
    return QStringLiteral(
        "#sectionPanel { background-color: #fbfdfd; border: 1px solid #dde8e9; border-radius: 24px; }"
        "QLabel[role='sectionTitle'] { color: #173137; font-size: 16px; font-weight: 800; }"
        "QLabel[role='caption'] { color: #739094; font-size: 12px; font-weight: 700; }"
        "QLabel[role='value'] { color: #173137; font-size: 18px; font-weight: 700; }");
}

QString demandItemSummary(const DemandListItem &item)
{
    QStringList parts;
    if (!item.manufacturerPart.trimmed().isEmpty()) {
        parts.append(item.manufacturerPart.trimmed());
    }
    if (!item.name.trimmed().isEmpty()) {
        parts.append(item.name.trimmed());
    }
    if (!item.value.trimmed().isEmpty()) {
        parts.append(item.value.trimmed());
    }
    if (!item.footprint.trimmed().isEmpty()) {
        parts.append(item.footprint.trimmed());
    }
    if (!item.voltage.trimmed().isEmpty()) {
        parts.append(item.voltage.trimmed());
    }
    return parts.join(QStringLiteral(" / "));
}
}

DemandLibraryDetailPage::DemandLibraryDetailPage(AppService *storageService,
                                                 std::function<void()> backToOverview,
                                                 QWidget *parent)
    : QWidget(parent),
      m_storageService(storageService),
      m_backToOverview(std::move(backToOverview))
{
    buildUi();
    updateSummaryLabels();
}

void DemandLibraryDetailPage::setRecord(const QVariantMap &record)
{
    m_record = record;
    reloadData();
}

void DemandLibraryDetailPage::reloadData()
{
    if (!hasRecord() || m_storageService == nullptr) {
        m_table->setRowCount(0);
        updateSummaryLabels();
        m_statusLabel->setText(QStringLiteral("请选择一份清单查看详情。"));
        return;
    }

    const QList<QVariantMap> records = m_storageService->loadPageRecords(QStringLiteral("demand_library"));
    for (const QVariantMap &record : records) {
        if (record.value(QStringLiteral("id")).toString() == m_record.value(QStringLiteral("id")).toString()) {
            m_record = record;
            break;
        }
    }

    QList<DemandListItem> items;
    QString message;
    if (!m_storageService->loadDemandListItems(m_record.value(QStringLiteral("id")).toString(), &items, &message)) {
        m_table->setRowCount(0);
        updateSummaryLabels();
        m_statusLabel->setText(message);
        return;
    }

    m_table->setRowCount(items.size());
    for (int row = 0; row < items.size(); ++row) {
        const DemandListItem &item = items.at(row);
        m_table->setItem(row, 0, new QTableWidgetItem(QString::number(item.sourceRow)));
        m_table->setItem(row, 1, new QTableWidgetItem(demandItemSummary(item)));
        m_table->setItem(row, 2, new QTableWidgetItem(QString::number(item.quantity)));
    }

    m_record.insert(QStringLiteral("itemCount"), items.size());
    updateSummaryLabels();
    m_statusLabel->setText(QStringLiteral("当前清单共 %1 条有效需求，可直接导出原始 Excel 或按制造数配单。").arg(items.size()));
}

bool DemandLibraryDetailPage::hasRecord() const
{
    return !m_record.value(QStringLiteral("id")).toString().trimmed().isEmpty();
}

void DemandLibraryDetailPage::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(16);

    auto *summaryPanel = new QFrame(this);
    summaryPanel->setObjectName(QStringLiteral("sectionPanel"));
    summaryPanel->setStyleSheet(sectionPanelStyle());
    auto *summaryLayout = new QVBoxLayout(summaryPanel);
    summaryLayout->setContentsMargins(20, 20, 20, 20);
    summaryLayout->setSpacing(14);

    auto *titleRow = new QHBoxLayout();
    auto *titleLabel = new QLabel(QStringLiteral("清单详情"), summaryPanel);
    titleLabel->setProperty("role", QStringLiteral("sectionTitle"));
    auto *backButton = new QPushButton(QStringLiteral("返回清单首页"), summaryPanel);
    auto *exportButton = new QPushButton(QStringLiteral("导出原始 Excel"), summaryPanel);
    auto *analyzeButton = new QPushButton(QStringLiteral("按制造数配单"), summaryPanel);
    auto *refreshButton = new QPushButton(QStringLiteral("刷新"), summaryPanel);
    backButton->setProperty("variant", QStringLiteral("subtle"));
    exportButton->setProperty("variant", QStringLiteral("subtle"));
    analyzeButton->setProperty("variant", QStringLiteral("primary"));
    refreshButton->setProperty("variant", QStringLiteral("subtle"));

    connect(backButton, &QPushButton::clicked, this, [this]() {
        if (m_backToOverview) {
            m_backToOverview();
        }
    });
    connect(exportButton, &QPushButton::clicked, this, &DemandLibraryDetailPage::exportDemandList);
    connect(analyzeButton, &QPushButton::clicked, this, &DemandLibraryDetailPage::analyzeDemandList);
    connect(refreshButton, &QPushButton::clicked, this, &DemandLibraryDetailPage::reloadData);

    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    titleRow->addWidget(refreshButton);
    titleRow->addWidget(exportButton);
    titleRow->addWidget(analyzeButton);
    titleRow->addWidget(backButton);

    auto *gridLayout = new QGridLayout();
    gridLayout->setHorizontalSpacing(20);
    gridLayout->setVerticalSpacing(12);

    auto addField = [summaryPanel, gridLayout](const QString &label,
                                               int row,
                                               int column,
                                               QLabel **valueLabel) {
        auto *captionLabel = new QLabel(label, summaryPanel);
        captionLabel->setProperty("role", QStringLiteral("caption"));
        auto *contentLabel = new QLabel(summaryPanel);
        contentLabel->setProperty("role", QStringLiteral("value"));
        contentLabel->setWordWrap(true);
        gridLayout->addWidget(captionLabel, row, column);
        gridLayout->addWidget(contentLabel, row + 1, column);
        *valueLabel = contentLabel;
    };

    addField(QStringLiteral("清单名称"), 0, 0, &m_nameValueLabel);
    addField(QStringLiteral("原始文件名"), 0, 1, &m_sourceFileValueLabel);
    addField(QStringLiteral("有效条目数"), 2, 0, &m_itemCountValueLabel);
    addField(QStringLiteral("更新时间"), 2, 1, &m_updatedAtValueLabel);

    summaryLayout->addLayout(titleRow);
    summaryLayout->addLayout(gridLayout);

    auto *tablePanel = new QFrame(this);
    tablePanel->setObjectName(QStringLiteral("sectionPanel"));
    tablePanel->setStyleSheet(sectionPanelStyle());
    auto *tableLayout = new QVBoxLayout(tablePanel);
    tableLayout->setContentsMargins(18, 18, 18, 14);
    tableLayout->setSpacing(10);

    auto *tableTitleLabel = new QLabel(QStringLiteral("清单内容预览"), tablePanel);
    tableTitleLabel->setProperty("role", QStringLiteral("sectionTitle"));

    m_table = new QTableWidget(tablePanel);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("来源行"),
        QStringLiteral("Manufacturer Part"),
        QStringLiteral("Quantity")
    });
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->setFrameShape(QFrame::NoFrame);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    m_statusLabel = new QLabel(tablePanel);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #71888c; font-size: 12px; padding-top: 6px;"));

    tableLayout->addWidget(tableTitleLabel);
    tableLayout->addWidget(m_table, 1);
    tableLayout->addWidget(m_statusLabel);

    rootLayout->addWidget(summaryPanel);
    rootLayout->addWidget(tablePanel, 1);
}

void DemandLibraryDetailPage::updateSummaryLabels()
{
    m_nameValueLabel->setText(m_record.value(QStringLiteral("name")).toString().trimmed().isEmpty()
                                  ? QStringLiteral("未选择")
                                  : m_record.value(QStringLiteral("name")).toString());
    m_sourceFileValueLabel->setText(m_record.value(QStringLiteral("sourceFileName")).toString().trimmed().isEmpty()
                                        ? QStringLiteral("-")
                                        : m_record.value(QStringLiteral("sourceFileName")).toString());
    m_itemCountValueLabel->setText(hasRecord()
                                       ? QString::number(m_record.value(QStringLiteral("itemCount")).toInt())
                                       : QStringLiteral("-"));
    m_updatedAtValueLabel->setText(m_record.value(QStringLiteral("updatedAt")).toString().trimmed().isEmpty()
                                       ? QStringLiteral("-")
                                       : m_record.value(QStringLiteral("updatedAt")).toString());
}

void DemandLibraryDetailPage::exportDemandList()
{
    if (!hasRecord()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一份清单。"));
        return;
    }

    const QString defaultName = m_record.value(QStringLiteral("sourceFileName")).toString().trimmed().isEmpty()
                                    ? m_record.value(QStringLiteral("name")).toString() + QStringLiteral(".xlsx")
                                    : m_record.value(QStringLiteral("sourceFileName")).toString();
    const QString filePath = QFileDialog::getSaveFileName(this,
                                                          QStringLiteral("导出原始清单"),
                                                          defaultName,
                                                          QStringLiteral("Excel 文件 (*.xlsx)"));
    if (filePath.isEmpty()) {
        return;
    }

    QString message;
    if (!m_storageService->exportDemandList(m_record.value(QStringLiteral("id")).toString(), filePath, &message)) {
        QMessageBox::critical(this, QStringLiteral("导出失败"), message);
        return;
    }

    QMessageBox::information(this, QStringLiteral("导出完成"), QStringLiteral("原始清单 Excel 已导出。"));
}

void DemandLibraryDetailPage::analyzeDemandList()
{
    if (!hasRecord()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一份清单。"));
        return;
    }

    bool ok = false;
    const int buildCount = QInputDialog::getInt(this,
                                                QStringLiteral("输入制造数"),
                                                QStringLiteral("请输入本次制造数："),
                                                1,
                                                1,
                                                1000000,
                                                1,
                                                &ok);
    if (!ok) {
        return;
    }

    QList<InventoryFulfillmentResult> results;
    QString message;
    if (!m_storageService->analyzeDemandListFulfillment(m_record.value(QStringLiteral("id")).toString(),
                                                        buildCount,
                                                        &results,
                                                        &message)) {
        QMessageBox::critical(this, QStringLiteral("配单失败"), message);
        return;
    }

    InventoryFulfillmentDialog dialog(m_storageService, this);
    dialog.loadResults(results,
                       QStringLiteral("%1 x%2")
                           .arg(m_record.value(QStringLiteral("name")).toString(),
                                QString::number(buildCount)));
    QMessageBox::information(this, QStringLiteral("配单完成"), message);
    dialog.exec();
}
