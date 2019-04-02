#ifndef FORMWEBINDEX_H
#define FORMWEBINDEX_H

#include <QWidget>
#include "formweb.h"
#include <QNetworkAccessManager>
#include <QMessageBox>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDesktopServices>

namespace Ui {
class Formwebindex;
}

class ZMyNetworkManager : public QNetworkAccessManager
{
    Q_OBJECT
public:
    explicit ZMyNetworkManager(const QNetworkAccessManager* manager,QObject *parent = 0);
    virtual QNetworkReply *createRequest(Operation op, const QNetworkRequest &request,
                                         QIODevice *outgoingData = 0);

};

class Formwebindex : public QWidget
{
    Q_OBJECT

public:
    explicit Formwebindex(QWidget *parent = 0);
    ~Formwebindex();
public slots:


private:
    Ui::Formwebindex *ui;
};

#endif // FORMWEBINDEX_H
