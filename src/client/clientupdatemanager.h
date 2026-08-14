#pragma once

#include <QString>

class QWidget;
class TcpAppServiceClient;

enum class ClientUpdateAction {
    Proceed,
    ExitApplication,
    RelaunchingForUpdate
};

class ClientUpdateManager {
public:
    static ClientUpdateAction handlePostLoginUpdateCheck(QWidget *parent,
                                                         TcpAppServiceClient *client,
                                                         QString *errorMessage = nullptr);
};
