#include "dialogadddownload.h"
#include "ui_dialogadddownload.h"

DialogAddDownload::DialogAddDownload(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogAddDownload)
{
    ui->setupUi(this);
    connect(ui->pushButton, SIGNAL(clicked()), SLOT(accept()));
}

DialogAddDownload::~DialogAddDownload()
{
    delete ui;
}

QString DialogAddDownload::getUrl() {
    return ui->plainTextEdit->toPlainText();
}

QString DialogAddDownload::getFileName() {
    return ui->lineEdit->text();
}
