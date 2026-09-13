#include "inventorytransactiondialog.h"

#include <QDate>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFrame>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

InventoryTransactionDialog::InventoryTransactionDialog(Mode mode,
                                                       const QString &itemSummary,
                                                       int currentQuantity,
                                                       QWidget *parent)
    : QDialog(parent),
      m_mode(mode)
{
    setWindowTitle(mode == Mode::StockIn ? QStringLiteral("手动入库") : QStringLiteral("手动出库"));
    resize(620, 420);
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #f4f7f8; }"
        "QLabel { color: #284048; font-weight: 600; }"
        "QFrame#contentCard { background-color: #fbfdfd; border: 1px solid #dde8e9; border-radius: 22px; }"
        "QSpinBox, QDateEdit, QTextEdit { background-color: #fbfdfd; color: #1d3135; border: 1px solid #d7e3e5; border-radius: 14px; padding: 9px 12px; }"
        "QSpinBox:focus, QDateEdit:focus, QTextEdit:focus { border-color: #6d9894; }"
        "QPushButton { background-color: #fbfdfd; color: #1d3135; border: 1px solid #d7e3e5; border-radius: 14px; padding: 10px 16px; font-weight: 600; }"
        "QPushButton:hover { background-color: #f5fbfb; border-color: #bfd2d4; }"
        "QPushButton[variant='primary'] { background-color: #5f8e8a; color: #ffffff; border: 1px solid #5f8e8a; }"
        "QPushButton[variant='primary']:hover { background-color: #547f7b; border-color: #547f7b; }"
        "QPushButton[variant='subtle'] { background-color: #f7fbfb; color: #557075; border: 1px solid #dce7e8; }"
        "QPushButton[variant='subtle']:hover { background-color: #eef6f6; border-color: #c8d9db; }"));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(18, 18, 18, 18);
    rootLayout->setSpacing(14);

    auto *headerLabel = new QLabel(mode == Mode::StockIn ? QStringLiteral("确认入库详情") : QStringLiteral("确认出库详情"), this);
    headerLabel->setStyleSheet(QStringLiteral("font-size: 24px; font-weight: 800; color: #173137;"));

    auto *contentCard = new QFrame(this);
    contentCard->setObjectName(QStringLiteral("contentCard"));
    auto *contentLayout = new QVBoxLayout(contentCard);
    contentLayout->setContentsMargins(20, 20, 20, 20);
    contentLayout->setSpacing(14);

    auto *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight);
    formLayout->setSpacing(12);

    m_itemLabel = new QLabel(itemSummary, this);
    m_itemLabel->setWordWrap(true);
    m_currentQuantityLabel = new QLabel(QString::number(currentQuantity), this);

    m_quantitySpinBox = new QSpinBox(this);
    m_quantitySpinBox->setRange(1, 1000000000);
    m_quantitySpinBox->setValue(1);

    m_dateEdit = new QDateEdit(QDate::currentDate(), this);
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));

    m_noteEdit = new QTextEdit(this);
    m_noteEdit->setMinimumHeight(90);

    formLayout->addRow(QStringLiteral("物料："), m_itemLabel);
    formLayout->addRow(QStringLiteral("当前库存："), m_currentQuantityLabel);
    formLayout->addRow(mode == Mode::StockIn ? QStringLiteral("入库数量：") : QStringLiteral("出库数量："), m_quantitySpinBox);
    formLayout->addRow(QStringLiteral("业务日期："), m_dateEdit);
    formLayout->addRow(QStringLiteral("备注："), m_noteEdit);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok)) {
        okButton->setText(QStringLiteral("确认"));
        okButton->setProperty("variant", QStringLiteral("primary"));
    }
    if (QPushButton *cancelButton = buttonBox->button(QDialogButtonBox::Cancel)) {
        cancelButton->setText(QStringLiteral("取消"));
        cancelButton->setProperty("variant", QStringLiteral("subtle"));
    }
    connect(buttonBox, &QDialogButtonBox::accepted, this, &InventoryTransactionDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &InventoryTransactionDialog::reject);

    contentLayout->addLayout(formLayout);
    rootLayout->addWidget(headerLabel);
    rootLayout->addWidget(contentCard, 1);
    rootLayout->addWidget(buttonBox);
}

int InventoryTransactionDialog::quantity() const
{
    return m_quantitySpinBox->value();
}

QString InventoryTransactionDialog::transactionDate() const
{
    return m_dateEdit->date().toString(Qt::ISODate);
}

QString InventoryTransactionDialog::note() const
{
    return m_noteEdit->toPlainText().trimmed();
}

void InventoryTransactionDialog::accept()
{
    if (m_quantitySpinBox->value() <= 0) {
        QMessageBox::warning(this,
                             QStringLiteral("数量无效"),
                             QStringLiteral("变更数量必须大于 0。"));
        return;
    }

    QDialog::accept();
}