#include "reimbursementoverviewpage.h"

#include "appservice.h"

#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMap>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {
struct OwnerSummary {
    double totalAmount = 0.0;
    double invoicedAmount = 0.0;
    double uninvoicedAmount = 0.0;
    double reimbursedAmount = 0.0;
    double unreimbursedAmount = 0.0;
    int recordCount = 0;
};

bool variantBoolValue(const QVariant &value)
{
    if (value.type() == QVariant::Bool) {
        return value.toBool();
    }

    const QString text = value.toString().trimmed().toLower();
    return text == QStringLiteral("true")
           || text == QStringLiteral("1")
           || text == QStringLiteral("yes")
           || text == QStringLiteral("y")
           || text == QStringLiteral("是")
           || text == QStringLiteral("已开票")
           || text == QStringLiteral("已报账");
}

double recordAmountValue(const QVariantMap &record)
{
    bool ok = false;
    const double value = record.value(QStringLiteral("amount")).toString().toDouble(&ok);
    return ok ? value : record.value(QStringLiteral("amount")).toDouble();
}

QString amountText(double value)
{
    return QStringLiteral("%1 元").arg(QString::number(value, 'f', 2));
}

QString sectionPanelStyle()
{
    return QStringLiteral(
        "#sectionPanel { background-color: #fbfdfd; border: 1px solid #dde8e9; border-radius: 24px; }"
        "QLabel[role='sectionTitle'] { color: #173137; font-size: 16px; font-weight: 800; }"
        "QLabel[role='caption'] { color: #739094; font-size: 12px; font-weight: 700; }"
        "QLabel[role='amount'] { color: #173137; font-size: 24px; font-weight: 800; }");
}

QString summaryCardStyle()
{
    return QStringLiteral(
        "#summaryCard { background-color: #f7fbfb; border: 1px solid #e1ebec; border-radius: 18px; }");
}
}

ReimbursementOverviewPage::ReimbursementOverviewPage(AppService *storageService,
                                                     QWidget *parent)
    : QWidget(parent),
      m_storageService(storageService)
{
    buildUi();
    reloadData();
}

void ReimbursementOverviewPage::reloadData()
{
    const QList<QVariantMap> records = m_storageService == nullptr
                                           ? QList<QVariantMap>{}
                                           : m_storageService->loadPageRecords(QStringLiteral("reimbursement"));

    double totalAmount = 0.0;
    double uninvoicedAmount = 0.0;
    double invoicedAmount = 0.0;
    double unreimbursedAmount = 0.0;
    double reimbursedAmount = 0.0;
    QMap<QString, OwnerSummary> ownerSummaries;

    for (const QVariantMap &record : records) {
        const double amount = recordAmountValue(record);
        const QString owner = record.value(QStringLiteral("reimbursementOwner")).toString().trimmed();
        const QString ownerKey = owner.isEmpty() ? QStringLiteral("未填写") : owner;

        OwnerSummary summary = ownerSummaries.value(ownerKey);
        summary.totalAmount += amount;
        ++summary.recordCount;

        totalAmount += amount;
        if (variantBoolValue(record.value(QStringLiteral("invoiceIssued")))) {
            invoicedAmount += amount;
            summary.invoicedAmount += amount;
        } else {
            uninvoicedAmount += amount;
            summary.uninvoicedAmount += amount;
        }

        if (variantBoolValue(record.value(QStringLiteral("reimbursed")))) {
            reimbursedAmount += amount;
            summary.reimbursedAmount += amount;
        } else {
            unreimbursedAmount += amount;
            summary.unreimbursedAmount += amount;
        }

        ownerSummaries.insert(ownerKey, summary);
    }

    m_totalAmountLabel->setText(amountText(totalAmount));
    m_uninvoicedAmountLabel->setText(amountText(uninvoicedAmount));
    m_invoicedAmountLabel->setText(amountText(invoicedAmount));
    m_unreimbursedAmountLabel->setText(amountText(unreimbursedAmount));
    m_reimbursedAmountLabel->setText(amountText(reimbursedAmount));

    m_ownerSummaryTable->setRowCount(ownerSummaries.size());
    int row = 0;
    for (auto it = ownerSummaries.constBegin(); it != ownerSummaries.constEnd(); ++it, ++row) {
        const OwnerSummary &summary = it.value();
        m_ownerSummaryTable->setItem(row, 0, new QTableWidgetItem(it.key()));
        m_ownerSummaryTable->setItem(row, 1, new QTableWidgetItem(QString::number(summary.recordCount)));
        m_ownerSummaryTable->setItem(row, 2, new QTableWidgetItem(amountText(summary.totalAmount)));
        m_ownerSummaryTable->setItem(row, 3, new QTableWidgetItem(amountText(summary.uninvoicedAmount)));
        m_ownerSummaryTable->setItem(row, 4, new QTableWidgetItem(amountText(summary.invoicedAmount)));
        m_ownerSummaryTable->setItem(row, 5, new QTableWidgetItem(amountText(summary.unreimbursedAmount)));
        m_ownerSummaryTable->setItem(row, 6, new QTableWidgetItem(amountText(summary.reimbursedAmount)));
    }

    m_statusLabel->setText(QStringLiteral("当前共 %1 条报账记录，汇总 %2 位报账人。")
                               .arg(records.size())
                               .arg(ownerSummaries.size()));
}

void ReimbursementOverviewPage::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(16);

    auto *summaryPanel = new QFrame(this);
    summaryPanel->setObjectName(QStringLiteral("sectionPanel"));
    summaryPanel->setStyleSheet(sectionPanelStyle());
    auto *summaryLayout = new QGridLayout(summaryPanel);
    summaryLayout->setContentsMargins(20, 20, 20, 20);
    summaryLayout->setHorizontalSpacing(14);
    summaryLayout->setVerticalSpacing(14);

    auto *summaryTitleLabel = new QLabel(QStringLiteral("费用总览"), summaryPanel);
    summaryTitleLabel->setProperty("role", QStringLiteral("sectionTitle"));
    summaryLayout->addWidget(summaryTitleLabel, 0, 0, 1, 3);

    auto createSummaryCard = [summaryPanel, summaryLayout](const QString &title,
                                                           int row,
                                                           int column,
                                                           QLabel **amountLabel) {
        auto *card = new QFrame(summaryPanel);
        card->setObjectName(QStringLiteral("summaryCard"));
        card->setStyleSheet(summaryCardStyle());
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(16, 16, 16, 16);
        cardLayout->setSpacing(6);

        auto *titleLabel = new QLabel(title, card);
        titleLabel->setProperty("role", QStringLiteral("caption"));
        auto *valueLabel = new QLabel(amountText(0.0), card);
        valueLabel->setProperty("role", QStringLiteral("amount"));

        cardLayout->addWidget(titleLabel);
        cardLayout->addWidget(valueLabel);
        cardLayout->addStretch();
        summaryLayout->addWidget(card, row, column);
        *amountLabel = valueLabel;
    };

    createSummaryCard(QStringLiteral("总金额"), 2, 0, &m_totalAmountLabel);
    createSummaryCard(QStringLiteral("未开票金额"), 2, 1, &m_uninvoicedAmountLabel);
    createSummaryCard(QStringLiteral("已开票金额"), 2, 2, &m_invoicedAmountLabel);
    createSummaryCard(QStringLiteral("未报账金额"), 3, 0, &m_unreimbursedAmountLabel);
    createSummaryCard(QStringLiteral("已报账金额"), 3, 1, &m_reimbursedAmountLabel);

    auto *ownerPanel = new QFrame(this);
    ownerPanel->setObjectName(QStringLiteral("sectionPanel"));
    ownerPanel->setStyleSheet(sectionPanelStyle());
    auto *ownerLayout = new QVBoxLayout(ownerPanel);
    ownerLayout->setContentsMargins(18, 18, 18, 14);
    ownerLayout->setSpacing(10);

    auto *ownerTitleLabel = new QLabel(QStringLiteral("报账人汇总"), ownerPanel);
    ownerTitleLabel->setProperty("role", QStringLiteral("sectionTitle"));

    m_ownerSummaryTable = new QTableWidget(ownerPanel);
    m_ownerSummaryTable->setColumnCount(7);
    m_ownerSummaryTable->setHorizontalHeaderLabels({
        QStringLiteral("报账人"),
        QStringLiteral("条目数"),
        QStringLiteral("总金额"),
        QStringLiteral("未开票"),
        QStringLiteral("已开票"),
        QStringLiteral("未报账"),
        QStringLiteral("已报账")
    });
    m_ownerSummaryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_ownerSummaryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ownerSummaryTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_ownerSummaryTable->setAlternatingRowColors(true);
    m_ownerSummaryTable->setShowGrid(false);
    m_ownerSummaryTable->setFrameShape(QFrame::NoFrame);
    m_ownerSummaryTable->horizontalHeader()->setStretchLastSection(true);
    m_ownerSummaryTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_ownerSummaryTable->verticalHeader()->setVisible(false);

    m_statusLabel = new QLabel(ownerPanel);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #71888c; font-size: 12px; padding-top: 6px;"));

    ownerLayout->addWidget(ownerTitleLabel);
    ownerLayout->addWidget(m_ownerSummaryTable, 1);
    ownerLayout->addWidget(m_statusLabel);

    rootLayout->addWidget(summaryPanel);
    rootLayout->addWidget(ownerPanel, 1);
}