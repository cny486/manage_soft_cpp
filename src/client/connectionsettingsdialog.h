#pragma once

#include "aiapisettings.h"
#include "connectionsettings.h"
#include "emailsettings.h"

#include <QDialog>

class AppService;

class QCheckBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

class ConnectionSettingsDialog : public QDialog {
public:
    explicit ConnectionSettingsDialog(const ConnectionSettings &settings,
                                      AppService *service,
                                      QWidget *parent = nullptr);

    ConnectionSettings settings() const;
    AiApiSettings aiSettings() const;
    EmailSettings emailSettings() const;

private:
    void updateFieldState();
    void testConnection();
    void testAiConnection();

    EmailSettings m_loadedEmailSettings;
    AppService *m_service = nullptr;

    QCheckBox *m_localStorageCheck = nullptr;
    QLineEdit *m_serverHostEdit = nullptr;
    QSpinBox *m_serverPortSpin = nullptr;
    QSpinBox *m_timeoutSpin = nullptr;
    QLineEdit *m_aiApiUrlEdit = nullptr;
    QLineEdit *m_aiApiKeyEdit = nullptr;
    QLineEdit *m_aiModelEdit = nullptr;
    QSpinBox *m_aiTimeoutSpin = nullptr;
    QPushButton *m_testButton = nullptr;
    QPushButton *m_testAiButton = nullptr;
};
