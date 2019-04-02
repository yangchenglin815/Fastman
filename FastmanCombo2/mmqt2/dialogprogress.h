#ifndef DIALOGPROGRESS_H
#define DIALOGPROGRESS_H

#include <QDialog>

class QPoint;
namespace Ui {
class DialogProgress;
}

class DialogProgress : public QDialog
{
    Q_OBJECT

public:
    explicit DialogProgress(QWidget *parent = 0);
    ~DialogProgress();

    void mousePressEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
    void keyPressEvent(QKeyEvent *);

    void setTitle(const QString &title);
    void setStopable(bool b = true);

public slots:
    void slot_setProgress(int value, int max);
    void slot_setState(const QString &text);
    void slot_stop();

signals:
    void signal_stop();

private:
    Ui::DialogProgress *ui;

    QPoint m_lastPoint;
    bool m_dragged;
};

#endif // DIALOGPROGRESS_H
