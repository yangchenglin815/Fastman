#include "dialogprogress.h"
#include "ui_dialogprogress.h"

#include <QKeyEvent>

DialogProgress::DialogProgress(QWidget *parent) :
    QDialog(parent, Qt::FramelessWindowHint),
    ui(new Ui::DialogProgress)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_TranslucentBackground);

    connect(ui->btn_stop, SIGNAL(clicked()), this, SLOT(slot_stop()));
}

DialogProgress::~DialogProgress()
{
    delete ui;
}

void DialogProgress::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_lastPoint = event->globalPos() - this->pos();
        event->accept();
        m_dragged = true;
    }
}

void DialogProgress::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragged
            && (event->buttons() & Qt::LeftButton)) {
        this->move(event->globalPos() - m_lastPoint);
        event->accept();
    }
}

void DialogProgress::mouseReleaseEvent(QMouseEvent *) {
    m_dragged = false;
}

void DialogProgress::keyPressEvent(QKeyEvent *event) {
    event->ignore();
}

void DialogProgress::setTitle(const QString &title) {
    ui->lbl_title->setText(title);
}

void DialogProgress::setStopable(bool b) {
    ui->btn_stop->setVisible(b);
}

void DialogProgress::slot_setProgress(int value, int max) {
    if (value < 0) {
        value = 0;
        max = 0;
    }

    ui->progressBar->setMaximum(max);
    ui->progressBar->setValue(value);
}

void DialogProgress::slot_setState(const QString &text) {
    ui->lbl_state->setText(text);
}

void DialogProgress::slot_stop() {
    ui->btn_stop->setDisabled(true);
    emit signal_stop();
}
