#include "logindialog.h"

#include "appservice.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpacerItem>
#include <QHBoxLayout>
#include <QVBoxLayout>

LoginDialog::LoginDialog(std::function<AppService *()> serviceProvider,
                         std::function<QString()> targetDescriptionProvider,
                         std::function<bool(QWidget *)> openConnectionSettings,
                         QWidget *parent)
    : QDialog(parent),
      m_serviceProvider(std::move(serviceProvider)),
      m_targetDescriptionProvider(std::move(targetDescriptionProvider)),
      m_openConnectionSettings(std::move(openConnectionSettings))
{
    setWindowTitle(QStringLiteral("用户登录"));
    setModal(true);
    setFixedSize(500, 500);
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #f4f7f8; }"
        "QLabel { color: #1d3135; background: transparent; border: none; }"
        "QLineEdit {"
        "  background-color: #fbfdfd;"
        "  color: #1d3135;"
        "  border: 1px solid #d7e3e5;"
        "  border-radius: 14px;"
        "  padding: 10px 12px;"
        "  selection-background-color: #c7dcda;"
        "  selection-color: #163035;"
        "}"
        "QLineEdit:focus { border-color: #6d9894; }"
        "QPushButton {"
        "  background-color: #fbfdfd;"
        "  color: #1d3135;"
        "  border: 1px solid #d7e3e5;"
        "  border-radius: 14px;"
        "  padding: 10px 16px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background-color: #f5fbfb; border-color: #bfd2d4; }"
        "QPushButton:pressed { background-color: #eaf4f3; }"
        "QPushButton[variant='primary'] { background-color: #5f8e8a; color: #ffffff; border: 1px solid #5f8e8a; }"
        "QPushButton[variant='primary']:hover { background-color: #547f7b; border-color: #547f7b; }"
        "QPushButton[variant='primary']:pressed { background-color: #486e6b; }"
        "QPushButton[variant='subtle'] { background-color: #f7fbfb; color: #557075; border: 1px solid #dce7e8; }"
        "QPushButton[variant='subtle']:hover { background-color: #eef6f6; border-color: #c8d9db; }"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 22, 22, 22);
    layout->setSpacing(18);

    auto *titleCard = new QFrame(this);
    titleCard->setObjectName(QStringLiteral("titleCard"));
    titleCard->setStyleSheet(QStringLiteral(
        "#titleCard { background-color: #fbfdfd; border: 1px solid #dde8e9; border-radius: 28px; }"));
    auto *titleLayout = new QVBoxLayout(titleCard);
    titleLayout->setContentsMargins(24, 18, 24, 18);
    titleLayout->setSpacing(0);

    auto *titleLabel = new QLabel(QStringLiteral("登录管理系统"), titleCard);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 28px; font-weight: 800; color: #173137;"));
    titleLayout->addWidget(titleLabel);

    auto *formCard = new QFrame(this);
    formCard->setObjectName(QStringLiteral("formCard"));
    formCard->setStyleSheet(QStringLiteral(
        "#formCard { background-color: #fbfdfd; border: 1px solid #dde8e9; border-radius: 28px; }"));
    auto *formCardLayout = new QVBoxLayout(formCard);
    formCardLayout->setContentsMargins(24, 22, 24, 22);
    formCardLayout->setSpacing(16);

    auto *formTitleLabel = new QLabel(QStringLiteral("账户验证"), formCard);
    formTitleLabel->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: 800; color: #173137;"));

    m_targetLabel = new QLabel(formCard);
    m_targetLabel->setWordWrap(true);
    m_targetLabel->setStyleSheet(QStringLiteral(
        "background-color: #f2f8f8; color: #587277; border: 1px solid #d8e6e7; border-radius: 16px; padding: 12px 14px;"));

    auto *formLayout = new QFormLayout();
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setHorizontalSpacing(16);
    formLayout->setVerticalSpacing(14);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_userNameEdit = new QLineEdit(formCard);
    m_userNameEdit->setPlaceholderText(QStringLiteral("请输入用户名"));
    m_userNameEdit->setMinimumHeight(44);
    m_userNameEdit->setMinimumWidth(320);

    m_passwordEdit = new QLineEdit(formCard);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(QStringLiteral("请输入密码"));
    m_passwordEdit->setMinimumHeight(44);
    m_passwordEdit->setMinimumWidth(320);

    auto *userNameLabel = new QLabel(QStringLiteral("用户名"), formCard);
    userNameLabel->setStyleSheet(QStringLiteral("font-weight: 700; color: #587277;"));
    userNameLabel->setFixedWidth(56);
    auto *passwordLabel = new QLabel(QStringLiteral("密码"), formCard);
    passwordLabel->setStyleSheet(QStringLiteral("font-weight: 700; color: #587277;"));
    passwordLabel->setFixedWidth(56);

    formLayout->addRow(userNameLabel, m_userNameEdit);
    formLayout->addRow(passwordLabel, m_passwordEdit);

    auto *connectionSettingsButton = new QPushButton(QStringLiteral("连接设置"), formCard);
    connectionSettingsButton->setProperty("variant", QStringLiteral("subtle"));
    connectionSettingsButton->setMinimumHeight(42);

    connect(connectionSettingsButton, &QPushButton::clicked, this, [this]() {
        if (m_openConnectionSettings && m_openConnectionSettings(this)) {
            refreshTargetDescription();
            m_passwordEdit->clear();
            m_passwordEdit->setFocus();
        }
    });

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, formCard);
    if (QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok)) {
        okButton->setText(QStringLiteral("登录"));
        okButton->setProperty("variant", QStringLiteral("primary"));
        okButton->setMinimumHeight(42);
    }
    if (QPushButton *cancelButton = buttonBox->button(QDialogButtonBox::Cancel)) {
        cancelButton->setText(QStringLiteral("退出"));
        cancelButton->setProperty("variant", QStringLiteral("subtle"));
        cancelButton->setMinimumHeight(42);
    }

    connect(buttonBox, &QDialogButtonBox::accepted, this, &LoginDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &LoginDialog::reject);

    auto *actionsLayout = new QHBoxLayout();
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->setSpacing(12);
    actionsLayout->addWidget(connectionSettingsButton);
    actionsLayout->addStretch();

    formCardLayout->addWidget(formTitleLabel);
    formCardLayout->addWidget(m_targetLabel);
    formCardLayout->addLayout(formLayout);
    formCardLayout->addLayout(actionsLayout);
    formCardLayout->addWidget(buttonBox);

    layout->addWidget(titleCard);
    layout->addWidget(formCard);

    refreshTargetDescription();
}

void LoginDialog::refreshTargetDescription()
{
    if (m_targetLabel == nullptr) {
        return;
    }

    m_targetLabel->setText(m_targetDescriptionProvider ? m_targetDescriptionProvider() : QString());
}

void LoginDialog::accept()
{
    AppService *service = m_serviceProvider ? m_serviceProvider() : nullptr;
    if (service == nullptr) {
        QMessageBox::critical(this, QStringLiteral("登录失败"), QStringLiteral("内部错误：服务未初始化。"));
        return;
    }

    QString errorMessage;
    switch (service->authenticate(m_userNameEdit->text(), m_passwordEdit->text(), &errorMessage)) {
    case AuthenticationStatus::Success:
        QDialog::accept();
        return;
    case AuthenticationStatus::UserNotFound:
        QMessageBox::warning(this, QStringLiteral("登录失败"), QStringLiteral("用户不存在。"));
        m_userNameEdit->setFocus();
        m_userNameEdit->selectAll();
        return;
    case AuthenticationStatus::WrongPassword:
        QMessageBox::warning(this, QStringLiteral("登录失败"), QStringLiteral("密码错误。"));
        m_passwordEdit->setFocus();
        m_passwordEdit->selectAll();
        return;
    case AuthenticationStatus::Failed:
    default:
        QMessageBox::critical(this,
                              QStringLiteral("登录失败"),
                              errorMessage.trimmed().isEmpty()
                                  ? QStringLiteral("登录请求失败，请稍后重试。")
                                  : errorMessage);
        return;
    }
}