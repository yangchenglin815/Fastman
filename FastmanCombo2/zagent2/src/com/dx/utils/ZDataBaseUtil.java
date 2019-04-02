package com.dx.utils;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.support.v4.util.ArrayMap;

public abstract class ZDataBaseUtil<TypeData> {
    final String TAG = "ZDataBaseUtil";

    class ZDataBase extends SQLiteOpenHelper {
        SQLiteDatabase db;

        public ZDataBase(Context context, String dbname) {
            super(context, dbname, null, 1);
            db = this.getWritableDatabase();
        }

        void destroy() {
            db.close();
            close();
        }

        @Override
        public void onCreate(SQLiteDatabase db) {
            Log.i(TAG, "onCreate");

        }

        @Override
        public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
            Log.i(TAG, "onUpgrade " + oldVersion + "->" + newVersion);
        }

        boolean createTable(String tablename) {
            try {
                String sql = "CREATE TABLE IF NOT EXISTS " + tablename + "(id TEXT, data BLOB);";
                db.execSQL(sql);
                return true;
            } catch (Exception e) {
                Log.printStackTrace(e);
            }
            return false;
        }

        boolean setData(String tablename, String id, byte[] data) {
            ContentValues values = new ContentValues();
            values.put("id", id);
            values.put("data", data);

            String[] args = { id };
            long n = db.update(tablename, values, "id=?", args);
            if (n == 0) {
                n = db.insert(tablename, null, values);
            }
            Log.i(TAG, "setData n = " + n);
            return n >= 0;
        }

        Cursor getDataCursor(String tablename, String id) {
            String[] args = { id };
            return db.query(tablename, null, id == null ? null : "id=?", id == null ? null : args, null, null, null);
        }

        byte[] getData(String tablename, String id) {
            byte[] ret = null;
            Cursor cursor = getDataCursor(tablename, id);
            if (cursor != null) {
                if (cursor.moveToNext()) {
                    ret = cursor.getBlob(1);
                }
                cursor.close();
            }
            return ret;
        }

        int remove(String tablename, String id) {
            String[] args = { id };
            return db.delete(tablename, id == null ? null : "id=?", id == null ? null : args);
        }

        public boolean drop(String tablename) {
            try {
                String sql = "DROP TABLE IF EXISTS " + tablename;
                db.execSQL(sql);
                return true;
            } catch (Exception e) {
                Log.printStackTrace(e);
            }
            return false;
        }
    }

    ZDataBase database;

    public ZDataBaseUtil(Context context, String dbname) {
        database = new ZDataBase(context, dbname);
    }

    public void destroy() {
        database.destroy();
        database = null;
    }

    public boolean createTable(String tablename) {
        synchronized (database) {
            return database.createTable(tablename);
        }
    }

    public boolean dropTable(String tablename) {
        synchronized (database) {
            return database.drop(tablename);
        }
    }

    public boolean setData(String tablename, TypeData obj) {
        String id = getId(obj);
        synchronized (database) {
            return database.setData(tablename, id, toByteArray(obj));
        }
    }

    public TypeData getData(String tablename, String id) {
        byte[] data = null;
        synchronized (database) {
            data = database.getData(tablename, id);
        }
        TypeData obj = fromByteArray(data);
        setId(obj, id);
        return obj;
    }

    public void setAllData(String tablename, List<TypeData> objList) {
        synchronized (database) {
            database.db.beginTransaction();
            for (TypeData obj : objList) {
                String id = getId(obj);
                database.setData(tablename, id, toByteArray(obj));
            }
            database.db.endTransaction();
        }
    }

    public List<TypeData> getAllData(String tablename) {
        List<TypeData> ret = new ArrayList<TypeData>();
        synchronized (database) {
            Cursor cursor = database.getDataCursor(tablename, null);
            if (cursor != null) {
                while (cursor.moveToNext()) {
                    String id = cursor.getString(0);
                    byte[] data = cursor.getBlob(1);
                    TypeData obj = fromByteArray(data);
                    setId(obj, id);
                    ret.add(obj);
                }
                cursor.close();
            }
        }
        return ret;
    }

    public Map<String, TypeData> getAllDataMap(String tablename) {
        Map<String, TypeData> ret = new ArrayMap<String, TypeData>();
        synchronized (database) {
            Cursor cursor = database.getDataCursor(tablename, null);
            if (cursor != null) {
                try {
                    while (cursor.moveToNext()) {
                        String id = cursor.getString(0);
                        byte[] data = cursor.getBlob(1);
                        TypeData obj = fromByteArray(data);
                        if (obj != null) {
                            setId(obj, id);
                            ret.put(id, obj);
                        }
                    }
                } catch (Exception e) {
                    // TODO: handle exception
                    Log.e(TAG, "getAllDataMap() \\ " + e.toString());
                }
                cursor.close();
            }
        }
        Log.i("", "getAllData from " + tablename + " done.");
        return ret;
    }

    public int getCount(String tablename) {
        int ret = -1;
        synchronized (database) {
            String columns[] = { "rowid" };
            Cursor cursor = database.db.query(tablename, columns, null, null, null, null, null);
            if (cursor != null) {
                ret = cursor.getCount();
                cursor.close();
            }
        }
        Log.i(TAG, tablename + " => count=" + ret);
        return ret;
    }

    public int remove(String tablename, TypeData obj) {
        String id = getId(obj);
        synchronized (database) {
            return database.remove(tablename, id);
        }
    }

    public int removeById(String tablename, String id) {
        synchronized (database) {
            return database.remove(tablename, id);
        }
    }

    public int removeAll(String tablename, List<TypeData> objList) {
        int ret = 0;
        synchronized (database) {
            database.db.beginTransaction();
            try {
                for (TypeData obj : objList) {
                    String id = getId(obj);
                    ret += database.remove(tablename, id);
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
            database.db.setTransactionSuccessful();
            database.db.endTransaction();
        }
        return ret;
    }

    public int removeAll(String tablename) {
        int ret = 0;
        synchronized (database) {
            ret = database.remove(tablename, null);
        }
        return ret;
    }

    public abstract byte[] toByteArray(TypeData obj);

    public abstract TypeData fromByteArray(byte[] data);

    public abstract String getId(TypeData obj);

    public void deleteOldData(String tablename, int maxKeepRows) {
        synchronized (database) {
            int lastRowId = -1;
            String columns[] = { "rowid" };
            Cursor cursor = database.db.query(tablename, columns, null, null, null, null, "rowid desc", "1");
            if (cursor != null) {
                if (cursor.moveToNext()) {
                    lastRowId = cursor.getInt(0);
                }
                cursor.close();
            }

            Log.i(TAG, "last rowid = " + lastRowId);
            String args[] = { Integer.toString(lastRowId - maxKeepRows) };
            int count = database.db.delete(tablename, "rowid < ?", args);
            Log.i(TAG, "delete count = " + count);
        }
    }

    public abstract void setId(TypeData obj, String id);
}
