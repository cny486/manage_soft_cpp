#pragma once

#include <QDateEdit>
#include <QWheelEvent>

class NoWheelDateEdit : public QDateEdit {
    Q_OBJECT
public:
    explicit NoWheelDateEdit(QWidget *parent = nullptr) : QDateEdit(parent) {}
    explicit NoWheelDateEdit(const QDate &date, QWidget *parent = nullptr) : QDateEdit(date, parent) {}

protected:
    void wheelEvent(QWheelEvent *event) override {
        event->ignore();
    }
};