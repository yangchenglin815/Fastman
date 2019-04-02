#include <QApplication>
#include "zsingleprocesshelper.h"
#include <QMessageBox>
#include <QTextCodec>

extern const QString SERVERNAME_MMQT2 = "MMQT2_SERVER";

static QString fromGBK(const QByteArray& data) {
    QTextCodec *c = QTextCodec::codecForName("GB2312");
    if(c == NULL) {
        return QString();
    }
    return c->toUnicode(data);
}

int main(int argc, char **argv) {
    QApplication a(argc, argv);

    ZSingleProcessHelper zph(SERVERNAME_MMQT2, fromGBK("≤‚ ‘HOLDER"));
    QString holder;

    if(zph.hasRemote(holder)) {
        QMessageBox::information(0, "found holder!", QString("holder = ")+holder);
    } else {
        zph.takeLock();
        QMessageBox::information(0, "lock taken", "I am the holder");
    }

    return 0;
}
