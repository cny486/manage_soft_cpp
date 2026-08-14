#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>

#include <memory>

#include "aiapisettings.h"
#include "appservice.h"
#include "clientupdatemanager.h"
#include "connectionsettings.h"
#include "connectionsettingsdialog.h"
#include "datainitializer.h"
#include "emailsettings.h"
#include "jsonstorageservice.h"
#include "logindialog.h"
#include "mainwindow.h"
#include "tcpappserviceclient.h"

namespace {
bool sameConnectionSettings(const ConnectionSettings &left, const ConnectionSettings &right)
{
    return left.useLocalStorage == right.useLocalStorage
           && left.serverHost.trimmed().compare(right.serverHost.trimmed(), Qt::CaseInsensitive) == 0
           && left.serverPort == right.serverPort
           && left.timeoutMs == right.timeoutMs;
}

QString statusMessageWithUser(const QString &statusMessage, const AppService *service)
{
    if (service == nullptr || service->currentUserName().trimmed().isEmpty()) {
        return statusMessage;
    }

    return QStringLiteral("%1 | 当前用户：%2").arg(statusMessage, service->currentUserName());
}

bool buildAppService(const ConnectionSettings &settings,
                     std::unique_ptr<AppService> *service,
                     QString *statusMessage,
                     QString *errorMessage,
                     bool verifyTcpConnection = true)
{
    if (service == nullptr || statusMessage == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("内部错误：服务输出参数无效。");
        }
        return false;
    }

    service->reset();
    statusMessage->clear();

    if (settings.useLocalStorage) {
        auto localService = std::make_unique<JsonStorageService>();
        ensureInventorySampleData(localService.get());
        *statusMessage = QStringLiteral("当前使用本地文件存储：%1").arg(localService->storageRoot());
        *service = std::move(localService);
        return true;
    }

    auto tcpService = std::make_unique<TcpAppServiceClient>(settings.serverHost, settings.serverPort, settings.timeoutMs);
    if (verifyTcpConnection) {
        QString pingError;
        if (!tcpService->ping(&pingError)) {
            if (errorMessage != nullptr) {
                QString hint;
                const QString normalizedHost = settings.serverHost.trimmed().toLower();
                if (normalizedHost == QStringLiteral("127.0.0.1")
                    || normalizedHost == QStringLiteral("localhost")
                    || normalizedHost == QStringLiteral("0.0.0.0")) {
                    hint = QStringLiteral("\n\n如果当前是本机联调，请先启动 dist/ManageSoftServer/ManageSoftServer.exe。也可以在连接设置里切换为本地文件存储模式。");
                }
                *errorMessage = QStringLiteral("无法连接后端服务：%1%2").arg(pingError, hint);
            }
            return false;
        }
    }

    *statusMessage = verifyTcpConnection
                         ? QStringLiteral("当前连接服务器：%1").arg(tcpService->storageRoot())
                         : QStringLiteral("当前目标服务器：%1，可在“连接设置”中修改。")
                               .arg(tcpService->storageRoot());
    *service = std::move(tcpService);
    return true;
}

bool applyConnectionSettings(QWidget *parent,
                             std::unique_ptr<AppService> *service,
                             QString *statusMessage,
                             bool verifyTcpConnection,
                             bool showSuccessMessage)
{
    const ConnectionSettings currentSettings = loadConnectionSettings();
    ConnectionSettingsDialog dialog(currentSettings, parent);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const ConnectionSettings newSettings = dialog.settings();
    const AiApiSettings newAiSettings = dialog.aiSettings();
    const EmailSettings newEmailSettings = dialog.emailSettings();

    if (sameConnectionSettings(currentSettings, newSettings)) {
        saveConnectionSettings(newSettings);
        saveAiApiSettings(newAiSettings);
        saveEmailSettings(newEmailSettings);
        if (showSuccessMessage) {
            QMessageBox::information(parent,
                                     QStringLiteral("设置已应用"),
                                     QStringLiteral("AI 设置和邮件设置已保存，当前后端连接保持不变。"));
        }
        return true;
    }

    std::unique_ptr<AppService> replacementService;
    QString replacementStatusMessage;
    QString rebuildError;
    if (!buildAppService(newSettings,
                         &replacementService,
                         &replacementStatusMessage,
                         &rebuildError,
                         verifyTcpConnection)) {
        QMessageBox::critical(parent,
                              QStringLiteral("应用设置失败"),
                              QStringLiteral("无法建立新连接，当前连接和原有配置已保持不变：%1").arg(rebuildError));
        return false;
    }

    saveConnectionSettings(newSettings);
    saveAiApiSettings(newAiSettings);
    saveEmailSettings(newEmailSettings);
    *statusMessage = replacementStatusMessage;
    *service = std::move(replacementService);

    if (showSuccessMessage) {
        QMessageBox::information(parent,
                                 QStringLiteral("设置已应用"),
                                 QStringLiteral("连接设置和 AI 设置已保存，并已立即重建当前连接。"));
    }
    return true;
}

ClientUpdateAction handleClientUpdateAfterLogin(QWidget *parent,
                                                AppService *service)
{
    auto *tcpClient = dynamic_cast<TcpAppServiceClient *>(service);
    if (tcpClient == nullptr) {
        return ClientUpdateAction::Proceed;
    }

    QString updateError;
    return ClientUpdateManager::handlePostLoginUpdateCheck(parent, tcpClient, &updateError);
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("ManageSoftCpp");
    QApplication::setOrganizationName("ManageSoftCpp");
    QApplication::setOrganizationDomain("local.manage.soft");

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("ManageSoftCpp 客户端"));
    parser.addHelpOption();

    QCommandLineOption localStorageOption(QStringList() << QStringLiteral("local-storage"),
                                          QStringLiteral("Use embedded local JSON storage instead of TCP backend."));
    ConnectionSettings settings = loadConnectionSettings();

    QCommandLineOption serverHostOption(QStringList() << QStringLiteral("server-host"),
                                        QStringLiteral("TCP backend host."),
                                        QStringLiteral("host"),
                                        settings.serverHost);
    QCommandLineOption serverPortOption(QStringList() << QStringLiteral("server-port"),
                                        QStringLiteral("TCP backend port."),
                                        QStringLiteral("port"),
                                        QString::number(settings.serverPort));
    QCommandLineOption timeoutOption(QStringList() << QStringLiteral("server-timeout-ms"),
                                     QStringLiteral("TCP request timeout in milliseconds."),
                                     QStringLiteral("timeout"),
                                     QString::number(settings.timeoutMs));

    parser.addOption(localStorageOption);
    parser.addOption(serverHostOption);
    parser.addOption(serverPortOption);
    parser.addOption(timeoutOption);
    parser.process(app);

    if (parser.isSet(localStorageOption)) {
        settings.useLocalStorage = true;
    }
    if (parser.isSet(serverHostOption)) {
        settings.serverHost = parser.value(serverHostOption).trimmed();
    }
    if (parser.isSet(serverPortOption)) {
        bool portOk = false;
        const int portValue = parser.value(serverPortOption).toInt(&portOk);
        if (!portOk || portValue <= 0 || portValue > 65535) {
            QMessageBox::critical(nullptr, QStringLiteral("启动失败"), QStringLiteral("服务器端口参数无效。"));
            return 1;
        }
        settings.serverPort = static_cast<quint16>(portValue);
    }
    if (parser.isSet(timeoutOption)) {
        bool timeoutOk = false;
        const int timeoutValue = parser.value(timeoutOption).toInt(&timeoutOk);
        if (!timeoutOk || timeoutValue <= 0) {
            QMessageBox::critical(nullptr, QStringLiteral("启动失败"), QStringLiteral("请求超时参数无效。"));
            return 1;
        }
        settings.timeoutMs = timeoutValue;
    }

    std::unique_ptr<AppService> initialService;
    QString statusMessage;
    QString startupError;
    if (!buildAppService(settings, &initialService, &statusMessage, &startupError, false)) {
        QMessageBox::critical(nullptr,
                              QStringLiteral("连接服务器失败"),
                              startupError);
        return 1;
    }

    LoginDialog startupLoginDialog([&initialService]() { return initialService.get(); },
                                   [&statusMessage]() { return statusMessage; },
                                   [&initialService, &statusMessage](QWidget *parent) {
                                       return applyConnectionSettings(parent,
                                                                      &initialService,
                                                                      &statusMessage,
                                                                      true,
                                                                      false);
                                   },
                                   nullptr);
    if (startupLoginDialog.exec() != QDialog::Accepted) {
        return 0;
    }

    const ClientUpdateAction startupUpdateAction = handleClientUpdateAfterLogin(nullptr, initialService.get());
    if (startupUpdateAction != ClientUpdateAction::Proceed) {
        return 0;
    }

    statusMessage = statusMessageWithUser(statusMessage, initialService.get());

    std::function<void(QWidget *)> openConnectionSettings;
    openConnectionSettings = [&openConnectionSettings](QWidget *parent) {
        const ConnectionSettings currentSettings = loadConnectionSettings();
        ConnectionSettingsDialog dialog(currentSettings, parent);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }

        const ConnectionSettings newSettings = dialog.settings();
        const AiApiSettings newAiSettings = dialog.aiSettings();
        const EmailSettings newEmailSettings = dialog.emailSettings();

        if (sameConnectionSettings(currentSettings, newSettings)) {
            saveConnectionSettings(newSettings);
            saveAiApiSettings(newAiSettings);
            saveEmailSettings(newEmailSettings);
            QMessageBox::information(parent,
                                     QStringLiteral("设置已应用"),
                                     QStringLiteral("AI 设置和邮件设置已保存，当前后端连接保持不变。"));
            return;
        }

        std::unique_ptr<AppService> replacementService;
        QString replacementStatusMessage;
        QString rebuildError;
        if (!buildAppService(newSettings, &replacementService, &replacementStatusMessage, &rebuildError)) {
            QMessageBox::critical(parent,
                                  QStringLiteral("应用设置失败"),
                                  QStringLiteral("无法建立新连接，当前连接和原有配置已保持不变：%1").arg(rebuildError));
            return;
        }

        LoginDialog loginDialog([&replacementService]() { return replacementService.get(); },
                                [&replacementStatusMessage]() { return replacementStatusMessage; },
                                [&replacementService, &replacementStatusMessage](QWidget *dialogParent) {
                                    return applyConnectionSettings(dialogParent,
                                                                   &replacementService,
                                                                   &replacementStatusMessage,
                                                                   true,
                                                                   false);
                                },
                                parent);
        if (loginDialog.exec() != QDialog::Accepted) {
            return;
        }

        const ClientUpdateAction updateAction = handleClientUpdateAfterLogin(parent, replacementService.get());
        if (updateAction != ClientUpdateAction::Proceed) {
            qApp->quit();
            return;
        }

        replacementStatusMessage = statusMessageWithUser(replacementStatusMessage, replacementService.get());
        saveConnectionSettings(newSettings);
        saveAiApiSettings(newAiSettings);
        saveEmailSettings(newEmailSettings);

        MainWindow *currentWindow = dynamic_cast<MainWindow *>(parent);
        auto *newWindow = new MainWindow(replacementService.release(),
                                         replacementStatusMessage,
                                         openConnectionSettings);

        if (currentWindow != nullptr) {
            newWindow->setGeometry(currentWindow->geometry());
            if (currentWindow->isMaximized()) {
                newWindow->showMaximized();
            } else {
                newWindow->show();
            }
            currentWindow->close();
            currentWindow->deleteLater();
        } else {
            newWindow->show();
        }

        QMessageBox::information(newWindow,
                                 QStringLiteral("设置已应用"),
                                 QStringLiteral("连接设置和 AI 设置已保存，并已立即重建当前连接。"));
    };

    auto *window = new MainWindow(initialService.release(),
                                  statusMessage,
                                  openConnectionSettings);
    window->show();
    return app.exec();
}
