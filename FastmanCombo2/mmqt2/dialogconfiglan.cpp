#include "dialogconfiglan.h"
#include "ui_dialogconfiglan.h"

#include "globalcontroller.h"
#include "appconfig.h"
#include "dialoghint.h"
#include "QJson/parser.h"
#include "QJson/serializer.h"
#include "zlog.h"
#include "zstringutil.h"

#include <QFile>
#include <QMouseEvent>
#include <QTextCodec>
#include <QSettings>

DialogConfigLAN::DialogConfigLAN(QWidget *parent) :
    QDialog(parent, Qt::FramelessWindowHint),
    ui(new Ui::DialogConfigLAN)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->setupUi(this);

    connect(ui->btn_save, SIGNAL(clicked()), SLOT(slot_save()));

    init();
}

DialogConfigLAN::~DialogConfigLAN()
{
    delete ui;
}

void DialogConfigLAN::init()
{
    QSettings settings(qApp->applicationDirPath() + "/mmqt2.ini", QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    QString url = settings.value("LAN/Url").toString();
    QString port = settings.value("LAN/Port").toString();

    ui->lineEdit_url->setText(url);
    ui->lineEdit_port->setText(port);
}

void DialogConfigLAN::mousePressEvent(QMouseEvent *event) {
    // current
    int x = event->pos().x();
    int y = event->pos().y();

    // top left
    int x1 = ui->wgt_title->pos().x();
    int y1 = ui->wgt_title->pos().y();

    // bottom right
    int x2 = x1 + ui->wgt_title->width();
    int y2 = y1 + ui->wgt_title->height();

    if(x > x1 && x < x2 && y > y1 && y < y2) {
        lastMousePos = event->pos();
        isDraggingWindow = true;
    }
}

void DialogConfigLAN::mouseMoveEvent(QMouseEvent *event) {
    if(isDraggingWindow) {
        this->move(event->globalPos()-lastMousePos);
    }
}

void DialogConfigLAN::mouseReleaseEvent(QMouseEvent *event) {
    isDraggingWindow = false;
}

void DialogConfigLAN::slot_save()
{
    QString url = ui->lineEdit_url->text();
    QString port = ui->lineEdit_port->text();

    if (url.isEmpty() || port.isEmpty()) {
        DialogHint::exec_hint(this, tr("hint"), tr("url or port can't be empty!"), QString(), DialogHint::TYPE_INFORMATION);
		return;
	}

    QSettings settings(qApp->applicationDirPath() + "/mmqt2.ini", QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.setValue("LAN/Url", url);
    settings.setValue("LAN/Port", port);
    settings.sync();

    AppConfig *appconfig = GlobalController::getInstance()->appconfig;
    appconfig->LANUrl = url;
    appconfig->LANPort = port;

    emit signal_LANConfigChanged();
    close();
}
