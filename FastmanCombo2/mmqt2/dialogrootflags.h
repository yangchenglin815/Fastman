#ifndef FORMROOTFLAGS_H
#define FORMROOTFLAGS_H

#include <QDialog>

namespace Ui {
class DialogRootFlags;
}

class AppConfig;
class DialogRootFlags : public QDialog
{
    Q_OBJECT
    AppConfig *config;
public:
    explicit DialogRootFlags(QWidget *parent = 0);
    ~DialogRootFlags();
private slots:
    void slot_change();
private:
    Ui::DialogRootFlags *ui;
};

#endif // FORMROOTFLAGS_H
