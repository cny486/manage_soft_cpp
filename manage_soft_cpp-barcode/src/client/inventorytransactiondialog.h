#pragma once

#include <QDialog>

class QDateEdit;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTextEdit;

class InventoryTransactionDialog : public QDialog {
public:
    enum class Mode {
        StockIn,
        StockOut
    };

    InventoryTransactionDialog(Mode mode,
                               const QString &itemSummary,
                               int currentQuantity,
                               QWidget *parent = nullptr);

    int quantity() const;
    QString transactionDate() const;
    QString note() const;

protected:
    void accept() override;

private:
    Mode m_mode;
    QLabel *m_itemLabel = nullptr;
    QLabel *m_currentQuantityLabel = nullptr;
    QSpinBox *m_quantitySpinBox = nullptr;
    QDateEdit *m_dateEdit = nullptr;
    QTextEdit *m_noteEdit = nullptr;
};