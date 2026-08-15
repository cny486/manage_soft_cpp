#pragma once

#include <QString>

struct AiApiSettings {
    QString apiUrl;
    QString apiKey;
    QString model;
    int timeoutMs = 30000;
};

AiApiSettings loadAiApiSettings();
void saveAiApiSettings(const AiApiSettings &settings);