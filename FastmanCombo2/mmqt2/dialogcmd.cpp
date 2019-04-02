#include "dialogcmd.h"
#include "ui_dialogcmd.h"

#include "zadbdevicenode.h"
#include "fastman2.h"

DialogCmd::DialogCmd(AdbDeviceNode *node, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogCmd)
{
    ui->setupUi(this);
    this->node = node;

    ui->lineEdit->setFocus();
    ui->btn_send->setDefault(true);
    connect(ui->btn_send, SIGNAL(clicked()), SLOT(slot_exec()));
}

DialogCmd::~DialogCmd()
{
    delete ui;
}

void DialogCmd::slot_exec() {
    static bool needs_root = false;
    ZAdbDeviceNode *znode = (ZAdbDeviceNode *)node;
    QString cmd = ui->lineEdit->text();
    FastManClient *cli = new FastManClient(znode->adb_serial, znode->zm_port);
    if(cmd == "SWITCH_ROOT") {
        needs_root = true;
        cmd = "id";
    } else if(cmd == "SWITCH_USER") {
        needs_root = false;
        cmd = "id";
    }

    if(needs_root) {
        cli->switchRootCli();
    }

    QByteArray out;
    cli->exec(out, 3, "/system/bin/sh", "-c", cmd.toUtf8().data());
    ui->textBrowser->setPlainText(QString::fromUtf8(out.data(), out.size()));
    delete cli;
}
