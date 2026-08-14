#include "demandlibraryoverviewpage.h"

#include "appservice.h"

#include <QFileDialog>
#include <QFileInfo>
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
        "QLabel[role='sectionTitle'] { color: #173137; font-size: 16px; font-weight: 800; }");
}
}

DemandLibraryOverviewPage::DemandLibraryOverviewPage(AppService *storageService,
                                                     std::function<void(const QVariantMap &)> openDetail,
                                                     QWidget *parent)
    : QWidget(parent),
      m_storageService(storageService),
      m_openDetail(std::move(openDetail))
{
    buildUi();
    reloadData();
}

void DemandLibraryOverviewPage::reloadData()
{
    m_records = m_storageService == nullptr
                    ? QList<QVariantMap>{}
                    : m_storageService->loadPageRecords(QStringLiteral("demand_library"));

    m_table->setRowCount(m_records.size());
    for (int row = 0; row < m_records.size(); ++row) {
        const QVariantMap &record = m_records.at(row);
        auto *nameItem = new QTableWidgetItem(record.value(QStringLiteral("name")).toString());
        nameItem->setData(Qt::UserRole, record.value(QStringLiteral("id")).toString());
        m_table->setItem(row, 0, nameItem);
        m_table->setItem(row, 1, new QTableWidgetItem(record.value(QStringLiteral("sourceFileName")).toString()));
        m_table->setItem(row, 2, new QTableWidgetItem(QString::number(record.value(QStringLiteral("itemCount")).toInt())));
        m_table->setItem(row, 3, new QTableWidgetItem(record.value(QStringLiteral("updatedAt")).toString()));
    }

    m_statusLabel->setText(QStringLiteral("当前共 %1 份已保存清单。双击任意清单可查看详情。").arg(m_records.size()));
}

void DemandLibraryOverviewPage::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(16);

    auto *actionPanel = new QFrame(this);
    actionPanel->setObjectName(QStringLiteral("sectionPanel"));
    actionPanel->setStyleSheet(sectionPanelStyle());
    auto *actionLayout = new QHBoxLayout(actionPanel);
    actionLayout->setContentsMargins(18, 18, 18, 18);
    actionLayout->setSpacing(10);

    auto *titleLabel = new QLabel(QStringLiteral("清单库首页"), actionPanel);
    titleLabel->setProperty("role", QStringLiteral("sectionTitle"));

    auto *importButton = new QPushButton(QStringLiteral("导入清单并命名"), actionPanel);
    auto *refreshButton = new QPushButton(QStringLiteral("刷新"), actionPanel);
    importButton->setProperty("variant", QStringLiteral("primary"));
    refreshButton->setProperty("variant", QStringLiteral("subtle"));
    connect(importButton, &QPushButton::clicked, this, &DemandLibraryOverviewPage::importDemandList);
    connect(refreshButton, &QPushButton::clicked, this, &DemandLibraryOverviewPage::reloadData);

    actionLayout->addWidget(titleLabel);
    actionLayout->addStretch();
    actionLayout->addWidget(importButton);
    actionLayout->addWidget(refreshButton);

    auto *tablePanel = new QFrame(this);
    tablePanel->setObjectName(QStringLiteral("sectionPanel"));
    tablePanel->setStyleSheet(sectionPanelStyle());
    auto *tableLayout = new QVBoxLayout(tablePanel);
    tableLayout->setContentsMargins(18, 18, 18, 14);
    tableLayout->setSpacing(10);

    auto *tableTitleLabel = new QLabel(QStringLiteral("已保存清单"), tablePanel);
    tableTitleLabel->setProperty("role", QStringLiteral("sectionTitle"));

    m_table = new QTableWidget(tablePanel);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("清单名称"),
        QStringLiteral("原始文件名"),
        QStringLiteral("识别条目数"),
        QStringLiteral("更新时间")
    });
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->setFrameShape(QFrame::NoFrame);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        const QVariantMap record = recordAtRow(row);
        if (!record.isEmpty() && m_openDetail) {
            m_openDetail(record);
        }
    });

    m_statusLabel = new QLabel(tablePanel);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #71888c; font-size: 12px; padding-top: 6px;"));

    tableLayout->addWidget(tableTitleLabel);
    tableLayout->addWidget(m_table, 1);
    tableLayout->addWidget(m_statusLabel);

    rootLayout->addWidget(actionPanel);
    rootLayout->addWidget(tablePanel, 1);
}

void DemandLibraryOverviewPage::importDemandList()
{
    const QString filePath = QFileDialog::getOpenFileName(this,
                                                          QStringLiteral("选择要保存的清单"),
                                                          QString(),
                                                          QStringLiteral("Excel 文件 (*.xlsx)"));
    if (filePath.isEmpty()) {
        return;
    }

    bool ok = false;
    const QString defaultName = QFileInfo(filePath).completeBaseName();
    const QString name = QInputDialog::getText(this,
                                               QStringLiteral("输入清单名称"),
                                               QStringLiteral("请为这份清单输入名称："),
                                               QLineEdit::Normal,
                                               defaultName,
                                               &ok).trimmed();
    if (!ok) {
        return;
    }

    QString message;
    if (!m_storageService->importDemandList(name, filePath, &message)) {
        QMessageBox::critical(this, QStringLiteral("导入失败"), message);
        return;
    }

    reloadData();
    QMessageBox::information(this, QStringLiteral("导入完成"), message);

    for (const QVariantMap &record : m_records) {
        if (record.value(QStringLiteral("name")).toString().trimmed().compare(name, Qt::CaseInsensitive) == 0) {
            if (m_openDetail) {
                m_openDetail(record);
            }
            break;
        }
    }
}

QVariantMap DemandLibraryOverviewPage::recordAtRow(int row) const
{
    if (row < 0 || row >= m_records.size()) {
        return {};
    }

    return m_records.at(row);
}