 #include "recorddialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {
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

QString displayFieldLabel(const FieldDefinition &field)
{
    return field.required
               ? QStringLiteral("%1（必填）：").arg(field.label)
               : field.label + QStringLiteral("：");
}

class AttachmentEditor : public QWidget {
public:
    AttachmentEditor(bool multiple, const QString &filter, QWidget *parent = nullptr)
        : QWidget(parent),
          m_multiple(multiple),
          m_filter(filter)
    {
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);

        if (m_multiple) {
            m_textEdit = new QTextEdit(this);
            m_textEdit->setMinimumHeight(90);
            m_textEdit->setPlaceholderText(QStringLiteral("每行一个附件路径"));
            layout->addWidget(m_textEdit, 1);
        } else {
            m_lineEdit = new QLineEdit(this);
            m_lineEdit->setPlaceholderText(QStringLiteral("选择附件路径"));
            layout->addWidget(m_lineEdit, 1);
        }

        auto *buttonLayout = new QVBoxLayout();
        buttonLayout->setContentsMargins(0, 0, 0, 0);
        buttonLayout->setSpacing(6);

        auto *browseButton = new QPushButton(QStringLiteral("选择"), this);
        auto *clearButton = new QPushButton(QStringLiteral("清空"), this);
        browseButton->setMinimumHeight(36);
        clearButton->setMinimumHeight(36);

        connect(browseButton, &QPushButton::clicked, this, [this]() { browse(); });
        connect(clearButton, &QPushButton::clicked, this, [this]() { setValue(QString()); });

        buttonLayout->addWidget(browseButton);
        buttonLayout->addWidget(clearButton);
        buttonLayout->addStretch();
        layout->addLayout(buttonLayout);
    }

    QString value() const
    {
        return m_multiple ? m_textEdit->toPlainText().trimmed() : m_lineEdit->text().trimmed();
    }

    void setValue(const QString &value)
    {
        if (m_multiple) {
            m_textEdit->setPlainText(value);
        } else {
            m_lineEdit->setText(value);
        }
    }

private:
    void browse()
    {
        const QString filter = m_filter.isEmpty() ? QStringLiteral("所有文件 (*.*)") : m_filter;
        if (m_multiple) {
            const QStringList files = QFileDialog::getOpenFileNames(this,
                                                                    QStringLiteral("选择附件"),
                                                                    QString(),
                                                                    filter);
            if (!files.isEmpty()) {
                setValue(files.join(QStringLiteral("\n")));
            }
            return;
        }

        const QString filePath = QFileDialog::getOpenFileName(this,
                                                              QStringLiteral("选择附件"),
                                                              QString(),
                                                              filter);
        if (!filePath.isEmpty()) {
            setValue(filePath);
        }
    }

    bool m_multiple = false;
    QString m_filter;
    QLineEdit *m_lineEdit = nullptr;
    QTextEdit *m_textEdit = nullptr;
};
}

RecordDialog::RecordDialog(const QString &title,
                           const QList<FieldDefinition> &fields,
                           QWidget *parent)
    : QDialog(parent),
      m_fields(fields)
{
    setWindowTitle(title);
    resize(620, 680);
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
        formLayout->addRow(displayFieldLabel(field), editor);
    }

    formContainer->setStyleSheet(QStringLiteral(
        "#formContainer { background-color: #fbfdfd; border: 1px solid #dde8e9; border-radius: 22px; }"));
    formContainer->setLayout(formLayout);
    scrollArea->setWidget(formContainer);
    scrollArea->setWidgetResizable(true);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok)) {
        okButton->setProperty("variant", QStringLiteral("primary"));
    }
    if (QPushButton *cancelButton = buttonBox->button(QDialogButtonBox::Cancel)) {
        cancelButton->setProperty("variant", QStringLiteral("subtle"));
    }
    connect(buttonBox, &QDialogButtonBox::accepted, this, &RecordDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &RecordDialog::reject);

    rootLayout->addWidget(headerLabel);
    rootLayout->addWidget(scrollArea, 1);
    rootLayout->addWidget(buttonBox);
}

void RecordDialog::setRecordData(const QVariantMap &record)
{
    m_originalRecord = record;
    for (const FieldDefinition &field : m_fields) {
        if (QWidget *editor = m_editors.value(field.key, nullptr)) {
            setEditorValue(field, editor, record.value(field.key));
        }
    }
}

QVariantMap RecordDialog::recordData() const
{
    QVariantMap result = m_originalRecord;
    for (const FieldDefinition &field : m_fields) {
        if (QWidget *editor = m_editors.value(field.key, nullptr)) {
            result.insert(field.key, editorValue(field, editor));
        }
    }
    return result;
}

void RecordDialog::accept()
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

QWidget *RecordDialog::createEditor(const FieldDefinition &field)
{
    switch (field.type) {
    case FieldType::Integer: {
        auto *editor = new QSpinBox(this);
        editor->setRange(0, 1000000000);
        return editor;
    }
    case FieldType::Double: {
        auto *editor = new QDoubleSpinBox(this);
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
    case FieldType::Boolean: {
        auto *editor = new QCheckBox(QStringLiteral("是"), this);
        return editor;
    }
    case FieldType::File:
        return new AttachmentEditor(false, field.options.value(0), this);
    case FieldType::Files:
        return new AttachmentEditor(true, field.options.value(0), this);
    case FieldType::Text:
    default:
        return new QLineEdit(this);
    }
}

QVariant RecordDialog::editorValue(const FieldDefinition &field, QWidget *editor) const
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
    case FieldType::Boolean:
        return static_cast<QCheckBox *>(editor)->isChecked();
    case FieldType::File:
    case FieldType::Files:
        return static_cast<AttachmentEditor *>(editor)->value();
    case FieldType::Text:
    default:
        return static_cast<QLineEdit *>(editor)->text();
    }
}

void RecordDialog::setEditorValue(const FieldDefinition &field, QWidget *editor, const QVariant &value)
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
    case FieldType::Boolean:
        static_cast<QCheckBox *>(editor)->setChecked(variantBoolValue(value));
        break;
    case FieldType::File:
    case FieldType::Files:
        static_cast<AttachmentEditor *>(editor)->setValue(value.toString());
        break;
    case FieldType::Text:
    default:
        static_cast<QLineEdit *>(editor)->setText(value.toString());
        break;
    }
}