#include "formdevicenode.h"
#include "ui_formdevicenode.h"

#include "zadbdevicenode.h"
#include "adbclient.h"
#include "dialogcmd.h"
#include "dialogdeviceview.h"

#include <QMessageBox>
#include <QMenu>
#include <QFileDialog>
#include <QDateTime>
#include <QProcess>

#include "zstringutil.h"
#include "bundle.h"
#include "zlog.h"

FormDeviceNode::FormDeviceNode(AdbDeviceNode *node, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormDeviceNode)
{
    ui->setupUi(this);
    this->node = node;

    QMenu *m = new QMenu(this);
    m->addAction(tr("details"), this, SLOT(slot_detail()));
#ifdef CMD_DEBUG
    m->addAction(tr("console"), this, SLOT(slot_cmd()));
#endif
    m->addAction(tr("reboot"), this, SLOT(slot_reboot()));
    m->addAction(tr("export log"), this, SLOT(slot_saveLog()));
    ui->btn_manage->setPopupMode(QToolButton::InstantPopup);
    ui->btn_manage->setMenu(m);
}

FormDeviceNode::~FormDeviceNode()
{
    delete ui;
}

void FormDeviceNode::slot_cmd() {
    DialogCmd *dlg = new DialogCmd(node, this);
    dlg->exec();
    delete dlg;
}

RebootThread::RebootThread(AdbDeviceNode *node, QObject *parent)
    : QThread(parent) {
    adb_serial = node->adb_serial;
    connect(this, SIGNAL(finished()), SLOT(deleteLater()));
}

void RebootThread::run() {
    AdbClient adb(adb_serial);
    adb.adb_cmd("reboot:");
}

void FormDeviceNode::slot_reboot() {
    int ret = QMessageBox::question(this, tr("reboot"), tr("are you sure to reboot?"), QMessageBox::Yes, QMessageBox::No);
    if(ret == QMessageBox::Yes) {
        RebootThread *t = new RebootThread(node, this);
        t->start();
    }
}

void FormDeviceNode::slot_detail() {
    DialogDeviceView dv(node, this);
    dv.exec();
}

void FormDeviceNode::slot_saveLog()
{
    QString path = qApp->applicationDirPath() + "/" + node->adb_serial + "_" + QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm-ss.log");
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    tr("Save ZM log"),
                                                    path,
                                                    tr("Log(*.log)"),
                                                    0,
                                                    QFileDialog::DontUseNativeDialog);
    QString cmd = QString("\"" + qApp->applicationDirPath() + "/zxlycon.exe\" -s %1 pull /data/local/tmp/work/zm.log \"%2\"").arg(node->adb_serial).arg(fileName);
    DBG("save zm log, cmd=<%s>\n", qPrintable(cmd));
    QProcess ps;
    ps.execute(cmd);
}

static void setDfInfo(quint64 total, quint64 free, QProgressBar *pb, QLabel *lb) {
    if(total > 0) {
        int max = total/4096;
        int value = (total-free)/4096;
        pb->setMaximum(max);
        pb->setValue(value);
        lb->setText(ZStringUtil::getSizeStr(free));
    } else {
        pb->setMaximum(100);
        pb->setValue(0);
        lb->setText(QObject::tr("unknown"));
    }
}

void FormDeviceNode::slot_refreshUI(AdbDeviceNode *n) {
    if(n != node) {
        return;
    }
    ZAdbDeviceNode *znode = (ZAdbDeviceNode *)n;
    static int last_stat = 255;

    QFontMetrics fm(ui->label_name->font());
    ui->label_name->setText(fm.elidedText(znode->getName(), Qt::ElideRight, ui->label_status->width()));
    ui->label_name->setToolTip(znode->getName());

    if (last_stat != znode->connect_stat) {
        ui->label_status->setText(znode->connect_statname);
        last_stat = znode->connect_stat;
    }

    setDfInfo(znode->zm_info.sys_total, znode->zm_info.sys_free, ui->pb_sysfree, ui->label_sysfree);
    setDfInfo(znode->zm_info.tmp_total, znode->zm_info.tmp_free, ui->pb_datafree, ui->label_datafree);
    setDfInfo(znode->zm_info.store_total, znode->zm_info.store_free, ui->pb_sdfree, ui->label_sdfree);

    ui->label_sysfree->setToolTip("/system");
    ui->pb_sysfree->setToolTip("/system");
    ui->label_datafree->setToolTip(znode->zm_info.tmp_dir);
    ui->pb_datafree->setToolTip(znode->zm_info.tmp_dir);
    ui->label_sdfree->setToolTip(znode->zm_info.store_dir);
    ui->pb_sdfree->setToolTip(znode->zm_info.store_dir);
}

void FormDeviceNode::slot_setHint(AdbDeviceNode *n, const QString &hint) {
    if(n != node) {
        return;
    }

    QFontMetrics fm(ui->label_status->font());
    ui->label_status->setText(fm.elidedText(hint, Qt::ElideRight, ui->label_status->width()));
    ui->label_status->setToolTip(hint);
}

void FormDeviceNode::slot_setProgress(AdbDeviceNode *n, int progress, int max) {
    if(n != node) {
        return;
    }

    ui->pb_main->setMaximum(max);
    ui->pb_main->setValue(progress);
}
