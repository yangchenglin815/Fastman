#include "dialoglogview.h"
#include "ui_dialoglogview.h"
#include "dialoglogshow.h"

#include "QJson/parser.h"
#include <QClipboard>
#include <QMouseEvent>

LogDataModel::LogDataModel(QObject *parent)
    : QStandardItemModel(parent) {
    QStringList labels;
    labels.append(tr("key"));
    labels.append(tr("value"));
    this->setColumnCount(2);
    this->setHorizontalHeaderLabels(labels);
}

void LogDataModel::setLogData(InstLog *d) {
    QJson::Parser p;
    QVariantMap map = p.parse(d->json.toUtf8()).toMap();
    QMapIterator<QString, QVariant> it(map);
    while(it.hasNext()) {
        it.next();

        int row = rowCount();
        insertRow(row);

        setData(index(row, COL_KEY), it.key());
        setData(index(row, COL_VALUE), it.value());
    }
    int row = rowCount();
    insertRow(row);
    setData(index(row, COL_KEY), tr("result"));
    setData(index(row, COL_VALUE), d->result);
}

DialogLogView::DialogLogView(InstLog *d, QWidget *parent) :
    QDialog(parent, Qt::FramelessWindowHint),
    ui(new Ui::DialogLogView)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_TranslucentBackground);
    ui->label->setText(tr("Double click to copy - ") + LogModel::getTimeStr(d->time));

    LogDataModel *model = new LogDataModel(this);
    ui->treeView->setModel(model);
    model->setLogData(d);

    connect(ui->treeView, SIGNAL(doubleClicked(QModelIndex)), SLOT(doubleClicked(QModelIndex)));
}

DialogLogView::~DialogLogView()
{
    delete ui;
}

void DialogLogView::doubleClicked(const QModelIndex &index) {
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(index.data().toString());
}

void DialogLogView::mousePressEvent(QMouseEvent *event) {
    if (event->buttons() = Qt::LeftButton) {
        int nx = event->pos().x();
        int ny = event->pos().y();

        int x = ui->title_wiget->pos().x();
        int y = ui->title_wiget->pos().y();

        int x1 = x + ui->title_wiget->width();
        int y1 = y + ui->title_wiget->height();

        if (nx > x && nx < x1 && ny > y && ny < y1) {
            isdraggwindow = true;
        }
    }
    lastmouse = event->pos();
}

void DialogLogView::mouseMoveEvent(QMouseEvent *event) {
    if (isdraggwindow) {
        this->move(event->globalPos() - lastmouse);
    }
}

void DialogLogView::mouseReleaseEvent(QMouseEvent *event) {
    isdraggwindow = false;
}
