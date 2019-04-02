#include "formweb.h"
#include "ui_formweb.h"

FormWeb::FormWeb(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormWeb)
{
    ui->setupUi(this);
    connect(ui->btn_back, SIGNAL(clicked()), SLOT(slot_back()));
    connect(ui->btn_forward, SIGNAL(clicked()), SLOT(slot_forward()));
    connect(ui->btn_refresh, SIGNAL(clicked()), SLOT(slot_refresh()));
    connect(ui->btn_home, SIGNAL(clicked()), SLOT(slot_home()));
}

FormWeb::~FormWeb()
{
    delete ui;
}

void FormWeb::setHomeUrl(const QUrl &url) {
    home_url = url;
    slot_home();
}

void FormWeb::slot_back() {
    ui->webView->back();
}

void FormWeb::slot_forward() {
    ui->webView->forward();
}

void FormWeb::slot_refresh() {
    ui->webView->reload();
}

ZWebView *FormWeb::getWebView() {
    return ui->webView;
}

void FormWeb::slot_home() {
    ui->webView->load(home_url);
}
