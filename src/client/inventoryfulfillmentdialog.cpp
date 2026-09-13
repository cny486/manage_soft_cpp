#include "inventoryfulfillmentdialog.h"

#include "inventoryitempickerdialog.h"

#include <QColor>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
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
        return QStringLiteral("已确认且库存充足");
    case InventoryFulfillmentStatus::Insufficient:
        return QStringLiteral("已确认但库存不足");
    case InventoryFulfillmentStatus::Missing:
        return QStringLiteral("无匹配元件");
    case InventoryFulfillmentStatus::PendingConfirmation:
        return QStringLiteral("候选待确认");
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
    if (!result.manufacturerPart.trimmed().isEmpty()) {
        parts.append(result.manufacturerPart.trimmed());
    }
    return parts.join(QStringLiteral(" / "));
}

QString candidateText(const InventoryMatchCandidate &candidate)
{
    QStringList parts;
    if (!candidate.uniqueId.trimmed().isEmpty()) {
        parts.append(candidate.uniqueId.trimmed());
    }
    if (!candidate.manufacturerPart.trimmed().isEmpty()) {
        parts.append(candidate.manufacturerPart.trimmed());
    }
    if (!candidate.name.trimmed().isEmpty()) {
        parts.append(candidate.name.trimmed());
    }
    if (!candidate.value.trimmed().isEmpty()) {
        parts.append(candidate.value.trimmed());
    }
    if (!candidate.footprint.trimmed().isEmpty()) {
        parts.append(candidate.footprint.trimmed());
    }
    if (!candidate.manufacturer.trimmed().isEmpty()) {
        parts.append(candidate.manufacturer.trimmed());
    }
    return parts.join(QStringLiteral(" / "));
}

InventoryMatchCandidate candidateFromInventoryRecord(const QVariantMap &record)
{
    InventoryMatchCandidate candidate;
    candidate.itemId = record.value(QStringLiteral("id")).toString();
    candidate.manufacturerPart = record.value(QStringLiteral("manufacturerPart")).toString();
    candidate.manufacturer = record.value(QStringLiteral("manufacturer")).toString();
    candidate.supplier = record.value(QStringLiteral("supplier")).toString();
    candidate.name = record.value(QStringLiteral("name")).toString();
    candidate.value = record.value(QStringLiteral("value")).toString();
    candidate.footprint = record.value(QStringLiteral("footprint")).toString();
    candidate.voltage = record.value(QStringLiteral("voltage")).toString();
    candidate.uniqueId = record.value(QStringLiteral("uniqueId")).toString();
    candidate.unit = record.value(QStringLiteral("unit")).toString();
    candidate.location = record.value(QStringLiteral("location")).toString();
    candidate.availableQuantity = record.value(QStringLiteral("quantity")).toInt();
    return candidate;
}

QStringList relevanceKeywords(const InventoryFulfillmentResult &result)
{
    QStringList keywords = {
        result.requestManufacturerPart,
        result.requestName,
        result.requestValue,
        result.requestFootprint,
        result.requestVoltage,
        result.requestManufacturer,
        result.requestSupplier,
        result.requestDevice,
        result.requestCategory
    };
    keywords.removeAll(QString());
    keywords.removeDuplicates();
    return keywords;
}

QString matchedFieldsText(const QStringList &matchedFields)
{
    if (matchedFields.isEmpty()) {
        return QStringLiteral("-");
    }
    return matchedFields.join(QStringLiteral(", "));
}

QStringList displayHeadersForResults(const QList<InventoryFulfillmentResult> &results)
{
    for (const InventoryFulfillmentResult &result : results) {
        if (!result.sourceHeaders.isEmpty()) {
            return result.sourceHeaders;
        }
    }
    return {};
}

QString sourceCellValue(const InventoryFulfillmentResult &result, int index)
{
    if (index < 0 || index >= result.sourceRowValues.size()) {
        return QString();
    }
    return result.sourceRowValues.at(index);
}

QString componentCellText(const InventoryFulfillmentResult &result)
{
    if (result.status == InventoryFulfillmentStatus::Missing && result.confirmed) {
        return QStringLiteral("已确认无匹配元件");
    }

    const QString text = matchedItemText(result);
    if (!text.isEmpty()) {
        return text;
    }
    if (!result.candidates.isEmpty()) {
        return QStringLiteral("点击选择候选元件");
    }
    return QStringLiteral("无候选元件");
}

int matchScoreMaximum(const InventoryFulfillmentResult &result)
{
    int maximum = 0;
    if (!result.requestManufacturerPart.trimmed().isEmpty()) {
        maximum += 300;
    }
    if (!result.requestValue.trimmed().isEmpty()) {
        maximum += 150;
    }
    if (!result.requestFootprint.trimmed().isEmpty()) {
        maximum += 145;
    }
    if (!result.requestVoltage.trimmed().isEmpty()) {
        maximum += 130;
    }
    if (!result.requestName.trimmed().isEmpty()) {
        maximum += 125;
    }
    if (!result.requestManufacturer.trimmed().isEmpty()) {
        maximum += 90;
    }
    if (!result.requestSupplier.trimmed().isEmpty()) {
        maximum += 85;
    }
    if (!result.requestDevice.trimmed().isEmpty()) {
        maximum += 75;
    }
    if (!result.requestCategory.trimmed().isEmpty()) {
        maximum += 60;
    }
    if (!result.requestDesignator.trimmed().isEmpty()) {
        maximum += 45;
    }
    return maximum;
}

QString matchScorePercentText(int score, int maximum)
{
    if (score <= 0 || maximum <= 0) {
        return QStringLiteral("-");
    }

    const int percent = qBound(0,
                               qRound((static_cast<double>(score) * 100.0)
                                      / static_cast<double>(maximum)),
                               100);
    return QStringLiteral("%1%").arg(percent);
}

QString summaryText(const QList<InventoryFulfillmentResult> &results)
{
    int sufficientCount = 0;
    int insufficientCount = 0;
    int missingCount = 0;
    int pendingCount = 0;
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
        case InventoryFulfillmentStatus::PendingConfirmation:
            ++pendingCount;
            break;
        }
    }

    return QStringLiteral("共 %1 项：充足 %2 项，库存不足 %3 项，待确认 %4 项，无匹配 %5 项。")
        .arg(results.size())
        .arg(sufficientCount)
        .arg(insufficientCount)
        .arg(pendingCount)
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

void adoptCandidate(InventoryFulfillmentResult *result, const InventoryMatchCandidate &candidate)
{
    result->itemId = candidate.itemId;
    result->manufacturerPart = candidate.manufacturerPart;
    result->manufacturer = candidate.manufacturer;
    result->name = candidate.name;
    result->uniqueId = candidate.uniqueId;
    result->unit = candidate.unit;
    result->location = candidate.location;
    result->availableQuantity = candidate.availableQuantity;
    result->matchScore = candidate.score;
    result->matchedFields = candidate.matchedFields;
    result->confirmed = true;
    result->status = result->availableQuantity >= result->requiredQuantity
                         ? InventoryFulfillmentStatus::Sufficient
                         : InventoryFulfillmentStatus::Insufficient;
}

int currentCandidateIndex(const InventoryFulfillmentResult &result)
{
    if (result.candidates.isEmpty()) {
        return -1;
    }

    if (!result.itemId.trimmed().isEmpty()) {
        for (int index = 0; index < result.candidates.size(); ++index) {
            if (result.candidates.at(index).itemId == result.itemId) {
                return index;
            }
        }
    }

    return 0;
}

QColor statusBackgroundColor(InventoryFulfillmentStatus status)
{
    switch (status) {
    case InventoryFulfillmentStatus::Sufficient:
        return QColor(QStringLiteral("#c6efce"));
    case InventoryFulfillmentStatus::Insufficient:
        return QColor(QStringLiteral("#ffeb9c"));
    case InventoryFulfillmentStatus::PendingConfirmation:
        return QColor(QStringLiteral("#ddebf7"));
    case InventoryFulfillmentStatus::Missing:
        return QColor(QStringLiteral("#ffc7ce"));
    }

    return QColor(Qt::white);
}

QColor statusForegroundColor(InventoryFulfillmentStatus status)
{
    switch (status) {
    case InventoryFulfillmentStatus::Sufficient:
        return QColor(QStringLiteral("#006100"));
    case InventoryFulfillmentStatus::Insufficient:
        return QColor(QStringLiteral("#9c5700"));
    case InventoryFulfillmentStatus::PendingConfirmation:
        return QColor(QStringLiteral("#1f4e78"));
    case InventoryFulfillmentStatus::Missing:
        return QColor(QStringLiteral("#9c0006"));
    }

    return QColor(Qt::black);
}
}

InventoryFulfillmentDialog::InventoryFulfillmentDialog(AppService *storageService,
                                                       QWidget *parent)
    : QDialog(parent),
      m_storageService(storageService)
{
    setWindowTitle(QStringLiteral("配单确认"));
    resize(1440, 760);
    setStyleSheet(QStringLiteral(
        "InventoryFulfillmentDialog { background: #f3f6f9; }"
        "QLabel { color: #1f2937; }"
        "QTableWidget {"
        "  background: #ffffff;"
        "  alternate-background-color: #f8fbff;"
        "  border: 1px solid #b8cce4;"
        "  gridline-color: #d9e2f3;"
        "  color: #1f2937;"
        "  selection-background-color: #cfe8ff;"
        "  selection-color: #1f2937;"
        "  font-family: 'Aptos', 'Microsoft YaHei UI', sans-serif;"
        "}"
        "QTableWidget::item { padding: 4px 8px; border: 0px; }"
        "QHeaderView::section {"
        "  background: #4472c4; color: white; font-weight: 700;"
        "  border: 0px; border-right: 1px solid #8eaadb;"
        "  border-bottom: 1px solid #2f5597; padding: 7px 8px;"
        "}"
        "QTableCornerButton::section { background: #4472c4; border: 0px; }"
        "QTableWidget QTableCornerButton::section { background: #4472c4; }"
        "QHeaderView::section:vertical { background: #f2f2f2; color: #595959;"
        "  border-right: 1px solid #d9e2f3; border-bottom: 1px solid #d9e2f3; }"));

    auto *rootLayout = new QVBoxLayout(this);

    auto *actionLayout = new QHBoxLayout();
    auto *importButton = new QPushButton(QStringLiteral("导入配单表"), this);
    m_exportButton = new QPushButton(QStringLiteral("导出匹配结果"), this);
    connect(importButton, &QPushButton::clicked, this, &InventoryFulfillmentDialog::importDemandFile);
    connect(m_exportButton, &QPushButton::clicked, this, &InventoryFulfillmentDialog::exportResults);
    actionLayout->addWidget(importButton);
    actionLayout->addWidget(m_exportButton);
    actionLayout->addStretch();

    m_sourceLabel = new QLabel(QStringLiteral("当前未导入配单表。"), this);
    m_summaryLabel = new QLabel(QStringLiteral("请先导入配单表。"), this);

    m_resultsTable = new QTableWidget(this);
    m_resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_resultsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_resultsTable->setAlternatingRowColors(true);
    m_resultsTable->setShowGrid(true);
    m_resultsTable->setGridStyle(Qt::SolidLine);
    m_resultsTable->setWordWrap(false);
    m_resultsTable->horizontalHeader()->setStretchLastSection(false);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_resultsTable->horizontalHeader()->setMinimumSectionSize(100);
    m_resultsTable->verticalHeader()->setVisible(true);
    m_resultsTable->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);
    m_resultsTable->verticalHeader()->setMinimumSectionSize(48);
    m_resultsTable->verticalHeader()->setDefaultSectionSize(34);
    connect(m_resultsTable, &QTableWidget::cellClicked, this, &InventoryFulfillmentDialog::handleResultCellClicked);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_applyButton = new QPushButton(QStringLiteral("一键出库已确认项"), this);
    m_applyButton->setProperty("variant", QStringLiteral("primary"));
    connect(buttonBox->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &InventoryFulfillmentDialog::reject);
    connect(m_applyButton, &QPushButton::clicked, this, &InventoryFulfillmentDialog::applySelectedStockOut);

    auto *footerLayout = new QHBoxLayout();
    footerLayout->addStretch();
    footerLayout->addWidget(m_applyButton);
    footerLayout->addWidget(buttonBox->button(QDialogButtonBox::Close));

    rootLayout->addLayout(actionLayout);
    rootLayout->addWidget(m_sourceLabel);
    rootLayout->addWidget(m_summaryLabel);
    rootLayout->addWidget(m_resultsTable, 1);
    rootLayout->addLayout(footerLayout);

    updateActionState();
}

bool InventoryFulfillmentDialog::inventoryChanged() const
{
    return m_inventoryChanged;
}

void InventoryFulfillmentDialog::loadResults(const QList<InventoryFulfillmentResult> &results,
                                             const QString &sourceLabel)
{
    m_results = results;
    m_sourceFilePath = sourceLabel;
    m_fulfillmentSetCount = 0;
    refreshTable();
}

void InventoryFulfillmentDialog::importDemandFile()
{
    const QString filePath = QFileDialog::getOpenFileName(this,
                                                          QStringLiteral("选择配单表"),
                                                          QString(),
                                                          QStringLiteral("Excel 文件 (*.xlsx)"));
    if (filePath.isEmpty()) {
        return;
    }

    bool accepted = false;
    const int fulfillmentSetCount = QInputDialog::getInt(
        this,
        QStringLiteral("选择配单套数"),
        QStringLiteral("请输入本次需要配单的套数：\nExcel 中的 Quantity 将按 单套用量 × 配单套数 计算。"),
        1,
        1,
        1000000,
        1,
        &accepted);
    if (!accepted) {
        return;
    }

    QList<InventoryFulfillmentResult> results;
    QString message;
    if (!m_storageService->analyzeInventoryFulfillment(filePath, fulfillmentSetCount, &results, &message)) {
        showResultMessage(this, QStringLiteral("配单失败"), QMessageBox::Warning, message);
        return;
    }

    m_results = results;
    m_sourceFilePath = filePath;
    m_fulfillmentSetCount = fulfillmentSetCount;
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
    if (!m_storageService->exportInventoryFulfillment(m_results, m_sourceFilePath, filePath, &errorMessage)) {
        QMessageBox::critical(this, QStringLiteral("导出失败"), errorMessage);
        return;
    }

    QMessageBox::information(this, QStringLiteral("导出完成"), QStringLiteral("配单结果已导出。"));
}

void InventoryFulfillmentDialog::handleResultCellClicked(int row, int column)
{
    if (row < 0 || row >= m_results.size()) {
        return;
    }

    if (column == 0) {
        showStatusMenu(row);
    } else if (column == 1) {
        showCandidateMenu(row);
    }
}

void InventoryFulfillmentDialog::showStatusMenu(int resultRow)
{
    if (resultRow < 0 || resultRow >= m_results.size()) {
        return;
    }

    const InventoryFulfillmentResult &result = m_results.at(resultRow);
    QMenu menu(this);

    if (!result.candidates.isEmpty()) {
        QAction *confirmAction = menu.addAction(QStringLiteral("确认当前候选元件"));
        connect(confirmAction, &QAction::triggered, this, [this, resultRow]() {
            const int candidateIndex = currentCandidateIndex(m_results.at(resultRow));
            if (candidateIndex >= 0) {
                confirmCandidateForRow(resultRow, candidateIndex);
            }
        });

        QAction *pendingAction = menu.addAction(QStringLiteral("设为候选待确认"));
        connect(pendingAction, &QAction::triggered, this, [this, resultRow]() {
            clearRowConfirmation(resultRow);
        });
    }

    QAction *missingAction = menu.addAction(QStringLiteral("标记无匹配元件"));
    connect(missingAction, &QAction::triggered, this, [this, resultRow]() {
        markRowMissing(resultRow);
    });

    const QRect cellRect = m_resultsTable->visualItemRect(m_resultsTable->item(resultRow, 0));
    menu.exec(m_resultsTable->viewport()->mapToGlobal(cellRect.bottomLeft()));
}

void InventoryFulfillmentDialog::showCandidateMenu(int resultRow)
{
    if (resultRow < 0 || resultRow >= m_results.size()) {
        return;
    }

    const InventoryFulfillmentResult &result = m_results.at(resultRow);
    QMenu menu(this);
    const int maximumScore = matchScoreMaximum(result);
    const int selectedIndex = currentCandidateIndex(result);
    for (int index = 0; index < result.candidates.size(); ++index) {
        const InventoryMatchCandidate &candidate = result.candidates.at(index);
        QString label = QStringLiteral("%1库存 %2 | 匹配 %3 | %4")
                            .arg(index == selectedIndex ? QStringLiteral("已选 | ") : QString())
                            .arg(candidate.availableQuantity)
                            .arg(matchScorePercentText(candidate.score, maximumScore))
                            .arg(candidateText(candidate));
        QAction *action = menu.addAction(label);
        action->setToolTip(QStringLiteral("命中字段：%1\n库位：%2\n单位：%3")
                               .arg(matchedFieldsText(candidate.matchedFields),
                                    candidate.location,
                                    candidate.unit));
        connect(action, &QAction::triggered, this, [this, resultRow, index]() {
            confirmCandidateForRow(resultRow, index);
        });
    }

    menu.addSeparator();
    QAction *browseAllAction = menu.addAction(QStringLiteral("浏览全部库存元件（按相关度排序）"));
    connect(browseAllAction, &QAction::triggered, this, [this, resultRow]() {
        browseAllInventoryItems(resultRow);
    });

    const QRect cellRect = m_resultsTable->visualItemRect(m_resultsTable->item(resultRow, 1));
    menu.exec(m_resultsTable->viewport()->mapToGlobal(cellRect.bottomLeft()));
}

void InventoryFulfillmentDialog::browseAllInventoryItems(int resultRow)
{
    if (resultRow < 0 || resultRow >= m_results.size() || m_storageService == nullptr) {
        return;
    }

    const QList<QVariantMap> inventoryRecords = m_storageService->loadPageRecords(QStringLiteral("inventory"));
    if (inventoryRecords.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("库存为空"), QStringLiteral("当前没有可浏览的库存元件。"));
        return;
    }

    InventoryItemPickerDialog picker(inventoryRecords, relevanceKeywords(m_results.at(resultRow)), this);
    picker.setWindowTitle(QStringLiteral("浏览库存并选择配单元件"));
    if (picker.exec() != QDialog::Accepted) {
        return;
    }

    const QVariantMap selectedRecord = picker.selectedRecord();
    if (selectedRecord.isEmpty()) {
        return;
    }

    InventoryFulfillmentResult &result = m_results[resultRow];
    const InventoryMatchCandidate selectedCandidate = candidateFromInventoryRecord(selectedRecord);
    bool exists = false;
    for (const InventoryMatchCandidate &candidate : result.candidates) {
        if (candidate.itemId == selectedCandidate.itemId) {
            exists = true;
            break;
        }
    }
    if (!exists) {
        result.candidates.append(selectedCandidate);
    }

    adoptCandidate(&result, selectedCandidate);
    refreshTable();
    m_resultsTable->setCurrentCell(resultRow, 1);
}

void InventoryFulfillmentDialog::confirmCandidateForCurrentRow(int candidateRow)
{
    confirmCandidateForRow(m_resultsTable->currentRow(), candidateRow);
}

void InventoryFulfillmentDialog::confirmCandidateForRow(int resultRow, int candidateRow)
{
    if (resultRow < 0 || resultRow >= m_results.size()) {
        return;
    }

    InventoryFulfillmentResult &result = m_results[resultRow];
    if (candidateRow < 0 || candidateRow >= result.candidates.size()) {
        return;
    }

    adoptCandidate(&result, result.candidates.at(candidateRow));
    refreshTable();
    m_resultsTable->setCurrentCell(resultRow, 1);
}

void InventoryFulfillmentDialog::markCurrentRowMissing()
{
    markRowMissing(m_resultsTable->currentRow());
}

void InventoryFulfillmentDialog::markRowMissing(int resultRow)
{
    if (resultRow < 0 || resultRow >= m_results.size()) {
        return;
    }

    InventoryFulfillmentResult &result = m_results[resultRow];
    result.confirmed = true;
    result.status = InventoryFulfillmentStatus::Missing;
    result.itemId.clear();
    result.manufacturerPart.clear();
    result.manufacturer.clear();
    result.name.clear();
    result.uniqueId.clear();
    result.unit.clear();
    result.location.clear();
    result.availableQuantity = 0;
    result.matchScore = 0;
    result.matchedFields.clear();

    refreshTable();
    m_resultsTable->setCurrentCell(resultRow, 0);
}

void InventoryFulfillmentDialog::clearCurrentConfirmation()
{
    clearRowConfirmation(m_resultsTable->currentRow());
}

void InventoryFulfillmentDialog::clearRowConfirmation(int resultRow)
{
    if (resultRow < 0 || resultRow >= m_results.size()) {
        return;
    }

    InventoryFulfillmentResult &result = m_results[resultRow];
    result.confirmed = false;
    result.itemId.clear();
    result.manufacturerPart.clear();
    result.manufacturer.clear();
    result.name.clear();
    result.uniqueId.clear();
    result.unit.clear();
    result.location.clear();
    result.availableQuantity = 0;
    result.matchScore = 0;
    result.matchedFields.clear();
    if (!result.candidates.isEmpty()) {
        const InventoryMatchCandidate &bestCandidate = result.candidates.constFirst();
        result.itemId = bestCandidate.itemId;
        result.manufacturerPart = bestCandidate.manufacturerPart;
        result.manufacturer = bestCandidate.manufacturer;
        result.name = bestCandidate.name;
        result.uniqueId = bestCandidate.uniqueId;
        result.unit = bestCandidate.unit;
        result.location = bestCandidate.location;
        result.availableQuantity = bestCandidate.availableQuantity;
        result.matchScore = bestCandidate.score;
        result.matchedFields = bestCandidate.matchedFields;
        result.status = InventoryFulfillmentStatus::PendingConfirmation;
    } else {
        result.status = InventoryFulfillmentStatus::Missing;
    }

    refreshTable();
    m_resultsTable->setCurrentCell(resultRow, 0);
}

void InventoryFulfillmentDialog::applySelectedStockOut()
{
    const QList<InventoryFulfillmentResult> results = confirmedStockOutResults();
    if (results.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("当前没有“已确认且库存充足”的项目可执行一键出库。"));
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        QStringLiteral("确认出库"),
        QStringLiteral("确认对已确认且库存充足的 %1 项执行一键出库吗？").arg(results.size()));
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
    const QStringList headers = displayHeaders();
    QStringList tableHeaders = {
        QStringLiteral("配单状态"),
        QStringLiteral("配单元件"),
        QStringLiteral("库存匹配数"),
        QStringLiteral("实际配单数")
    };
    tableHeaders.append(headers);

    m_resultsTable->clear();
    m_resultsTable->setColumnCount(tableHeaders.size());
    m_resultsTable->setHorizontalHeaderLabels(tableHeaders);
    m_resultsTable->setRowCount(m_results.size());

    for (int row = 0; row < m_results.size(); ++row) {
        const InventoryFulfillmentResult &result = m_results.at(row);

        auto *statusItem = new QTableWidgetItem(fulfillmentStatusText(result.status));
        statusItem->setBackground(statusBackgroundColor(result.status));
        statusItem->setForeground(statusForegroundColor(result.status));
        QFont statusFont = statusItem->font();
        statusFont.setBold(true);
        statusItem->setFont(statusFont);
        statusItem->setTextAlignment(Qt::AlignCenter);
        statusItem->setToolTip(QStringLiteral("点击可修改状态。"));
        m_resultsTable->setItem(row, 0, statusItem);

        auto *componentItem = new QTableWidgetItem(componentCellText(result));
        componentItem->setToolTip(QStringLiteral("点击可切换候选元件。\n命中字段：%1\n库存：%2\n库位：%3")
                                      .arg(matchedFieldsText(result.matchedFields),
                                           QString::number(result.availableQuantity),
                                           result.location));
        m_resultsTable->setItem(row, 1, componentItem);

        auto *availableQuantityItem = new QTableWidgetItem(QString::number(result.availableQuantity));
        availableQuantityItem->setTextAlignment(Qt::AlignCenter);
        availableQuantityItem->setToolTip(QStringLiteral("当前匹配库存元件的可用库存数量"));
        m_resultsTable->setItem(row, 2, availableQuantityItem);

        auto *requiredQuantityItem = new QTableWidgetItem(QString::number(result.requiredQuantity));
        requiredQuantityItem->setTextAlignment(Qt::AlignCenter);
        requiredQuantityItem->setToolTip(QStringLiteral("实际配单数 = Excel Quantity × 配单套数"));
        m_resultsTable->setItem(row, 3, requiredQuantityItem);

        for (int column = 0; column < headers.size(); ++column) {
            auto *cellItem = new QTableWidgetItem(sourceCellValue(result, column));
            m_resultsTable->setItem(row, column + 4, cellItem);
        }

        auto *rowHeaderItem = new QTableWidgetItem(sourceRowsText(result.sourceRows));
        rowHeaderItem->setToolTip(QStringLiteral("来源行：%1").arg(sourceRowsText(result.sourceRows)));
        m_resultsTable->setVerticalHeaderItem(row, rowHeaderItem);
    }

    m_resultsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_resultsTable->setColumnWidth(0, 150);
    m_resultsTable->setColumnWidth(1, 210);
    m_resultsTable->setColumnWidth(2, 105);
    m_resultsTable->setColumnWidth(3, 105);
    for (int column = 4; column < tableHeaders.size(); ++column) {
        m_resultsTable->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }

    if (m_sourceFilePath.isEmpty()) {
        m_sourceLabel->setText(QStringLiteral("当前未导入配单表。"));
    } else {
        const QString fulfillmentSetText = m_fulfillmentSetCount > 0
                                               ? QStringLiteral("（配单 %1 套，Quantity 已按套数计算）")
                                                     .arg(m_fulfillmentSetCount)
                                               : QString();
        m_sourceLabel->setText(QStringLiteral("来源文件：%1 %2").arg(m_sourceFilePath, fulfillmentSetText));
    }
    m_summaryLabel->setText(m_results.isEmpty() ? QStringLiteral("请先导入配单表。") : summaryText(m_results));

    updateActionState();
}

void InventoryFulfillmentDialog::updateActionState()
{
    const bool hasResults = !m_results.isEmpty();
    m_exportButton->setEnabled(hasResults);
    m_applyButton->setEnabled(!confirmedStockOutResults().isEmpty());
}

QStringList InventoryFulfillmentDialog::displayHeaders() const
{
    return displayHeadersForResults(m_results);
}

QList<InventoryFulfillmentResult> InventoryFulfillmentDialog::confirmedStockOutResults() const
{
    QList<InventoryFulfillmentResult> results;
    for (const InventoryFulfillmentResult &result : m_results) {
        if (result.confirmed && result.status == InventoryFulfillmentStatus::Sufficient) {
            results.append(result);
        }
    }
    return results;
}
