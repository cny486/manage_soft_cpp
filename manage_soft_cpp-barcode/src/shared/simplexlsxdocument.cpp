#include "simplexlsxdocument.h"

#include <QByteArray>
#include <QFile>
#include <QHash>
#include <QList>
#include <QMap>
#include <QStringList>
#include <QVariant>
#include <QXmlStreamReader>
#include <QtEndian>

#include <zlib.h>

namespace {
struct ZipEntryInfo {
    QString name;
    quint16 method = 0;
    quint32 compressedSize = 0;
    quint32 uncompressedSize = 0;
    quint32 localHeaderOffset = 0;
};

QString xmlEscaped(QString text)
{
    text.replace('&', QStringLiteral("&amp;"));
    text.replace('<', QStringLiteral("&lt;"));
    text.replace('>', QStringLiteral("&gt;"));
    text.replace('"', QStringLiteral("&quot;"));
    return text;
}

quint16 readUInt16(const QByteArray &buffer, int offset)
{
    if (offset + 2 > buffer.size()) {
        return 0;
    }
    return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(buffer.constData() + offset));
}

quint32 readUInt32(const QByteArray &buffer, int offset)
{
    if (offset + 4 > buffer.size()) {
        return 0;
    }
    return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(buffer.constData() + offset));
}

void appendUInt16(QByteArray &buffer, quint16 value)
{
    char raw[2];
    qToLittleEndian<quint16>(value, reinterpret_cast<uchar *>(raw));
    buffer.append(raw, 2);
}

void appendUInt32(QByteArray &buffer, quint32 value)
{
    char raw[4];
    qToLittleEndian<quint32>(value, reinterpret_cast<uchar *>(raw));
    buffer.append(raw, 4);
}

QString columnName(int column)
{
    QString name;
    int current = column;
    while (current > 0) {
        const int remainder = (current - 1) % 26;
        name.prepend(QChar('A' + remainder));
        current = (current - 1) / 26;
    }
    return name;
}

int columnIndexFromReference(const QString &reference)
{
    int column = 0;
    for (const QChar ch : reference) {
        if (!ch.isLetter()) {
            break;
        }
        column = column * 26 + (ch.toUpper().unicode() - 'A' + 1);
    }
    return column;
}

QString readInlineString(QXmlStreamReader &xml)
{
    QString value;
    while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == QStringLiteral("is"))) {
        xml.readNext();
        if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("t")) {
            value += xml.readElementText();
        }
    }
    return value;
}

QStringList parseSharedStrings(const QByteArray &xmlContent, QString *errorMessage)
{
    QStringList sharedStrings;
    QXmlStreamReader xml(xmlContent);
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("si")) {
            QString combined;
            while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == QStringLiteral("si"))) {
                xml.readNext();
                if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("t")) {
                    combined += xml.readElementText();
                }
            }
            sharedStrings.append(combined);
        }
    }

    if (xml.hasError()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("无法解析共享字符串：%1").arg(xml.errorString());
        }
        return {};
    }

    return sharedStrings;
}

bool inflateRawDeflate(const QByteArray &compressed, quint32 expectedSize, QByteArray *output, QString *errorMessage)
{
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(compressed.constData()));
    stream.avail_in = static_cast<uInt>(compressed.size());

    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("无法初始化 ZIP 解压器。");
        }
        return false;
    }

    QByteArray result;
    result.resize(static_cast<int>(expectedSize > 0 ? expectedSize : compressed.size() * 4 + 1024));
    int status = Z_OK;

    do {
        if (stream.total_out >= static_cast<uLong>(result.size())) {
            result.resize(result.size() * 2);
        }

        stream.next_out = reinterpret_cast<Bytef *>(result.data() + stream.total_out);
        stream.avail_out = static_cast<uInt>(result.size() - stream.total_out);
        status = inflate(&stream, Z_NO_FLUSH);
    } while (status == Z_OK);

    inflateEnd(&stream);

    if (status != Z_STREAM_END) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("无法解压 XLSX 数据。");
        }
        return false;
    }

    result.resize(static_cast<int>(stream.total_out));
    *output = result;
    return true;
}

bool readZipArchive(const QString &filePath, QHash<QString, QByteArray> *entries, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("无法打开 Excel 文件：%1").arg(file.errorString());
        }
        return false;
    }

    const QByteArray data = file.readAll();
    const QByteArray eocdSignature("PK\x05\x06", 4);
    const int eocdOffset = data.lastIndexOf(eocdSignature);
    if (eocdOffset < 0 || eocdOffset + 22 > data.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Excel 文件格式无效。");
        }
        return false;
    }

    const quint32 centralDirectoryOffset = readUInt32(data, eocdOffset + 16);
    const quint16 entryCount = readUInt16(data, eocdOffset + 10);
    int cursor = static_cast<int>(centralDirectoryOffset);

    for (quint16 index = 0; index < entryCount; ++index) {
        if (cursor + 46 > data.size() || readUInt32(data, cursor) != 0x02014b50) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Excel 文件目录结构无效。");
            }
            return false;
        }

        const quint16 fileNameLength = readUInt16(data, cursor + 28);
        const quint16 extraFieldLength = readUInt16(data, cursor + 30);
        const quint16 fileCommentLength = readUInt16(data, cursor + 32);
        ZipEntryInfo info;
        info.method = readUInt16(data, cursor + 10);
        info.compressedSize = readUInt32(data, cursor + 20);
        info.uncompressedSize = readUInt32(data, cursor + 24);
        info.localHeaderOffset = readUInt32(data, cursor + 42);
        info.name = QString::fromUtf8(data.mid(cursor + 46, fileNameLength));
        cursor += 46 + fileNameLength + extraFieldLength + fileCommentLength;

        if (info.name.endsWith('/')) {
            continue;
        }

        const int localHeaderOffset = static_cast<int>(info.localHeaderOffset);
        if (localHeaderOffset + 30 > data.size() || readUInt32(data, localHeaderOffset) != 0x04034b50) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Excel 文件条目头无效：%1").arg(info.name);
            }
            return false;
        }

        const quint16 localNameLength = readUInt16(data, localHeaderOffset + 26);
        const quint16 localExtraLength = readUInt16(data, localHeaderOffset + 28);
        const int payloadOffset = localHeaderOffset + 30 + localNameLength + localExtraLength;
        const QByteArray compressed = data.mid(payloadOffset, static_cast<int>(info.compressedSize));

        QByteArray uncompressed;
        if (info.method == 0) {
            uncompressed = compressed;
        } else if (info.method == 8) {
            if (!inflateRawDeflate(compressed, info.uncompressedSize, &uncompressed, errorMessage)) {
                return false;
            }
        } else {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("暂不支持的 Excel 压缩方式：%1").arg(info.method);
            }
            return false;
        }

        entries->insert(info.name, uncompressed);
    }

    return true;
}

bool writeZipArchive(const QString &filePath,
                     const QList<QPair<QString, QByteArray>> &entries,
                     QString *errorMessage)
{
    QByteArray archive;
    QByteArray centralDirectory;
    QList<quint32> localOffsets;
    localOffsets.reserve(entries.size());

    for (const auto &entry : entries) {
        const QByteArray name = entry.first.toUtf8();
        const QByteArray &payload = entry.second;
        const quint32 crc = crc32(0L,
                                  reinterpret_cast<const Bytef *>(payload.constData()),
                                  static_cast<uInt>(payload.size()));
        localOffsets.append(static_cast<quint32>(archive.size()));

        appendUInt32(archive, 0x04034b50);
        appendUInt16(archive, 20);
        appendUInt16(archive, 0);
        appendUInt16(archive, 0);
        appendUInt16(archive, 0);
        appendUInt16(archive, 0);
        appendUInt32(archive, crc);
        appendUInt32(archive, static_cast<quint32>(payload.size()));
        appendUInt32(archive, static_cast<quint32>(payload.size()));
        appendUInt16(archive, static_cast<quint16>(name.size()));
        appendUInt16(archive, 0);
        archive.append(name);
        archive.append(payload);
    }

    const quint32 centralDirectoryOffset = static_cast<quint32>(archive.size());

    for (int index = 0; index < entries.size(); ++index) {
        const QByteArray name = entries.at(index).first.toUtf8();
        const QByteArray &payload = entries.at(index).second;
        const quint32 crc = crc32(0L,
                                  reinterpret_cast<const Bytef *>(payload.constData()),
                                  static_cast<uInt>(payload.size()));

        appendUInt32(centralDirectory, 0x02014b50);
        appendUInt16(centralDirectory, 20);
        appendUInt16(centralDirectory, 20);
        appendUInt16(centralDirectory, 0);
        appendUInt16(centralDirectory, 0);
        appendUInt16(centralDirectory, 0);
        appendUInt16(centralDirectory, 0);
        appendUInt32(centralDirectory, crc);
        appendUInt32(centralDirectory, static_cast<quint32>(payload.size()));
        appendUInt32(centralDirectory, static_cast<quint32>(payload.size()));
        appendUInt16(centralDirectory, static_cast<quint16>(name.size()));
        appendUInt16(centralDirectory, 0);
        appendUInt16(centralDirectory, 0);
        appendUInt16(centralDirectory, 0);
        appendUInt16(centralDirectory, 0);
        appendUInt32(centralDirectory, 0);
        appendUInt32(centralDirectory, localOffsets.at(index));
        centralDirectory.append(name);
    }

    archive.append(centralDirectory);
    appendUInt32(archive, 0x06054b50);
    appendUInt16(archive, 0);
    appendUInt16(archive, 0);
    appendUInt16(archive, static_cast<quint16>(entries.size()));
    appendUInt16(archive, static_cast<quint16>(entries.size()));
    appendUInt32(archive, static_cast<quint32>(centralDirectory.size()));
    appendUInt32(archive, centralDirectoryOffset);
    appendUInt16(archive, 0);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("无法写入 Excel 文件：%1").arg(file.errorString());
        }
        return false;
    }

    file.write(archive);
    return true;
}

QString sheetXml(const QStringList &headers, const QList<QList<QVariant>> &rows)
{
    QString xml;
    xml += QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>");
    xml += QStringLiteral("<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData>");

    xml += QStringLiteral("<row r=\"1\">");
    for (int column = 0; column < headers.size(); ++column) {
        xml += QStringLiteral("<c r=\"") + columnName(column + 1) + QStringLiteral("1\" t=\"inlineStr\"><is><t>")
               + xmlEscaped(headers.at(column)) + QStringLiteral("</t></is></c>");
    }
    xml += QStringLiteral("</row>");

    for (int rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        const QList<QVariant> &row = rows.at(rowIndex);
        const int excelRow = rowIndex + 2;
        xml += QStringLiteral("<row r=\"") + QString::number(excelRow) + QStringLiteral("\">");
        for (int column = 0; column < headers.size(); ++column) {
            const QVariant value = column < row.size() ? row.at(column) : QVariant();
            const QString cellRef = columnName(column + 1) + QString::number(excelRow);
            if (!value.isValid() || value.toString().isEmpty()) {
                continue;
            }

            const QVariant::Type type = value.type();
            if (type == QVariant::Int || type == QVariant::LongLong || type == QVariant::UInt
                || type == QVariant::ULongLong || type == QVariant::Double) {
                xml += QStringLiteral("<c r=\"") + cellRef + QStringLiteral("\"><v>")
                       + xmlEscaped(value.toString()) + QStringLiteral("</v></c>");
            } else {
                xml += QStringLiteral("<c r=\"") + cellRef + QStringLiteral("\" t=\"inlineStr\"><is><t>")
                       + xmlEscaped(value.toString()) + QStringLiteral("</t></is></c>");
            }
        }
        xml += QStringLiteral("</row>");
    }

    xml += QStringLiteral("</sheetData></worksheet>");
    return xml;
}

QString workbookXml(const QString &sheetName)
{
    return QStringLiteral(
               "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
               "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
               "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
               "<sheets><sheet name=\"")
           + xmlEscaped(sheetName)
           + QStringLiteral("\" sheetId=\"1\" r:id=\"rId1\"/></sheets></workbook>");
}

QString relationshipRootXml()
{
    return QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
        "</Relationships>");
}

QString workbookRelationshipsXml()
{
    return QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
        "</Relationships>");
}

QString contentTypesXml()
{
    return QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
        "</Types>");
}

bool parseSheetXml(const QByteArray &xmlContent,
                   const QStringList &sharedStrings,
                   XlsxSheetData *sheetData,
                   QString *errorMessage)
{
    QMap<int, QMap<int, QString>> cellMatrix;
    QXmlStreamReader xml(xmlContent);
    int currentRow = 0;

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("row")) {
            const QString rowRef = xml.attributes().value(QStringLiteral("r")).toString();
            currentRow = rowRef.isEmpty() ? currentRow + 1 : rowRef.toInt();
        }

        if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("c")) {
            const auto attributes = xml.attributes();
            const QString cellRef = attributes.value(QStringLiteral("r")).toString();
            const QString type = attributes.value(QStringLiteral("t")).toString();
            QString value;

            while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == QStringLiteral("c"))) {
                xml.readNext();
                if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("v")) {
                    value = xml.readElementText();
                } else if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("is")) {
                    value = readInlineString(xml);
                }
            }

            if (type == QStringLiteral("s")) {
                const int sharedIndex = value.toInt();
                value = sharedIndex >= 0 && sharedIndex < sharedStrings.size() ? sharedStrings.at(sharedIndex) : QString();
            }

            const int column = columnIndexFromReference(cellRef);
            if (currentRow > 0 && column > 0) {
                cellMatrix[currentRow].insert(column, value);
            }
        }
    }

    if (xml.hasError()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("无法解析工作表：%1").arg(xml.errorString());
        }
        return false;
    }

    if (cellMatrix.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Excel 工作表没有可读取的数据。");
        }
        return false;
    }

    const QMap<int, QString> headerCells = cellMatrix.first();
    const int lastColumn = headerCells.isEmpty() ? 0 : headerCells.lastKey();
    for (int column = 1; column <= lastColumn; ++column) {
        sheetData->headers.append(headerCells.value(column).trimmed());
    }

    auto it = cellMatrix.constBegin();
    ++it;
    for (; it != cellMatrix.constEnd(); ++it) {
        QStringList rowValues;
        rowValues.reserve(lastColumn);
        for (int column = 1; column <= lastColumn; ++column) {
            rowValues.append(it.value().value(column));
        }
        sheetData->rows.append(rowValues);
    }

    return true;
}
}

bool SimpleXlsxDocument::readSheet(const QString &filePath, XlsxSheetData *sheetData, QString *errorMessage)
{
    if (sheetData == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Excel 读取目标无效。");
        }
        return false;
    }

    sheetData->headers.clear();
    sheetData->rows.clear();

    QHash<QString, QByteArray> entries;
    if (!readZipArchive(filePath, &entries, errorMessage)) {
        return false;
    }

    QStringList sharedStrings;
    if (entries.contains(QStringLiteral("xl/sharedStrings.xml"))) {
        QString sharedError;
        sharedStrings = parseSharedStrings(entries.value(QStringLiteral("xl/sharedStrings.xml")), &sharedError);
        if (!sharedError.isEmpty()) {
            if (errorMessage != nullptr) {
                *errorMessage = sharedError;
            }
            return false;
        }
    }

    QString sheetEntryName = QStringLiteral("xl/worksheets/sheet1.xml");
    if (!entries.contains(sheetEntryName)) {
        const auto keys = entries.keys();
        for (const QString &key : keys) {
            if (key.startsWith(QStringLiteral("xl/worksheets/")) && key.endsWith(QStringLiteral(".xml"))) {
                sheetEntryName = key;
                break;
            }
        }
    }

    if (!entries.contains(sheetEntryName)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Excel 文件中未找到工作表。");
        }
        return false;
    }

    return parseSheetXml(entries.value(sheetEntryName), sharedStrings, sheetData, errorMessage);
}

bool SimpleXlsxDocument::writeSheet(const QString &filePath,
                                    const QString &sheetName,
                                    const QStringList &headers,
                                    const QList<QList<QVariant>> &rows,
                                    QString *errorMessage)
{
    if (headers.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Excel 表头不能为空。");
        }
        return false;
    }

    const QList<QPair<QString, QByteArray>> entries = {
        {QStringLiteral("[Content_Types].xml"), contentTypesXml().toUtf8()},
        {QStringLiteral("_rels/.rels"), relationshipRootXml().toUtf8()},
        {QStringLiteral("xl/workbook.xml"), workbookXml(sheetName).toUtf8()},
        {QStringLiteral("xl/_rels/workbook.xml.rels"), workbookRelationshipsXml().toUtf8()},
        {QStringLiteral("xl/worksheets/sheet1.xml"), sheetXml(headers, rows).toUtf8()}
    };

    return writeZipArchive(filePath, entries, errorMessage);
}