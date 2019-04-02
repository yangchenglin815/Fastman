#include "dialogconfigwifi.h"
#include "ui_dialogconfigwifi.h"

#include "dialoghint.h"
#include "QJson/parser.h"
#include "QJson/serializer.h"
#include "zlog.h"
#include "zstringutil.h"

#include <QFile>
#include <QMouseEvent>
#include <QTextCodec>
#include <QSettings>

DialogConfigWiFi::DialogConfigWiFi(QWidget *parent) :
    QDialog(parent, Qt::FramelessWindowHint),
    ui(new Ui::DialogConfigWiFi)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->setupUi(this);

    connect(ui->btn_save, SIGNAL(clicked()), SLOT(slot_save()));
    connect(ui->chx_notHintNextTime, SIGNAL(toggled(bool)), SLOT(slot_notHintToggled(bool)));

    init();
}

DialogConfigWiFi::~DialogConfigWiFi()
{
    delete ui;
}

void DialogConfigWiFi::init()
{
    QSettings settings(qApp->applicationDirPath() + "/WiFiConfiguration.ini", QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    QString username = settings.value("username").toString();
    QString password = settings.value("password").toString();

    ui->lineEdit_username->setText(username);
    ui->lineEdit_password->setText(password);

    QSettings settings2(qApp->applicationDirPath() + "/mmqt2.ini", QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    bool ret = settings2.value("CFG/NotHintWiFiConfigNextTime").toBool();
    ui->chx_notHintNextTime->setChecked(ret);
}

void DialogConfigWiFi::mousePressEvent(QMouseEvent *event) {
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

void DialogConfigWiFi::mouseMoveEvent(QMouseEvent *event) {
    if(isDraggingWindow) {
        this->move(event->globalPos()-lastMousePos);
    }
}

void DialogConfigWiFi::mouseReleaseEvent(QMouseEvent *event) {
    isDraggingWindow = false;
}

void DialogConfigWiFi::slot_save()
{
	QString username = ui->lineEdit_username->text();
	QString password = ui->lineEdit_password->text();

	if (username.isEmpty() || password.isEmpty()) {
		DialogHint::exec_hint(this, tr("hint"), tr("username or password can't be empty!"), QString(), DialogHint::TYPE_INFORMATION);
		return;
	}

#if 0 // use json
	QJson::Serializer json;
	QVariantMap map;
	map.insert("password", password);
	map.insert("username", username);
	QByteArray out = json.serialize(map);

	QString jsonStr = QString::fromLocal8Bit(out);

	QFile file(qApp->applicationDirPath() + "/WiFiConfiguration");
	if (file.open(QIODevice::WriteOnly)) {
		DBG("save WiFi configuration succeed\n");
		file.write(jsonStr.toLocal8Bit());
		file.flush();
		file.close();
		close();
	} else {
		DialogHint::exec_hint(this, tr("hint"), tr("save failed, retry it!"));
	}
#else
    QSettings settings(qApp->applicationDirPath() + "/WiFiConfiguration.ini", QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.setValue("username", username);
    settings.setValue("password", password);
    settings.sync();

    emit signal_WiFiConfigChanged();
    close();
#endif
}

void DialogConfigWiFi::slot_notHintToggled(bool checked)
{
    QSettings settings(qApp->applicationDirPath() + "/mmqt2.ini", QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.setValue("CFG/NotHintWiFiConfigNextTime", checked);
    settings.sync();
}
