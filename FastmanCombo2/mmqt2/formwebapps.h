#ifndef FORMWEBAPPS_H
#define FORMWEBAPPS_H

#include <QWidget>
#include "zwebview.h"
#include <QThread>
#include <QWebElement>
#include <QWebFrame>
#include "globalcontroller.h"
#include "bundle.h"
namespace Ui {
class FormWebApps;
}

class FormWebApps : public QWidget
{
    Q_OBJECT
    QList<QWebElement> elementinfo;
    unsigned int sum,sumsize;
    GlobalController *controller;
public:
    explicit FormWebApps(QWidget *parent = 0);
    ~FormWebApps();
    void GetSumSize(const QWebElement& element);
public slots:
    void slot_displaySum();
    void slot_getwebInfo(QList<QWebElement>);
    void slot_downApps();
    void slot_notice(int,int,int);
signals:
    void signal_notice(int,int,int);
private:
    Ui::FormWebApps *ui;
};

#endif // FORMWEBAPPS_H
