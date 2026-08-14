#include "managementpage.h"

#include "appservice.h"
#include "inventoryhistorydialog.h"
#include "inventoryfulfillmentdialog.h"
#include "manualstockindialog.h"
#include "inventoryitempickerdialog.h"
#include "inventoryrecorddialog.h"
#include "inventorysearchutils.h"
#include "searchhighlightdelegate.h"
#include "inventorytransactiondialog.h"

#include "recorddialog.h"

#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QComboBox>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
enum class InventoryActionModeSelection {
    Cancel,
    Manual,
    Bom
};

QString displayAttachmentName(const QString &reference);

QString uniqueFilePath(const QString &directoryPath, const QString &fileName);

QStringList splitAttachmentReferences(const QVariant &value)
{
    return value.toString().split(QChar('\n'), Qt::SkipEmptyParts);
}

QString displayDateValue(const QString &rawValue)
{
    const QString trimmed = rawValue.trimmed();
    const QDate isoDate = QDate::fromString(trimmed, Qt::ISODate);
    if (isoDate.isValid()) {
        return isoDate.toString(Qt::ISODate);
    }

    bool ok = false;
    const int serial = trimmed.toInt(&ok);
    if (!ok || serial <= 0) {
        return trimmed;
    }

    const QDate excelEpoch(1899, 12, 30);
    const QDate converted = excelEpoch.addDays(serial);
    return converted.isValid() ? converted.toString(Qt::ISODate) : trimmed;
}

QString recordValueText(const QVariantMap &record, const FieldDefinition &field)
{
    if (field.type == FieldType::Date) {
        return displayDateValue(record.value(field.key).toString());
    }

    if (field.type == FieldType::Double) {
        return QString::number(record.value(field.key).toDouble(), 'f', 2);
    }

    if (field.type == FieldType::Boolean) {
        return record.value(field.key).toBool() ? QStringLiteral("是") : QStringLiteral("否");
    }

    if (field.type == FieldType::File || field.type == FieldType::Files) {
        const QString rawValue = record.value(field.key).toString().trimmed();
        if (rawValue.isEmpty()) {
            return QString();
        }

        QStringList displayNames;
        const QStringList paths = rawValue.split(QChar('\n'), Qt::SkipEmptyParts);
        for (const QString &path : paths) {
            const QString trimmedPath = path.trimmed();
            const QString displayName = displayAttachmentName(trimmedPath);
            displayNames.append(displayName.isEmpty() ? trimmedPath : displayName);
        }
        return displayNames.join(QStringLiteral(" | "));
    }

    return record.value(field.key).toString();
}

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

QString displayAttachmentName(const QString &reference)
{
    const QString fileName = QFileInfo(reference).fileName();
    const QRegularExpression archivedNamePattern(QStringLiteral("^[0-9a-fA-F]{32}_(.+)$"));
    const QRegularExpressionMatch match = archivedNamePattern.match(fileName);
    return match.hasMatch() ? match.captured(1) : fileName;
}

QString uniqueFilePath(const QString &directoryPath, const QString &fileName)
{
    QFileInfo info(fileName);
    const QString completeBaseName = info.completeBaseName();
    const QString suffix = info.suffix();
    QString candidate = QDir(directoryPath).filePath(fileName);
    int counter = 2;
    while (QFileInfo::exists(candidate)) {
        const QString numberedName = suffix.isEmpty()
                                         ? QStringLiteral("%1 (%2)").arg(completeBaseName).arg(counter)
                                         : QStringLiteral("%1 (%2).%3").arg(completeBaseName).arg(counter).arg(suffix);
        candidate = QDir(directoryPath).filePath(numberedName);
        ++counter;
    }
    return candidate;
}

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

QString sectionPanelStyle()
{
    return QStringLiteral(
        "#sectionPanel { background-color: #fbfdfd; border: 1px solid #dde8e9; border-radius: 24px; }"
        "QLabel[role='sectionTitle'] { color: #173137; font-size: 16px; font-weight: 800; }");
}

QString summaryPanelStyle()
{
    return QStringLiteral(
        "#summaryPanel { background-color: #fbfdfd; border: 1px solid #dde8e9; border-radius: 24px; }"
        "QLabel[role='sectionTitle'] { color: #173137; font-size: 16px; font-weight: 800; }"
        "QLabel[role='caption'] { color: #739094; font-size: 12px; font-weight: 700; }"
        "QLabel[role='amount'] { color: #173137; font-size: 24px; font-weight: 800; }");
}

QString summaryCardStyle()
{
    return QStringLiteral(
        "#summaryCard { background-color: #f7fbfb; border: 1px solid #e1ebec; border-radius: 18px; }");
}

QString menuStyle()
{
    return QStringLiteral(
        "QMenu { background-color: #fbfdfd; color: #1d3135; border: 1px solid #d7e3e5; padding: 8px; border-radius: 14px; }"
        "QMenu::item { padding: 8px 18px; border-radius: 8px; }"
        "QMenu::item:selected { background-color: #edf5f5; }");
}

QString modalStyle()
{
    return QStringLiteral(
        "QDialog { background-color: #f4f7f8; }"
        "QLabel { color: #1d3135; }");
}

void setButtonVariant(QPushButton *button, const QString &variant)
{
    if (button != nullptr) {
        button->setProperty("variant", variant);
    }
}

QList<FieldDefinition> stockEditDialogFields(const QList<FieldDefinition> &fields)
{
    const QStringList editableKeys = {
        QStringLiteral("manufacturerPart"),
        QStringLiteral("quantity"),
        QStringLiteral("category"),
        QStringLiteral("value"),
        QStringLiteral("footprint"),
        QStringLiteral("voltage"),
        QStringLiteral("supplier"),
        QStringLiteral("date"),
        QStringLiteral("location")
    };

    QList<FieldDefinition> filteredFields;
    filteredFields.reserve(editableKeys.size());
    for (const QString &key : editableKeys) {
        for (const FieldDefinition &field : fields) {
            if (field.key != key) {
                continue;
            }

            FieldDefinition adjustedField = field;
            if (key == QStringLiteral("manufacturerPart")
                || key == QStringLiteral("quantity")) {
                adjustedField.required = true;
            }

            filteredFields.append(adjustedField);
            break;
        }
    }

    return filteredFields;
}

QToolButton *createOverflowMenuButton(QMenu *menu, QWidget *parent)
{
    auto *button = new QToolButton(parent);
    button->setText(QStringLiteral("..."));
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setPopupMode(QToolButton::InstantPopup);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    button->setFixedSize(52, 40);
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "  background-color: #f7fbfb;"
        "  color: #557075;"
        "  border: 1px solid #dce7e8;"
        "  border-radius: 14px;"
        "  padding: 0px;"
        "  text-align: center;"
        "  font-size: 22px;"
        "  font-weight: 700;"
        "}"
        "QToolButton:hover { background-color: #eef6f6; border-color: #c8d9db; }"
        "QToolButton:pressed { background-color: #e4efef; }"
        "QToolButton::menu-indicator { image: none; width: 0px; }"));

    button->setMenu(menu);
    return button;
}

InventoryActionModeSelection selectInventoryActionMode(QWidget *parent,
                                                      const QString &windowTitle,
                                                      const QString &titleText,
                                                      const QString &hintText,
                                                      const QString &manualButtonText,
                                                      const QString &bomButtonText)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(windowTitle);
    dialog.resize(420, 220);
    dialog.setStyleSheet(modalStyle());

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(10);
    auto *titleLabel = new QLabel(titleText, &dialog);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: 800; color: #173137;"));

    auto *manualButton = new QPushButton(manualButtonText, &dialog);
    auto *bomButton = new QPushButton(bomButtonText, &dialog);
    auto *cancelButton = new QPushButton(QStringLiteral("取消"), &dialog);
    setButtonVariant(manualButton, QStringLiteral("primary"));
    setButtonVariant(bomButton, QStringLiteral("subtle"));
    setButtonVariant(cancelButton, QStringLiteral("subtle"));
    manualButton->setMinimumHeight(44);
    bomButton->setMinimumHeight(44);
    cancelButton->setMinimumHeight(40);

    QObject::connect(manualButton, &QPushButton::clicked, &dialog, [&dialog]() { dialog.done(1); });
    QObject::connect(bomButton, &QPushButton::clicked, &dialog, [&dialog]() { dialog.done(2); });
    QObject::connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

    layout->addWidget(titleLabel);
    layout->addSpacing(8);
    layout->addWidget(manualButton);
    layout->addWidget(bomButton);
    layout->addStretch();
    layout->addWidget(cancelButton);

    const int result = dialog.exec();
    if (result == 1) {
        return InventoryActionModeSelection::Manual;
    }
    if (result == 2) {
        return InventoryActionModeSelection::Bom;
    }
    return InventoryActionModeSelection::Cancel;
}

QList<QVariantMap> filteredHistoryRecords(const QList<QVariantMap> &records, const QString &inputTypeFilter)
{
    if (inputTypeFilter.trimmed().isEmpty()) {
        return records;
    }

    QList<QVariantMap> filtered;
    for (const QVariantMap &record : records) {
        if (record.value(QStringLiteral("operationType")).toString() == inputTypeFilter) {
            filtered.append(record);
        }
    }
    return filtered;
}

bool promptInventoryOperationNote(QWidget *parent,
                                  const QString &title,
                                  const QString &label,
                                  const QString &defaultValue,
                                  QString *note)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.resize(560, 360);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background-color: #f4f7f8; }"
        "QLabel { color: #284048; }"
        "QFrame#noteCard { background-color: #fbfdfd; border: 1px solid #dde8e9; border-radius: 22px; }"
        "QTextEdit { background-color: #fbfdfd; color: #1d3135; border: 1px solid #d7e3e5; border-radius: 14px; padding: 10px 12px; }"
        "QTextEdit:focus { border-color: #6d9894; }"
        "QPushButton { background-color: #fbfdfd; color: #1d3135; border: 1px solid #d7e3e5; border-radius: 14px; padding: 10px 16px; font-weight: 600; }"
        "QPushButton:hover { background-color: #f5fbfb; border-color: #bfd2d4; }"
        "QPushButton[variant='primary'] { background-color: #5f8e8a; color: #ffffff; border: 1px solid #5f8e8a; }"
        "QPushButton[variant='primary']:hover { background-color: #547f7b; border-color: #547f7b; }"
        "QPushButton[variant='subtle'] { background-color: #f7fbfb; color: #557075; border: 1px solid #dce7e8; }"
        "QPushButton[variant='subtle']:hover { background-color: #eef6f6; border-color: #c8d9db; }"));

    auto *rootLayout = new QVBoxLayout(&dialog);
    rootLayout->setContentsMargins(18, 18, 18, 18);
    rootLayout->setSpacing(14);

    auto *titleLabel = new QLabel(title, &dialog);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 22px; font-weight: 800; color: #173137;"));

    auto *noteCard = new QFrame(&dialog);
    noteCard->setObjectName(QStringLiteral("noteCard"));
    auto *noteCardLayout = new QVBoxLayout(noteCard);
    noteCardLayout->setContentsMargins(18, 18, 18, 18);
    noteCardLayout->setSpacing(10);

    auto *editorLabel = new QLabel(QStringLiteral("备注内容"), noteCard);
    editorLabel->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: 800; color: #173137;"));
    auto *noteEdit = new QTextEdit(noteCard);
    noteEdit->setMinimumHeight(160);
    noteEdit->setPlainText(defaultValue);

    noteCardLayout->addWidget(editorLabel);
    noteCardLayout->addWidget(noteEdit, 1);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    if (QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok)) {
        okButton->setText(QStringLiteral("确认"));
        okButton->setProperty("variant", QStringLiteral("primary"));
    }
    if (QPushButton *cancelButton = buttonBox->button(QDialogButtonBox::Cancel)) {
        cancelButton->setText(QStringLiteral("取消"));
        cancelButton->setProperty("variant", QStringLiteral("subtle"));
    }
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    rootLayout->addWidget(titleLabel);
    rootLayout->addWidget(noteCard, 1);
    rootLayout->addWidget(buttonBox);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    if (note != nullptr) {
        *note = noteEdit->toPlainText().trimmed();
    }
    return true;
}

QString combineInventoryNotes(const QString &operationNote, const QString &itemNote)
{
    const QString trimmedOperationNote = operationNote.trimmed();
    const QString trimmedItemNote = itemNote.trimmed();
    if (trimmedOperationNote.isEmpty()) {
        return trimmedItemNote;
    }
    if (trimmedItemNote.isEmpty() || trimmedItemNote == trimmedOperationNote) {
        return trimmedOperationNote;
    }

    return QStringLiteral("操作备注：%1\n条目备注：%2").arg(trimmedOperationNote, trimmedItemNote);
}

QString sanitizeFileNamePart(QString value)
{
    value = value.trimmed();
    value.replace(QRegularExpression(QStringLiteral("[\\/:*?\"<>|]")), QStringLiteral("_"));
    value.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("_"));
    return value.isEmpty() ? QStringLiteral("未填写") : value;
}

QString reimbursementAttachmentBaseName(const QVariantMap &record, const QString &prefix)
{
    const QString category = sanitizeFileNamePart(record.value(QStringLiteral("category")).toString());
    const QString owner = sanitizeFileNamePart(record.value(QStringLiteral("reimbursementOwner")).toString());
    const QString date = sanitizeFileNamePart(displayDateValue(record.value(QStringLiteral("date")).toString()));
    const QString amount = sanitizeFileNamePart(QString::number(record.value(QStringLiteral("amount")).toDouble(), 'f', 2));
    return QStringLiteral("%1_%2_%3_%4_%5").arg(prefix, category, owner, date, amount);
}

bool exportReimbursementAttachmentsForRecord(AppService *storageService,
                                             const QVariantMap &record,
                                             const QString &targetDirectory,
                                             QStringList *failures,
                                             int *successCount)
{
    QList<ReimbursementAttachmentContent> attachments;
    QString errorMessage;
    if (!storageService->loadReimbursementAttachments(record, &attachments, &errorMessage)) {
        if (failures != nullptr) {
            failures->append(QStringLiteral("%1：%2")
                                 .arg(reimbursementAttachmentBaseName(record, QStringLiteral("附件")), errorMessage));
        }
        return false;
    }

    const QStringList invoiceReferences = splitAttachmentReferences(record.value(QStringLiteral("invoiceAttachment")));
    const QStringList otherReferences = splitAttachmentReferences(record.value(QStringLiteral("otherAttachments")));

    for (const ReimbursementAttachmentContent &attachment : attachments) {
        const QString reference = attachment.reference.trimmed();
        QString prefix;
        if (invoiceReferences.contains(reference)) {
            prefix = QStringLiteral("发票");
        } else if (otherReferences.contains(reference)) {
            prefix = QStringLiteral("其它附件");
        } else {
            prefix = QStringLiteral("附件");
        }

        QString sourceName = attachment.fileName.trimmed();
        if (sourceName.isEmpty()) {
            sourceName = displayAttachmentName(reference);
        }

        const QString suffix = QFileInfo(sourceName).suffix();
        const QString baseName = reimbursementAttachmentBaseName(record, prefix);
        const QString targetFileName = suffix.isEmpty() ? baseName : QStringLiteral("%1.%2").arg(baseName, suffix);
        const QString targetFilePath = uniqueFilePath(targetDirectory, targetFileName);

        QFile file(targetFilePath);
        if (!file.open(QIODevice::WriteOnly)) {
            if (failures != nullptr) {
                failures->append(QStringLiteral("%1：%2").arg(QFileInfo(targetFilePath).fileName(), file.errorString()));
            }
            continue;
        }

        if (file.write(attachment.content) != attachment.content.size()) {
            if (failures != nullptr) {
                failures->append(QStringLiteral("%1：写入不完整").arg(QFileInfo(targetFilePath).fileName()));
            }
            continue;
        }

        if (successCount != nullptr) {
            ++(*successCount);
        }
    }

    return true;
}

void showImportResultMessage(QWidget *parent, bool success, const QString &message)
{
    if (message.trimmed().isEmpty()) {
        QMessageBox::information(parent,
                                 QStringLiteral("导入完成"),
                                 QStringLiteral("Excel 已导入。"));
        return;
    }

    const QStringList lines = message.split(QChar('\n'));
    const QString summary = lines.isEmpty() ? message : lines.first();
    const QString details = lines.size() > 1 ? lines.mid(1).join(QStringLiteral("\n")).trimmed() : QString();

    QMessageBox box(parent);
    box.setIcon(success ? QMessageBox::Information : QMessageBox::Warning);
    box.setWindowTitle(success ? QStringLiteral("导入完成") : QStringLiteral("导入失败"));
    box.setText(summary);
    if (!details.isEmpty()) {
        box.setDetailedText(details);
        box.setInformativeText(QStringLiteral("可展开查看具体行失败明细。"));
    }
    box.exec();
}

QStringList inventoryCategories(const QList<QVariantMap> &records)
{
    QStringList categories;
    for (const QVariantMap &record : records) {
        const QString category = record.value(QStringLiteral("category")).toString().trimmed();
        if (!category.isEmpty() && !categories.contains(category, Qt::CaseInsensitive)) {
            categories.append(category);
        }
    }

    categories.sort(Qt::CaseInsensitive);
    return categories;
}
}

ManagementPage::ManagementPage(const PageConfig &config,
                               AppService *storageService,
                               QWidget *parent)
    : QWidget(parent),
      m_config(config),
      m_storageService(storageService)
{
    buildUi();
    reloadRecords();
}

void ManagementPage::reloadRecords()
{
    const QList<QVariantMap> allRecords = m_storageService->loadPageRecords(m_config.pageId);
    if (isInventoryPage() || isReimbursementPage()) {
        updateCategoryFilterOptions(allRecords);
    }
    if (isReimbursementPage()) {
        updateReimbursementFilterOptions(allRecords);
    }

    const QString category = m_categoryFilterCombo == nullptr
                                 ? QString()
                                 : m_categoryFilterCombo->currentData().toString();
    refreshTable(filteredRecords(allRecords, m_searchEdit->text().trimmed(), category));
}

bool ManagementPage::isInventoryPage() const
{
    return m_config.pageId == QStringLiteral("inventory");
}

bool ManagementPage::isReimbursementPage() const
{
    return m_config.pageId == QStringLiteral("reimbursement");
}

void ManagementPage::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(16);

    auto *filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(10);
    auto *filterLabel = new QLabel(QStringLiteral("搜索："), this);
    filterLabel->setStyleSheet(QStringLiteral("color: #70888c; font-size: 13px; font-weight: 700;"));
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("输入关键字"));
    m_searchEdit->setMinimumHeight(42);
    auto *categoryLabel = new QLabel(QStringLiteral("种类："), this);
    categoryLabel->setStyleSheet(QStringLiteral("color: #70888c; font-size: 13px; font-weight: 700;"));
    m_categoryFilterCombo = new QComboBox(this);
    m_categoryFilterCombo->setMinimumHeight(42);
    m_categoryFilterCombo->setVisible(isInventoryPage() || isReimbursementPage());
    auto *ownerLabel = new QLabel(QStringLiteral("报账人："), this);
    ownerLabel->setStyleSheet(QStringLiteral("color: #70888c; font-size: 13px; font-weight: 700;"));
    m_ownerFilterCombo = new QComboBox(this);
    m_ownerFilterCombo->setMinimumHeight(42);
    m_ownerFilterCombo->setVisible(isReimbursementPage());
    auto *invoiceStatusLabel = new QLabel(QStringLiteral("开票："), this);
    invoiceStatusLabel->setStyleSheet(QStringLiteral("color: #70888c; font-size: 13px; font-weight: 700;"));
    m_invoiceStatusFilterCombo = new QComboBox(this);
    m_invoiceStatusFilterCombo->setMinimumHeight(42);
    m_invoiceStatusFilterCombo->setVisible(isReimbursementPage());
    m_invoiceStatusFilterCombo->addItem(QStringLiteral("全部状态"), QString());
    m_invoiceStatusFilterCombo->addItem(QStringLiteral("未开票"), QStringLiteral("no"));
    m_invoiceStatusFilterCombo->addItem(QStringLiteral("已开票"), QStringLiteral("yes"));
    auto *reimbursedStatusLabel = new QLabel(QStringLiteral("报账："), this);
    reimbursedStatusLabel->setStyleSheet(QStringLiteral("color: #70888c; font-size: 13px; font-weight: 700;"));
    m_reimbursedStatusFilterCombo = new QComboBox(this);
    m_reimbursedStatusFilterCombo->setMinimumHeight(42);
    m_reimbursedStatusFilterCombo->setVisible(isReimbursementPage());
    m_reimbursedStatusFilterCombo->addItem(QStringLiteral("全部状态"), QString());
    m_reimbursedStatusFilterCombo->addItem(QStringLiteral("未报账"), QStringLiteral("no"));
    m_reimbursedStatusFilterCombo->addItem(QStringLiteral("已报账"), QStringLiteral("yes"));
    auto *searchButton = new QPushButton(QStringLiteral("查询"), this);
    auto *resetButton = new QPushButton(QStringLiteral("重置"), this);
    setButtonVariant(searchButton, QStringLiteral("primary"));
    setButtonVariant(resetButton, QStringLiteral("subtle"));
    searchButton->setMinimumHeight(42);
    resetButton->setMinimumHeight(42);
    connect(searchButton, &QPushButton::clicked, this, [this]() { reloadRecords(); });
    connect(resetButton, &QPushButton::clicked, this, [this]() {
        m_searchEdit->clear();
        if (m_categoryFilterCombo != nullptr) {
            m_categoryFilterCombo->setCurrentIndex(0);
        }
        if (m_ownerFilterCombo != nullptr) {
            m_ownerFilterCombo->setCurrentIndex(0);
        }
        if (m_invoiceStatusFilterCombo != nullptr) {
            m_invoiceStatusFilterCombo->setCurrentIndex(0);
        }
        if (m_reimbursedStatusFilterCombo != nullptr) {
            m_reimbursedStatusFilterCombo->setCurrentIndex(0);
        }
        reloadRecords();
    });
    if (isInventoryPage() || isReimbursementPage()) {
        connect(m_categoryFilterCombo,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                [this](int) { reloadRecords(); });
    }
        if (isReimbursementPage()) {
        connect(m_ownerFilterCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int) { reloadRecords(); });
        connect(m_invoiceStatusFilterCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int) { reloadRecords(); });
        connect(m_reimbursedStatusFilterCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int) { reloadRecords(); });
    }

    filterLayout->addWidget(filterLabel);
    filterLayout->addWidget(m_searchEdit, 1);
    if (isInventoryPage() || isReimbursementPage()) {
        filterLayout->addWidget(categoryLabel);
        filterLayout->addWidget(m_categoryFilterCombo);
    }
    if (isReimbursementPage()) {
        filterLayout->addWidget(ownerLabel);
        filterLayout->addWidget(m_ownerFilterCombo);
        filterLayout->addWidget(invoiceStatusLabel);
        filterLayout->addWidget(m_invoiceStatusFilterCombo);
        filterLayout->addWidget(reimbursedStatusLabel);
        filterLayout->addWidget(m_reimbursedStatusFilterCombo);
    }
    filterLayout->addWidget(searchButton);
    filterLayout->addWidget(resetButton);

    auto *actionWrapperLayout = new QVBoxLayout();
    actionWrapperLayout->setContentsMargins(0, 0, 0, 0);
    actionWrapperLayout->setSpacing(8);

    if (isInventoryPage()) {
        auto *manualLayout = new QHBoxLayout();
        manualLayout->setSpacing(10);

        auto *overflowMenu = new QMenu(this);
        overflowMenu->setStyleSheet(menuStyle());

        auto *directMenu = overflowMenu->addMenu(QStringLiteral("直接修改"));
        QAction *manualDirectAction = directMenu->addAction(QStringLiteral("手动"));
        QAction *excelDirectAction = directMenu->addAction(QStringLiteral("Excel"));

        auto *historyMenu = overflowMenu->addMenu(QStringLiteral("历史记录"));
        QAction *allHistoryAction = historyMenu->addAction(QStringLiteral("全部"));
        QAction *stockInHistoryAction = historyMenu->addAction(QStringLiteral("入库"));
        QAction *stockOutHistoryAction = historyMenu->addAction(QStringLiteral("出库"));
        QAction *directUpdateHistoryAction = historyMenu->addAction(QStringLiteral("直接修改"));
        QAction *deleteHistoryAction = historyMenu->addAction(QStringLiteral("删除"));

        auto *fulfillmentButton = new QPushButton(QStringLiteral("配单"), this);
        auto *stockOutButton = new QPushButton(QStringLiteral("出库"), this);
        auto *stockInButton = new QPushButton(QStringLiteral("入库"), this);
        auto *selectAllButton = new QPushButton(QStringLiteral("全选"), this);
        auto *deleteSelectedButton = new QPushButton(QStringLiteral("删除选中"), this);
        auto *exportButton = new QPushButton(QStringLiteral("导出 Excel"), this);
        auto *refreshButton = new QPushButton(QStringLiteral("刷新"), this);

        setButtonVariant(fulfillmentButton, QStringLiteral("primary"));
        setButtonVariant(stockOutButton, QStringLiteral("primary"));
        setButtonVariant(stockInButton, QStringLiteral("primary"));
        setButtonVariant(selectAllButton, QStringLiteral("subtle"));
        setButtonVariant(deleteSelectedButton, QStringLiteral("danger"));
        setButtonVariant(exportButton, QStringLiteral("subtle"));
        setButtonVariant(refreshButton, QStringLiteral("subtle"));

        connect(manualDirectAction, &QAction::triggered, this, [this]() { directUpdateRecord(); });
        connect(excelDirectAction, &QAction::triggered, this, [this]() {
            importInventoryRecords(InventoryOperationType::DirectUpdate);
        });
        connect(fulfillmentButton, &QPushButton::clicked, this, [this]() { fulfillDemandRecord(); });
        connect(stockOutButton, &QPushButton::clicked, this, [this]() { stockOutRecord(); });
        connect(stockInButton, &QPushButton::clicked, this, [this]() { stockInRecord(); });
        connect(selectAllButton, &QPushButton::clicked, this, [this]() { selectAllRecords(); });
        connect(deleteSelectedButton, &QPushButton::clicked, this, [this]() { deleteSelectedRecords(); });
        connect(allHistoryAction, &QAction::triggered, this, [this]() { viewInventoryHistory(); });
        connect(stockInHistoryAction, &QAction::triggered, this, [this]() {
            viewInventoryHistory(QStringLiteral("入库"));
        });
        connect(stockOutHistoryAction, &QAction::triggered, this, [this]() {
            viewInventoryHistory(QStringLiteral("出库"));
        });
        connect(directUpdateHistoryAction, &QAction::triggered, this, [this]() {
            viewInventoryHistory(QStringLiteral("直接修改"));
        });
        connect(deleteHistoryAction, &QAction::triggered, this, [this]() {
            viewInventoryHistory(QStringLiteral("删除"));
        });
        connect(exportButton, &QPushButton::clicked, this, [this]() { exportRecords(); });
        connect(refreshButton, &QPushButton::clicked, this, [this]() { reloadRecords(); });

        manualLayout->addWidget(stockInButton);
        manualLayout->addWidget(stockOutButton);
        manualLayout->addWidget(fulfillmentButton);
        manualLayout->addWidget(deleteSelectedButton);
        manualLayout->addWidget(selectAllButton);
        manualLayout->addWidget(exportButton);
        manualLayout->addWidget(refreshButton);
        manualLayout->addWidget(createOverflowMenuButton(overflowMenu, this));
        manualLayout->addStretch();

        actionWrapperLayout->addLayout(manualLayout);
    } else {
        auto *actionLayout = new QHBoxLayout();
        auto *addButton = new QPushButton(QStringLiteral("新增"), this);
        auto *editButton = new QPushButton(QStringLiteral("编辑"), this);
        auto *deleteButton = new QPushButton(QStringLiteral("删除"), this);
        QPushButton *exportAttachmentsButton = nullptr;
        QPushButton *exportPendingAttachmentsButton = nullptr;
        QPushButton *markReimbursedButton = nullptr;
        QPushButton *importButton = nullptr;
        QPushButton *exportButton = nullptr;
        auto *refreshButton = new QPushButton(QStringLiteral("刷新"), this);

        setButtonVariant(addButton, QStringLiteral("primary"));
        setButtonVariant(editButton, QStringLiteral("subtle"));
        setButtonVariant(deleteButton, QStringLiteral("danger"));
        setButtonVariant(refreshButton, QStringLiteral("subtle"));

        connect(addButton, &QPushButton::clicked, this, [this]() { addRecord(); });
        connect(editButton, &QPushButton::clicked, this, [this]() { editRecord(); });
        connect(deleteButton, &QPushButton::clicked, this, [this]() { deleteRecord(); });
        if (isReimbursementPage()) {
            exportAttachmentsButton = new QPushButton(QStringLiteral("导出附件"), this);
            exportPendingAttachmentsButton = new QPushButton(QStringLiteral("导出已开票未报账附件"), this);
            markReimbursedButton = new QPushButton(QStringLiteral("一键报账"), this);
            setButtonVariant(exportAttachmentsButton, QStringLiteral("subtle"));
            setButtonVariant(exportPendingAttachmentsButton, QStringLiteral("subtle"));
            setButtonVariant(markReimbursedButton, QStringLiteral("primary"));
            connect(exportAttachmentsButton, &QPushButton::clicked, this, [this]() { exportSelectedAttachments(); });
            connect(exportPendingAttachmentsButton, &QPushButton::clicked, this, [this]() { exportPendingInvoiceAttachments(); });
            connect(markReimbursedButton, &QPushButton::clicked, this, [this]() { markSelectedReimbursed(); });
        } else {
            importButton = new QPushButton(QStringLiteral("导入 Excel"), this);
            exportButton = new QPushButton(QStringLiteral("导出 Excel"), this);
            setButtonVariant(importButton, QStringLiteral("primary"));
            setButtonVariant(exportButton, QStringLiteral("subtle"));
            connect(importButton, &QPushButton::clicked, this, [this]() { importRecords(); });
            connect(exportButton, &QPushButton::clicked, this, [this]() { exportRecords(); });
        }
        connect(refreshButton, &QPushButton::clicked, this, [this]() { reloadRecords(); });

        actionLayout->addWidget(addButton);
        actionLayout->addWidget(editButton);
        actionLayout->addWidget(deleteButton);
        if (exportAttachmentsButton != nullptr) {
            actionLayout->addWidget(exportAttachmentsButton);
        }
        if (exportPendingAttachmentsButton != nullptr) {
            actionLayout->addWidget(exportPendingAttachmentsButton);
        }
        if (markReimbursedButton != nullptr) {
            actionLayout->addWidget(markReimbursedButton);
        }
        if (importButton != nullptr) {
            actionLayout->addWidget(importButton);
        }
        if (exportButton != nullptr) {
            actionLayout->addWidget(exportButton);
        }
        actionLayout->addWidget(refreshButton);
        actionLayout->addStretch();
        actionWrapperLayout->addLayout(actionLayout);
    }

    m_table = new QTableWidget(this);
    configureTableColumns();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode((isInventoryPage() || isReimbursementPage()) ? QAbstractItemView::MultiSelection
                                                                           : QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->setFrameShape(QFrame::NoFrame);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_table->verticalHeader()->setVisible(false);
    m_highlightDelegate = new SearchHighlightDelegate(m_table);
    m_table->setItemDelegate(m_highlightDelegate);
    if (isInventoryPage()) {
        m_table->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_table, &QTableWidget::customContextMenuRequested, this, [this](const QPoint &position) {
            showInventoryContextMenu(position);
        });
    }
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int, int) { editRecord(); });

    m_statusLabel = new QLabel(this);

    auto *filterPanel = new QFrame(this);
    filterPanel->setObjectName(QStringLiteral("sectionPanel"));
    filterPanel->setStyleSheet(sectionPanelStyle());
    auto *filterPanelLayout = new QVBoxLayout(filterPanel);
    filterPanelLayout->setContentsMargins(18, 18, 18, 18);
    filterPanelLayout->setSpacing(10);
    auto *filterTitleLabel = new QLabel(QStringLiteral("检索与筛选"), filterPanel);
    filterTitleLabel->setProperty("role", QStringLiteral("sectionTitle"));
    filterPanelLayout->addWidget(filterTitleLabel);
    filterPanelLayout->addLayout(filterLayout);

    auto *actionPanel = new QFrame(this);
    actionPanel->setObjectName(QStringLiteral("sectionPanel"));
    actionPanel->setStyleSheet(sectionPanelStyle());
    auto *actionPanelLayout = new QVBoxLayout(actionPanel);
    actionPanelLayout->setContentsMargins(18, 18, 18, 18);
    actionPanelLayout->setSpacing(10);
    auto *actionTitleLabel = new QLabel(QStringLiteral("快捷操作"), actionPanel);
    actionTitleLabel->setProperty("role", QStringLiteral("sectionTitle"));
    actionPanelLayout->addWidget(actionTitleLabel);
    actionPanelLayout->addLayout(actionWrapperLayout);

    auto *tablePanel = new QFrame(this);
    tablePanel->setObjectName(QStringLiteral("sectionPanel"));
    tablePanel->setStyleSheet(sectionPanelStyle());
    auto *tablePanelLayout = new QVBoxLayout(tablePanel);
    tablePanelLayout->setContentsMargins(18, 18, 18, 14);
    tablePanelLayout->setSpacing(10);
    auto *tableTitleLabel = new QLabel(QStringLiteral("数据列表"), tablePanel);
    tableTitleLabel->setProperty("role", QStringLiteral("sectionTitle"));
    tablePanelLayout->addWidget(tableTitleLabel);
    tablePanelLayout->addWidget(m_table, 1);
    tablePanelLayout->addWidget(m_statusLabel);

    rootLayout->addWidget(filterPanel);
    rootLayout->addWidget(actionPanel);
    rootLayout->addWidget(tablePanel, 1);
}

void ManagementPage::refreshTable(const QList<QVariantMap> &records)
{
    m_visibleRecords = records;
    if (m_highlightDelegate != nullptr) {
        m_highlightDelegate->setKeyword(isInventoryPage() && m_searchEdit != nullptr
                                            ? m_searchEdit->text().trimmed()
                                            : QString());
    }
    m_table->setRowCount(records.size());
    const QList<FieldDefinition> visibleFields = listFields();

    for (int row = 0; row < records.size(); ++row) {
        const QVariantMap &record = records.at(row);
        m_table->setRowHeight(row, 42);
        for (int column = 0; column < visibleFields.size(); ++column) {
            const FieldDefinition &field = visibleFields.at(column);
            const int tableColumn = column;
            if (isReimbursementPage() && field.key == QStringLiteral("invoiceAttachment")) {
                const QStringList references = splitAttachmentReferences(record.value(field.key));
                if (references.isEmpty()) {
                    auto *item = new QTableWidgetItem();
                    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                    if (column == 0) {
                        item->setData(Qt::UserRole, record.value("id").toString());
                    }
                    m_table->setItem(row, tableColumn, item);
                } else {
                    const QString displayName = displayAttachmentName(references.constFirst().trimmed());
                    auto *button = new QPushButton(displayName.isEmpty() ? QStringLiteral("下载附件")
                                                                         : QStringLiteral("下载 %1").arg(displayName),
                                                   m_table);
                    setButtonVariant(button, QStringLiteral("subtle"));
                    button->setCursor(Qt::PointingHandCursor);
                    connect(button, &QPushButton::clicked, this, [this, row]() {
                        m_table->selectRow(row);
                        downloadInvoiceAttachment(row);
                    });
                    m_table->setCellWidget(row, tableColumn, button);

                    auto *item = new QTableWidgetItem(displayName);
                    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                    if (column == 0) {
                        item->setData(Qt::UserRole, record.value("id").toString());
                    }
                    m_table->setItem(row, tableColumn, item);
                }
                continue;
            }

            auto *item = new QTableWidgetItem(recordValueText(record, field));
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            if (column == 0) {
                item->setData(Qt::UserRole, record.value("id").toString());
            }
            m_table->setItem(row, tableColumn, item);
        }
        if (m_config.showUpdatedAt) {
            auto *updatedAtItem = new QTableWidgetItem(record.value("updatedAt").toString());
            updatedAtItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            m_table->setItem(row, visibleFields.size(), updatedAtItem);
        }
    }

    m_statusLabel->setText(QStringLiteral("当前共 %1 条记录，本地目录：%2")
                               .arg(records.size())
                               .arg(m_storageService->storageRoot()));
    m_statusLabel->setStyleSheet(QStringLiteral("color: #71888c; font-size: 12px; padding-top: 6px;"));
    m_table->viewport()->update();
}

void ManagementPage::configureTableColumns()
{
    const QList<FieldDefinition> visibleFields = listFields();

    m_table->clearContents();
    m_table->setColumnCount(visibleFields.size() + (m_config.showUpdatedAt ? 1 : 0));

    QStringList headers;
    for (const FieldDefinition &field : visibleFields) {
        headers.append(field.label);
    }
    if (m_config.showUpdatedAt) {
        headers.append(QStringLiteral("更新时间"));
    }
    m_table->setHorizontalHeaderLabels(headers);
}

QList<FieldDefinition> ManagementPage::listFields() const
{
    if (m_config.listFieldKeys.isEmpty()) {
        return m_config.fields;
    }

    QList<FieldDefinition> fields;
    fields.reserve(m_config.listFieldKeys.size());
    for (const QString &key : m_config.listFieldKeys) {
        for (const FieldDefinition &field : m_config.fields) {
            if (field.key == key) {
                fields.append(field);
                break;
            }
        }
    }
    return fields;
}

QList<QVariantMap> ManagementPage::filteredRecords(const QList<QVariantMap> &allRecords,
                                                   const QString &keyword,
                                                   const QString &category) const
{
    const QString owner = m_ownerFilterCombo == nullptr
                              ? QString()
                              : m_ownerFilterCombo->currentData().toString();
    const QString invoiceStatus = m_invoiceStatusFilterCombo == nullptr
                                      ? QString()
                                      : m_invoiceStatusFilterCombo->currentData().toString();
    const QString reimbursedStatus = m_reimbursedStatusFilterCombo == nullptr
                                         ? QString()
                                         : m_reimbursedStatusFilterCombo->currentData().toString();

    QList<QVariantMap> candidateMatches;
    for (const QVariantMap &record : allRecords) {
        if (!category.isEmpty()
            && record.value(QStringLiteral("category")).toString().trimmed() != category) {
            continue;
        }

        if (!owner.isEmpty()
            && record.value(QStringLiteral("reimbursementOwner")).toString().trimmed() != owner) {
            continue;
        }

        if (!invoiceStatus.isEmpty()) {
            const bool invoiced = variantBoolValue(record.value(QStringLiteral("invoiceIssued")));
            if ((invoiceStatus == QStringLiteral("yes") && !invoiced)
                || (invoiceStatus == QStringLiteral("no") && invoiced)) {
                continue;
            }
        }

        if (!reimbursedStatus.isEmpty()) {
            const bool reimbursed = variantBoolValue(record.value(QStringLiteral("reimbursed")));
            if ((reimbursedStatus == QStringLiteral("yes") && !reimbursed)
                || (reimbursedStatus == QStringLiteral("no") && reimbursed)) {
                continue;
            }
        }

        if (keyword.isEmpty()) {
            candidateMatches.append(record);
            continue;
        }

        if (isInventoryPage()) {
            candidateMatches.append(record);
            continue;
        }

        for (const QString &key : m_config.searchableKeys) {
            if (record.value(key).toString().contains(keyword, Qt::CaseInsensitive)) {
                candidateMatches.append(record);
                break;
            }
        }
    }

    if (keyword.isEmpty() || !isInventoryPage()) {
        return candidateMatches;
    }

    return rankInventoryRecordsByKeyword(candidateMatches, keyword);
}

void ManagementPage::updateCategoryFilterOptions(const QList<QVariantMap> &allRecords)
{
    if ((!isInventoryPage() && !isReimbursementPage()) || m_categoryFilterCombo == nullptr) {
        return;
    }

    const QString selectedCategory = m_categoryFilterCombo->currentData().toString();
    const QStringList categories = inventoryCategories(allRecords);
    const QSignalBlocker blocker(m_categoryFilterCombo);

    m_categoryFilterCombo->clear();
    m_categoryFilterCombo->addItem(QStringLiteral("全部种类"), QString());
    for (const QString &category : categories) {
        m_categoryFilterCombo->addItem(category, category);
    }

    const int selectedIndex = selectedCategory.isEmpty()
                                  ? 0
                                  : m_categoryFilterCombo->findData(selectedCategory);
    m_categoryFilterCombo->setCurrentIndex(selectedIndex >= 0 ? selectedIndex : 0);
}

void ManagementPage::updateReimbursementFilterOptions(const QList<QVariantMap> &allRecords)
{
    if (!isReimbursementPage()) {
        return;
    }

    if (m_ownerFilterCombo != nullptr) {
        const QString selectedOwner = m_ownerFilterCombo->currentData().toString();
        QStringList owners;
        for (const QVariantMap &record : allRecords) {
            const QString owner = record.value(QStringLiteral("reimbursementOwner")).toString().trimmed();
            if (!owner.isEmpty() && !owners.contains(owner, Qt::CaseInsensitive)) {
                owners.append(owner);
            }
        }

        owners.sort(Qt::CaseInsensitive);
        const QSignalBlocker blocker(m_ownerFilterCombo);
        m_ownerFilterCombo->clear();
        m_ownerFilterCombo->addItem(QStringLiteral("全部报账人"), QString());
        for (const QString &owner : owners) {
            m_ownerFilterCombo->addItem(owner, owner);
        }

        const int selectedIndex = selectedOwner.isEmpty()
                                      ? 0
                                      : m_ownerFilterCombo->findData(selectedOwner);
        m_ownerFilterCombo->setCurrentIndex(selectedIndex >= 0 ? selectedIndex : 0);
    }
}

QVariantMap ManagementPage::selectedRecord() const
{
    const int currentRow = m_table->currentRow();
    if (currentRow < 0 || currentRow >= m_visibleRecords.size()) {
        return {};
    }
    return m_visibleRecords.at(currentRow);
}

QList<QVariantMap> ManagementPage::selectedRecords() const
{
    QList<QVariantMap> records;
    if (m_table == nullptr || m_table->selectionModel() == nullptr) {
        return records;
    }

    const QModelIndexList rows = m_table->selectionModel()->selectedRows();
    for (const QModelIndex &index : rows) {
        const int row = index.row();
        if (row >= 0 && row < m_visibleRecords.size()) {
            records.append(m_visibleRecords.at(row));
        }
    }

    return records;
}

void ManagementPage::showInventoryContextMenu(const QPoint &position)
{
    if (!isInventoryPage()) {
        return;
    }

    const int row = m_table->rowAt(position.y());
    if (row < 0 || row >= m_visibleRecords.size()) {
        return;
    }

    m_table->selectRow(row);

    QMenu menu(this);
    menu.setStyleSheet(menuStyle());

    QAction *deleteAction = menu.addAction(QStringLiteral("删除"));
    QAction *selectedAction = menu.exec(m_table->viewport()->mapToGlobal(position));
    if (selectedAction == deleteAction) {
        deleteRecord();
    }
}

void ManagementPage::directUpdateRecord()
{
    const QVariantMap record = selectedRecord();
    if (record.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一条库存记录。"));
        return;
    }

    InventoryRecordDialog dialog(QStringLiteral("直接修改库存记录"),
                                 stockEditDialogFields(m_config.fields),
                                 m_storageService,
                                 {QStringLiteral("category"), QStringLiteral("value")},
                                 this);
    dialog.setRecordData(record);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString note;
    if (!promptInventoryOperationNote(this,
                                      QStringLiteral("填写直接修改备注"),
                                      QStringLiteral("确认保存前可填写本次直接修改备注，留空则不记录备注。"),
                                      QString(),
                                      &note)) {
        return;
    }

    QString errorMessage;
    if (!m_storageService->applyInventoryChange(dialog.recordData(),
                                                InventoryOperationType::DirectUpdate,
                                                InventoryInputType::Manual,
                                                note,
                                                QString(),
                                                0,
                                                &errorMessage)) {
        QMessageBox::critical(this, QStringLiteral("保存失败"), errorMessage);
        return;
    }

    reloadRecords();
}

void ManagementPage::stockInRecord()
{
    switch (selectInventoryActionMode(this,
                                      QStringLiteral("选择入库方式"),
                                      QStringLiteral("请选择入库方式"),
                                      QStringLiteral("手动入库支持多条项目录入；Excel 入库用于从文件批量导入库存字段。"),
                                      QStringLiteral("手动入库"),
                                      QStringLiteral("Excel 入库"))) {
    case InventoryActionModeSelection::Manual:
        manualStockInRecord();
        break;
    case InventoryActionModeSelection::Bom:
        excelStockInRecord();
        break;
    case InventoryActionModeSelection::Cancel:
    default:
        break;
    }
}

void ManagementPage::manualStockInRecord()
{
    ManualStockInDialog dialog(m_config.fields,
                               m_storageService->loadPageRecords(QStringLiteral("inventory")),
                               this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString operationNote;
    if (!promptInventoryOperationNote(this,
                                      QStringLiteral("填写入库备注"),
                                      QStringLiteral("确认入库前可填写本次操作备注，留空则不记录备注。"),
                                      QString(),
                                      &operationNote)) {
        return;
    }

    int successCount = 0;
    QStringList failures;
    const QList<ManualStockInEntry> pendingEntries = dialog.entries();
    for (int index = 0; index < pendingEntries.size(); ++index) {
        const ManualStockInEntry &entry = pendingEntries.at(index);
        QString errorMessage;
        if (m_storageService->applyInventoryChange(entry.itemData,
                                                   InventoryOperationType::StockIn,
                                                   InventoryInputType::Manual,
                                                   combineInventoryNotes(operationNote, entry.note),
                                                   QString(),
                                                   0,
                                                   &errorMessage)) {
            ++successCount;
            continue;
        }

        failures.append(QStringLiteral("第 %1 条：%2").arg(index + 1).arg(errorMessage));
    }

    reloadRecords();
    QString message = QStringLiteral("成功 %1 条，失败 %2 条。")
                          .arg(successCount)
                          .arg(failures.size());
    if (!failures.isEmpty()) {
        message += QStringLiteral("\n\n") + failures.join(QStringLiteral("\n"));
    }
    showImportResultMessage(this, successCount > 0, message);
}

void ManagementPage::excelStockInRecord()
{
    importInventoryRecords(InventoryOperationType::StockIn);
}

void ManagementPage::stockOutRecord()
{
    switch (selectInventoryActionMode(this,
                                      QStringLiteral("选择出库方式"),
                                      QStringLiteral("请选择出库方式"),
                                      QStringLiteral("手动出库用于对现有库存逐条扣减；BOM 表出库后续用于从文件批量扣减。"),
                                      QStringLiteral("手动出库"),
                                      QStringLiteral("BOM 表出库"))) {
    case InventoryActionModeSelection::Manual:
        manualStockOutRecord();
        break;
    case InventoryActionModeSelection::Bom:
        bomStockOutRecord();
        break;
    case InventoryActionModeSelection::Cancel:
    default:
        break;
    }
}

void ManagementPage::fulfillDemandRecord()
{
    InventoryFulfillmentDialog dialog(m_storageService, this);
    dialog.exec();
    if (dialog.inventoryChanged()) {
        reloadRecords();
    }
}

void ManagementPage::manualStockOutRecord()
{
    const QVariantMap record = selectedRecord();
    if (record.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一条库存记录。"));
        return;
    }

    InventoryTransactionDialog dialog(InventoryTransactionDialog::Mode::StockOut,
                                      inventoryRecordSummary(record),
                                      record.value(QStringLiteral("quantity")).toInt(),
                                      this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString note;
    if (!promptInventoryOperationNote(this,
                                      QStringLiteral("填写出库备注"),
                                      QStringLiteral("确认出库前可填写本次操作备注，留空则不记录备注。"),
                                      dialog.note(),
                                      &note)) {
        return;
    }

    QVariantMap changeRecord;
    changeRecord.insert(QStringLiteral("id"), record.value(QStringLiteral("id")));
    changeRecord.insert(QStringLiteral("quantity"), dialog.quantity());
    changeRecord.insert(QStringLiteral("date"), dialog.transactionDate());

    QString errorMessage;
    if (!m_storageService->applyInventoryChange(changeRecord,
                                                InventoryOperationType::StockOut,
                                                InventoryInputType::Manual,
                                                note,
                                                QString(),
                                                0,
                                                &errorMessage)) {
        QMessageBox::critical(this, QStringLiteral("出库失败"), errorMessage);
        return;
    }

    reloadRecords();
}

void ManagementPage::bomStockOutRecord()
{
    const QString filePath = QFileDialog::getOpenFileName(this,
                                                          QStringLiteral("选择 BOM 表"),
                                                          QString(),
                                                          QStringLiteral("Excel 文件 (*.xlsx)"));
    if (filePath.isEmpty()) {
        return;
    }

    QString note;
    if (!promptInventoryOperationNote(this,
                                      QStringLiteral("填写出库备注"),
                                      QStringLiteral("确认出库前可填写本次 BOM 出库备注，留空则不记录备注。"),
                                      QString(),
                                      &note)) {
        return;
    }

    QString message;
    const bool success = m_storageService->importInventoryBom(filePath,
                                                              InventoryOperationType::StockOut,
                                                              note,
                                                              &message);
    reloadRecords();
    showImportResultMessage(this, success, message);
}

void ManagementPage::importInventoryRecords(InventoryOperationType operationType)
{
    const QString filePath = QFileDialog::getOpenFileName(this,
                                                          QStringLiteral("导入 Excel"),
                                                          QString(),
                                                          QStringLiteral("Excel 文件 (*.xlsx)"));
    if (filePath.isEmpty()) {
        return;
    }

    const QString title = operationType == InventoryOperationType::StockIn
                              ? QStringLiteral("填写入库备注")
                              : QStringLiteral("填写直接修改备注");
    const QString label = operationType == InventoryOperationType::StockIn
                              ? QStringLiteral("确认导入前可填写本次 Excel 入库备注，留空则不记录备注。")
                              : QStringLiteral("确认导入前可填写本次 Excel 直接修改备注，留空则不记录备注。");
    QString note;
    if (!promptInventoryOperationNote(this, title, label, QString(), &note)) {
        return;
    }

    QString message;
    const bool success = m_storageService->importInventoryExcel(m_config.fields,
                                                                filePath,
                                                                operationType,
                                                                note,
                                                                &message);
    if (!success) {
        showImportResultMessage(this, false, message);
        reloadRecords();
        return;
    }

    reloadRecords();
    showImportResultMessage(this, true, message);
}

void ManagementPage::viewInventoryHistory(const QString &inputTypeFilter)
{
    InventoryHistoryDialog dialog(filteredHistoryRecords(m_storageService->loadInventoryHistory(), inputTypeFilter), this);
    dialog.exec();
}

void ManagementPage::addRecord()
{
    QVariantMap data;
    if (isInventoryPage()) {
        InventoryRecordDialog dialog(QStringLiteral("新增%1").arg(m_config.title),
                                     m_config.fields,
                                     m_storageService,
                                     {},
                                     this);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        data = dialog.recordData();
    } else {
        RecordDialog dialog(QStringLiteral("新增%1").arg(m_config.title), m_config.fields, this);
        if (isReimbursementPage() && m_storageService != nullptr) {
            dialog.setRecordData({{QStringLiteral("reimbursementOwner"), m_storageService->currentUserName()}});
        }
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        data = dialog.recordData();
    }

    QString errorMessage;
    if (!m_storageService->upsertRecord(m_config.pageId, data, &errorMessage)) {
        QMessageBox::critical(this, QStringLiteral("保存失败"), errorMessage);
        return;
    }

    reloadRecords();
}

void ManagementPage::editRecord()
{
    const QVariantMap record = selectedRecord();
    if (record.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一条记录。"));
        return;
    }

    QVariantMap data;
    if (isInventoryPage()) {
        InventoryRecordDialog dialog(QStringLiteral("编辑%1").arg(m_config.title),
                                     stockEditDialogFields(m_config.fields),
                                     m_storageService,
                                     {},
                                     this);
        dialog.setRecordData(record);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        data = dialog.recordData();

        QString note;
        if (!promptInventoryOperationNote(this,
                                          QStringLiteral("填写修改备注"),
                                          QStringLiteral("确认保存前可填写本次修改备注，留空则不记录备注。"),
                                          QString(),
                                          &note)) {
            return;
        }

        QString errorMessage;
        if (!m_storageService->applyInventoryChange(data,
                                                    InventoryOperationType::DirectUpdate,
                                                    InventoryInputType::Manual,
                                                    note,
                                                    QString(),
                                                    0,
                                                    &errorMessage)) {
            QMessageBox::critical(this, QStringLiteral("保存失败"), errorMessage);
            return;
        }

        reloadRecords();
        return;
    } else {
        RecordDialog dialog(QStringLiteral("编辑%1").arg(m_config.title), m_config.fields, this);
        dialog.setRecordData(record);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        data = dialog.recordData();
    }

    QString errorMessage;
    if (!m_storageService->upsertRecord(m_config.pageId, data, &errorMessage)) {
        QMessageBox::critical(this, QStringLiteral("保存失败"), errorMessage);
        return;
    }

    reloadRecords();
}

void ManagementPage::selectAllRecords()
{
    if (m_table == nullptr) {
        return;
    }

    m_table->selectAll();
}

void ManagementPage::deleteRecord()
{
    const QVariantMap record = selectedRecord();
    if (record.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一条记录。"));
        return;
    }

    const auto answer = QMessageBox::question(this,
                                              QStringLiteral("确认删除"),
                                              QStringLiteral("确认删除当前选中的记录吗？"));
    if (answer != QMessageBox::Yes) {
        return;
    }

    QString note;
    if (isInventoryPage() && !promptInventoryOperationNote(this,
                                                           QStringLiteral("填写删除备注"),
                                                           QStringLiteral("确认删除前可填写本次删除备注，留空则不记录备注。"),
                                                           QString(),
                                                           &note)) {
        return;
    }

    QString errorMessage;
    const bool success = isInventoryPage()
                             ? m_storageService->deleteInventoryRecord(record.value("id").toString(), note, &errorMessage)
                             : m_storageService->removeRecord(m_config.pageId, record.value("id").toString(), &errorMessage);
    if (!success) {
        QMessageBox::critical(this, QStringLiteral("删除失败"), errorMessage);
        return;
    }

    reloadRecords();
}

void ManagementPage::deleteSelectedRecords()
{
    const QList<QVariantMap> records = selectedRecords();
    if (records.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择要删除的库存记录。"));
        return;
    }

    const auto answer = QMessageBox::question(this,
                                              QStringLiteral("确认删除选中条目"),
                                              QStringLiteral("确认删除当前选中的 %1 条库存记录吗？").arg(records.size()));
    if (answer != QMessageBox::Yes) {
        return;
    }

    QString note;
    if (!promptInventoryOperationNote(this,
                                      QStringLiteral("填写删除备注"),
                                      QStringLiteral("确认删除前可填写本次批量删除备注，留空则不记录备注。"),
                                      QString(),
                                      &note)) {
        return;
    }

    int successCount = 0;
    QStringList failures;
    for (const QVariantMap &record : records) {
        QString errorMessage;
        if (m_storageService->deleteInventoryRecord(record.value("id").toString(), note, &errorMessage)) {
            ++successCount;
        } else {
            const QString label = record.value(QStringLiteral("manufacturerPart")).toString().trimmed();
            failures.append(QStringLiteral("%1：%2")
                                .arg(label.isEmpty() ? record.value("id").toString() : label,
                                     errorMessage));
        }
    }

    reloadRecords();

    QString message = QStringLiteral("成功删除 %1 条，失败 %2 条。")
                          .arg(successCount)
                          .arg(failures.size());
    if (!failures.isEmpty()) {
        message += QStringLiteral("\n\n") + failures.join(QStringLiteral("\n"));
    }

    if (failures.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("删除完成"), message);
        return;
    }

    QMessageBox box(this);
    box.setIcon(successCount > 0 ? QMessageBox::Information : QMessageBox::Warning);
    box.setWindowTitle(successCount > 0 ? QStringLiteral("删除完成") : QStringLiteral("删除失败"));
    box.setText(message.split(QChar('\n')).first());
    box.setDetailedText(failures.join(QStringLiteral("\n")));
    box.exec();
}

void ManagementPage::exportSelectedAttachments()
{
    if (!isReimbursementPage()) {
        return;
    }

    const QList<QVariantMap> records = selectedRecords();
    if (records.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择至少一条报账记录。"));
        return;
    }

    const QString targetDirectory = QFileDialog::getExistingDirectory(this,
                                                                      QStringLiteral("选择附件导出目录"),
                                                                      QString());
    if (targetDirectory.isEmpty()) {
        return;
    }

    int successCount = 0;
    QStringList failures;
    for (const QVariantMap &record : records) {
        exportReimbursementAttachmentsForRecord(m_storageService, record, targetDirectory, &failures, &successCount);
    }

    QString message = QStringLiteral("成功导出 %1 个附件。").arg(successCount);
    if (!failures.isEmpty()) {
        message += QStringLiteral("\n\n失败明细：\n") + failures.join(QStringLiteral("\n"));
    }

    if (failures.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("导出完成"), message);
    } else {
        QMessageBox::warning(this, QStringLiteral("部分附件导出失败"), message);
    }
}

void ManagementPage::exportPendingInvoiceAttachments()
{
    if (!isReimbursementPage()) {
        return;
    }

    const QList<QVariantMap> allRecords = m_storageService->loadPageRecords(m_config.pageId);
    QList<QVariantMap> pendingRecords;
    for (const QVariantMap &record : allRecords) {
        if (variantBoolValue(record.value(QStringLiteral("invoiceIssued")))
            && !variantBoolValue(record.value(QStringLiteral("reimbursed")))) {
            const bool hasAttachments = !record.value(QStringLiteral("invoiceAttachment")).toString().trimmed().isEmpty()
                                        || !record.value(QStringLiteral("otherAttachments")).toString().trimmed().isEmpty();
            if (hasAttachments) {
                pendingRecords.append(record);
            }
        }
    }

    if (pendingRecords.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("当前没有已开票未报账且带附件的记录。"));
        return;
    }

    const QString targetDirectory = QFileDialog::getExistingDirectory(this,
                                                                      QStringLiteral("选择附件导出目录"),
                                                                      QString());
    if (targetDirectory.isEmpty()) {
        return;
    }

    int successCount = 0;
    QStringList failures;
    for (const QVariantMap &record : pendingRecords) {
        exportReimbursementAttachmentsForRecord(m_storageService, record, targetDirectory, &failures, &successCount);
    }

    QString message = QStringLiteral("成功导出 %1 个附件。匹配记录 %2 条。").arg(successCount).arg(pendingRecords.size());
    if (!failures.isEmpty()) {
        message += QStringLiteral("\n\n失败明细：\n") + failures.join(QStringLiteral("\n"));
    }

    if (failures.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("导出完成"), message);
    } else {
        QMessageBox::warning(this, QStringLiteral("部分附件导出失败"), message);
    }
}

void ManagementPage::markSelectedReimbursed()
{
    if (!isReimbursementPage()) {
        return;
    }

    const QList<QVariantMap> records = selectedRecords();
    if (records.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择至少一条报账记录。"));
        return;
    }

    int successCount = 0;
    QStringList failures;
    for (QVariantMap record : records) {
        if (variantBoolValue(record.value(QStringLiteral("reimbursed")))) {
            continue;
        }
        record.insert(QStringLiteral("reimbursed"), true);
        QString errorMessage;
        if (m_storageService->upsertRecord(m_config.pageId, record, &errorMessage)) {
            ++successCount;
        } else {
            const QString label = record.value(QStringLiteral("description")).toString().trimmed();
            failures.append(QStringLiteral("%1：%2")
                                .arg(label.isEmpty() ? record.value(QStringLiteral("id")).toString() : label,
                                     errorMessage));
        }
    }

    reloadRecords();

    QString message = QStringLiteral("成功报账 %1 条，失败 %2 条。").arg(successCount).arg(failures.size());
    if (!failures.isEmpty()) {
        message += QStringLiteral("\n\n") + failures.join(QStringLiteral("\n"));
    }
    if (failures.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("报账完成"), message);
    } else {
        QMessageBox::warning(this, QStringLiteral("部分报账失败"), message);
    }
}

void ManagementPage::downloadInvoiceAttachment(int row)
{
    if (!isReimbursementPage() || row < 0 || row >= m_visibleRecords.size()) {
        return;
    }

    const QVariantMap record = m_visibleRecords.at(row);
    const QStringList references = splitAttachmentReferences(record.value(QStringLiteral("invoiceAttachment")));
    if (references.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("当前记录没有发票附件。"));
        return;
    }

    QList<ReimbursementAttachmentContent> attachments;
    QString errorMessage;
    if (!m_storageService->loadReimbursementAttachments(record, &attachments, &errorMessage)) {
        QMessageBox::critical(this, QStringLiteral("下载失败"), errorMessage);
        return;
    }

    const QString reference = references.constFirst().trimmed();
    const auto attachmentIt = std::find_if(attachments.cbegin(),
                                           attachments.cend(),
                                           [&reference](const ReimbursementAttachmentContent &attachment) {
                                               return attachment.reference.trimmed() == reference;
                                           });
    if (attachmentIt == attachments.cend()) {
        QMessageBox::warning(this, QStringLiteral("下载失败"), QStringLiteral("未找到当前记录对应的发票附件内容。"));
        return;
    }

    const QString defaultFileName = attachmentIt->fileName.trimmed().isEmpty()
                                        ? displayAttachmentName(reference)
                                        : attachmentIt->fileName.trimmed();
    const QString targetFilePath = QFileDialog::getSaveFileName(this,
                                                                QStringLiteral("保存发票附件"),
                                                                defaultFileName,
                                                                QStringLiteral("PDF 文件 (*.pdf);;所有文件 (*.*)"));
    if (targetFilePath.isEmpty()) {
        return;
    }

    QFile file(targetFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, QStringLiteral("下载失败"), file.errorString());
        return;
    }

    if (file.write(attachmentIt->content) != attachmentIt->content.size()) {
        QMessageBox::critical(this, QStringLiteral("下载失败"), QStringLiteral("附件写入不完整。"));
        return;
    }

    QMessageBox::information(this,
                             QStringLiteral("下载完成"),
                             QStringLiteral("发票附件已保存到：\n%1").arg(QDir::toNativeSeparators(targetFilePath)));
}

void ManagementPage::importRecords()
{
    const QString filePath = QFileDialog::getOpenFileName(this,
                                                          QStringLiteral("导入 Excel"),
                                                          QString(),
                                                          QStringLiteral("Excel 文件 (*.xlsx)"));
    if (filePath.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!m_storageService->importExcel(m_config.pageId, m_config.fields, filePath, &errorMessage)) {
        QMessageBox::critical(this, QStringLiteral("导入失败"), errorMessage);
        return;
    }

    reloadRecords();
}

void ManagementPage::exportRecords()
{
    const QString filePath = QFileDialog::getSaveFileName(this,
                                                          QStringLiteral("导出 Excel"),
                                                          m_config.pageId + QStringLiteral(".xlsx"),
                                                          QStringLiteral("Excel 文件 (*.xlsx)"));
    if (filePath.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!m_storageService->exportExcel(m_config.pageId, m_config.fields, m_config.title, filePath, &errorMessage)) {
        QMessageBox::critical(this, QStringLiteral("导出失败"), errorMessage);
        return;
    }

    QMessageBox::information(this, QStringLiteral("导出完成"), QStringLiteral("Excel 已导出。"));
}
