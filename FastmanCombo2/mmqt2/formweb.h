#ifndef FORMWEB_H
#define FORMWEB_H

#include <QWidget>
#include <QUrl>
#include "zwebview.h"

namespace Ui {
class FormWeb;
}

class FormWeb : public QWidget
{
    Q_OBJECT
    QUrl home_url;

public:
    explicit FormWeb(QWidget *parent = 0);
    ~FormWeb();

    ZWebView *getWebView();
    void setHomeUrl(const QUrl& url);
public slots:
    void slot_back();
    void slot_forward();
    void slot_refresh();
    void slot_home();


private:
    Ui::FormWeb *ui;
};

#endif // FORMWEB_H
