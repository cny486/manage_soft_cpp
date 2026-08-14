#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

struct XlsxSheetData {
    QStringList headers;
    QList<QStringList> rows;
};

struct XlsxCellFormat {
    QString fillColor;
    QString fontColor;
    bool bold = false;
};

struct XlsxWorkbookSheet {
    QString name;
    QStringList headers;
    QList<QList<QVariant>> rows;
    QHash<QString, XlsxCellFormat> cellFormats;
};

class SimpleXlsxDocument {
public:
    static bool readSheet(const QString &filePath, XlsxSheetData *sheetData, QString *errorMessage = nullptr);
    static bool writeSheet(const QString &filePath,
                           const QString &sheetName,
                           const QStringList &headers,
                           const QList<QList<QVariant>> &rows,
                           QString *errorMessage = nullptr);
    static bool writeWorkbook(const QString &filePath,
                              const QList<XlsxWorkbookSheet> &sheets,
                              QString *errorMessage = nullptr);
};
