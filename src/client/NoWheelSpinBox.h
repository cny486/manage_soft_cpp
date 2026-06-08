#pragma once

#include <QSpinBox>
#include <QWheelEvent>

class NoWheelSpinBox : public QSpinBox {
    Q_OBJECT
public:
    explicit NoWheelSpinBox(QWidget *parent = nullptr) : QSpinBox(parent) {
        setFocusPolicy(Qt::StrongFocus);
    }

protected:
    void wheelEvent(QWheelEvent *event) override {
        if (!hasFocus()) {
            event->ignore();
        } else {
            QSpinBox::wheelEvent(event);
        }
    }
};
