#pragma once

#include <functional>

#include <QDialog>

class AppService;
class QLabel;
class QLineEdit;
class QWidget;

class LoginDialog : public QDialog {
public:
    LoginDialog(std::function<AppService *()> serviceProvider,
                std::function<QString()> targetDescriptionProvider,
                std::function<bool(QWidget *)> openConnectionSettings,
                QWidget *parent = nullptr);

protected:
    void accept() override;

private:
    void refreshTargetDescription();

    std::function<AppService *()> m_serviceProvider;
    std::function<QString()> m_targetDescriptionProvider;
    std::function<bool(QWidget *)> m_openConnectionSettings;
    QLabel *m_targetLabel = nullptr;
    QLineEdit *m_userNameEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
};