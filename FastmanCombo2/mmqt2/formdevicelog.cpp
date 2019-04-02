#include "formdevicelog.h"
#include "ui_formdevicelog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QDesktopServices>

#include "zadbdevicenode.h"

FormDeviceLog::FormDeviceLog(AdbDeviceNode *node, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormDeviceLog),
    logFilePath("")
{
    ui->setupUi(this);
    this->node = node;

    ui->textBrowser->setOpenExternalLinks(true);
}

FormDeviceLog::~FormDeviceLog()
{
    delete ui;
}

void FormDeviceLog::slotDeviceLog(AdbDeviceNode *n, const QString &log, int type, bool needShow) {
    if (n != node) {
        return;
    }

    // write in file
    ZAdbDeviceNode *znode = (ZAdbDeviceNode *)node;
    if (!znode->logFileName.isEmpty()) {
        QString currentDate = QDate::currentDate().toString("yyyy-MM-dd");
        QString dirPath = qApp->applicationDirPath() + "/log/" + currentDate;
        QDir dir(dirPath);
        dir.mkpath(dirPath);

        QFile file(logFilePath = dirPath + "/" + znode->logFileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
            QString curDateTime = QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss:zzz] ").toLocal8Bit().data();
            file.write(curDateTime.toLocal8Bit());
            file.write(log.toLocal8Bit());
            file.write("\r\n");
            file.flush();
            file.close();
        }
    }

    // show on textBrowser
    if (needShow) {
        ui->textBrowser->clearHistory();

        // save
        int fw = ui->textBrowser->fontWeight();
        QColor tc = ui->textBrowser->textColor();

        // append
        switch (type) {
        case LOG_NORMAL:
            ui->textBrowser->setFontWeight(QFont::Normal);
            ui->textBrowser->setTextColor(QColor("#000000"));
            break;
        case LOG_UNPID:
            ui->textBrowser->setFontWeight(QFont::DemiBold);
            ui->textBrowser->setTextColor(QColor("#ED1C24"));
            break;
        case LOG_STEP:
            ui->textBrowser->setFontWeight(QFont::DemiBold);
            ui->textBrowser->setTextColor(QColor("#00ABEB"));
            break;
        case LOG_WARNING:
            ui->textBrowser->setFontWeight(QFont::DemiBold);
            ui->textBrowser->setTextColor(QColor("#FFC90E"));
            break;
        case LOG_ERROR:
            ui->textBrowser->setFontWeight(QFont::DemiBold);
            ui->textBrowser->setTextColor(QColor("#ED1C24"));
            break;
        case LOG_FINISHED:
            ui->textBrowser->setFontWeight(QFont::DemiBold);
            ui->textBrowser->setTextColor(QColor("#22B14C"));
            break;
        }

        QString now = QDateTime::currentDateTime().toString("[hh:mm:ss:zzz] ");
        ui->textBrowser->append(now + log);

        // restore
        ui->textBrowser->setFontWeight(fw);
        ui->textBrowser->setTextColor(tc);
        ui->textBrowser->repaint();
    }
}

void FormDeviceLog::slot_clearLog(AdbDeviceNode *n)
{
    ui->textBrowser->clear();
}

void FormDeviceLog::slot_openLogFile()
{
    QDesktopServices::openUrl(QUrl(QString("file:///%1").arg(logFilePath), QUrl::TolerantMode));
}
