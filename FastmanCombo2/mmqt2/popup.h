#ifndef POPUP_H
#define POPUP_H

#include <QObject>
#include <QWidget>
#include <QMouseEvent>
#include <QWebPage>
#include <QWebView>

class QLabel;
class QPushButton;
class QToolBar;
class QAction;
class QPoint;
class QPropertyAnimation;

class Popup : public QObject
{
    Q_OBJECT
public:
    explicit Popup(QObject *parent = 0);
    ~Popup();

public:
    static void showCentralPop(QWidget *parent, const QUrl &url);
    static void showCornerPop(QWidget *parent, const QUrl &url);
};


// PopBase
class PopBase : public QWidget
{
    Q_OBJECT
public:
    explicit PopBase(int y_header, QWidget *parent = 0);

public:
    void setTitle(const QString &title);
    void loadUrl(const QUrl &url);
    void applyStyle();

public:
    QPushButton *m_minBtn;
    QPushButton *m_closeBtn;
    QLabel *m_titleLbl;

public slots:
    void slot_btnClicked();
    void slot_navigation();
    virtual void slot_loadFinished(bool bOK);

protected:
    void mousePressEvent(QMouseEvent *e);
    void mouseMoveEvent(QMouseEvent *e);
    void mouseReleaseEvent(QMouseEvent *e);

private:
    bool m_drag;
    QPoint m_position;

    QToolBar *m_navigationBar;
    QAction *m_goBackAction;
    QAction *m_goForwardAction;
    QAction *m_reloadAction;

    QWebView *m_webView;
    QPropertyAnimation *m_popAnimation;
    bool bUseAnimation;
};


// CentralPop
class CentralPop : public PopBase
{
    Q_OBJECT
public:
    explicit CentralPop(int y_header, QWidget *parent = 0);
    ~CentralPop();

public:
    void pop(const QUrl &url);
};


// CorenerPop
class CornerPop : public PopBase
{
    Q_OBJECT
public:
    explicit CornerPop(int y_header, QWidget *parent = 0);
    ~CornerPop();

public:
    void pop(const QUrl &url);

public slots:
    void slot_loadFinished(bool bOK);

private:
    QPropertyAnimation *m_popAnimation;
};


// WebPage
class CWebPage : public QWebPage
{
    Q_OBJECT

public:
    CWebPage(QObject *parent = 0);

protected:
    QString userAgentForUrl(const QUrl &url) const;
    bool acceptNavigationRequest(QWebFrame *frame, const QNetworkRequest &request, NavigationType type);

};


// WebView
class QMouseEvent;
class QPoint;
class CWebViewHandler;

class CWebView : public QWebView
{
    Q_OBJECT

public:
    CWebView(QWidget *parent = 0);

public:
    QWebPage::WebAction getUrlAction(const QUrl &url) const;

public:
    CWebViewHandler *mHandler;

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    //void contextMenuEvent(QContextMenuEvent *event);

private:
    QPoint lastClickedPoint;

private slots:
    void clickedUrl(const QUrl &url);

};


// WebHandler
class CWebViewHandler
{
public:
    virtual bool mousePressEvent(CWebView *webview, QMouseEvent *event) = 0;
    virtual bool mouseReleaseEvent(CWebView *webview, QMouseEvent *event) = 0;
    virtual bool mouseMoveEvent(CWebView *webview, QMouseEvent *event) = 0;
};
#endif // POPUP_H
