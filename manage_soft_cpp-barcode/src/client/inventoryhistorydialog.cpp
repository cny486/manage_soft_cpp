#include "inventoryhistorydialog.h"

#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {
QString historyListText(const QVariant &value)
{
    if (value.type() == QVariant::StringList) {
        return value.toStringList().join(QStringLiteral("\n"));
    }

    if (value.type() == QVariant::List) {
        QStringList lines;
        for (const QVariant &entry : value.toList()) {
            lines.append(entry.toString());
        }
        return lines.join(QStringLiteral("\n"));
    }

    return value.toString();
}

QStringList historyListLines(const QVariant &value)
{
    if (value.type() == QVariant::StringList) {
        return value.toStringList();
    }

    if (value.type() == QVariant::List) {
        QStringList lines;
        for (const QVariant &entry : value.toList()) {
            const QString line = entry.toString().trimmed();
            if (!line.isEmpty()) {
                lines.append(line);
            }
        }
        return lines;
    }

    const QString text = value.toString().trimmed();
    return text.isEmpty() ? QStringList{} : text.split(QChar('\n'), Qt::SkipEmptyParts);
}

QString historySummaryText(const QVariant &value, bool *hasMoreDetails = nullptr)
{
    const QStringList lines = historyListLines(value);
    const bool truncated = lines.size() > 2;
    if (hasMoreDetails != nullptr) {
        *hasMoreDetails = truncated;
    }

    QStringList visibleLines = lines.mid(0, 2);
    if (truncated) {
        visibleLines.append(QStringLiteral("... 双击查看详情"));
    }
    return visibleLines.join(QStringLiteral("\n"));
}

QString historySearchText(const QVariantMap &record)
{
    return QStringLiteral("%1 %2 %3 %4 %5 %6")
        .arg(record.value(QStringLiteral("changeTime")).toString(),
             record.value(QStringLiteral("itemKey")).toString(),
             record.value(QStringLiteral("operationType")).toString(),
             record.value(QStringLiteral("inputType")).toString(),
             historyListText(record.value(QStringLiteral("changedFields"))),
             record.value(QStringLiteral("note")).toString());
}

QDate historyDateValue(const QVariantMap &record)
{
    const QString raw = record.value(QStringLiteral("changeTime")).toString().left(10);
    return QDate::fromString(raw, Qt::ISODate);
}
}

InventoryHistoryDialog::InventoryHistoryDialog(const QList<QVariantMap> &historyRecords,
                                               QWidget *parent)
    : QDialog(parent),
      m_historyRecords(historyRecords)
{
    setWindowTitle(QStringLiteral("库存修改历史"));
    resize(1100, 620);

    auto *rootLayout = new QVBoxLayout(this);

    auto *filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QLabel(QStringLiteral("开始日期："), this));

    m_startDateEdit = new QDateEdit(QDate::currentDate().addMonths(-1), this);
    m_startDateEdit->setCalendarPopup(true);
    m_startDateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));

    filterLayout->addWidget(m_startDateEdit);
    filterLayout->addWidget(new QLabel(QStringLiteral("结束日期："), this));

    m_endDateEdit = new QDateEdit(QDate::currentDate(), this);
    m_endDateEdit->setCalendarPopup(true);
    m_endDateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));

    filterLayout->addWidget(m_endDateEdit);
    filterLayout->addWidget(new QLabel(QStringLiteral("操作类型："), this));

    m_operationTypeComboBox = new QComboBox(this);
    m_operationTypeComboBox->addItems({
        QStringLiteral("全部"),
        QStringLiteral("直接修改"),
        QStringLiteral("入库"),
        QStringLiteral("出库"),
        QStringLiteral("删除")
    });
    filterLayout->addWidget(m_operationTypeComboBox);
    filterLayout->addWidget(new QLabel(QStringLiteral("关键字："), this));

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("可搜索时间、物料、操作类型、修改内容"));
    auto *searchButton = new QPushButton(QStringLiteral("查询"), this);
    auto *resetButton = new QPushButton(QStringLiteral("重置"), this);

    connect(searchButton, &QPushButton::clicked, this, [this]() { refreshTable(); });
    connect(resetButton, &QPushButton::clicked, this, [this]() {
        m_startDateEdit->setDate(QDate::currentDate().addMonths(-1));
        m_endDateEdit->setDate(QDate::currentDate());
        m_operationTypeComboBox->setCurrentIndex(0);
        m_searchEdit->clear();
        refreshTable();
    });

    filterLayout->addWidget(m_searchEdit, 1);
    filterLayout->addWidget(searchButton);
    filterLayout->addWidget(resetButton);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(10);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("修改时间"),
        QStringLiteral("物料"),
        QStringLiteral("操作类型"),
        QStringLiteral("输入方式"),
        QStringLiteral("修改前"),
        QStringLiteral("变更量"),
        QStringLiteral("修改后"),
        QStringLiteral("修改明细"),
        QStringLiteral("备注"),
        QStringLiteral("修改人")
    });
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(8, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) { showHistoryDetails(row); });

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &InventoryHistoryDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &InventoryHistoryDialog::accept);
    connect(buttonBox->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &InventoryHistoryDialog::close);

    rootLayout->addLayout(filterLayout);
    rootLayout->addWidget(m_table, 1);
    rootLayout->addWidget(buttonBox);

    refreshTable();
}

void InventoryHistoryDialog::refreshTable()
{
    const QString keyword = m_searchEdit->text().trimmed();
    const QDate startDate = m_startDateEdit->date();
    const QDate endDate = m_endDateEdit->date();
    const QString operationType = m_operationTypeComboBox->currentText();
    QList<QVariantMap> filteredRecords;
    for (const QVariantMap &record : m_historyRecords) {
        const QDate changeDate = historyDateValue(record);
        if (changeDate.isValid() && (changeDate < startDate || changeDate > endDate)) {
            continue;
        }

        if (operationType != QStringLiteral("全部")
            && record.value(QStringLiteral("operationType")).toString() != operationType) {
            continue;
        }

        if (!keyword.isEmpty() && !historySearchText(record).contains(keyword, Qt::CaseInsensitive)) {
            continue;
        }

        filteredRecords.append(record);
    }

    std::sort(filteredRecords.begin(), filteredRecords.end(), [](const QVariantMap &left, const QVariantMap &right) {
        return left.value(QStringLiteral("changeTime")).toString()
               > right.value(QStringLiteral("changeTime")).toString();
    });

    m_filteredRecords = filteredRecords;
    m_table->setRowCount(m_filteredRecords.size());
    for (int row = 0; row < m_filteredRecords.size(); ++row) {
        const QVariantMap &record = m_filteredRecords.at(row);
        m_table->setItem(row, 0, new QTableWidgetItem(record.value(QStringLiteral("changeTime")).toString()));
        m_table->setItem(row, 1, new QTableWidgetItem(record.value(QStringLiteral("itemKey")).toString()));
        m_table->setItem(row, 2, new QTableWidgetItem(record.value(QStringLiteral("operationType")).toString()));
        m_table->setItem(row, 3, new QTableWidgetItem(record.value(QStringLiteral("inputType")).toString()));
        m_table->setItem(row, 4, new QTableWidgetItem(record.value(QStringLiteral("beforeQuantity")).toString()));
        m_table->setItem(row, 5, new QTableWidgetItem(record.value(QStringLiteral("changeQuantity")).toString()));
        m_table->setItem(row, 6, new QTableWidgetItem(record.value(QStringLiteral("afterQuantity")).toString()));
        bool hasMoreDetails = false;
        auto *summaryItem = new QTableWidgetItem(historySummaryText(record.value(QStringLiteral("changedFields")), &hasMoreDetails));
        if (hasMoreDetails) {
            summaryItem->setToolTip(historyListText(record.value(QStringLiteral("changedFields"))));
        }
        m_table->setItem(row, 7, summaryItem);
        m_table->setItem(row, 8, new QTableWidgetItem(record.value(QStringLiteral("note")).toString().trimmed()));
        m_table->setItem(row, 9, new QTableWidgetItem(record.value(QStringLiteral("modifier")).toString().trimmed()));
    }
}

void InventoryHistoryDialog::showHistoryDetails(int row)
{
    if (row < 0 || row >= m_filteredRecords.size()) {
        return;
    }

    const QVariantMap &record = m_filteredRecords.at(row);

    QStringList sections;
    sections.append(QStringLiteral("修改时间：%1").arg(record.value(QStringLiteral("changeTime")).toString()));
    sections.append(QStringLiteral("物料：%1").arg(record.value(QStringLiteral("itemKey")).toString()));
    sections.append(QStringLiteral("操作类型：%1").arg(record.value(QStringLiteral("operationType")).toString()));
    sections.append(QStringLiteral("输入方式：%1").arg(record.value(QStringLiteral("inputType")).toString()));
    sections.append(QStringLiteral("修改前：%1").arg(record.value(QStringLiteral("beforeQuantity")).toString()));
    sections.append(QStringLiteral("变更量：%1").arg(record.value(QStringLiteral("changeQuantity")).toString()));
    sections.append(QStringLiteral("修改后：%1").arg(record.value(QStringLiteral("afterQuantity")).toString()));
    sections.append(QString());
    sections.append(QStringLiteral("修改明细："));
    const QStringList detailLines = historyListLines(record.value(QStringLiteral("changedFields")));
    sections.append(detailLines.isEmpty() ? QStringLiteral("- 无") : detailLines.join(QStringLiteral("\n")));
    sections.append(QString());
    sections.append(QStringLiteral("备注：%1").arg(record.value(QStringLiteral("note")).toString().trimmed()));
    sections.append(QStringLiteral("修改人：%1").arg(record.value(QStringLiteral("modifier")).toString().trimmed()));

    QStringList sourceLines;
    if (!record.value(QStringLiteral("sourceFile")).toString().trimmed().isEmpty()) {
        sourceLines.append(QStringLiteral("文件：%1").arg(record.value(QStringLiteral("sourceFile")).toString()));
    }
    if (record.value(QStringLiteral("sourceRow")).toInt() > 0) {
        sourceLines.append(QStringLiteral("行号：%1").arg(record.value(QStringLiteral("sourceRow")).toInt()));
    }
    if (!sourceLines.isEmpty()) {
        sections.append(QString());
        sections.append(QStringLiteral("来源："));
        sections.append(sourceLines.join(QStringLiteral("\n")));
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("修改记录详情"));
    dialog.resize(720, 520);

    auto *layout = new QVBoxLayout(&dialog);
    auto *detailEdit = new QTextEdit(&dialog);
    detailEdit->setReadOnly(true);
    detailEdit->setPlainText(sections.join(QStringLiteral("\n")));

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::Close), &QPushButton::clicked, &dialog, &QDialog::accept);

    layout->addWidget(detailEdit, 1);
    layout->addWidget(buttonBox);
    dialog.exec();
}