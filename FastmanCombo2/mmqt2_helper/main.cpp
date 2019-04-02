#include "trackermain.h"
#include <QApplication>
#include <QDir>
#include "zlocalserver.h"
#include "zlocalclient.h"
#include "zsingleprocesshelper.h"
#include "zlog.h"

extern const QString SERVERNAME_MMQT2 = "MMQT2_SERVER";
extern const QString SERVERNAME_HELPER = "HELPER_SERVER";

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    if (a.argc() > 1 && QString(a.argv()[1]) == "-q") {
        DBG("Execute the cmd: -q\n");
        ZSingleProcessHelper::killRemote(SERVERNAME_MMQT2);
        ZSingleProcessHelper::killRemote(SERVERNAME_HELPER);
        exit(0);
    }

    ZSingleProcessHelper single(SERVERNAME_HELPER);
    if (single.hasRemote()) {
        DBG("There is already a instance running.\n");
        return 0;
    }
    single.takeLock();

    QDir::setCurrent(a.applicationDirPath());
    a.addLibraryPath(a.applicationDirPath() + "/plugins");

    TrackerMain w;

    return a.exec();
}
