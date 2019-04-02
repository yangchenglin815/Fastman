#ifndef FORMDOWNLOAD_H
#define FORMDOWNLOAD_H
#include <QStandardItemModel>
#include <QWidget>
#include <QMutex>
#include <QThread>
#include <QTreeView>
#include <QStyledItemDelegate>
#include <QUrl>
#include <QSet>
#include <QHostInfo>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QTimer>
#include "zdownloadmanager.h"
#include "adbprocess.h"
#include "zlog.h"
namespace Ui {
class FormDownload;
}

class ZDownloadManager;
class ZDownloadTask;

class Bundle;
class BundleItem;

class ProgressDelegate : public QStyledItemDelegate {
    Q_OBJECT

    QTreeView *m_tv;

public:
    ProgressDelegate(QTreeView *tv, QObject *parent = 0);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};
class TestThread : public QThread
{
	Q_OBJECT
	bool bExit;
	QSet<ZDownloadTask*> tasks;
	QNetworkAccessManager am;
	QMutex mutex;
public:
	TestThread(){bExit = false;}
	~TestThread(){bExit = true;}
	void addTask(ZDownloadTask* task);
	void run();
	void setExit(){bExit = true;}
public slots:
	void OnFinished(QNetworkReply * reply);
};
class DownloadModel : public QStandardItemModel {
	Q_OBJECT
	Bundle *baseBundle;
	ZDownloadManager *dm;
	TestThread testThread;
public:
    QMutex mutex;
    QStringList checkList;
    enum {
        COL_NAME = 0,
        COL_SIZE,
        COL_PROGRESS,
        COL_SPEED,
        COL_STAT,
        COL_ACTION
    };

    enum {
        ACTION_NONE = 0,
        ACTION_START,
        ACTION_PAUSE
    };

    DownloadModel(Bundle *baseBundle, ZDownloadManager *dm, QObject *parent);
	~DownloadModel(){testThread.terminate();}
public slots:
    void setDownloadData(ZDownloadTask *task);
    void setDownloadSpeed(ZDownloadTask *task, int bytePerSecond);

    void slot_clicked(const QModelIndex &index);
    void slot_selAll();
    void slot_selRev();
};

class ItemParseThread : public QThread {
    Q_OBJECT
    BundleItem *item;
public:
    ItemParseThread(BundleItem *item);
    void run();
signals:
    void signal_itemParseDone(BundleItem *item);
};

class GlobalController;
class FormDownload : public QWidget
{
    Q_OBJECT
    GlobalController *controller;
    DownloadModel *model;
    ZDownloadManager *dm;
public:
    explicit FormDownload(ZDownloadManager *dm, QWidget *parent = 0);
    ~FormDownload();

private slots:
    void slot_loadDownloadList();
    void slot_updateItemStatus(ZDownloadTask *task);
    void slot_itemParseDone(BundleItem *item);

    void slot_add();
    void slot_start();
    void slot_stop();
    void slot_cleanup();
signals:
    void signal_itemStatusChanged(BundleItem *item);
private:
    Ui::FormDownload *ui;
};
#endif // FORMDOWNLOAD_H
