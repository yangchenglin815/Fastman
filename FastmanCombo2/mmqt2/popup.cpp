#include "popup.h"

#include <QLabel>
#include <QToolBar>
#include <QRect>
#include <QPoint>
#include <QWebFrame>
#include <QWebPage>
#include <QWebView>
#include <QAction>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QSpacerItem>
#include <QApplication>
#include <QPropertyAnimation>
#include <QUrl>
#include <QDesktopWidget>
#include <QDesktopServices>
#include <QWebElement>
#include <QFile>
#include <QDebug>

Popup::Popup(QObject *parent) :
    QObject(parent)
{
}

Popup::~Popup()
{
}

void Popup::showCentralPop(QWidget *parent, const QUrl &url) {
    CentralPop *centralPop = new CentralPop(60, parent);
    centralPop->pop(url);
}

void Popup::showCornerPop(QWidget *parent, const QUrl &url) {
    CornerPop *cornerPop = new CornerPop(38, parent);
    cornerPop->pop(url);
}


// PopBase
PopBase::PopBase(int y_header, QWidget *parent) :
    QWidget(parent),
    m_drag(false),
    m_navigationBar(0),
    m_goBackAction(0),
    m_goForwardAction(0),
    m_reloadAction(0),
    m_webView(0),
    m_popAnimation(0),
    bUseAnimation(false)
{
    this->setWindowFlags(Qt::FramelessWindowHint | Qt::Window | Qt::ToolTip);

    QFrame *frm = new QFrame(this);
    frm->setObjectName("mainFrm");
    frm->setFrameShape(QFrame::NoFrame);
    QGridLayout *mainLayout = new QGridLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(frm);

    QFrame *frm_title = new QFrame(frm);
    frm_title->setFrameShape(QFrame::NoFrame);
    frm_title->setObjectName("frm_title");
    frm_title->setFixedHeight(y_header);

    m_minBtn = new QPushButton(frm_title);
    m_minBtn->setObjectName("btn_min");
    connect(m_minBtn, SIGNAL(clicked()), this, SLOT(showMinimized()));
    m_minBtn->hide();

    m_closeBtn = new QPushButton(QString(""), frm_title);
    m_closeBtn->setObjectName("btn_close");
    connect(m_closeBtn, SIGNAL(clicked()), this, SLOT(close()));

    m_titleLbl = new QLabel(QString::fromUtf8("   新闻公告"), frm_title);
    m_titleLbl->setObjectName("lbl_title");
    m_titleLbl->setAlignment(Qt::AlignCenter);

    QSpacerItem *topSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding);
    QHBoxLayout *topLayout = new QHBoxLayout(frm_title);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->addWidget(m_titleLbl);
    topLayout->addSpacerItem(topSpacer);

    topLayout->addWidget(m_minBtn);
    topLayout->addWidget(m_closeBtn);   

    m_goBackAction = new QAction(frm);
    m_goBackAction->setIcon(QIcon(QLatin1String(":/skins/skins/arrow_left_green_1.png")));
    connect(m_goBackAction, SIGNAL(triggered()), this, SLOT(slot_navigation()));

    m_goForwardAction = new QAction(frm);
    m_goForwardAction->setIcon(QIcon(QLatin1String(":/skins/skins/arrow_right_green_1.png")));
    connect(m_goForwardAction, SIGNAL(triggered()), this, SLOT(slot_navigation()));

    m_reloadAction = new QAction(frm);
    m_reloadAction->setIcon(QIcon(QLatin1String(":/skins/skins/nav_refresh_green.png")));
    connect(m_reloadAction, SIGNAL(triggered()), this, SLOT(slot_navigation()));

    m_navigationBar = new QToolBar(frm);
    m_navigationBar->addAction(m_goBackAction);
    m_navigationBar->addAction(m_goForwardAction);
    m_navigationBar->addAction(m_reloadAction);
    m_navigationBar->hide();

    m_webView = new CWebView(frm);
    m_webView->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
    connect(m_webView, SIGNAL(loadFinished(bool)), this, SLOT(slot_loadFinished(bool)));

    QVBoxLayout *wholeLayout = new QVBoxLayout(frm);
    wholeLayout->setContentsMargins(0, 0, 0, 0);
    wholeLayout->addWidget(frm_title);
    wholeLayout->addWidget(m_navigationBar);
    wholeLayout->addWidget(m_webView);

    this->resize(700, 450);

    // set sytle
    applyStyle();
}

void PopBase::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        m_drag = true;
        m_position = e->globalPos() - this->pos();
        e->accept();
    }
}

void PopBase::mouseMoveEvent(QMouseEvent *e) {
    if (m_drag && (e->buttons() && Qt::LeftButton)) {
        this->move(e->globalPos() - m_position);
        e->accept();
    }
}

void PopBase::mouseReleaseEvent(QMouseEvent *e) {
    m_drag = false;
}

void PopBase::setTitle(const QString &title) {
    m_titleLbl->setText(title);
}

void PopBase::slot_btnClicked() {
    this->close();
}

void PopBase::slot_navigation() {
    QAction *action = dynamic_cast<QAction *>(sender());

    if (NULL == action) {
        return;
    }
    if (m_goBackAction == action) {
        this->m_webView->back();
    } else if (m_goForwardAction == action) {
        this->m_webView->forward();
    } else if (m_reloadAction == action) {
        this->m_webView->reload();
    }
}

void PopBase::slot_loadFinished(bool bOK) {
    if (bOK) {
        this->show();
    } else {
        qDebug() << "Url loade error!";
    }
}

void PopBase::loadUrl(const QUrl &url) {
    m_webView->load(url);
}

void PopBase::applyStyle() {
    QFile qssFile(":/ui/popup.qss");
    if (qssFile.open(QIODevice::ReadOnly)) {
        setStyleSheet(QString::fromUtf8(qssFile.readAll().data()));
    }
}


// CentralPop
CentralPop::CentralPop(int y_header, QWidget *parent) :
    PopBase(y_header, parent)
{
}

CentralPop::~CentralPop()
{
}

void CentralPop::pop(const QUrl &url) {
    loadUrl(url);
    int screenIdx = QApplication::desktop()->screenNumber(this);
    QRect rect = QApplication::desktop()->availableGeometry(screenIdx);
    this->move((rect.width() - this->width()) / 2,
               (rect.height() - this->height()) / 2);
}


// CornerPop
CornerPop::CornerPop(int y_header, QWidget *parent) :
    PopBase(y_header, parent),
    m_popAnimation(0)
{
    m_popAnimation = new QPropertyAnimation(this, "geometry", this);
    this->resize(310, 235);
}

CornerPop::~CornerPop()
{
}

void CornerPop::pop(const QUrl &url) {
    loadUrl(url);
}

void CornerPop::slot_loadFinished(bool bOK) {
    if (bOK) {
        int screenIdx = QApplication::desktop()->screenNumber(this);
        QRect rect = QApplication::desktop()->availableGeometry(screenIdx);
        int x = rect.width() - this->width();

        if(!m_popAnimation) {
            m_popAnimation = new QPropertyAnimation(this, "geometry", this);
        }

        this->show();
        // set animation
        m_popAnimation->setDuration(1000);
        m_popAnimation->setStartValue(QRect(x - 10, rect.height(), width(), height()));
        m_popAnimation->setEndValue(QRect(x - 10, rect.height() - this->height() - 5, width(), height()));
        m_popAnimation->start();
    } else {
        qDebug() << "Url error!";
    }
}


// WebPage
CWebPage::CWebPage(QObject *parent)
    : QWebPage(parent)
{
}

QString CWebPage::userAgentForUrl(const QUrl &url) const {
    return "Mozilla/5.0 (compatible; MSIE 9.0; Windows NT 6.1; Trident/5.0)";
}

bool CWebPage::acceptNavigationRequest(QWebFrame *frame, const QNetworkRequest &request,
                                       NavigationType type) {
    return QWebPage::acceptNavigationRequest(frame, request, type);
}


// WebView
CWebView::CWebView(QWidget *parent)
    : QWebView(parent),
      mHandler(NULL)
{
    setAcceptDrops(false);
    setPage(new CWebPage());

    connect(this, SIGNAL(linkClicked(const QUrl &)), this, SLOT(clickedUrl(const QUrl &)));
}

void CWebView::mousePressEvent(QMouseEvent *event) {
    if (mHandler != NULL) {
        if (mHandler->mousePressEvent(this, event))
            return;
    }
    lastClickedPoint = event->pos();
    QWebView::mousePressEvent(event);
}

void CWebView::mouseMoveEvent(QMouseEvent *event) {
    if(mHandler != NULL) {
        if(mHandler->mouseMoveEvent(this, event)) {
            return;
        }
    }
    QWebView::mouseMoveEvent(event);
}

void CWebView::mouseReleaseEvent(QMouseEvent *event) {
    if(mHandler != NULL) {
        if(mHandler->mouseReleaseEvent(this, event)) {
            return;
        }
    }
    lastClickedPoint = event->pos();
    QWebView::mouseReleaseEvent(event);
}

QWebPage::WebAction CWebView::getUrlAction(const QUrl &url) const {
    QWebElement elem = this->page()->mainFrame()->hitTestContent(lastClickedPoint).linkElement();
    elem.toOuterXml();
    if(elem.attribute("target") == "_blank") {
        qDebug() << "_blank";
        return QWebPage::OpenLinkInNewWindow;
    }

    return QWebPage::OpenLink;
}

void CWebView::clickedUrl(const QUrl &url) {
    qDebug() << url;
    if (QWebPage::OpenLinkInNewWindow == getUrlAction(url)) {
        QDesktopServices::openUrl(url);
    }
    else if (QWebPage::OpenLink == getUrlAction(url)) {
        // do sth.
        this->load(url);
    }
}
