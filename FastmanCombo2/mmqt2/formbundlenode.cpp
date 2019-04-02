#include "formbundlenode.h"
#include "ui_formbundlenode.h"

#include "globalcontroller.h"
#include "appconfig.h"
#include "bundle.h"

FormBundleNode::FormBundleNode(Bundle *bundle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormBundleNode)
{
    ui->setupUi(this);
    this->bundle = bundle;
    connect(ui->checkBox, SIGNAL(clicked(bool)), SLOT(slot_bundleClicked(bool)));
    ui->label_description->hide();
}

FormBundleNode::~FormBundleNode()
{
    delete ui;
}

void FormBundleNode::slot_refreshUI(Bundle *b) {
    if(b != bundle) {
        return;
    }
    ui->label_name->setText(bundle->name);
    ui->label_description->setText(bundle->getDescription());
}

void FormBundleNode::slot_bundleClicked(bool checked) {
    emit signal_bundleClicked(bundle, checked);
}

void FormBundleNode::setChecked(bool b) {
    ui->checkBox->setChecked(b);
}

bool FormBundleNode::isChecked() {
    return ui->checkBox->isChecked();
}

void FormBundleNode::setIcon(int index) {
    ui->label_icon->setPixmap(Bundle::getIcon(index));
}
