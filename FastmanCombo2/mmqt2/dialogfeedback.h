#ifndef DIALOGFEEDBACK_H
#define DIALOGFEEDBACK_H

#include <QDialog>

namespace Ui {
class DialogFeedback;
}

class DialogFeedback : public QDialog
{
    Q_OBJECT

public:
    explicit DialogFeedback(QWidget *parent = 0);
    ~DialogFeedback();
private:
    Ui::DialogFeedback *ui;
};

#endif // DIALOGFEEDBACK_H
