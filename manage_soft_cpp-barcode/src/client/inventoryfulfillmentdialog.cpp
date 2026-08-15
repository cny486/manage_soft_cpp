#include "inventoryfulfillmentdialog.h"

#include <QColor>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {
QString fulfillmentStatusText(InventoryFulfillmentStatus status)
{
    switch (status) {
    case InventoryFulfillmentStatus::Sufficient:
        return QStringLiteral("有该元件且充足");
    case InventoryFulfillmentStatus::Insufficient:
        return QStringLiteral("有该元件但数量不足");
    case InventoryFulfillmentStatus::Missing:
        return QStringLiteral("无该元件");
    }

    return QStringLiteral("未知");
}

QString sourceRowsText(const QList<int> &sourceRows)
{
    QStringList lines;
    lines.reserve(sourceRows.size());
    for (const int sourceRow : sourceRows) {
        lines.append(QString::number(sourceRow));
    }
    return lines.join(QStringLiteral(", "));
}

QString matchedItemText(const InventoryFulfillmentResult &result)
{
    QStringList parts;
    if (!result.uniqueId.trimmed().isEmpty()) {
        parts.append(result.uniqueId.trimmed());
    }
    if (!result.name.trimmed().isEmpty()) {
        parts.append(result.name.trimmed());
    }
    if (!result.manufacturer.trimmed().isEmpty()) {
        parts.append(result.manufacturer.trimmed());
    }
    if (parts.isEmpty()) {
        return QStringLiteral("-");
    }
    return parts.join(QStringLiteral(" / "));
}

QString summaryText(const QList<InventoryFulfillmentResult> &results)
{
    int sufficientCount = 0;
    int insufficientCount = 0;
    int missingCount = 0;
    for (const InventoryFulfillmentResult &result : results) {
        switch (result.status) {
        case InventoryFulfillmentStatus::Sufficient:
            ++sufficientCount;
            break;
        case InventoryFulfillmentStatus::Insufficient:
            ++insufficientCount;
            break;
        case InventoryFulfillmentStatus::Missing:
            ++missingCount;
            break;
        }
    }

    return QStringLiteral("共 %1 种元件：充足 %2 种，数量不足 %3 种，无料 %4 种。")
        .arg(results.size())
        .arg(sufficientCount)
        .arg(insufficientCount)
        .arg(missingCount);
}

void showResultMessage(QWidget *parent, const QString &title, QMessageBox::Icon icon, const QString &message)
{
    const QStringList lines = message.split(QChar('\n'));
    const QString summary = lines.isEmpty() ? message : lines.first();
    const QString details = lines.size() > 1 ? lines.mid(1).join(QStringLiteral("\n")).trimmed() : QString();

    QMessageBox box(parent);
    box.setIcon(icon);
    box.setWindowTitle(title);
    box.setText(summary);
    if (!details.isEmpty()) {
        box.setDetailedText(details);
        box.setInformativeText(QStringLiteral("可展开查看详细信息。"));
    }
    box.exec();
}
}

InventoryFulfillmentDialog::InventoryFulfillmentDialog(AppService *storageService,
                                                       QWidget *parent)
    : QDialog(parent),
      m_storageService(storageService)
{
    setWindowTitle(QStringLiteral("配单"));
    resize(1180, 640);

    auto *rootLayout = new QVBoxLayout(this);

    auto *actionLayout = new QHBoxLayout();
    auto *importButton = new QPushButton(QStringLiteral("导入清单"), this);
    auto *selectButton = new QPushButton(QStringLiteral("全选可出库"), this);
    auto *clearButton = new QPushButton(QStringLiteral("清空选择"), this);
    m_exportButton = new QPushButton(QStringLiteral("导出结果"), this);
    m_applyButton = new QPushButton(QStringLiteral("一键出库"), this);

    connect(importButton, &QPushButton::clicked, this, &InventoryFulfillmentDialog::importDemandFile);
    connect(selectButton, &QPushButton::clicked, this, &InventoryFulfillmentDialog::selectSufficientRows);
    connect(clearButton, &QPushButton::clicked, this, &InventoryFulfillmentDialog::clearSelection);
    connect(m_exportButton, &QPushButton::clicked, this, &InventoryFulfillmentDialog::exportResults);
    connect(m_applyButton, &QPushButton::clicked, this, &InventoryFulfillmentDialog::applySelectedStockOut);

    actionLayout->addWidget(importButton);
    actionLayout->addWidget(selectButton);
    actionLayout->addWidget(clearButton);
    actionLayout->addWidget(m_exportButton);
    actionLayout->addWidget(m_applyButton);
    actionLayout->addStretch();

    m_sourceLabel = new QLabel(QStringLiteral("当前未导入配单清单。"), this);
    m_summaryLabel = new QLabel(QStringLiteral("请先导入配单清单。"), this);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(10);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("出库"),
        QStringLiteral("配单状态"),
        QStringLiteral("Manufacturer Part"),
        QStringLiteral("需求数量"),
        QStringLiteral("可用库存"),
        QStringLiteral("缺口数量"),
        QStringLiteral("匹配库存物料"),
        QStringLiteral("单位"),
        QStringLiteral("存储位置"),
        QStringLiteral("来源行")
    });
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &InventoryFulfillmentDialog::reject);

    rootLayout->addLayout(actionLayout);
    rootLayout->addWidget(m_sourceLabel);
    rootLayout->addWidget(m_summaryLabel);
    rootLayout->addWidget(m_table, 1);
    rootLayout->addWidget(buttonBox);

    m_exportButton->setEnabled(false);
    m_applyButton->setEnabled(false);
}

bool InventoryFulfillmentDialog::inventoryChanged() const
{
    return m_inventoryChanged;
}

void InventoryFulfillmentDialog::importDemandFile()
{
    const QString filePath = QFileDialog::getOpenFileName(this,
                                                          QStringLiteral("选择配单清单"),
                                                          QString(),
                                                          QStringLiteral("Excel 文件 (*.xlsx)"));
    if (filePath.isEmpty()) {
        return;
    }

    QList<InventoryFulfillmentResult> results;
    QString message;
    if (!m_storageService->analyzeInventoryFulfillment(filePath, &results, &message)) {
        showResultMessage(this, QStringLiteral("配单失败"), QMessageBox::Warning, message);
        return;
    }

    m_results = results;
    m_sourceFilePath = filePath;
    refreshTable();
    showResultMessage(this, QStringLiteral("配单完成"), QMessageBox::Information, message);
}

void InventoryFulfillmentDialog::exportResults()
{
    if (m_results.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("当前没有可导出的配单结果。"));
        return;
    }

    QString defaultName = QStringLiteral("inventory_fulfillment.xlsx");
    if (!m_sourceFilePath.isEmpty()) {
        const QFileInfo fileInfo(m_sourceFilePath);
        defaultName = fileInfo.completeBaseName() + QStringLiteral("_配单结果.xlsx");
    }

    const QString filePath = QFileDialog::getSaveFileName(this,
                                                          QStringLiteral("导出配单结果"),
                                                          defaultName,
                                                          QStringLiteral("Excel 文件 (*.xlsx)"));
    if (filePath.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!m_storageService->exportInventoryFulfillment(m_results, filePath, &errorMessage)) {
        QMessageBox::critical(this, QStringLiteral("导出失败"), errorMessage);
        return;
    }

    QMessageBox::information(this, QStringLiteral("导出完成"), QStringLiteral("配单结果已导出。"));
}

void InventoryFulfillmentDialog::selectSufficientRows()
{
    for (int row = 0; row < m_results.size(); ++row) {
        QTableWidgetItem *item = m_table->item(row, 0);
        if (item == nullptr || !(item->flags() & Qt::ItemIsUserCheckable)) {
            continue;
        }
        item->setCheckState(m_results.at(row).status == InventoryFulfillmentStatus::Sufficient
                                ? Qt::Checked
                                : Qt::Unchecked);
    }
}

void InventoryFulfillmentDialog::clearSelection()
{
    for (int row = 0; row < m_results.size(); ++row) {
        QTableWidgetItem *item = m_table->item(row, 0);
        if (item != nullptr && (item->flags() & Qt::ItemIsUserCheckable)) {
            item->setCheckState(Qt::Unchecked);
        }
    }
}

void InventoryFulfillmentDialog::applySelectedStockOut()
{
    const QList<InventoryFulfillmentResult> results = selectedResults();
    if (results.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先勾选可出库的元件。"));
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        QStringLiteral("确认出库"),
        QStringLiteral("确认对已勾选的 %1 种元件执行一键出库吗？").arg(results.size()));
    if (answer != QMessageBox::Yes) {
        return;
    }

    QString message;
    const bool success = m_storageService->applyInventoryFulfillment(results, &message);
    showResultMessage(this,
                      success ? QStringLiteral("出库完成") : QStringLiteral("出库失败"),
                      success ? QMessageBox::Information : QMessageBox::Warning,
                      message);
    if (!success) {
        return;
    }

    m_inventoryChanged = true;
    accept();
}

void InventoryFulfillmentDialog::refreshTable()
{
    m_table->setRowCount(m_results.size());
    for (int row = 0; row < m_results.size(); ++row) {
        const InventoryFulfillmentResult &result = m_results.at(row);

        auto *checkItem = new QTableWidgetItem(result.status == InventoryFulfillmentStatus::Sufficient
                                                   ? QStringLiteral("出库")
                                                   : QStringLiteral("不可出库"));
        checkItem->setTextAlignment(Qt::AlignCenter);
        if (result.status == InventoryFulfillmentStatus::Sufficient) {
            checkItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
            checkItem->setCheckState(Qt::Unchecked);
        } else {
            checkItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        }
        m_table->setItem(row, 0, checkItem);

        auto *statusItem = new QTableWidgetItem(fulfillmentStatusText(result.status));
        if (result.status == InventoryFulfillmentStatus::Sufficient) {
            statusItem->setBackground(QColor(QStringLiteral("#e8f5e9")));
        } else if (result.status == InventoryFulfillmentStatus::Insufficient) {
            statusItem->setBackground(QColor(QStringLiteral("#fff8e1")));
        } else {
            statusItem->setBackground(QColor(QStringLiteral("#ffebee")));
        }
        m_table->setItem(row, 1, statusItem);
        m_table->setItem(row, 2, new QTableWidgetItem(result.manufacturerPart));
        m_table->setItem(row, 3, new QTableWidgetItem(QString::number(result.requiredQuantity)));
        m_table->setItem(row, 4, new QTableWidgetItem(QString::number(result.availableQuantity)));
        m_table->setItem(row, 5, new QTableWidgetItem(QString::number(qMax(0, result.requiredQuantity - result.availableQuantity))));
        m_table->setItem(row, 6, new QTableWidgetItem(matchedItemText(result)));
        m_table->setItem(row, 7, new QTableWidgetItem(result.unit));
        m_table->setItem(row, 8, new QTableWidgetItem(result.location));
        m_table->setItem(row, 9, new QTableWidgetItem(sourceRowsText(result.sourceRows)));
    }

    if (m_sourceFilePath.isEmpty()) {
        m_sourceLabel->setText(QStringLiteral("当前未导入配单清单。"));
    } else {
        m_sourceLabel->setText(QStringLiteral("来源文件：%1").arg(m_sourceFilePath));
    }
    m_summaryLabel->setText(m_results.isEmpty() ? QStringLiteral("请先导入配单清单。") : summaryText(m_results));
    m_exportButton->setEnabled(!m_results.isEmpty());
    m_applyButton->setEnabled(!m_results.isEmpty());
}

QList<InventoryFulfillmentResult> InventoryFulfillmentDialog::selectedResults() const
{
    QList<InventoryFulfillmentResult> results;
    for (int row = 0; row < m_results.size(); ++row) {
        const QTableWidgetItem *item = m_table->item(row, 0);
        if (item == nullptr || !(item->flags() & Qt::ItemIsUserCheckable)) {
            continue;
        }
        if (item->checkState() == Qt::Checked) {
            results.append(m_results.at(row));
        }
    }
    return results;
}