#ifndef DIALOGHINT_H
#define DIALOGHINT_H

#ifndef NO_GUI
#include <QDialog>

namespace Ui {
class DialogHint;
}

class DialogHint: public QDialog {
    Q_OBJECT
    explicit DialogHint(QWidget *parent = 0);
    ~DialogHint();
public:
    enum {
        TYPE_INFORMATION,
        TYPE_YESNO,
        TYPE_OKCANCEL
    };
    static int exec_hint(QWidget *parent,
                         const QString &title, const QString &message,
                         const QString &tag = QString(),
                         int type = TYPE_YESNO);

    void mousePressEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
private:
    Ui::DialogHint *ui;
    QPoint lastmouse;
    bool isdraggwindow;
signals:
    void signal_btn_Ok();
    void signal_btn_Cancel();
};
#else
class QDialog {
public:
    enum {
        Rejected,
        Accepted
    };
};

class DialogHint {
public:
    enum {
        TYPE_INFORMATION,
        TYPE_YESNO,
        TYPE_OKCANCEL
    };
};
#endif
#endif // DIALOGHINT_H
