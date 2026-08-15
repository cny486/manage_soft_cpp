#include "accountsecuritydialog.h"

#include "appservice.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace {
QWidget *createCard(const QString &title, QWidget *parent, QVBoxLayout **contentLayout)
{
    auto *card = new QWidget(parent);
    card->setObjectName(QStringLiteral("securityCard"));
    card->setStyleSheet(QStringLiteral(
        "#securityCard { background-color: #fbfdfd; border: 1px solid #dde8e9; border-radius: 22px; }"));

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto *titleLabel = new QLabel(title, card);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: 800; color: #173137; border: none;"));
    layout->addWidget(titleLabel);

    auto *bodyLayout = new QVBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(10);
    layout->addLayout(bodyLayout);

    if (contentLayout != nullptr) {
        *contentLayout = bodyLayout;
    }
    return card;
}
}

AccountSecurityDialog::AccountSecurityDialog(AppService *service,
                                             QWidget *parent)
    : QDialog(parent),
      m_service(service)
{
    setWindowTitle(QStringLiteral("账户安全"));
    resize(560, 620);
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #f4f7f8; }"
        "QLabel { color: #284048; }"
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
        "QPushButton[variant='primary']:pressed { background-color: #486e6b; }"));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(18, 18, 18, 18);
    rootLayout->setSpacing(14);

    auto *titleLabel = new QLabel(QStringLiteral("账户安全设置"), this);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 24px; font-weight: 800; color: #173137;"));
    rootLayout->addWidget(titleLabel);

    QVBoxLayout *overviewLayout = nullptr;
    QWidget *overviewCard = createCard(QStringLiteral("当前账户"), this, &overviewLayout);
    auto *overviewForm = new QFormLayout();
    overviewForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_userNameValueLabel = new QLabel(overviewCard);
    m_emailValueLabel = new QLabel(overviewCard);
    m_emailValueLabel->setWordWrap(true);
    overviewForm->addRow(QStringLiteral("用户名"), m_userNameValueLabel);
    overviewForm->addRow(QStringLiteral("绑定邮箱"), m_emailValueLabel);
    overviewLayout->addLayout(overviewForm);
    rootLayout->addWidget(overviewCard);

    QVBoxLayout *bindLayout = nullptr;
    QWidget *bindCard = createCard(QStringLiteral("绑定邮箱"), this, &bindLayout);
    m_bindHintLabel = new QLabel(bindCard);
    m_bindHintLabel->setWordWrap(true);
    auto *bindForm = new QFormLayout();
    bindForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_bindEmailEdit = new QLineEdit(bindCard);
    m_bindEmailEdit->setPlaceholderText(QStringLiteral("请输入要绑定的邮箱"));
    m_bindCodeEdit = new QLineEdit(bindCard);
    m_bindCodeEdit->setPlaceholderText(QStringLiteral("请输入邮箱验证码"));
    bindForm->addRow(QStringLiteral("邮箱地址"), m_bindEmailEdit);
    bindForm->addRow(QStringLiteral("验证码"), m_bindCodeEdit);
    auto *bindButtonLayout = new QHBoxLayout();
    bindButtonLayout->addStretch();
    m_sendBindCodeButton = new QPushButton(QStringLiteral("获取验证码"), bindCard);
    m_confirmBindButton = new QPushButton(QStringLiteral("确认绑定"), bindCard);
    m_sendBindCodeButton->setProperty("variant", QStringLiteral("subtle"));
    m_confirmBindButton->setProperty("variant", QStringLiteral("primary"));
    bindButtonLayout->addWidget(m_sendBindCodeButton);
    bindButtonLayout->addWidget(m_confirmBindButton);
    bindLayout->addWidget(m_bindHintLabel);
    bindLayout->addLayout(bindForm);
    bindLayout->addLayout(bindButtonLayout);
    rootLayout->addWidget(bindCard);

    QVBoxLayout *passwordLayout = nullptr;
    QWidget *passwordCard = createCard(QStringLiteral("修改密码"), this, &passwordLayout);
    m_passwordHintLabel = new QLabel(passwordCard);
    m_passwordHintLabel->setWordWrap(true);
    auto *passwordForm = new QFormLayout();
    passwordForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_newPasswordEdit = new QLineEdit(passwordCard);
    m_newPasswordEdit->setEchoMode(QLineEdit::Password);
    m_newPasswordEdit->setPlaceholderText(QStringLiteral("请输入新密码，至少 8 位"));
    m_confirmPasswordEdit = new QLineEdit(passwordCard);
    m_confirmPasswordEdit->setEchoMode(QLineEdit::Password);
    m_confirmPasswordEdit->setPlaceholderText(QStringLiteral("请再次输入新密码"));
    m_passwordCodeEdit = new QLineEdit(passwordCard);
    m_passwordCodeEdit->setPlaceholderText(QStringLiteral("请输入邮箱验证码"));
    passwordForm->addRow(QStringLiteral("新密码"), m_newPasswordEdit);
    passwordForm->addRow(QStringLiteral("确认密码"), m_confirmPasswordEdit);
    passwordForm->addRow(QStringLiteral("验证码"), m_passwordCodeEdit);
    auto *passwordButtonLayout = new QHBoxLayout();
    passwordButtonLayout->addStretch();
    m_sendPasswordCodeButton = new QPushButton(QStringLiteral("获取验证码"), passwordCard);
    m_confirmPasswordButton = new QPushButton(QStringLiteral("确认修改"), passwordCard);
    m_sendPasswordCodeButton->setProperty("variant", QStringLiteral("subtle"));
    m_confirmPasswordButton->setProperty("variant", QStringLiteral("primary"));
    passwordButtonLayout->addWidget(m_sendPasswordCodeButton);
    passwordButtonLayout->addWidget(m_confirmPasswordButton);
    passwordLayout->addWidget(m_passwordHintLabel);
    passwordLayout->addLayout(passwordForm);
    passwordLayout->addLayout(passwordButtonLayout);
    rootLayout->addWidget(passwordCard);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    rootLayout->addWidget(buttonBox);

    connect(m_sendBindCodeButton, &QPushButton::clicked, this, [this]() { sendBindEmailCode(); });
    connect(m_confirmBindButton, &QPushButton::clicked, this, [this]() { confirmBindEmail(); });
    connect(m_sendPasswordCodeButton, &QPushButton::clicked, this, [this]() { sendPasswordChangeCode(); });
    connect(m_confirmPasswordButton, &QPushButton::clicked, this, [this]() { confirmPasswordChange(); });

    refreshState();
}

void AccountSecurityDialog::refreshState()
{
    if (m_service == nullptr) {
        return;
    }

    UserSecurityInfo info;
    QString errorMessage;
    if (!m_service->loadCurrentUserSecurityInfo(&info, &errorMessage)) {
        QMessageBox::critical(this,
                              QStringLiteral("加载失败"),
                              errorMessage.trimmed().isEmpty() ? QStringLiteral("无法读取当前账户信息。") : errorMessage);
        reject();
        return;
    }

    m_boundEmail = info.email.trimmed();
    m_userNameValueLabel->setText(info.userName);
    m_emailValueLabel->setText(m_boundEmail.isEmpty() ? QStringLiteral("未绑定") : m_boundEmail);

    const bool emailBound = !m_boundEmail.isEmpty();
    m_bindHintLabel->setText(emailBound
                                 ? QStringLiteral("当前账号已绑定邮箱。第二版暂不支持自行更换绑定邮箱。")
                                 : QStringLiteral("输入邮箱后获取验证码，校验成功即可完成绑定。1 分钟最多发送 3 次验证码。"));
    m_bindEmailEdit->setEnabled(!emailBound);
    m_bindCodeEdit->setEnabled(!emailBound);
    m_sendBindCodeButton->setEnabled(!emailBound);
    m_confirmBindButton->setEnabled(!emailBound);

    m_passwordHintLabel->setText(emailBound
                                     ? QStringLiteral("验证码将发送到已绑定邮箱：%1").arg(m_boundEmail)
                                     : QStringLiteral("当前账号尚未绑定邮箱，请先完成邮箱绑定后再修改密码。"));
    m_newPasswordEdit->setEnabled(emailBound);
    m_confirmPasswordEdit->setEnabled(emailBound);
    m_passwordCodeEdit->setEnabled(emailBound);
    m_sendPasswordCodeButton->setEnabled(emailBound);
    m_confirmPasswordButton->setEnabled(emailBound);
}

void AccountSecurityDialog::sendBindEmailCode()
{
    QString errorMessage;
    if (!m_service->sendCurrentUserVerificationCode(VerificationPurpose::BindEmail,
                                                    m_bindEmailEdit->text(),
                                                    &errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("发送失败"), errorMessage);
        return;
    }

    QMessageBox::information(this, QStringLiteral("发送成功"), QStringLiteral("验证码已发送，请查收邮箱。"));
}

void AccountSecurityDialog::confirmBindEmail()
{
    QString errorMessage;
    if (!m_service->bindCurrentUserEmail(m_bindEmailEdit->text(), m_bindCodeEdit->text(), &errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("绑定失败"), errorMessage);
        return;
    }

    QMessageBox::information(this, QStringLiteral("绑定成功"), QStringLiteral("邮箱已成功绑定。"));
    m_bindCodeEdit->clear();
    refreshState();
}

void AccountSecurityDialog::sendPasswordChangeCode()
{
    QString validationError;
    if (!validateNewPassword(&validationError)) {
        QMessageBox::warning(this, QStringLiteral("参数不完整"), validationError);
        return;
    }

    QString errorMessage;
    if (!m_service->sendCurrentUserVerificationCode(VerificationPurpose::ChangePassword,
                                                    QString(),
                                                    &errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("发送失败"), errorMessage);
        return;
    }

    QMessageBox::information(this, QStringLiteral("发送成功"), QStringLiteral("验证码已发送到绑定邮箱，请查收。"));
}

void AccountSecurityDialog::confirmPasswordChange()
{
    QString validationError;
    if (!validateNewPassword(&validationError)) {
        QMessageBox::warning(this, QStringLiteral("参数不完整"), validationError);
        return;
    }

    QString errorMessage;
    if (!m_service->changeCurrentUserPassword(m_newPasswordEdit->text(),
                                              m_passwordCodeEdit->text(),
                                              &errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("修改失败"), errorMessage);
        return;
    }

    QMessageBox::information(this, QStringLiteral("修改成功"), QStringLiteral("密码已成功修改，请使用新密码登录。"));
    m_newPasswordEdit->clear();
    m_confirmPasswordEdit->clear();
    m_passwordCodeEdit->clear();
}

bool AccountSecurityDialog::validateNewPassword(QString *errorMessage) const
{
    const QString newPassword = m_newPasswordEdit->text();
    const QString confirmPassword = m_confirmPasswordEdit->text();
    if (newPassword.size() < 8) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("新密码至少需要 8 位。");
        }
        return false;
    }

    if (newPassword != confirmPassword) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("两次输入的新密码不一致。");
        }
        return false;
    }
    return true;
}