#pragma once

#include <QDialog>
#include <QList>
#include <QVariantMap>

class QLineEdit;
class QTableWidget;
class SearchHighlightDelegate;

class InventoryItemPickerDialog : public QDialog {
public:
    explicit InventoryItemPickerDialog(const QList<QVariantMap> &inventoryRecords,
                                       const QStringList &relevanceKeywords,
                                       QWidget *parent = nullptr);

    QVariantMap selectedRecord() const;

protected:
    void accept() override;

private:
    void refreshTable();
    QList<QVariantMap> filteredRecords() const;

    QList<QVariantMap> m_inventoryRecords;
    QList<QVariantMap> m_visibleRecords;
    QStringList m_relevanceKeywords;
    QLineEdit *m_searchEdit = nullptr;
    QTableWidget *m_table = nullptr;
    SearchHighlightDelegate *m_highlightDelegate = nullptr;
};
