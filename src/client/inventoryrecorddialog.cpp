#include "inventoryrecorddialog.h"

#include "NoWheelSpinBox.h"
#include "NoWheelDoubleSpinBox.h"
#include "aiinventoryenricher.h"

#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTextBrowser>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {
QString fieldSourcesToHtml(const InventoryEnrichmentResult &result,
                           const QList<FieldDefinition> &fields)
{
    QHash<QString, QString> labels;
    for (const FieldDefinition &field : fields) {
        labels.insert(field.key, field.label);
    }

    QString html = QStringLiteral("<h3>AI 补齐来源</h3><ul>");
    for (const InventoryEnrichmentField &field : result.fields) {
        const QString label = labels.value(field.key, field.key);
        const QString sourceLink = !AiInventoryEnricher::isSafeSourceUrl(field.sourceUrl)
                                        ? field.sourceTitle.toHtmlEscaped()
                                       : QStringLiteral("<a href=\"%1\">%2</a>")
                                             .arg(field.sourceUrl.toHtmlEscaped(),
                                                  field.sourceTitle.toHtmlEscaped());
        html += QStringLiteral("<li><b>%1</b>: %2<br/>来源: %3</li>")
                    .arg(label.toHtmlEscaped(), field.value.toHtmlEscaped(), sourceLink);
    }
    html += QStringLiteral("</ul>");
    return html;
}

QString displayFieldLabel(const FieldDefinition &field)
{
    return field.required
               ? QStringLiteral("%1（必填）：").arg(field.label)
               : field.label + QStringLiteral("：");
}
}

InventoryRecordDialog::InventoryRecordDialog(const QString &title,
                                             const QList<FieldDefinition> &fields,
                                             AppService *service,
                               const QStringList &allowedEnrichmentKeys,
                                             QWidget *parent)
    : QDialog(parent),
      m_fields(fields),
    m_service(service),
    m_allowedEnrichmentKeys(allowedEnrichmentKeys)
{
    setWindowTitle(title);
    resize(720, 760);
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #f4f7f8; }"
        "QScrollArea { border: none; background: transparent; }"
        "QLabel { color: #284048; font-weight: 600; }"
        "QDialogButtonBox QPushButton { min-width: 88px; }"));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(18, 18, 18, 18);
    rootLayout->setSpacing(14);

    auto *headerLabel = new QLabel(title, this);
    headerLabel->setStyleSheet(QStringLiteral("font-size: 24px; font-weight: 800; color: #173137;"));

    const QString hintText = m_allowedEnrichmentKeys.isEmpty()
                                 ? QStringLiteral("输入 Manufacturer Part 后可调用 AI 补齐元件信息。返回的每一项都会附带来源链接，保存前可查阅确认。")
                                 : QStringLiteral("输入 Manufacturer Part 后可调用 AI 补齐元件信息。当前界面仅回填 %1 两项，返回内容仍可通过来源窗口核对。")
                                       .arg(QStringList{QStringLiteral("category"), QStringLiteral("value")}.join(QStringLiteral("、")));
    Q_UNUSED(hintText);

    auto *scrollArea = new QScrollArea(this);
    auto *formContainer = new QWidget(scrollArea);
    formContainer->setObjectName(QStringLiteral("formContainer"));
    auto *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight);
    formLayout->setContentsMargins(20, 20, 20, 20);
    formLayout->setHorizontalSpacing(18);
    formLayout->setVerticalSpacing(14);

    for (const FieldDefinition &field : m_fields) {
        QWidget *editor = createEditor(field);
        m_editors.insert(field.key, editor);
        if (field.key == QStringLiteral("manufacturerPart")) {
            auto *rowWidget = new QWidget(this);
            auto *rowLayout = new QHBoxLayout(rowWidget);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            rowLayout->setSpacing(8);
            auto *enrichButton = new QPushButton(QStringLiteral("AI 补齐"), rowWidget);
            enrichButton->setProperty("variant", QStringLiteral("primary"));
            enrichButton->setMinimumHeight(40);
            connect(enrichButton, &QPushButton::clicked, this, [this]() { runAiEnrichment(); });
            rowLayout->addWidget(editor, 1);
            rowLayout->addWidget(enrichButton);
            formLayout->addRow(displayFieldLabel(field), rowWidget);
        } else {
            formLayout->addRow(displayFieldLabel(field), editor);
        }
    }

    formContainer->setStyleSheet(QStringLiteral(
        "#formContainer { background-color: #fbfdfd; border: 1px solid #dde8e9; border-radius: 22px; }"));
    formContainer->setLayout(formLayout);
    scrollArea->setWidget(formContainer);
    scrollArea->setWidgetResizable(true);

    m_enrichmentStatusLabel = new QLabel(QStringLiteral("尚未执行 AI 补齐。"), this);
    m_enrichmentStatusLabel->setStyleSheet(QStringLiteral("color: #71888c; font-weight: 500;"));
    m_showSourcesButton = new QPushButton(QStringLiteral("查看来源"), this);
    m_showSourcesButton->setProperty("variant", QStringLiteral("subtle"));
    m_showSourcesButton->setEnabled(false);
    connect(m_showSourcesButton, &QPushButton::clicked, this, [this]() { showSourceDetails(); });

    auto *footerLayout = new QHBoxLayout();
    footerLayout->addWidget(m_enrichmentStatusLabel, 1);
    footerLayout->addWidget(m_showSourcesButton);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok)) {
        okButton->setProperty("variant", QStringLiteral("primary"));
    }
    if (QPushButton *cancelButton = buttonBox->button(QDialogButtonBox::Cancel)) {
        cancelButton->setProperty("variant", QStringLiteral("subtle"));
    }
    connect(buttonBox, &QDialogButtonBox::accepted, this, &InventoryRecordDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &InventoryRecordDialog::reject);

    rootLayout->addWidget(headerLabel);
    rootLayout->addWidget(scrollArea, 1);
    rootLayout->addLayout(footerLayout);
    rootLayout->addWidget(buttonBox);
}

void InventoryRecordDialog::setRecordData(const QVariantMap &record)
{
    m_originalRecord = record;
    for (const FieldDefinition &field : m_fields) {
        if (QWidget *editor = m_editors.value(field.key, nullptr)) {
            setEditorValue(field, editor, record.value(field.key));
        }
    }
}

QVariantMap InventoryRecordDialog::recordData() const
{
    QVariantMap result = m_originalRecord;
    for (const FieldDefinition &field : m_fields) {
        if (QWidget *editor = m_editors.value(field.key, nullptr)) {
            result.insert(field.key, editorValue(field, editor));
        }
    }
    return result;
}

void InventoryRecordDialog::accept()
{
    for (const FieldDefinition &field : m_fields) {
        const QVariant value = editorValue(field, m_editors.value(field.key));
        const QString textValue = value.toString().trimmed();
        if (field.required && textValue.isEmpty()) {
            QMessageBox::warning(this,
                                 QStringLiteral("信息不完整"),
                                 QStringLiteral("请填写字段：%1").arg(field.label));
            return;
        }
    }

    QDialog::accept();
}

QWidget *InventoryRecordDialog::createEditor(const FieldDefinition &field)
{
    switch (field.type) {
    case FieldType::Integer: {
        auto *editor = new NoWheelSpinBox(this);
        editor->setRange(0, 1000000000);
        return editor;
    }
    case FieldType::Double: {
        auto *editor = new NoWheelDoubleSpinBox(this);
        editor->setRange(-1000000000.0, 1000000000.0);
        editor->setDecimals(2);
        return editor;
    }
    case FieldType::Date: {
        auto *editor = new QDateEdit(QDate::currentDate(), this);
        editor->setCalendarPopup(true);
        editor->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
        return editor;
    }
    case FieldType::Multiline: {
        auto *editor = new QTextEdit(this);
        editor->setMinimumHeight(90);
        return editor;
    }
    case FieldType::Choice: {
        auto *editor = new QComboBox(this);
        editor->addItems(field.options);
        return editor;
    }
    case FieldType::Text:
    default:
        return new QLineEdit(this);
    }
}

QVariant InventoryRecordDialog::editorValue(const FieldDefinition &field, QWidget *editor) const
{
    switch (field.type) {
    case FieldType::Integer:
        return static_cast<QSpinBox *>(editor)->value();
    case FieldType::Double:
        return static_cast<QDoubleSpinBox *>(editor)->value();
    case FieldType::Date:
        return static_cast<QDateEdit *>(editor)->date().toString(Qt::ISODate);
    case FieldType::Multiline:
        return static_cast<QTextEdit *>(editor)->toPlainText();
    case FieldType::Choice:
        return static_cast<QComboBox *>(editor)->currentText();
    case FieldType::Text:
    default:
        return static_cast<QLineEdit *>(editor)->text();
    }
}

void InventoryRecordDialog::setEditorValue(const FieldDefinition &field, QWidget *editor, const QVariant &value)
{
    switch (field.type) {
    case FieldType::Integer:
        static_cast<QSpinBox *>(editor)->setValue(value.toInt());
        break;
    case FieldType::Double:
        static_cast<QDoubleSpinBox *>(editor)->setValue(value.toDouble());
        break;
    case FieldType::Date: {
        const QDate date = QDate::fromString(value.toString(), Qt::ISODate);
        if (date.isValid()) {
            static_cast<QDateEdit *>(editor)->setDate(date);
        }
        break;
    }
    case FieldType::Multiline:
        static_cast<QTextEdit *>(editor)->setPlainText(value.toString());
        break;
    case FieldType::Choice: {
        auto *comboBox = static_cast<QComboBox *>(editor);
        const int index = comboBox->findText(value.toString());
        if (index >= 0) {
            comboBox->setCurrentIndex(index);
        }
        break;
    }
    case FieldType::Text:
    default:
        static_cast<QLineEdit *>(editor)->setText(value.toString());
        break;
    }
}

void InventoryRecordDialog::runAiEnrichment()
{
    auto *manufacturerPartEditor = qobject_cast<QLineEdit *>(m_editors.value(QStringLiteral("manufacturerPart")));
    if (manufacturerPartEditor == nullptr) {
        QMessageBox::critical(this, QStringLiteral("内部错误"), QStringLiteral("未找到 Manufacturer Part 输入框。"));
        return;
    }

    const QString manufacturerPart = manufacturerPartEditor->text().trimmed();
    if (manufacturerPart.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先填写 Manufacturer Part。"));
        return;
    }
    if (m_service == nullptr) {
        QMessageBox::critical(this, QStringLiteral("内部错误"), QStringLiteral("当前服务不可用。"));
        return;
    }

    InventoryEnrichmentResult enrichment;
    QString errorMessage;
    if (!m_service->enrichInventoryRecord(manufacturerPart,
                                          recordData(),
                                          m_allowedEnrichmentKeys,
                                          &enrichment,
                                          &errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("AI 补齐失败"), errorMessage);
        return;
    }

    if (!m_allowedEnrichmentKeys.isEmpty()) {
        QList<InventoryEnrichmentField> filteredFields;
        for (const InventoryEnrichmentField &field : enrichment.fields) {
            if (m_allowedEnrichmentKeys.contains(field.key)) {
                filteredFields.append(field);
            }
        }
        enrichment.fields = filteredFields;
    }

    if (enrichment.fields.isEmpty()) {
        m_lastEnrichment = enrichment;
        m_showSourcesButton->setEnabled(false);
        m_enrichmentStatusLabel->setText(QStringLiteral("AI 已返回结果，但当前界面没有可回填的字段。"));
        QMessageBox::information(this,
                                 QStringLiteral("AI 补齐完成"),
                                 QStringLiteral("AI 已返回结果，但当前界面仅允许回填 category 和 value，两项均未命中可用内容。"));
        return;
    }

    QStringList applyModes = {QStringLiteral("仅补空字段"), QStringLiteral("覆盖已有字段")};
    bool ok = false;
    const QString selectedMode = QInputDialog::getItem(this,
                                                       QStringLiteral("应用方式"),
                                                       QStringLiteral("AI 返回了 %1 项可用信息，请选择如何回填。\n所有字段都可通过“查看来源”继续核对。")
                                                           .arg(enrichment.fields.size()),
                                                       applyModes,
                                                       0,
                                                       false,
                                                       &ok);
    if (!ok) {
        return;
    }

    const bool overwriteExisting = selectedMode == QStringLiteral("覆盖已有字段");
    int appliedCount = 0;
    for (const InventoryEnrichmentField &field : enrichment.fields) {
        const FieldDefinition matchedField = [&]() {
            for (const FieldDefinition &candidate : m_fields) {
                if (candidate.key == field.key) {
                    return candidate;
                }
            }
            return FieldDefinition{};
        }();
        if (matchedField.key.isEmpty()) {
            continue;
        }

        QWidget *editor = m_editors.value(field.key, nullptr);
        if (editor == nullptr) {
            continue;
        }

        const QString existingValue = editorValue(matchedField, editor).toString().trimmed();
        if (!overwriteExisting && !existingValue.isEmpty()) {
            continue;
        }

        setEditorValue(matchedField, editor, field.value);
        ++appliedCount;
    }

    m_lastEnrichment = enrichment;
    m_enrichmentStatusLabel->setText(QStringLiteral("最近补齐：%1 项，来源提供者：%2")
                                         .arg(enrichment.fields.size())
                                         .arg(enrichment.provider.trimmed().isEmpty() ? QStringLiteral("unknown") : enrichment.provider));
    m_showSourcesButton->setEnabled(!m_lastEnrichment.fields.isEmpty());
    QMessageBox::information(this,
                             QStringLiteral("AI 补齐完成"),
                             QStringLiteral("已应用 %1 项字段。你可以点击“查看来源”继续核对每一项信息。")
                                 .arg(appliedCount));
}

void InventoryRecordDialog::showSourceDetails()
{
    if (m_lastEnrichment.fields.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("当前没有可查看的来源信息。"));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("补齐来源详情"));
    dialog.resize(760, 520);

    auto *layout = new QVBoxLayout(&dialog);
    auto *browser = new QTextBrowser(&dialog);
    browser->setOpenExternalLinks(true);
    browser->setHtml(fieldSourcesToHtml(m_lastEnrichment, m_fields));
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

    layout->addWidget(browser);
    layout->addWidget(buttonBox);
    dialog.exec();
}

QString InventoryRecordDialog::fieldLabel(const QString &fieldKey) const
{
    for (const FieldDefinition &field : m_fields) {
        if (field.key == fieldKey) {
            return field.label;
        }
    }
    return fieldKey;
}
