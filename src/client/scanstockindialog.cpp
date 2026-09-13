#include "scanstockindialog.h"

#include "inventorytransactiondialog.h"
#include "inventoryrecorddialog.h"

#include <QDate>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QShowEvent>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {
QString inventoryRecordSummary(const QVariantMap &record)
{
    QStringList parts;
    for (const QString &key : {QStringLiteral("uniqueId"), QStringLiteral("manufacturerPart"),
                               QStringLiteral("manufacturer"), QStringLiteral("name")}) {
        const QString value = record.value(key).toString().trimmed();
        if (!value.isEmpty()) {
            parts.append(value);
        }
    }
    return parts.join(QStringLiteral(" / "));
}

QList<FieldDefinition> stockEntryDialogFields(const QList<FieldDefinition> &fields)
{
    return prioritizedFields(fields,
                             {QStringLiteral("manufacturerPart"), QStringLiteral("quantity"),
                              QStringLiteral("location"), QStringLiteral("date")},
                             {QStringLiteral("manufacturerPart"), QStringLiteral("quantity"),
                              QStringLiteral("location"), QStringLiteral("date")});
}

bool parseScanResult(const QString &scanResult, QString *manufacturerPart, int *quantity)
{
    const QRegularExpression partExpression(QStringLiteral("(?:^|[,{\\s])pm\\s*:\\s*([^,}\\s]+)"),
                                             QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression quantityExpression(QStringLiteral("(?:^|[,{\\s])qty\\s*:\\s*(\\d+)"),
                                                 QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch partMatch = partExpression.match(scanResult);
    const QRegularExpressionMatch quantityMatch = quantityExpression.match(scanResult);
    bool quantityOk = false;
    const int parsedQuantity = quantityMatch.captured(1).toInt(&quantityOk);
    if (!partMatch.hasMatch() || !quantityMatch.hasMatch() || !quantityOk || parsedQuantity <= 0) {
        return false;
    }

    *manufacturerPart = partMatch.captured(1).trimmed();
    *quantity = parsedQuantity;
    return !manufacturerPart->isEmpty();
}
}

ScanStockInDialog::ScanStockInDialog(const QList<FieldDefinition> &fields,
                                     const QList<QVariantMap> &inventoryRecords,
                                     AppService *service,
                                     QWidget *parent)
    : QDialog(parent), m_fields(fields), m_inventoryRecords(inventoryRecords), m_service(service)
{
    setWindowTitle(QStringLiteral("扫码入库"));
    resize(920, 620);

    auto *layout = new QVBoxLayout(this);
    auto *title = new QLabel(QStringLiteral("扫码入库"), this);
    title->setStyleSheet(QStringLiteral("font-size: 24px; font-weight: 800;"));
    auto *hint = new QLabel(QStringLiteral("请使用扫码枪扫描二维码。每次扫描后会自动识别 pm 和 qty，并加入待入库列表。"), this);
    hint->setWordWrap(true);
    m_scanInput = new QLineEdit(this);
    m_scanInput->setPlaceholderText(QStringLiteral("等待扫码枪输入…（也可粘贴二维码文本后按回车）"));
    connect(m_scanInput, &QLineEdit::returnPressed, this, &ScanStockInDialog::processScanResult);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({QStringLiteral("类型"), QStringLiteral("物料"), QStringLiteral("入库数量"),
                                        QStringLiteral("业务日期"), QStringLiteral("备注")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);

    auto *removeButton = new QPushButton(QStringLiteral("删除所选"), this);
    connect(removeButton, &QPushButton::clicked, this, &ScanStockInDialog::removeSelectedEntry);
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确认入库"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("退出页面"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ScanStockInDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &ScanStockInDialog::reject);

    layout->addWidget(title);
    layout->addWidget(hint);
    layout->addWidget(m_scanInput);
    layout->addWidget(m_table, 1);
    layout->addWidget(removeButton);
    layout->addWidget(buttonBox);
}

QList<ManualStockInEntry> ScanStockInDialog::entries() const
{
    return m_entries;
}

void ScanStockInDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    m_scanInput->setFocus();
}

void ScanStockInDialog::processScanResult()
{
    const QString scanResult = m_scanInput->text().trimmed();
    m_scanInput->clear();

    QString manufacturerPart;
    int quantity = 0;
    if (!parseScanResult(scanResult, &manufacturerPart, &quantity)) {
        QMessageBox::warning(this, QStringLiteral("二维码格式无效"),
                             QStringLiteral("未能读取 pm 或 qty。二维码需包含类似 pm:C14709,qty:50 的内容。"));
        m_scanInput->setFocus();
        return;
    }

    QList<QVariantMap> matches;
    for (const QVariantMap &record : m_inventoryRecords) {
        if (record.value(QStringLiteral("manufacturerPart")).toString().trimmed()
                .compare(manufacturerPart, Qt::CaseInsensitive) == 0) {
            matches.append(record);
        }
    }
    if (matches.size() > 1) {
        QMessageBox::warning(this, QStringLiteral("物料匹配不唯一"),
                             QStringLiteral("Manufacturer Part %1 匹配到多条库存记录，请先处理重复数据。").arg(manufacturerPart));
        m_scanInput->setFocus();
        return;
    }

    ManualStockInEntry entry;
    entry.note = QStringLiteral("二维码扫码入库");
    if (matches.size() == 1) {
        const QVariantMap &record = matches.constFirst();
        QMessageBox::information(this, QStringLiteral("已有该元件"),
                                 QStringLiteral("已找到库存元件：%1").arg(inventoryRecordSummary(record)));
        InventoryTransactionDialog transactionDialog(InventoryTransactionDialog::Mode::StockIn,
                                                     inventoryRecordSummary(record),
                                                     record.value(QStringLiteral("quantity")).toInt(), this);
        transactionDialog.setQuantity(quantity);
        if (transactionDialog.exec() != QDialog::Accepted) {
            m_scanInput->setFocus();
            return;
        }
        entry.existingItem = true;
        entry.summary = inventoryRecordSummary(record);
        entry.itemData.insert(QStringLiteral("id"), record.value(QStringLiteral("id")));
        entry.itemData.insert(QStringLiteral("quantity"), transactionDialog.quantity());
        entry.itemData.insert(QStringLiteral("date"), transactionDialog.transactionDate());
        entry.note = transactionDialog.note().isEmpty() ? entry.note : transactionDialog.note();
    } else {
        InventoryRecordDialog recordDialog(QStringLiteral("新增入库项目"),
                                           stockEntryDialogFields(m_fields),
                                           m_service,
                                           {},
                                           this);
        QVariantMap defaultRecord;
        defaultRecord.insert(QStringLiteral("manufacturerPart"), manufacturerPart);
        defaultRecord.insert(QStringLiteral("quantity"), quantity);
        defaultRecord.insert(QStringLiteral("date"), QDate::currentDate().toString(Qt::ISODate));
        recordDialog.setRecordData(defaultRecord);
        if (recordDialog.exec() != QDialog::Accepted) {
            m_scanInput->setFocus();
            return;
        }
        entry.existingItem = false;
        entry.itemData = recordDialog.recordData();
        entry.summary = inventoryRecordSummary(entry.itemData);
        const QString note = entry.itemData.value(QStringLiteral("comment")).toString().trimmed();
        if (!note.isEmpty()) {
            entry.note = note;
        }
    }

    m_entries.append(entry);
    refreshTable();
    m_scanInput->setFocus();
}

void ScanStockInDialog::removeSelectedEntry()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_entries.size()) {
        return;
    }
    m_entries.removeAt(row);
    refreshTable();
    m_scanInput->setFocus();
}

void ScanStockInDialog::refreshTable()
{
    m_table->setRowCount(m_entries.size());
    for (int row = 0; row < m_entries.size(); ++row) {
        const ManualStockInEntry &entry = m_entries.at(row);
        m_table->setItem(row, 0, new QTableWidgetItem(entry.existingItem ? QStringLiteral("已有物料") : QStringLiteral("新物料")));
        m_table->setItem(row, 1, new QTableWidgetItem(entry.summary));
        m_table->setItem(row, 2, new QTableWidgetItem(entry.itemData.value(QStringLiteral("quantity")).toString()));
        m_table->setItem(row, 3, new QTableWidgetItem(entry.itemData.value(QStringLiteral("date")).toString()));
        m_table->setItem(row, 4, new QTableWidgetItem(entry.note));
    }
}

void ScanStockInDialog::accept()
{
    if (m_entries.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请至少扫描并添加一条入库项目。"));
        m_scanInput->setFocus();
        return;
    }
    QDialog::accept();
}
