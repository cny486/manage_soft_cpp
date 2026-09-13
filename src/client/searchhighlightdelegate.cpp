#include "searchhighlightdelegate.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QRegularExpression>
#include <QTextDocument>

namespace {
struct HighlightRange {
    int start = 0;
    int end = 0;
};

QString normalizeKeyword(QString keyword)
{
    keyword = keyword.trimmed().toLower();
    keyword.replace(QRegularExpression(QStringLiteral("[\\s\\-_/,;|]+")), QStringLiteral(" "));
    return keyword.simplified();
}

QStringList searchTokens(const QString &keyword)
{
    const QString normalized = normalizeKeyword(keyword);
    if (normalized.isEmpty()) {
        return {};
    }

    QStringList tokens = normalized.split(QChar(' '), Qt::SkipEmptyParts);
    tokens.removeDuplicates();
    std::sort(tokens.begin(), tokens.end(), [](const QString &left, const QString &right) {
        return left.size() > right.size();
    });
    return tokens;
}

QList<HighlightRange> findHighlightRanges(const QString &text, const QString &keyword)
{
    QList<HighlightRange> ranges;
    const QString lowerText = text.toLower();
    for (const QString &token : searchTokens(keyword)) {
        int position = 0;
        while (position >= 0) {
            position = lowerText.indexOf(token, position, Qt::CaseInsensitive);
            if (position < 0) {
                break;
            }

            const HighlightRange current{position, position + token.size()};
            bool merged = false;
            for (HighlightRange &existing : ranges) {
                if (current.start <= existing.end && current.end >= existing.start) {
                    existing.start = qMin(existing.start, current.start);
                    existing.end = qMax(existing.end, current.end);
                    merged = true;
                    break;
                }
            }

            if (!merged) {
                ranges.append(current);
            }
            position += token.size();
        }
    }

    std::sort(ranges.begin(), ranges.end(), [](const HighlightRange &left, const HighlightRange &right) {
        return left.start < right.start;
    });

    QList<HighlightRange> mergedRanges;
    for (const HighlightRange &range : ranges) {
        if (mergedRanges.isEmpty() || range.start > mergedRanges.last().end) {
            mergedRanges.append(range);
            continue;
        }

        mergedRanges.last().end = qMax(mergedRanges.last().end, range.end);
    }

    return mergedRanges;
}

QString highlightedHtml(const QString &text, const QString &keyword, const QColor &textColor)
{
    const QList<HighlightRange> ranges = findHighlightRanges(text, keyword);
    const QString baseColor = textColor.name();
    if (ranges.isEmpty()) {
        return QStringLiteral("<span style=\"color:%1;\">%2</span>")
            .arg(baseColor, text.toHtmlEscaped());
    }

    QString html = QStringLiteral("<span style=\"color:%1;\">").arg(baseColor);
    int current = 0;
    for (const HighlightRange &range : ranges) {
        if (range.start > current) {
            html += text.mid(current, range.start - current).toHtmlEscaped();
        }
        html += QStringLiteral("<span style=\"background-color:#ffe96b; color:%1;\">%2</span>")
                    .arg(baseColor, text.mid(range.start, range.end - range.start).toHtmlEscaped());
        current = range.end;
    }
    if (current < text.size()) {
        html += text.mid(current).toHtmlEscaped();
    }
    html += QStringLiteral("</span>");
    return html;
}
}

SearchHighlightDelegate::SearchHighlightDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void SearchHighlightDelegate::setKeyword(const QString &keyword)
{
    m_keyword = keyword;
}

void SearchHighlightDelegate::paint(QPainter *painter,
                                    const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const
{
    if (m_keyword.trimmed().isEmpty()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    QStyleOptionViewItem viewOption(option);
    initStyleOption(&viewOption, index);

    const QString text = viewOption.text;
    viewOption.text.clear();

    QStyle *style = viewOption.widget != nullptr ? viewOption.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &viewOption, painter, viewOption.widget);

    const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &viewOption, viewOption.widget);
    const QString elidedText = QFontMetrics(viewOption.font).elidedText(text,
                                                                          viewOption.textElideMode,
                                                                          textRect.width());

    QTextDocument document;
    document.setDefaultFont(viewOption.font);
    document.setDocumentMargin(0);
    const QColor textColor = (viewOption.state & QStyle::State_Selected)
                                 ? viewOption.palette.color(QPalette::HighlightedText)
                                 : viewOption.palette.color(QPalette::Text);
    document.setHtml(highlightedHtml(elidedText, m_keyword, textColor));
    document.setTextWidth(textRect.width());

    painter->save();
    painter->setClipRect(textRect);
    painter->translate(textRect.topLeft());
    const qreal verticalOffset = qMax<qreal>(0.0, (textRect.height() - document.size().height()) / 2.0);
    painter->translate(0.0, verticalOffset);
    QAbstractTextDocumentLayout::PaintContext context;
    document.documentLayout()->draw(painter, context);
    painter->restore();
}

QSize SearchHighlightDelegate::sizeHint(const QStyleOptionViewItem &option,
                                        const QModelIndex &index) const
{
    return QStyledItemDelegate::sizeHint(option, index);
}
