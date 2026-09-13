#pragma once

#include <QDoubleSpinBox>
#include <QWheelEvent>

class NoWheelDoubleSpinBox : public QDoubleSpinBox {
    Q_OBJECT
public:
    explicit NoWheelDoubleSpinBox(QWidget *parent = nullptr) : QDoubleSpinBox(parent) {
        setFocusPolicy(Qt::StrongFocus);
    }

protected:
    void wheelEvent(QWheelEvent *event) override {
        if (!hasFocus()) {
            event->ignore();
        } else {
            QDoubleSpinBox::wheelEvent(event);
        }
    }
};
