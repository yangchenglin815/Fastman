#ifndef DIALOGADDDOWNLOAD_H
#define DIALOGADDDOWNLOAD_H

#include <QDialog>

namespace Ui {
class DialogAddDownload;
}

class DialogAddDownload : public QDialog
{
    Q_OBJECT

public:
    explicit DialogAddDownload(QWidget *parent = 0);
    ~DialogAddDownload();

    QString getUrl();
    QString getFileName();
private:
    Ui::DialogAddDownload *ui;
};

#endif // DIALOGADDDOWNLOAD_H
