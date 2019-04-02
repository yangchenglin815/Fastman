#include "formwebindex.h"
#include "ui_formwebindex.h"
#include "loginmanager.h"
#include <QMessageBox>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDesktopServices>

Formwebindex::Formwebindex(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Formwebindex)
{
    ui->setupUi(this);
    QUrl url("http://zj.dengxian.net/v2/index.aspx");
    LoginManager::getInstance()->revokeUrl(url);
    LoginManager::getInstance()->fillUrlParams(url);
    ZMyNetworkManager* manager = new ZMyNetworkManager(ui->formWeb->getWebView()->page()->networkAccessManager());
    ui->formWeb->getWebView()->page()->setNetworkAccessManager(manager);

    ui->formWeb->setHomeUrl(url);
}

Formwebindex::~Formwebindex()
{
    delete ui;
}

ZMyNetworkManager::ZMyNetworkManager(const QNetworkAccessManager *manager, QObject *parent) :
    QNetworkAccessManager(parent)
{
    setCache(manager->cache());
    setCookieJar(manager->cookieJar());
    setProxy(manager->proxy());
    setProxyFactory(manager->proxyFactory());
}

QNetworkReply *ZMyNetworkManager::createRequest(QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *outgoingData)
{
    if (request.url().scheme() == "tencent"){
      QDesktopServices::openUrl(request.url());
      static QNetworkRequest req;
      return this->get(req);
    }
    return QNetworkAccessManager::createRequest(op, request, outgoingData);
}

