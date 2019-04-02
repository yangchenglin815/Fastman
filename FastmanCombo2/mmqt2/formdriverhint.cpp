#ifdef _WIN32

#include "formdriverhint.h"
#include "ui_formdriverhint.h"
#include <QProgressBar>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QTimer>
#include "driverhelper.h"
#include "zlog.h"

DriverItemProgressDelegate::DriverItemProgressDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {

}

void DriverItemProgressDelegate::paint(QPainter *painter,
                                       const QStyleOptionViewItem &option,
                                       const QModelIndex &index) const {
    QStyledItemDelegate::paint(painter, option, index);
    if(index.column() == DriverItemModel::COL_PROGRESS) {
        QProgressBar bar;
        bar.setAttribute(Qt::WA_TranslucentBackground);
        bar.setStyleSheet("QProgressBar { margin: 5px; }");
        bar.resize(option.rect.width(), option.rect.height());

        int value = index.data(Qt::UserRole).toInt();
        int max = index.data(Qt::UserRole+1).toInt();
        bar.setValue(value);
        bar.setMaximum(max);
        bar.setTextVisible(false);

        painter->save();
        painter->translate(QPoint(option.rect.x(), option.rect.y()));
        bar.render(painter);
        painter->restore();
    }
}

DriverItemModel::DriverItemModel(QObject *parent)
    : QStandardItemModel(parent) {
    setColumnCount(3);
    QStringList labels;
    labels << tr("Name") << tr("Status") << tr("Progress");
    setHorizontalHeaderLabels(labels);
}

void DriverItemModel::setDriverItemData(DriverDeviceNode *node, const QString& status, int value, int max) {
    if(node == NULL) {
        return;
    }

    mutex.lock();

    int row = -1;
    int i = 0;
    QStandardItem *item = NULL;
    while((item = this->item(i++, COL_NAME)) != NULL) {
        if(item->data(Qt::UserRole).toString() == node->device_id_long) {
            row = --i;
            break;
        }
    }
    if(row == -1) {
        row = rowCount();
        insertRow(row);
    }

    setData(index(row, COL_NAME), node->name);
    setData(index(row, COL_NAME), node->device_id_long, Qt::UserRole);

    setData(index(row, COL_STATUS), status);
    setData(index(row, COL_PROGRESS), value, Qt::UserRole);
    setData(index(row, COL_PROGRESS), max, Qt::UserRole + 1);

    mutex.unlock();
}

FormDriverHint::FormDriverHint(DriverManager *drvm, QWidget *parent) :
    QWidget(parent, Qt::FramelessWindowHint),
    ui(new Ui::FormDriverHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    ui->setupUi(this);
    _drvm = drvm;
    isDraggingWindow = false;
    needsHide = true;

    model = new DriverItemModel(this);
    ui->treeView->setModel(model);
    ui->treeView->setItemDelegateForColumn(model->COL_PROGRESS, new DriverItemProgressDelegate(this));

    ui->treeView->setColumnWidth(model->COL_NAME, 240);
    ui->treeView->setColumnWidth(model->COL_STATUS, 200);
    ui->treeView->setColumnWidth(model->COL_PROGRESS, 80);

    connect(drvm->installThread(),
            SIGNAL(signal_foundDriver(DriverDeviceNode*,bool)),
            SLOT(slot_foundDriver(DriverDeviceNode*,bool)));
    connect(drvm->installThread(),
            SIGNAL(signal_downloadProgress(DriverDeviceNode*,int,int)),
            SLOT(slot_downloadProgress(DriverDeviceNode*,int,int)));
    connect(drvm->installThread(),
            SIGNAL(signal_downloadDriver(DriverDeviceNode*,bool,QString)),
            SLOT(slot_downloadDriver(DriverDeviceNode*,bool,QString)));
    connect(drvm->installThread(),
            SIGNAL(signal_installDriver(DriverDeviceNode*,bool,QString)),
            SLOT(slot_installDriver(DriverDeviceNode*,bool,QString)));
}

FormDriverHint::~FormDriverHint()
{
    delete ui;
}

void FormDriverHint::showEvent(QShowEvent *event) {
    DBG("showEvent\n");
    event->accept();
    qApp->postEvent(this, new QEvent(QEvent::UpdateRequest), Qt::LowEventPriority);
}

void FormDriverHint::keyPressEvent(QKeyEvent *e) {
    e->ignore();
}

void FormDriverHint::mousePressEvent(QMouseEvent *event) {
    // current
    int x = event->pos().x();
    int y = event->pos().y();

    // top left
    int x1 = ui->layout_header->pos().x();
    int y1 = ui->layout_header->pos().y();

    // bottom right
    int x2 = x1 + ui->layout_header->width();
    int y2 = y1 + ui->layout_header->height();

    if(x > x1 && x < x2 && y > y1 && y < y2) {
        lastMousePos = event->pos();
        isDraggingWindow = true;
    }
}

void FormDriverHint::mouseMoveEvent(QMouseEvent *event) {
    if(isDraggingWindow) {
        this->move(event->globalPos()-lastMousePos);
    }
}

void FormDriverHint::mouseReleaseEvent(QMouseEvent *event) {
    isDraggingWindow = false;
}

void FormDriverHint::slot_foundDriver(DriverDeviceNode* device, bool found) {
    slot_show();
    if(found) {
        model->setDriverItemData(device, tr("Downloading..."), 0, 0);
    } else {
        model->setDriverItemData(device, tr("No online driver"), 0, 100);
    }
}

void FormDriverHint::slot_downloadProgress(DriverDeviceNode* device, int value, int max) {
    slot_show();
    model->setDriverItemData(device, tr("Downloading driver"), value, max);
}

void FormDriverHint::slot_downloadDriver(DriverDeviceNode *device, bool success, const QString &hint) {
    slot_show();
    if(success) {
        model->setDriverItemData(device, tr("Installing..."), 0, 0);
    } else {
        model->setDriverItemData(device, tr("Download failed, ")+hint.simplified(), 0, 100);
    }
}

void FormDriverHint::slot_installDriver(DriverDeviceNode *device, bool success, const QString &hint) {
    slot_show();
    if(success) {
        model->setDriverItemData(device, tr("Installed"), 100, 100);
    } else {
        model->setDriverItemData(device, tr("Install failed, ")+hint.simplified(), 0, 100);
    }
}

void FormDriverHint::slot_show() {
    DBG("slot_show\n");
    needsHide = false;
    setVisible(true);
    activateWindow();
    raise();
}

void FormDriverHint::slot_hide() {
    DBG("slot_hide, needsHide=%d\n", needsHide);
    if(needsHide) {
        setVisible(false);
    }
}

void FormDriverHint::slot_hideLater() {
    DBG("hide after 15000 msecs.\n");
    needsHide = true;
    QTimer::singleShot(15000, this, SLOT(slot_hide()));
}

#endif
