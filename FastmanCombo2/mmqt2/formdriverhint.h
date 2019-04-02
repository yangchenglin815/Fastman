#ifndef FORMDRIVERHINT_H
#define FORMDRIVERHINT_H

#include <QWidget>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QMutex>

class DriverItemProgressDelegate : public QStyledItemDelegate {
public:
    DriverItemProgressDelegate(QObject *parent);
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

class DriverDeviceNode;
class DriverManager;

class DriverItemModel : public QStandardItemModel {
    QMutex mutex;
public:
    enum {
        COL_NAME,
        COL_STATUS,
        COL_PROGRESS
    };

    DriverItemModel(QObject *parent);
    void setDriverItemData(DriverDeviceNode *node, const QString& status, int value, int max);
};


namespace Ui {
class FormDriverHint;
}

class FormDriverHint : public QWidget
{
    Q_OBJECT
    DriverManager *_drvm;
    DriverItemModel *model;

    bool isDraggingWindow;
    QPoint lastMousePos;

    bool needsHide;
public:
    explicit FormDriverHint(DriverManager *drvm, QWidget *parent = 0);
    ~FormDriverHint();

    void showEvent(QShowEvent * event);
    void keyPressEvent(QKeyEvent *e);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

private slots:
    void slot_foundDriver(DriverDeviceNode *device, bool found);
    void slot_downloadProgress(DriverDeviceNode *device, int value, int total);
    void slot_downloadDriver(DriverDeviceNode *device, bool success, const QString& hint);
    void slot_installDriver(DriverDeviceNode *device, bool success, const QString& hint);

    void slot_show();
    void slot_hide();
    void slot_hideLater();
private:
    Ui::FormDriverHint *ui;
};

#endif // FORMDRIVERHINT_H
