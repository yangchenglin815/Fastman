#include "dialogwait.h"
#include <QLabel>
#include <QHBoxLayout>
#include <QKeyEvent>

DialogWait::DialogWait(QWidget *parent) :
    QDialog(parent, Qt::SplashScreen)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    label = new QLabel(this);
    label->setStyleSheet("QLabel { color: white; padding: 10px; background-color: #1EBAEC; border: 2px solid #7ED4F4; border-radius: 8px;}");
    layout->addWidget(label);
    setLayout(layout);
}

void DialogWait::keyPressEvent(QKeyEvent *e) {
    e->ignore();
}

void DialogWait::setText(const QString &text) {
    label->setText(text);
}
