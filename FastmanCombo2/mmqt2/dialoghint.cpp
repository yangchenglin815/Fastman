#include "dialoghint.h"
#include "ui_dialoghint.h"

#include <QSettings>
#include <QMouseEvent>
#include <QScrollArea>
DialogHint::DialogHint(QWidget *parent) :
    QDialog(parent, Qt::FramelessWindowHint),
	ui(new Ui::DialogHint) {
    ui->setupUi(this);
    setAttribute(Qt::WA_TranslucentBackground);

    connect(ui->btn_ok, SIGNAL(clicked()), this, SLOT(accept()));
    connect(ui->btn_cancel, SIGNAL(clicked()), this, SLOT(reject()));
}

int DialogHint::exec_hint(QWidget *parent, const QString &title, const QString &message,
                          const QString &tag, int type) {
    QSettings config("mmqt2.ini", QSettings::IniFormat);
    if (!tag.isEmpty()) {
        int retvalue = config.value("hint/" + tag, -1).toInt();
        if (retvalue != -1) {
            return retvalue;
        }
    }

    DialogHint *dlg = new DialogHint(parent);
    dlg->ui->label_title->setText(title);
    if(type == TYPE_YESNO){
        dlg->ui->message_browser->setFontWeight(QFont::DemiBold);
        dlg->ui->message_browser->setTextColor(QColor("#ED1C24"));
    }
    if(type == TYPE_INFORMATION){
        dlg->ui->message_browser->setFontWeight(QFont::DemiBold);
        dlg->ui->message_browser->setTextColor(QColor("#FFC90E"));
    }
    dlg->ui->message_browser->setText(message);


    if (tag.isEmpty()) {
        dlg->ui->hint_checkBox->hide();
    }

    switch (type) {
    case TYPE_INFORMATION:
        dlg->ui->btn_cancel->hide();
        dlg->ui->btn_ok->setText(tr("Ok"));
        dlg->ui->hint_checkBox->setText(tr("Don't show this again"));        
        break;
    case TYPE_YESNO:
        dlg->ui->btn_ok->setText(tr("Yes"));
        dlg->ui->btn_cancel->setText(tr("No"));
        dlg->ui->hint_checkBox->setText(tr("Remember my choice"));
        break;
    case TYPE_OKCANCEL:
        dlg->ui->btn_ok->setText(tr("Ok"));
        dlg->ui->btn_cancel->setText(tr("Cancel"));
        dlg->ui->hint_checkBox->setText(tr("Remember my choice"));
        break;
    default:
        break;
    }

    int ret = dlg->exec();
    if (dlg->ui->hint_checkBox->checkState() == Qt::Checked) {
        config.setValue("hint/" + tag, ret);
        config.sync();
    }
    delete dlg;
    return ret;
}

void DialogHint::mousePressEvent(QMouseEvent *event) {
    if (event->buttons() = Qt::LeftButton) {
        int nx = event->pos().x();
        int ny = event->pos().y();

        int x = ui->header_widget->pos().x();
        int y = ui->header_widget->pos().y();

        int x1 = x + ui->header_widget->width();
        int y1 = y + ui->header_widget->height();

        if (nx > x && nx < x1 && ny > y && ny < y1) {
            isdraggwindow = true;
        }
    }
    lastmouse = event->pos();
}

void DialogHint::mouseMoveEvent(QMouseEvent *event) {
    if (isdraggwindow) {
        this->move(event->globalPos() - lastmouse);
    }
}

void DialogHint::mouseReleaseEvent(QMouseEvent *event) {
    isdraggwindow = false;
}

DialogHint::~DialogHint() {
    delete ui;
}
