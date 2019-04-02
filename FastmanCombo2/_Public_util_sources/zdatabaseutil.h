#ifndef ZDATABASEUTIL_H
#define ZDATABASEUTIL_H

#include <QString>
#include <QStringList>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

#include <QVariant>
#include <QByteArray>
#include <stdio.h>

#include "zbytearray.h"
#include "zlog.h"

template<class T>
class ZDatabaseUtil {
    QString tablename;
public:
    ZDatabaseUtil(const QString& tablename, bool *result = 0) {
        this->tablename = tablename;
        QSqlQuery query;
        QString cmd = QString(
                    "CREATE TABLE IF NOT EXISTS %1(id TEXT, data BLOB)").arg(
                    tablename);
        bool ret = query.exec(cmd);
        if (!ret) {
            DBG("ZDatabaseUtil: %s\n", query.lastError().text().toLocal8Bit().data());
        }

        if (result != NULL) {
            *result = ret;
        }
    }

    ZDatabaseUtil(const QString& tablename, int num, bool *result = 0) {
        this->tablename = tablename;
        QSqlQuery query;
        QString cmd = QString("CREATE TABLE IF NOT EXISTS %1(id TEXT, data BLOB, imei TEXT, ctime DATETIME, flag INT, uploadLog TEXT, binId TEXT, mode INT, imei1 TEXT, imei2 TEXT, reFlag INT)").arg(tablename);
        bool ret = query.exec(cmd);
        if (!ret) {
            DBG("ZDatabaseUtil: %s\n", query.lastError().text().toLocal8Bit().data());
        }

        if (result != NULL) {
            *result = ret;
        }
    }

    ZDatabaseUtil(const QString& dbname, const QString& tablename,
                  bool *result = 0) {
        this->tablename = tablename;
        bool ret = false;
        do {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
            db.setDatabaseName(dbname);

            if (!db.open()) {
                DBG("ZDatabaseUtil: %s\n", db.lastError().text().toLocal8Bit().data());
                break;
            }

            QSqlQuery query;
            QString cmd = QString(
                        "CREATE TABLE IF NOT EXISTS %1(id TEXT, data BLOB)").arg(
                        tablename);
            if (!query.exec(cmd)) {
                DBG("ZDatabaseUtil: %s\n", query.lastError().text().toLocal8Bit().data());
                break;
            }

            ret = true;
        } while (0);
        if (result != NULL) {
            *result = ret;
        }
    }

    T getData(const QString& id) {
        QSqlQuery query;
        query.prepare("SELECT id, data FROM " + tablename + " WHERE id = ?");
        query.bindValue(0, id);
        query.exec();
        if (query.next()) {
            QByteArray data = query.value(1).toByteArray();
            ZByteArray::decode_bytearray(data);
            T ret = fromByteArray(data);
            setId(ret, id);
            return ret;
        }
        return NULL;
    }

    bool addFlashData(T data, QString imei, QString hint, QString binId, int mode) {
        QSqlQuery query;
        int cnt = 0;
        bool ret;
        QByteArray d = toByteArray(data);
        ZByteArray::encode_bytearray(d);
        QString imei1,imei2;
        QStringList list = imei.split(",");
        imei1 = list.at(0);
        imei2 = "";
        if(list.size() == 2) {
            imei2 = list.at(1);
        }
        for(int i=0; i<list.size(); i++) {
            QString sImei = list.at(i);
            query.prepare("select count(*) from " + tablename + " where imei1 = ? or imei2 = ?");
            query.bindValue(0, sImei);
            query.bindValue(1, sImei);
            query.exec();
            while(query.next()) {
                cnt = query.value(0).toInt();
            }
            if(cnt > 0) {
                cnt = 1;
                break;
            }
        }
        query.prepare("INSERT INTO " + tablename + "(id,data,imei,ctime,flag,uploadLog,binId,mode,imei1,imei2,reFlag) VALUES(?,?,?,datetime('now','localtime'),?,?,?,?,?,?,?)");
        query.bindValue(0, getId(data));
        query.bindValue(1, d);
        query.bindValue(2, imei);
        query.bindValue(3, 0);
        query.bindValue(4, hint);
        query.bindValue(5, binId);
        query.bindValue(6, mode);
        query.bindValue(7, imei1);
        query.bindValue(8, imei2);
        query.bindValue(9, cnt);
        ret = query.exec();
        if (!ret) {
            DBG("error: %s\n", query.lastError().text().toLocal8Bit().data());
        }
        return ret;
    }

    QString getDataNum() {
        QSqlQuery query;
        QString dataNum;
        query.exec("select count(*) from " + tablename + " where ctime > datetime('now','start of day')");
        while(query.next()){
            dataNum = query.value(0).toString();
        }
        return dataNum;
    }

    QString getLocalNum() {
        QSqlQuery query;
        QString num;
        query.exec("select count(distinct(imei)) from " + tablename + " where mode = 0");
        while(query.next()){
            num = query.value(0).toString();
        }
        return num;
    }

    QString getRepeatNum() {
        QSqlQuery query;
        QString num;
        query.exec("select count(*) from " + tablename + " where reFlag = 1");
        while(query.next()) {
            num = query.value(0).toString();
        }
        return num;
    }

    QString getSuccessNum() {
        QSqlQuery query;
        QString num;
        query.exec("select count(*) from " + tablename + " where flag = 1");
        while(query.next()) {
            num = query.value(0).toString();
        }
        return num;
    }

    QString getFailureNum() {
        QSqlQuery query;
        QString num;
        query.exec("select count(*) from " + tablename + " where flag = 2");
        while(query.next()) {
            num = query.value(0).toString();
        }
        return num;
    }

    bool addData(T data) {
        QSqlQuery query;
        QByteArray d = toByteArray(data);
        ZByteArray::encode_bytearray(d);
        query.prepare("INSERT INTO " + tablename + "(id,data) VALUES(?,?)");
        query.bindValue(0, getId(data));
        query.bindValue(1, d);
        bool ret = query.exec();
        if (!ret) {
            DBG("error: %s\n", query.lastError().text().toLocal8Bit().data());
        }
        return ret;
    }

    bool updateUploadData(T data, QString imei, QString hint) {
        QSqlQuery query;
        bool ret;
        if(!hint.contains("ERR_01")) {
            query.prepare("UPDATe" + tablename + "SET falg = 2, uploadLog = ? WHERE imei = ? AND flag = 0");
            QByteArray d = toByteArray(data);
            ZByteArray::encode_bytearray(d);
            query.bindValue(0, hint);
            query.bindValue(1, imei);
            ret = query.exec();
            if (!ret) {
                DBG("error: %s\n", query.lastError().text().toLocal8Bit().data());
            }
        }
        return ret;
    }

    bool updateData(T data, QString imei, QString hint) {
        QSqlQuery query;
        query.prepare("UPDATE " + tablename + " SET flag = 1, uploadLog = ? WHERE imei = ? AND flag = 0");
        QByteArray d = toByteArray(data);
        ZByteArray::encode_bytearray(d);
        query.bindValue(0, hint);
        query.bindValue(1, imei);
        bool ret = query.exec();
        if (!ret) {
            DBG("error: %s\n", query.lastError().text().toLocal8Bit().data());
        }
        return ret;
    }

    bool setData(T data) {
        QSqlQuery query;
        query.prepare("UPDATE " + tablename + " SET data = ? WHERE id = ?");
        QByteArray d = toByteArray(data);
        ZByteArray::encode_bytearray(d);
        query.bindValue(0, d);
        query.bindValue(1, getId(data));
        bool ret = query.exec();
        if (!ret || query.numRowsAffected() <= 0) {
            query.prepare("INSERT INTO " + tablename + "(id,data) VALUES(?,?)");
            query.bindValue(0, getId(data));
            query.bindValue(1, d);
            ret = query.exec();
        }
        if (!ret) {
            DBG("error: %s\n", query.lastError().text().toLocal8Bit().data());
        }
        return ret;
    }

    QList<T> getFlashData() {
        QList<T> list;
        QSqlQuery query;
        query.prepare("SELECT id,data,imei FROM " + tablename + " where flag = ?");
        query.bindValue(0, 0);
        query.exec();
        while (query.next()) {
            QString id = query.value(0).toString();
            QByteArray data = query.value(1).toByteArray();
            QString imei = query.value(2).toString();
            DBG("upload imei : %s\n", qPrintable(imei));
            ZByteArray::decode_bytearray(data);
            T ret = fromByteArray(data);
            setId(ret, id);
            list.append(ret);
        }
        return list;
    }

    QList<T> getAllData() {
        QList<T> list;
        QSqlQuery query;
        query.exec("SELECT id,data FROM " + tablename);
        while (query.next()) {
            QString id = query.value(0).toString();
            QByteArray data = query.value(1).toByteArray();
            ZByteArray::decode_bytearray(data);
            T ret = fromByteArray(data);
            setId(ret, id);
            list.append(ret);
        }
        return list;
    }

    QList<T> getAllData(const QString& ids) {
        QStringList idList = ids.split(',', QString::SkipEmptyParts);
        QList<T> list;
        QSqlQuery query;
        foreach(const QString& id, idList) {
            query.prepare("SELECT id, data FROM " + tablename + " WHERE id = ?");
            query.bindValue(0, id);
            query.exec();
            if (query.next()) {
                QByteArray data = query.value(1).toByteArray();
                ZByteArray::decode_bytearray(data);
                T ret = fromByteArray(data);
                setId(ret, id);
                list.append(ret);
            }
        }
        return list;
    }

    void setAllData(const QList<T>& list) {
        QSqlDatabase db = QSqlDatabase::database();
        db.transaction();
        foreach (T t, list) {
            setData(t);
        }
        db.commit();
    }

    bool removeData(T data) {
        QSqlQuery query;
        query.prepare("DELETE FROM " + tablename + " WHERE id = ?");
        query.bindValue(0, getId(data));
        return query.exec();
    }

    bool removeDataById(const QString& id) {
        QSqlQuery query;
        query.prepare("DELETE FROM " + tablename + " WHERE id = ?");
        query.bindValue(0, id);
        return query.exec();
    }

    int removeAllData(const QList<T>& list) {
        QSqlDatabase db = QSqlDatabase::database();
        QSqlQuery query;
        int ret = 0;

        db.transaction();
        foreach(T t, list) {
            query.prepare("DELETE FROM "+tablename+" WHERE id = ?");
            query.bindValue(0, getId(t));
            if(query.exec()) {
                ret ++;
            }
        }
        db.commit();
        return ret;
    }

    int removeAllData(const QString& ids) {
        QStringList idList = ids.split(',', QString::SkipEmptyParts);
        return removeAllData(idList);
    }

    int removeAllData(const QStringList& idList) {
        if(idList.isEmpty()) {
            return 0;
        }

        QSqlDatabase db = QSqlDatabase::database();
        QSqlQuery query;
        int ret = 0;

        db.transaction();
        foreach(const QString& id, idList) {
            query.prepare("DELETE FROM "+tablename+" WHERE id = ?");
            query.bindValue(0, id);
            if(query.exec()) {
                ret ++;
            }
        }
        db.commit();
        return ret;
    }

    bool clearFlashData() {
        QSqlQuery query;
        return query.exec("DELETE FROM " + tablename + " where ctime <= datetime('now', 'localtime', '-10 day')");
    }

    bool clearData() {
        QSqlQuery query;
        return query.exec("DELETE FROM " + tablename);
    }

    bool dropTable() {
        QSqlQuery query;
        return query.exec("DROP TABLE " + tablename);
    }

    int deleteOldData(int keepCount) {
        int ret = 0;
        int lastRowId = -1;
        QSqlQuery query;
        query.prepare("SELECT rowid FROM "+tablename+" ORDER BY rowid DESC LIMIT 1");
        query.exec();
        if(query.next()) {
            lastRowId = query.value(0).toInt();
            DBG("lastRowId = %d\n", lastRowId);
            query.clear();
        }

        query.prepare("DELETE FROM "+tablename+" WHERE rowid < ?");
        query.bindValue(0, lastRowId - keepCount);
        if(query.exec()) {
            ret = query.numRowsAffected();
            DBG("deleted %d rows\n", ret);
        }
        return ret;
    }

    virtual QByteArray toByteArray(T obj) = 0;
    virtual T fromByteArray(const QByteArray& data) = 0;
    virtual QString getId(T obj) = 0;
    virtual void setId(T obj, const QString& id) = 0;
};

#endif // ZDATABASEUTIL_H
