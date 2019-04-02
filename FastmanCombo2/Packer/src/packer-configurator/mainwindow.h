#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    enum LogType {
        LOG_NORMAL = 0,
        LOG_STEP,
        LOG_WARNING,
        LOG_ERROR,
        LOG_FINISHED
    };

private slots:
    void slot_programChanged(const QString &program);
    void slot_chooseTargetDir();
    void slot_chooseIFWDir();

    void slot_addLog(const QString &msg, int type = LOG_NORMAL);
    void slot_apply();
    bool slot_applyCheck();

    void slot_chooseTargetDirDual();
    void slot_chooseNSISDir();
    void slot_versionChangedDual(const QString &version);
    void slot_applyDual();
    bool slot_applyCheckDual();

    bool slot_removeSvnFile(const QString &targetDir, QString &hint);
    bool slot_restoreSvnFile();

private:
    Ui::MainWindow *ui;

    void init();

    void loadConfigXML();
    void loadRootXML();
    void loadReadmeXML();
    void loadIncrementalXML();
    void setConfigXML();
    void setRootXML();
    void setReadmeXML();
    void setIncrementalXML();
    void loadCfg();
    void saveCfg();

    QString m_program;
    QString m_targetDir;
    QString m_IFWDir;

    QString m_NSISDir;
    QString m_targetDirDual;
    QString m_versionDual;

    QMap<QString, QString> m_svnFilesMap;
};
#endif // MAINWINDOW_H
