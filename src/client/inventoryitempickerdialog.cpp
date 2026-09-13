#include "inventoryitempickerdialog.h"
#include "inventorysearchutils.h"
#include "searchhighlightdelegate.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

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
}

InventoryItemPickerDialog::InventoryItemPickerDialog(const QList<QVariantMap> &inventoryRecords,
                                                     const QStringList &relevanceKeywords,
                                                     QWidget *parent)
    : QDialog(parent),
      m_inventoryRecords(inventoryRecords),
      m_relevanceKeywords(relevanceKeywords)
{
    setWindowTitle(QStringLiteral("选择已有物料"));
    resize(860, 520);

    auto *rootLayout = new QVBoxLayout(this);

    auto *searchLayout = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("输入关键字筛选物料"));
    auto *searchButton = new QPushButton(QStringLiteral("查询"), this);
    auto *resetButton = new QPushButton(QStringLiteral("重置"), this);
    connect(searchButton, &QPushButton::clicked, this, &InventoryItemPickerDialog::refreshTable);
    connect(resetButton, &QPushButton::clicked, this, [this]() {
        m_searchEdit->clear();
        refreshTable();
    });

    searchLayout->addWidget(m_searchEdit, 1);
    searchLayout->addWidget(searchButton);
    searchLayout->addWidget(resetButton);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("物料"),
        QStringLiteral("当前库存"),
        QStringLiteral("单位"),
        QStringLiteral("位置")
    });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_highlightDelegate = new SearchHighlightDelegate(m_table);
    m_table->setItemDelegate(m_highlightDelegate);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &InventoryItemPickerDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &InventoryItemPickerDialog::reject);

    rootLayout->addLayout(searchLayout);
    rootLayout->addWidget(m_table, 1);
    rootLayout->addWidget(buttonBox);

    refreshTable();
}

QVariantMap InventoryItemPickerDialog::selectedRecord() const
{
    const int currentRow = m_table->currentRow();
    if (currentRow < 0 || currentRow >= m_visibleRecords.size()) {
        return {};
    }
    return m_visibleRecords.at(currentRow);
}

void InventoryItemPickerDialog::accept()
{
    if (selectedRecord().isEmpty()) {
        QMessageBox::information(this,
                                 QStringLiteral("提示"),
                                 QStringLiteral("请先选择一条物料记录。"));
        return;
    }

    QDialog::accept();
}

void InventoryItemPickerDialog::refreshTable()
{
    m_visibleRecords = filteredRecords();
    if (m_highlightDelegate != nullptr) {
        m_highlightDelegate->setKeyword(m_searchEdit->text().trimmed());
    }
    m_table->setRowCount(m_visibleRecords.size());
    for (int row = 0; row < m_visibleRecords.size(); ++row) {
        const QVariantMap &record = m_visibleRecords.at(row);
        m_table->setItem(row, 0, new QTableWidgetItem(inventoryRecordSummary(record)));
        m_table->setItem(row, 1, new QTableWidgetItem(record.value(QStringLiteral("quantity")).toString()));
        m_table->setItem(row, 2, new QTableWidgetItem(record.value(QStringLiteral("unit")).toString()));
        m_table->setItem(row, 3, new QTableWidgetItem(record.value(QStringLiteral("location")).toString()));
    }
    m_table->viewport()->update();
}

QList<QVariantMap> InventoryItemPickerDialog::filteredRecords() const
{
    const QString keyword = m_searchEdit->text().trimmed();
    if (!keyword.isEmpty()) {
        return rankInventoryRecordsByKeyword(m_inventoryRecords, keyword);
    }

    if (m_relevanceKeywords.isEmpty()) {
        return m_inventoryRecords;
    }

    struct ScoredRecord {
        QVariantMap record;
        int score = 0;
        int originalIndex = -1;
    };

    QList<ScoredRecord> scoredRecords;
    scoredRecords.reserve(m_inventoryRecords.size());
    for (int index = 0; index < m_inventoryRecords.size(); ++index) {
        const QVariantMap &record = m_inventoryRecords.at(index);
        int score = 0;
        for (const QString &relevanceKeyword : m_relevanceKeywords) {
            score += inventoryRecordSearchScore(record, relevanceKeyword);
        }
        scoredRecords.append({record, score, index});
    }

    std::sort(scoredRecords.begin(), scoredRecords.end(), [](const ScoredRecord &left, const ScoredRecord &right) {
        if (left.score != right.score) {
            return left.score > right.score;
        }
        return left.originalIndex < right.originalIndex;
    });

    QList<QVariantMap> rankedRecords;
    rankedRecords.reserve(scoredRecords.size());
    for (const ScoredRecord &entry : scoredRecords) {
        rankedRecords.append(entry.record);
    }
    return rankedRecords;
}
