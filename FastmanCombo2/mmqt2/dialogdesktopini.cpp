#include "dialogdesktopini.h"
#include "ui_dialogdesktopini.h"

#include "dialoghint.h"
#include "QJson/parser.h"
#include "QJson/serializer.h"
#include "zlog.h"
#include "zstringutil.h"

#include <QFile>
#include <QMouseEvent>
#include <QTextCodec>
#include <QSettings>

Dialogdesktopini::Dialogdesktopini(QWidget *parent) :
    QDialog(parent, Qt::FramelessWindowHint),
    ui(new Ui::Dialogdesktopini)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    ui->setupUi(this);
    connect(ui->btn_save, SIGNAL(clicked()), SLOT(slot_save()));
    connect(ui->chx_notHintNextTime, SIGNAL(toggled(bool)), SLOT(slot_notHintToggled(bool)));
    connect(ui->btn_cancel, SIGNAL(clicked()), this, SLOT(reject()));
    connect(ui->btn_close, SIGNAL(clicked()), this, SLOT(reject()));
    init();
}

Dialogdesktopini::~Dialogdesktopini()
{
    delete ui;
}

void Dialogdesktopini::init()
{
    QSettings settings(qApp->applicationDirPath() + "/DesktopConfiguration.ini", QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    QString rows = settings.value("rows").toString();
    QString column = settings.value("column").toString();
    QString iconSize = settings.value("iconSize").toString();
    QString numHotseatIcons = settings.value("numHotseatIcons").toString();
    QString hotseatIconSize = settings.value("hotseatIconSize").toString();

    ui->lineEdit_rows->setText(rows);
    ui->lineEdit_column->setText(column);
    ui->lineEdit_iconsize->setText(iconSize);
    ui->lineEdit_numhotseaticons->setText(numHotseatIcons);
    ui->lineEdit_hotseaticonsize->setText(hotseatIconSize);

    QSettings settings2(qApp->applicationDirPath() + "/mmqt2.ini", QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    bool ret = settings2.value("cfg/notHintDesktopConfigNextTime").toBool();
    ui->chx_notHintNextTime->setChecked(ret);
}

void Dialogdesktopini::mousePressEvent(QMouseEvent *event) {
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

void Dialogdesktopini::mouseMoveEvent(QMouseEvent *event) {
    if(isDraggingWindow) {
        this->move(event->globalPos()-lastMousePos);
    }
}

void Dialogdesktopini::mouseReleaseEvent(QMouseEvent *event) {
    isDraggingWindow = false;
}

void Dialogdesktopini::slot_save()
{
    QString rows = ui->lineEdit_rows->text();
    QString column = ui->lineEdit_column->text();
    QString iconSize = ui->lineEdit_iconsize->text();
    QString numHotseatIcons = ui->lineEdit_numhotseaticons->text();
    QString hotseatIconSize = ui->lineEdit_hotseaticonsize->text();

    if (rows.isEmpty() || column.isEmpty() || iconSize.isEmpty() || numHotseatIcons.isEmpty() || hotseatIconSize.isEmpty()) {
        DialogHint::exec_hint(this, tr("hint"), tr("rows,column,iconsize,numhotseaticons or hotseaticonsize can't be empty!"), QString(), DialogHint::TYPE_INFORMATION);
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
    QSettings settings(qApp->applicationDirPath() + "/DesktopConfiguration.ini", QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.setValue("rows", rows);
    settings.setValue("cloumn", column);
    settings.setValue("iconSize", iconSize);
    settings.setValue("numHotseatIcons", numHotseatIcons);
    settings.setValue("hotseatIconSize", hotseatIconSize);

    settings.sync();

    emit signal_DesktopConfigChanged();
    close();
#endif
}

void Dialogdesktopini::slot_notHintToggled(bool checked)
{
    QSettings settings(qApp->applicationDirPath() + "/mmqt2.ini", QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.setValue("cfg/notHintDesktopConfigNextTime", checked);
    settings.sync();
}
