#ifndef DIALOGTIDYAPK_H
#define DIALOGTIDYAPK_H

#include <QDialog>
#include <QMutex>
#include <QModelIndex>

namespace Ui {
class DialogTidyApk;
}

class QPoint;
class Bundle;
class BundleItem;
class QStandardItemModel;
class AppConfig;
class DialogTidyApk : public QDialog
{
    Q_OBJECT
    enum {
        COL_NAME = 0,
        COL_ID = 0,
        COL_CHECKABLE = 0,
        COL_PINYIN = 0,
        COL_ICON = 0,
        COL_SIZE,
        COL_VERSION,
        COL_TIME,
        COL_END
    };

signals:
    void signal_checkedAll(bool);

public:
    explicit DialogTidyApk(QWidget *parent = 0);
    ~DialogTidyApk();

    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void keyPressEvent(QKeyEvent *e);

    void initialModel();
    void getUnusedApk();
    int getCheckedApps(QList<BundleItem *> &apps);
    void addItem(BundleItem *, int row);
    void removeItem(BundleItem *bundle);
    void removeItems(QList<BundleItem *> list);
    void resizeColumn();
    bool isAllChecked();
    QStringList getIds(QList<BundleItem *> &);

public slots:
    void slot_itemClicked(const QModelIndex &index);
    void slot_btnClicked();
    void slot_checkedAll();
    void slot_invertChk();
    void slot_refresh();
    void slot_remove();

private:
    Ui::DialogTidyApk *ui;

    int mouseX;
    int mouseY;
    bool m_isDraggingWindow;
    bool isInRange(QWidget *widget, int x, int y);
    QPoint m_lastPoint;
    bool m_dragged;

    AppConfig *m_config;
    Bundle *m_globalBundle;
    QStandardItemModel *m_model;
    QList<BundleItem *> m_apps;

    bool m_isRefreshing;
    QMutex mutex;
};

#endif // DIALOGTIDYAPK_H
