#pragma once

#include <QList>
#include <QString>
#include <QStringList>

class QVariant;

struct XlsxSheetData {
    QStringList headers;
    QList<QStringList> rows;
};

class SimpleXlsxDocument {
public:
    static bool readSheet(const QString &filePath, XlsxSheetData *sheetData, QString *errorMessage = nullptr);
    static bool writeSheet(const QString &filePath,
                           const QString &sheetName,
                           const QStringList &headers,
                           const QList<QList<QVariant>> &rows,
                           QString *errorMessage = nullptr);
};