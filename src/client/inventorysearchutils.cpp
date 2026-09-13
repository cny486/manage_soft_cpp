#include "inventorysearchutils.h"

#include <QRegularExpression>
#include <algorithm>

namespace {
struct SearchFieldWeight {
    const char *key;
    int weight;
};

QString normalizeSearchText(QString text)
{
    text = text.trimmed().toLower();
    text.replace(QRegularExpression(QStringLiteral("[\\s\\-_/,;|]+")), QStringLiteral(" "));
    return text.simplified();
}

QString compactSearchText(QString text)
{
    text = normalizeSearchText(std::move(text));
    text.remove(QChar(' '));
    return text;
}

QStringList searchTokens(const QString &keyword)
{
    const QString normalized = normalizeSearchText(keyword);
    if (normalized.isEmpty()) {
        return {};
    }

    QStringList tokens = normalized.split(QChar(' '), Qt::SkipEmptyParts);
    tokens.removeDuplicates();
    return tokens;
}

QList<SearchFieldWeight> inventorySearchFields()
{
    return {
        {"manufacturerPart", 20},
        {"name", 16},
        {"value", 15},
        {"footprint", 15},
        {"voltage", 14},
        {"manufacturer", 10},
        {"device", 9},
        {"category", 8},
        {"uniqueId", 8},
        {"designator", 7},
        {"location", 5},
        {"comment", 4}
    };
}

int scoreFieldText(const QString &fieldValue, const QString &token, int weight)
{
    const QString normalizedField = normalizeSearchText(fieldValue);
    if (normalizedField.isEmpty() || token.isEmpty()) {
        return 0;
    }

    if (normalizedField == token) {
        return weight * 10;
    }
    if (normalizedField.startsWith(token)) {
        return weight * 7;
    }
    if (normalizedField.contains(token)) {
        return weight * 5;
    }

    const QString compactField = compactSearchText(fieldValue);
    const QString compactToken = compactSearchText(token);
    if (!compactField.isEmpty() && !compactToken.isEmpty()) {
        if (compactField == compactToken) {
            return weight * 8;
        }
        if (compactField.contains(compactToken)) {
            return weight * 4;
        }
    }

    return 0;
}
}

int inventoryRecordSearchScore(const QVariantMap &record, const QString &keyword)
{
    const QString normalizedKeyword = normalizeSearchText(keyword);
    if (normalizedKeyword.isEmpty()) {
        return 0;
    }

    const QList<SearchFieldWeight> fields = inventorySearchFields();
    QStringList combinedParts;
    combinedParts.reserve(fields.size());

    int score = 0;
    for (const QString &token : searchTokens(normalizedKeyword)) {
        int bestTokenScore = 0;
        for (const SearchFieldWeight &field : fields) {
            const QString value = record.value(QString::fromLatin1(field.key)).toString().trimmed();
            if (!value.isEmpty()) {
                combinedParts.append(value);
            }
            bestTokenScore = std::max(bestTokenScore, scoreFieldText(value, token, field.weight));
        }

        if (bestTokenScore == 0) {
            return 0;
        }
        score += bestTokenScore;
    }

    const QString combinedText = normalizeSearchText(combinedParts.join(QChar(' ')));
    if (!combinedText.isEmpty()) {
        if (combinedText == normalizedKeyword) {
            score += 80;
        } else if (combinedText.contains(normalizedKeyword)) {
            score += 35;
        }
    }

    return score;
}

QList<QVariantMap> rankInventoryRecordsByKeyword(const QList<QVariantMap> &records, const QString &keyword)
{
    if (keyword.trimmed().isEmpty()) {
        return records;
    }

    struct ScoredRecord {
        QVariantMap record;
        int score = 0;
        int originalIndex = -1;
    };

    QList<ScoredRecord> scoredRecords;
    scoredRecords.reserve(records.size());
    for (int index = 0; index < records.size(); ++index) {
        const QVariantMap &record = records.at(index);
        const int score = inventoryRecordSearchScore(record, keyword);
        if (score <= 0) {
            continue;
        }
        scoredRecords.append({record, score, index});
    }

    std::sort(scoredRecords.begin(), scoredRecords.end(), [](const ScoredRecord &left, const ScoredRecord &right) {
        if (left.score != right.score) {
            return left.score > right.score;
        }
        return left.originalIndex < right.originalIndex;
    });

    QList<QVariantMap> rankedRecords;
    rankedRecords.reserve(scoredRecords.size());
    for (const ScoredRecord &entry : scoredRecords) {
        rankedRecords.append(entry.record);
    }
    return rankedRecords;
}
