#include "manualstockindialog.h"

#include "inventoryitempickerdialog.h"
#include "inventorytransactiondialog.h"
#include "inventoryrecorddialog.h"

#include <QDate>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {
QString inventoryRecordSummary(const QVariantMap &record)
{
    const QString uniqueId = record.value(QStringLiteral("uniqueId")).toString().trimmed();
    const QString manufacturerPart = record.value(QStringLiteral("manufacturerPart")).toString().trimmed();
    const QString manufacturer = record.value(QStringLiteral("manufacturer")).toString().trimmed();
    const QString name = record.value(QStringLiteral("name")).toString().trimmed();

    QStringList parts;
    if (!uniqueId.isEmpty()) {
        parts.append(uniqueId);
    }
    if (!manufacturerPart.isEmpty()) {
        parts.append(manufacturerPart);
    }
    if (!manufacturer.isEmpty()) {
        parts.append(manufacturer);
    }
    if (!name.isEmpty()) {
        parts.append(name);
    }

    return parts.join(QStringLiteral(" / "));
}

QList<FieldDefinition> stockEntryDialogFields(const QList<FieldDefinition> &fields)
{
    return prioritizedFields(fields,
                             {
                                 QStringLiteral("manufacturerPart"),
                                 QStringLiteral("quantity"),
                                 QStringLiteral("location"),
                                 QStringLiteral("date")
                             },
                             {
                                 QStringLiteral("manufacturerPart"),
                                 QStringLiteral("quantity"),
                                 QStringLiteral("location"),
                                 QStringLiteral("date")
                             });
}
}

ManualStockInDialog::ManualStockInDialog(const QList<FieldDefinition> &fields,
                                         const QList<QVariantMap> &inventoryRecords,
                                         AppService *service,
                                         QWidget *parent)
    : QDialog(parent),
      m_fields(fields),
      m_inventoryRecords(inventoryRecords),
      m_service(service)
{
    setWindowTitle(QStringLiteral("手动入库"));
    resize(920, 560);
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #f4f7f8; }"
        "QLabel { color: #284048; font-weight: 600; }"
        "QFrame#sectionCard { background-color: #fbfdfd; border: 1px solid #dde8e9; border-radius: 22px; }"
        "QPushButton { background-color: #fbfdfd; color: #1d3135; border: 1px solid #d7e3e5; border-radius: 14px; padding: 10px 16px; font-weight: 600; }"
        "QPushButton:hover { background-color: #f5fbfb; border-color: #bfd2d4; }"
        "QPushButton[variant='primary'] { background-color: #5f8e8a; color: #ffffff; border: 1px solid #5f8e8a; }"
        "QPushButton[variant='primary']:hover { background-color: #547f7b; border-color: #547f7b; }"
        "QPushButton[variant='subtle'] { background-color: #f7fbfb; color: #557075; border: 1px solid #dce7e8; }"
        "QPushButton[variant='subtle']:hover { background-color: #eef6f6; border-color: #c8d9db; }"
        "QPushButton[variant='danger'] { background-color: #fff4f2; color: #b14d43; border: 1px solid #f0c7c1; }"
        "QPushButton[variant='danger']:hover { background-color: #fde8e5; border-color: #e5b4ad; }"
        "QTableWidget { background-color: #fbfdfd; alternate-background-color: #f6faf9; color: #1d3135; border: none; gridline-color: #e8eff0; }"
        "QTableWidget::item { padding: 10px 12px; border-bottom: 1px solid #edf3f4; }"
        "QTableWidget::item:selected { background-color: #dcefee; color: #173137; }"
        "QHeaderView::section { background-color: #f7fbfb; color: #5e7478; border: none; border-bottom: 1px solid #e2ebec; padding: 13px 14px; font-weight: 700; }"));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(18, 18, 18, 18);
    rootLayout->setSpacing(14);

    auto *headerLabel = new QLabel(QStringLiteral("手动入库详情"), this);
    headerLabel->setStyleSheet(QStringLiteral("font-size: 24px; font-weight: 800; color: #173137;"));

    auto *actionCard = new QFrame(this);
    actionCard->setObjectName(QStringLiteral("sectionCard"));
    auto *actionCardLayout = new QVBoxLayout(actionCard);
    actionCardLayout->setContentsMargins(18, 18, 18, 18);
    actionCardLayout->setSpacing(10);
    auto *actionTitleLabel = new QLabel(QStringLiteral("快捷操作"), actionCard);
    actionTitleLabel->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: 800; color: #173137;"));
    auto *buttonLayout = new QHBoxLayout();
    auto *addExistingButton = new QPushButton(QStringLiteral("选择已有物料"), this);
    auto *addNewButton = new QPushButton(QStringLiteral("新增入库项目"), this);
    auto *removeButton = new QPushButton(QStringLiteral("删除所选"), this);
    addExistingButton->setProperty("variant", QStringLiteral("primary"));
    addNewButton->setProperty("variant", QStringLiteral("subtle"));
    removeButton->setProperty("variant", QStringLiteral("danger"));

    connect(addExistingButton, &QPushButton::clicked, this, &ManualStockInDialog::addExistingItem);
    connect(addNewButton, &QPushButton::clicked, this, &ManualStockInDialog::addNewItem);
    connect(removeButton, &QPushButton::clicked, this, &ManualStockInDialog::removeSelectedEntry);

    buttonLayout->addWidget(addExistingButton);
    buttonLayout->addWidget(addNewButton);
    buttonLayout->addWidget(removeButton);
    buttonLayout->addStretch();
    actionCardLayout->addWidget(actionTitleLabel);
    actionCardLayout->addLayout(buttonLayout);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("类型"),
        QStringLiteral("物料"),
        QStringLiteral("入库数量"),
        QStringLiteral("业务日期"),
        QStringLiteral("备注")
    });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);

    auto *tableCard = new QFrame(this);
    tableCard->setObjectName(QStringLiteral("sectionCard"));
    auto *tableCardLayout = new QVBoxLayout(tableCard);
    tableCardLayout->setContentsMargins(18, 18, 18, 14);
    tableCardLayout->setSpacing(10);
    auto *tableTitleLabel = new QLabel(QStringLiteral("待入库列表"), tableCard);
    tableTitleLabel->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: 800; color: #173137;"));
    tableCardLayout->addWidget(tableTitleLabel);
    tableCardLayout->addWidget(m_table, 1);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok)) {
        okButton->setText(QStringLiteral("确认"));
        okButton->setProperty("variant", QStringLiteral("primary"));
    }
    if (QPushButton *cancelButton = buttonBox->button(QDialogButtonBox::Cancel)) {
        cancelButton->setText(QStringLiteral("取消"));
        cancelButton->setProperty("variant", QStringLiteral("subtle"));
    }
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ManualStockInDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &ManualStockInDialog::reject);

    rootLayout->addWidget(headerLabel);
    rootLayout->addWidget(actionCard);
    rootLayout->addWidget(tableCard, 1);
    rootLayout->addWidget(buttonBox);

    refreshTable();
}

QList<ManualStockInEntry> ManualStockInDialog::entries() const
{
    return m_entries;
}

void ManualStockInDialog::accept()
{
    if (m_entries.isEmpty()) {
        QMessageBox::information(this,
                                 QStringLiteral("提示"),
                                 QStringLiteral("请至少添加一条入库项目。"));
        return;
    }

    QDialog::accept();
}

void ManualStockInDialog::addExistingItem()
{
    InventoryItemPickerDialog picker(m_inventoryRecords, {}, this);
    if (picker.exec() != QDialog::Accepted) {
        return;
    }

    const QVariantMap record = picker.selectedRecord();
    if (record.isEmpty()) {
        return;
    }

    InventoryTransactionDialog transactionDialog(InventoryTransactionDialog::Mode::StockIn,
                                                 inventoryRecordSummary(record),
                                                 record.value(QStringLiteral("quantity")).toInt(),
                                                 this);
    if (transactionDialog.exec() != QDialog::Accepted) {
        return;
    }

    ManualStockInEntry entry;
    entry.existingItem = true;
    entry.summary = inventoryRecordSummary(record);
    entry.note = transactionDialog.note();
    entry.itemData.insert(QStringLiteral("id"), record.value(QStringLiteral("id")));
    entry.itemData.insert(QStringLiteral("quantity"), transactionDialog.quantity());
    entry.itemData.insert(QStringLiteral("date"), transactionDialog.transactionDate());
    m_entries.append(entry);
    refreshTable();
}

void ManualStockInDialog::addNewItem()
{
    InventoryRecordDialog dialog(QStringLiteral("新增入库项目"),
                                 stockEntryDialogFields(m_fields),
                                 m_service,
                                 {},
                                 this);

    QVariantMap defaultRecord;
    defaultRecord.insert(QStringLiteral("quantity"), 1);
    defaultRecord.insert(QStringLiteral("date"), QDate::currentDate().toString(Qt::ISODate));
    dialog.setRecordData(defaultRecord);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    ManualStockInEntry entry;
    entry.existingItem = false;
    entry.itemData = dialog.recordData();
    entry.note = entry.itemData.value(QStringLiteral("comment")).toString().trimmed();
    entry.summary = inventoryRecordSummary(entry.itemData);
    m_entries.append(entry);
    refreshTable();
}

void ManualStockInDialog::removeSelectedEntry()
{
    const int currentRow = m_table->currentRow();
    if (currentRow < 0 || currentRow >= m_entries.size()) {
        QMessageBox::information(this,
                                 QStringLiteral("提示"),
                                 QStringLiteral("请先选择要删除的入库项目。"));
        return;
    }

    m_entries.removeAt(currentRow);
    refreshTable();
}

void ManualStockInDialog::refreshTable()
{
    m_table->setRowCount(m_entries.size());
    for (int row = 0; row < m_entries.size(); ++row) {
        const ManualStockInEntry &entry = m_entries.at(row);
        m_table->setItem(row, 0, new QTableWidgetItem(entry.existingItem ? QStringLiteral("已有物料")
                                                                          : QStringLiteral("新物料")));
        m_table->setItem(row, 1, new QTableWidgetItem(entry.summary));
        m_table->setItem(row, 2, new QTableWidgetItem(entry.itemData.value(QStringLiteral("quantity")).toString()));
        m_table->setItem(row, 3, new QTableWidgetItem(entry.itemData.value(QStringLiteral("date")).toString()));
        m_table->setItem(row, 4, new QTableWidgetItem(entry.note));
    }
}
