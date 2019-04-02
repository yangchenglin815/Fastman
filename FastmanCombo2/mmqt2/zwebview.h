#ifndef ZWEBVIEW_H
#define ZWEBVIEW_H

#include <QWebView>
#include <QMouseEvent>

class ZWebView;
class ZWebViewHandler {
public:
    virtual bool mousePressEvent(ZWebView *webview, QMouseEvent *event) = 0;
    virtual bool mouseReleaseEvent(ZWebView *webview, QMouseEvent *event) = 0;
    virtual bool mouseMoveEvent(ZWebView *webview, QMouseEvent *event) = 0;
};

class ZWebView : public QWebView
{
    Q_OBJECT
    ZWebViewHandler *mHandler;
    QWebView* createWindow(QWebPage::WebWindowType);
public:
    explicit ZWebView(QWidget *parent = 0);
    void setHandler(ZWebViewHandler *handler);

    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
signals:

public slots:

};

#endif // ZWEBVIEW_H
