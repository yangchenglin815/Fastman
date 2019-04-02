#ifndef DIALOGBUNDLEVIEW_H
#define DIALOGBUNDLEVIEW_H

#include <QObject>
#include <QDialog>
#include <QMutex>
#include <QThread>
#include <QTreeView>
#include <QAbstractItemModel>
#include <QStandardItemModel>

class AppConfig;
class Bundle;
class BundleItem;
class BundleModel;
class QPoint;
class QModelIndex;
class QTreeView;
class QToolButton;

namespace Ui {
class DialogBundleView;
}

// View
class DialogBundleView : public QDialog
{
    Q_OBJECT

signals:
    void signal_bundleChanged();

public:
    explicit DialogBundleView(Bundle *bundle, bool bNewBundle = false, QWidget *parent = 0);
    ~DialogBundleView();

    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void keyPressEvent(QKeyEvent *e);
    void closeEvent(QCloseEvent *e);

    void initGlobalView();
    void initSpecialView();
    void initUI();

    void setAllChecked(QTreeView *tvw, bool checked);
    bool isCheckedAll(QTreeView *tvw);

    void addApp(BundleItem *item);
    void removeApps();

    bool isNeedAddToBundle(BundleItem *item);
    void resizeColumn(QTreeView *tvw);

    bool maybeSave();

public slots:
    void slot_itemClicked(const QModelIndex &index);

    void slot_btnClicked();
    void slot_addGlobal();
    void slot_addLocal();

    void slot_refreshGlobalView();
    void slot_refreshSpecialView();

    void slot_onFilterTextChanged(const QString &text);
    void slot_onChkedStateChanged();
    void slot_onParseAppFinished(BundleItem *item);
    void slot_onItemStatusChanged(BundleItem *item);
    void slot_onSplitterMoved(int, int);
    void slot_addClrBtn();

    void slot_saveBundle();

private:
    Ui::DialogBundleView *ui;

    int mouseX;
    int mouseY;
    bool isDraggingWindow;
    bool isInRange(QWidget *widget, int x, int y);
    QPoint m_lastPoint;
    bool m_dragged;

    AppConfig *m_config;

    Bundle *m_specialBundle;
    BundleModel *m_specialModel;
    QList<BundleItem *> m_appList;

    Bundle *m_globalBundle;
    BundleModel *m_globalModel;

    bool m_bNewBundle;
    bool m_isSaveBtnclicked;
    int successedCnt;
    int upgradedCnt;
    int ignoredCnt;

    QToolButton *clrFilterBtn;
};


// Model
class BundleItem;
class BundleModel : public QStandardItemModel
{
    Q_OBJECT

    QTreeView *m_tvw;

public:
    explicit BundleModel(QTreeView *tvw, QObject *parent = 0);

    enum {
        COL_NAME = 0,
        COL_ID = 0,
        COL_PINYIN = 0,
        COL_CHECKABLE = 0,
        COL_ICON = 0,
        COL_SIZE,        
        COL_VERSION,
        COL_STATUS,
        COL_INSLOCATION,
        COL_TIME,
        COL_END
    };

    void setBundleData(Bundle *bundle);
    void setBundleItemData(BundleItem *item);

    QMutex mutex;
};


// Thread
class LoadAppThread : public QThread
{
    Q_OBJECT
public:
    explicit LoadAppThread(QStringList pathList, QObject *parent = 0);

signals:
    void signal_parseAppFinished(BundleItem *item);
    void signal_progressChanged(int value, int max);
    void signal_stateChanged(const QString &state);

protected:
    void run();

private:
    QStringList m_pathList;
};
#endif // DIALOGBUNDLEVIEW_H
