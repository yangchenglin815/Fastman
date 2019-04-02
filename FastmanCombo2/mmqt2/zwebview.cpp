#include "zwebview.h"
#include <QDialog>

ZWebView::ZWebView(QWidget *parent) :
    QWebView(parent)
{
    mHandler = NULL;
    setAcceptDrops(false);
}

void ZWebView::setHandler(ZWebViewHandler *handler) {
    mHandler = handler;
}

void ZWebView::mousePressEvent(QMouseEvent *event) {
    if(mHandler != NULL) {
        if(mHandler->mousePressEvent(this, event)) {
            return;
        }
    }
    QWebView::mousePressEvent(event);
}

void ZWebView::mouseReleaseEvent(QMouseEvent *event) {
    if(mHandler != NULL) {
        if(mHandler->mouseReleaseEvent(this, event)) {
            return;
        }
    }
    QWebView::mouseReleaseEvent(event);
}

void ZWebView::mouseMoveEvent(QMouseEvent *event) {
    if(mHandler != NULL) {
        if(mHandler->mouseMoveEvent(this, event)) {
            return;
        }
    }
    QWebView::mouseMoveEvent(event);
}

QWebView* ZWebView::createWindow(QWebPage::WebWindowType) {
    QDialog *dlg = new QDialog(this);
    QWebView *v = new QWebView(dlg);
    dlg->setMinimumSize(800,550);
    dlg->show();
    return v;
}
