#pragma once

#include <QDialog>

class AppService;
class QLabel;
class QLineEdit;
class QPushButton;

class AccountSecurityDialog : public QDialog {
public:
    explicit AccountSecurityDialog(AppService *service,
                                   QWidget *parent = nullptr);

private:
    void refreshState();
    void sendBindEmailCode();
    void confirmBindEmail();
    void sendPasswordChangeCode();
    void confirmPasswordChange();
    bool validateNewPassword(QString *errorMessage) const;

    AppService *m_service = nullptr;
    QLabel *m_userNameValueLabel = nullptr;
    QLabel *m_emailValueLabel = nullptr;
    QLabel *m_bindHintLabel = nullptr;
    QLabel *m_passwordHintLabel = nullptr;
    QLineEdit *m_bindEmailEdit = nullptr;
    QLineEdit *m_bindCodeEdit = nullptr;
    QPushButton *m_sendBindCodeButton = nullptr;
    QPushButton *m_confirmBindButton = nullptr;
    QLineEdit *m_newPasswordEdit = nullptr;
    QLineEdit *m_confirmPasswordEdit = nullptr;
    QLineEdit *m_passwordCodeEdit = nullptr;
    QPushButton *m_sendPasswordCodeButton = nullptr;
    QPushButton *m_confirmPasswordButton = nullptr;
    QString m_boundEmail;
};