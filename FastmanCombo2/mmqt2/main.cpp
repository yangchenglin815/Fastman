#ifndef NO_GUI
#include "zapplication.h"
#include "zsingleprocesshelper.h"
#include "formmain.h"
#include <QTextCodec>
#include <QMessageBox>
#else
#include <QCoreApplication>
#include "coremain.h"
#endif

#include <QDir>
#include <QTranslator>
#include <QTime>
#include <QProxyStyle>
#include <QSettings>
#include <QtCore/qstring.h>
#include "qtextstream.h"

#include "zlog.h"

#define _TIME_ qPrintable (QTime::currentTime().toString ("hh:mm:ss:zzz"))

#ifndef NO_GUI
extern const QString SERVERNAME_MMQT2 = "MMQT2_SERVER";
extern const QString SERVERNAME_HELPER = "HELPER_SERVER";

class MySPH : public ZSingleProcessHelper {
    QWidget *m_Win;
public:
    MySPH(const QString &name, QObject *parent = 0)
        : ZSingleProcessHelper(name, QString::fromUtf8("秒装助手"), parent) {
        m_Win = NULL;
    }

    void setWin(QWidget *w) {
        m_Win = w;
    }

    void handleQuitRequest() {
        if (NULL != m_Win) {
            m_Win->close();
        }
    }
};
#endif

int main(int argc, char *argv[]) {
#ifndef NO_GUI
    ZApplication a(argc, argv);
#else
    QCoreApplication a(argc, argv);
#endif

    QDir::setCurrent(a.applicationDirPath());
    a.addLibraryPath(a.applicationDirPath() + "/plugins");

    //指定默认编码
//    QTextCodec *codec = QTextCodec::codecForName("UTF-8");
//    QTextCodec::setCodecForTr(codec);
//    QTextCodec::setCodecForLocale(QTextCodec::codecForLocale());
//    QTextCodec::setCodecForCStrings(QTextCodec::codecForLocale());

    QTranslator t;
    t.load("fastman_all2.qm");
    a.installTranslator(&t);

#ifndef NO_GUI
    MySPH single(SERVERNAME_MMQT2);
    QString holder;
    if (single.hasRemote(holder)) {
        QMessageBox::information(NULL, QObject::tr("Hint"), QObject::tr("%1 is running, close it before using %2")
                                 .arg(holder).arg(QString::fromUtf8("秒装助手")));
        single.activateRemote();
        return 0;
    }
    single.takeLock();

    FormMain w;
    w.hide();

    single.setWin(&w);
    single.setActivationWindow(&w);
#else
    CoreMain w;
#endif
    return a.exec();
}
