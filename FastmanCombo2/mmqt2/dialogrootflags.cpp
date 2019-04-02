#include "dialogrootflags.h"
#include "ui_dialogrootflags.h"
#include "globalcontroller.h"
#include "appconfig.h"
#include "fastman2.h"

DialogRootFlags::DialogRootFlags(QWidget *parent) :
    QDialog(parent, Qt::CustomizeWindowHint|Qt::WindowCloseButtonHint),
    ui(new Ui::DialogRootFlags)
{
    ui->setupUi(this);

    config = GlobalController::getInstance()->appconfig;
    ui->chk_masterkey_root->setChecked((config->rootFlag & FastManRooter::FLAG_MASTERKEY_ROOT) != 0);
    ui->chk_frama_root->setChecked((config->rootFlag & FastManRooter::FLAG_FRAMA_ROOT) != 0);
    ui->chk_vroot->setChecked((config->rootFlag & FastManRooter::FLAG_VROOT) != 0);
    ui->chk_towel_root->setChecked((config->rootFlag & FastManRooter::FLAG_TOWEL_ROOT) != 0);

    connect(ui->chk_masterkey_root, SIGNAL(toggled(bool)), SLOT(slot_change()));
    connect(ui->chk_frama_root, SIGNAL(toggled(bool)), SLOT(slot_change()));
    connect(ui->chk_vroot, SIGNAL(toggled(bool)), SLOT(slot_change()));
    connect(ui->chk_towel_root, SIGNAL(toggled(bool)), SLOT(slot_change()));
}

DialogRootFlags::~DialogRootFlags()
{
    delete ui;
}

void DialogRootFlags::slot_change() {
    int rootFlag = 0;
    if(ui->chk_masterkey_root->isChecked()) {
        rootFlag |= FastManRooter::FLAG_MASTERKEY_ROOT;
    }
    if(ui->chk_frama_root->isChecked()) {
        rootFlag |= FastManRooter::FLAG_FRAMA_ROOT;
    }
    if(ui->chk_vroot->isChecked()) {
        rootFlag |= FastManRooter::FLAG_VROOT;
    }
    if(ui->chk_towel_root->isChecked()) {
        rootFlag |= FastManRooter::FLAG_TOWEL_ROOT;
    }
    config->rootFlag = rootFlag;
}
