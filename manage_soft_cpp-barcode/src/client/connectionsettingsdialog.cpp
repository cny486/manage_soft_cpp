#include "connectionsettingsdialog.h"

#include "aiapisettings.h"
#include "emailsettings.h"
#include "tcpappserviceclient.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

ConnectionSettingsDialog::ConnectionSettingsDialog(const ConnectionSettings &settings,
                                                   QWidget *parent)
        : QDialog(parent),
            m_loadedEmailSettings(loadEmailSettings())
{
    setWindowTitle(QStringLiteral("连接设置"));
        resize(560, 420);
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #f4f7f8; }"
        "QLabel { color: #284048; }"));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(18, 18, 18, 18);
    rootLayout->setSpacing(12);

    auto *titleLabel = new QLabel(QStringLiteral("配置客户端的后端连接方式"), this);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: 800; color: #173137;"));

    const AiApiSettings aiSettings = loadAiApiSettings();

    m_localStorageCheck = new QCheckBox(QStringLiteral("使用本地文件存储（单机模式）"), this);
    m_localStorageCheck->setChecked(settings.useLocalStorage);

    const QString initialServerHost = settings.serverHost.trimmed().isEmpty()
                                          ? QStringLiteral("127.0.0.1")
                                          : settings.serverHost.trimmed();
    m_serverHostEdit = new QLineEdit(initialServerHost, this);
    m_serverPortSpin = new QSpinBox(this);
    m_serverPortSpin->setRange(1, 65535);
    m_serverPortSpin->setValue(settings.serverPort);
    m_timeoutSpin = new QSpinBox(this);
    m_timeoutSpin->setRange(1000, 60000);
    m_timeoutSpin->setSingleStep(1000);
    m_timeoutSpin->setSuffix(QStringLiteral(" ms"));
    m_timeoutSpin->setValue(settings.timeoutMs);

    auto *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->addRow(QStringLiteral("服务器地址"), m_serverHostEdit);
    formLayout->addRow(QStringLiteral("服务器端口"), m_serverPortSpin);
    formLayout->addRow(QStringLiteral("请求超时"), m_timeoutSpin);

    auto *connectionCard = new QWidget(this);
    connectionCard->setObjectName(QStringLiteral("connectionCard"));
    connectionCard->setStyleSheet(QStringLiteral(
        "#connectionCard { background-color: #fbfdfd; border: 1px solid #dde8e9; border-radius: 22px; }"));
    auto *connectionCardLayout = new QVBoxLayout(connectionCard);
    connectionCardLayout->setContentsMargins(18, 18, 18, 18);
    connectionCardLayout->setSpacing(10);
    auto *connectionTitleLabel = new QLabel(QStringLiteral("连接方式"), connectionCard);
    connectionTitleLabel->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: 800; color: #173137;"));
    connectionCardLayout->addWidget(connectionTitleLabel);
    connectionCardLayout->addWidget(m_localStorageCheck);
    connectionCardLayout->addLayout(formLayout);

    auto *aiTitleLabel = new QLabel(QStringLiteral("AI 补齐设置"), this);
    aiTitleLabel->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: 800; color: #173137;"));

    m_aiApiUrlEdit = new QLineEdit(aiSettings.apiUrl, this);
    m_aiApiKeyEdit = new QLineEdit(aiSettings.apiKey, this);
    m_aiApiKeyEdit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    m_aiModelEdit = new QLineEdit(aiSettings.model, this);
    m_aiTimeoutSpin = new QSpinBox(this);
    m_aiTimeoutSpin->setRange(1000, 120000);
    m_aiTimeoutSpin->setSingleStep(1000);
    m_aiTimeoutSpin->setSuffix(QStringLiteral(" ms"));
    m_aiTimeoutSpin->setValue(aiSettings.timeoutMs);

    auto *aiFormLayout = new QFormLayout();
    aiFormLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    aiFormLayout->addRow(QStringLiteral("API 地址"), m_aiApiUrlEdit);
    aiFormLayout->addRow(QStringLiteral("API Key"), m_aiApiKeyEdit);
    aiFormLayout->addRow(QStringLiteral("模型"), m_aiModelEdit);
    aiFormLayout->addRow(QStringLiteral("AI 超时"), m_aiTimeoutSpin);

    auto *aiCard = new QWidget(this);
    aiCard->setObjectName(QStringLiteral("aiCard"));
    aiCard->setStyleSheet(QStringLiteral(
        "#aiCard { background-color: #fbfdfd; border: 1px solid #dde8e9; border-radius: 22px; }"));
    auto *aiCardLayout = new QVBoxLayout(aiCard);
    aiCardLayout->setContentsMargins(18, 18, 18, 18);
    aiCardLayout->setSpacing(10);
    aiCardLayout->addWidget(aiTitleLabel);
    aiCardLayout->addLayout(aiFormLayout);

    auto *emailCard = new QWidget(this);
    emailCard->setObjectName(QStringLiteral("emailCard"));
    emailCard->setStyleSheet(QStringLiteral(
        "#emailCard { background-color: #fbfdfd; border: 1px solid #dde8e9; border-radius: 22px; }"));
    emailCard->setVisible(false);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    m_testButton = new QPushButton(QStringLiteral("测试连接"), this);
    m_testButton->setProperty("variant", QStringLiteral("subtle"));
    m_testAiButton = new QPushButton(QStringLiteral("测试 AI 连接"), this);
    m_testAiButton->setProperty("variant", QStringLiteral("subtle"));
    buttonLayout->addWidget(m_testButton);
    buttonLayout->addWidget(m_testAiButton);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    if (QPushButton *saveButton = buttonBox->button(QDialogButtonBox::Save)) {
        saveButton->setProperty("variant", QStringLiteral("primary"));
    }
    if (QPushButton *cancelButton = buttonBox->button(QDialogButtonBox::Cancel)) {
        cancelButton->setProperty("variant", QStringLiteral("subtle"));
    }

    connect(m_localStorageCheck, &QCheckBox::toggled, this, [this](bool) { updateFieldState(); });
    connect(m_testButton, &QPushButton::clicked, this, [this]() { testConnection(); });
    connect(m_testAiButton, &QPushButton::clicked, this, [this]() { testAiConnection(); });
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        if (!m_localStorageCheck->isChecked() && m_serverHostEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("参数不完整"), QStringLiteral("请填写服务器地址。"));
            return;
        }
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    rootLayout->addWidget(titleLabel);
    rootLayout->addSpacing(4);
    rootLayout->addWidget(connectionCard);
    rootLayout->addWidget(aiCard);
    rootLayout->addLayout(buttonLayout);
    rootLayout->addWidget(buttonBox);

    updateFieldState();
}

ConnectionSettings ConnectionSettingsDialog::settings() const
{
    ConnectionSettings value;
    value.useLocalStorage = m_localStorageCheck->isChecked();
    value.serverHost = m_serverHostEdit->text().trimmed();
    value.serverPort = static_cast<quint16>(m_serverPortSpin->value());
    value.timeoutMs = m_timeoutSpin->value();
    return value;
}

AiApiSettings ConnectionSettingsDialog::aiSettings() const
{
    AiApiSettings value;
    value.apiUrl = m_aiApiUrlEdit->text().trimmed();
    value.apiKey = m_aiApiKeyEdit->text().trimmed();
    value.model = m_aiModelEdit->text().trimmed();
    value.timeoutMs = m_aiTimeoutSpin->value();
    return value;
}

EmailSettings ConnectionSettingsDialog::emailSettings() const
{
    return m_loadedEmailSettings;
}

void ConnectionSettingsDialog::updateFieldState()
{
    const bool useLocalStorage = m_localStorageCheck->isChecked();
    m_serverHostEdit->setEnabled(!useLocalStorage);
    m_serverPortSpin->setEnabled(!useLocalStorage);
    m_timeoutSpin->setEnabled(!useLocalStorage);
    m_testButton->setEnabled(!useLocalStorage);
    m_testAiButton->setEnabled(!useLocalStorage);
}

void ConnectionSettingsDialog::testConnection()
{
    const QString host = m_serverHostEdit->text().trimmed();
    if (host.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("参数不完整"), QStringLiteral("请先填写服务器地址。"));
        return;
    }

    TcpAppServiceClient client(host,
                               static_cast<quint16>(m_serverPortSpin->value()),
                               m_timeoutSpin->value());
    QString errorMessage;
    if (!client.ping(&errorMessage)) {
        QString hint;
        const QString normalizedHost = host.toLower();
        if (normalizedHost == QStringLiteral("127.0.0.1")
            || normalizedHost == QStringLiteral("localhost")
            || normalizedHost == QStringLiteral("0.0.0.0")) {
            hint = QStringLiteral("\n\n如果你在本机联调，请先启动 dist/ManageSoftServer/ManageSoftServer.exe，再测试连接。");
        }
        QMessageBox::critical(this,
                              QStringLiteral("连接失败"),
                              QStringLiteral("无法连接到后端服务：%1%2").arg(errorMessage, hint));
        return;
    }

    QMessageBox::information(this,
                             QStringLiteral("连接成功"),
                             QStringLiteral("已成功连接到服务器 %1。")
                                 .arg(client.storageRoot()));
}

void ConnectionSettingsDialog::testAiConnection()
{
    if (m_localStorageCheck->isChecked()) {
        QMessageBox::warning(this,
                             QStringLiteral("当前不可测试"),
                             QStringLiteral("AI 测试已改为通过后端发起，请先关闭本地文件存储并填写服务器连接信息。"));
        return;
    }

    const QString host = m_serverHostEdit->text().trimmed();
    if (host.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("参数不完整"), QStringLiteral("请先填写服务器地址。"));
        return;
    }

    const AiApiSettings currentAiSettings = aiSettings();
    if (currentAiSettings.apiUrl.trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("参数不完整"), QStringLiteral("请先填写 AI API 地址。"));
        return;
    }
    if (currentAiSettings.model.trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("参数不完整"), QStringLiteral("请先填写 AI 模型。"));
        return;
    }

    TcpAppServiceClient client(host,
                               static_cast<quint16>(m_serverPortSpin->value()),
                               m_timeoutSpin->value());
    QString responsePreview;
    QString errorMessage;
    if (!client.testAiConnection(currentAiSettings, &responsePreview, &errorMessage)) {
        QMessageBox::critical(this,
                              QStringLiteral("AI 连接失败"),
                              QStringLiteral("后端发起 AI 测试失败：%1").arg(errorMessage));
        return;
    }

    const QString previewText = responsePreview.trimmed().isEmpty()
                                    ? QStringLiteral("接口已成功返回响应。")
                                    : QStringLiteral("接口已成功返回响应：%1").arg(responsePreview.trimmed());
    QMessageBox::information(this,
                             QStringLiteral("AI 连接成功"),
                             previewText);
}