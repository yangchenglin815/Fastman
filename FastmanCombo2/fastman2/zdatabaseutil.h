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
