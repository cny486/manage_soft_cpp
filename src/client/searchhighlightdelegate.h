#pragma once

#include <QStyledItemDelegate>

class SearchHighlightDelegate : public QStyledItemDelegate {
public:
    explicit SearchHighlightDelegate(QObject *parent = nullptr);

    void setKeyword(const QString &keyword);

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

private:
    QString m_keyword;
};
