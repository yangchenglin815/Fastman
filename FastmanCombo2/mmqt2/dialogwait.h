#ifndef DIALOGWAIT_H
#define DIALOGWAIT_H

#include <QDialog>
class QLabel;
class DialogWait : public QDialog
{
    Q_OBJECT
    QLabel *label;
public:
    explicit DialogWait(QWidget *parent = 0);
    void keyPressEvent(QKeyEvent *e);

    void setText(const QString& text);
signals:

public slots:

};

#endif // DIALOGWAIT_H
