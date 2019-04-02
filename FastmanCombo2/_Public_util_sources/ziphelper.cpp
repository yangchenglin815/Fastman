#include "ziphelper.h"
#include <QStringList>
#include <QDir>
#include <QFile>
#include <QFileInfoList>
#include "unzip.h"
#include "zip.h"
#include "zlog.h"

extern QString g_tmpDriverPath;

bool ZipHelper::readEntryList(const QString &path, QStringList &list) {
    unzFile file = unzOpen64(path.toLocal8Bit().data(), 0);
    if(file == NULL) {
        DBG("unzOpen '%s' fail!\n", path.toLocal8Bit().data());
        return false;
    }

    bool ret = true;
    unz_global_info global_info;
    unz_file_info file_info;
    char entry[512];
    list.clear();

    int r = unzGetGlobalInfo(file, &global_info);
    for(uLong i=0; i<global_info.number_entry; i++) {
        if((r = unzGetCurrentFileInfo(file, &file_info, entry, sizeof(entry), NULL, 0, NULL, 0)) != UNZ_OK) {
            DBG("unzGetCurrentFileInfo fail!\n");
            ret = false;
            break;
        }

        //DBG("get entry: %s\n", entry);
        list.append(QString::fromLocal8Bit(entry));

        if(i < global_info.number_entry - 1) {
            if((r = unzGoToNextFile(file)) != UNZ_OK) {
                ret = false;
                break;
            }
        }
    }

    unzClose(file);
    DBG("read file done.\n");
    return ret;
}

bool ZipHelper::readFileInZip(const QString &path, const QString &entryname, QByteArray &out) {
    unzFile file = unzOpen64(path.toLocal8Bit().data(), 0);
    if(file == NULL) {
        DBG("unzOpen '%s' fail!\n", path.toLocal8Bit().data());
        return false;
    }

    bool ret = false;
    unz_global_info global_info;
    unz_file_info file_info;
    char entry[512];
    char buf[4096];
    int n = -1;
    out.clear();

    int r = unzGetGlobalInfo(file, &global_info);
    for(uLong i=0; i<global_info.number_entry; i++) {
        if((r = unzGetCurrentFileInfo(file, &file_info, entry, sizeof(entry), NULL, 0, NULL, 0)) != UNZ_OK) {
            DBG("unzGetCurrentFileInfo fail!\n");
            ret = false;
            break;
        }

        //DBG("got entry '%s'\n", entry);
        if(strcmp(entry, entryname.toLocal8Bit().data()) == 0) {

            if((r = unzOpenCurrentFile(file)) != UNZ_OK) {
                DBG("unzOpenCurrentFile %s fail!\n", entry);
                ret = false;
                break;
            }

            while((n = unzReadCurrentFile(file, buf, sizeof(buf))) > 0) {
                out.append(buf, n);
            }
            DBG("entry '%s', size %d\n", entry, out.size());
            unzCloseCurrentFile(file);
            ret = true;
            break;
        }

        if(i < global_info.number_entry - 1) {
            if((r = unzGoToNextFile(file)) != UNZ_OK) {
                break;
            }
        }
    }

    unzClose(file);
    DBG("read file done.\n");
    return ret;
}

static void rmdir_recursively(const QString& path) {
    QDir dir(path);
    if(dir.exists()) {
        QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot|QDir::AllEntries);
        foreach(const QFileInfo& info, entries) {
            QString entry = info.canonicalFilePath();
            DBG("entry = '%s'\n", entry.toLocal8Bit().data());

            if(info.isDir()) {
                rmdir_recursively(entry);
            } else {
                if (!info.isWritable()) {
                    QFile::setPermissions(entry, (QFile::Permission)0777);
                }
                QFile::remove(entry);
            }
        }

        DBG("%s rmdir '%s'\n", path.toLocal8Bit().data(), path.toLocal8Bit().data());
        dir.rmdir(path);
    }
}

static bool file_decrypt(const QString& path, QString &decrypt_path, unsigned char decrypt_num)
{
    QFile crypy_file(path);
    if (!crypy_file.open(QFile::ReadOnly)) {
        return false;
    }
    qint64 file_size = crypy_file.size();
    char *buf = new char[1024*1024];
    qint64 buf_size = 1024*1024;
    if (buf_size > crypy_file.read(buf, buf_size)) return false;
    for(int i=0; i<buf_size; i++) {
        unsigned char xor_num = (file_size + i)%decrypt_num;
        buf[i] = buf[i] ^ xor_num;
    }
    decrypt_path = path+".dec";

    QFile dec_file(decrypt_path);
    if (!dec_file.open(QFile::WriteOnly)) {
        return false;
    }

    dec_file.write(buf, buf_size);

    int read_size = 0;
    do {
        read_size = crypy_file.read(buf, buf_size);
        if (read_size > 0) {
            dec_file.write(buf, read_size);
        }
    } while (read_size > 0);

    crypy_file.close();
    dec_file.close();
    delete[] buf;
    return true;
}

bool ZipHelper::extractZip(const QString &path, const QString &dest) {
    unzFile file = unzOpen64(path.toLocal8Bit().data(), 0);
    if(file == NULL) {
        DBG("unzOpen '%s' fail!\n", path.toLocal8Bit().data());
#ifdef _WIN32
        DBG("GetLastError()=%d\n", GetLastError());
#endif
        return false;
    }

    QDir destDir(dest);
    QDir curDir;
    rmdir_recursively(dest);

    bool ret = true;
    unz_global_info global_info;
    unz_file_info file_info;
    char entry[512];
    char buf[4096];
    int n = -1;

    int r = unzGetGlobalInfo(file, &global_info);
    for(uLong i=0; i<global_info.number_entry; i++) {
        if((r = unzGetCurrentFileInfo(file, &file_info, entry, sizeof(entry), NULL, 0, NULL, 0)) != UNZ_OK) {
            DBG("unzGetCurrentFileInfo fail!\n");
            ret = false;
            break;
        }

        QString entryName = QString::fromLocal8Bit(entry);
        entryName.replace("\\", "/");
        if(entryName.endsWith('/')) {
            entryName.chop(1);
            DBG("mkpath dest+'%s'\n", entryName.toLocal8Bit().data());
            destDir.mkpath(entryName);
        } else {
            QString fileName = dest + "/" + entryName;
            DBG("extract file '%s'\n", fileName.toLocal8Bit().data());

            n = fileName.lastIndexOf('/');
            QString tmpPath = fileName.mid(0, n);
            DBG("mkpath '%s'\n", tmpPath.toLocal8Bit().data());
            curDir.setPath(tmpPath);
            if ( !curDir.exists() ) {
                curDir.mkpath(tmpPath);
            }

            QFile f(fileName);
            if(!f.open(QIODevice::WriteOnly)) {
                DBG("Error write file\n");
                ret = false;
                break;
            }

            if((r = unzOpenCurrentFile(file)) != UNZ_OK) {
                DBG("unzOpenCurrentFile %s fail!\n", entry);
                ret = false;
                f.close();
                break;
            }

            while((n = unzReadCurrentFile(file, buf, sizeof(buf))) > 0) {
                f.write(buf, n);
                f.flush();
            }
            f.close();

            unzCloseCurrentFile(file);
        }

        if(i < global_info.number_entry - 1) {
            if((r = unzGoToNextFile(file)) != UNZ_OK) {
                break;
            }
        }
    }

    unzClose(file);
    DBG("read file done.\n");

    return ret;
}

ZipEntry::ZipEntry() {
    compress_level = Z_DEFAULT_COMPRESSION;
}

ZipHelper::ZipHelper()
{
}

ZipHelper::~ZipHelper() {
    while(!entries.isEmpty()) {
        delete entries.takeFirst();
    }
}

bool ZipHelper::hasExistEntry(const QString &name) {
    foreach(ZipEntry *entry, entries) {
        if(entry->name == name) {
            DBG("duplicate entry '%s'\n", name.toLocal8Bit().data());
            return true;
        }
    }
    return false;
}

bool ZipHelper::readZipFile(const QString &path) {
    //DBG("reading zip file '%s' ...\n", TR_C(path));
    while(!entries.isEmpty()) {
        delete entries.takeFirst();
    }

    bool ret = true;
    unzFile file = unzOpen64(path.toLocal8Bit().data(), 0);
    if(file == NULL) {
        DBG("unzOpen fail!\n");
        return false;
    }

    bool entryExists = false;
    unz_global_info global_info;
    unz_file_info file_info;
    char entry[512];
    char buf[4096];
    int n = -1;

    int r = unzGetGlobalInfo(file, &global_info);
    for(uLong i=0; i<global_info.number_entry; i++) {
        if((r = unzGetCurrentFileInfo(file, &file_info, entry, sizeof(entry), NULL, 0, NULL, 0)) != UNZ_OK) {
            DBG("unzGetCurrentFileInfo fail!\n");
            ret = false;
            break;
        }

        //DBG("got entry '%s'\n", entry);
        if((r = unzOpenCurrentFile(file)) != UNZ_OK) {
            DBG("unzOpenCurrentFile %s fail!\n", entry);
            ret = false;
            break;
        }

        entryExists = hasExistEntry(QString(entry));
        ZipEntry *zipEntry = new ZipEntry();
        zipEntry->compress_level = Z_DEFAULT_COMPRESSION;
        zipEntry->name = QString::fromUtf8(entry);
        if(entryExists) {
            zipEntry->name += "_jread";
        }

        while((n = unzReadCurrentFile(file, buf, sizeof(buf))) > 0) {
            zipEntry->data.append(buf, n);
        }
        //DBG("entry '%s', size %d\n", entry, zipEntry->data.size());
        unzCloseCurrentFile(file);

        //DBG("got entry '%s', zipLen=%d, unzLen=%d, readLen=%d\n",
        //    entry, file_info.compressed_size, file_info.uncompressed_size,
        //    zipEntry->data.size());
        entries.append(zipEntry);

        if(i < global_info.number_entry - 1) {
            if((r = unzGoToNextFile(file)) != UNZ_OK) {
                break;
            }
        }
    }

    unzClose(file);
    //DBG("read file done.\n");
    return ret;
}

bool ZipHelper::writeZipFile(const QString &path, bool append) {
    if(!append) {
        QFile::remove(path);
    }

    bool ret = true;
    zipFile file = zipOpen64(path.toLocal8Bit().data(), append ? 1: 0);
    if(file == NULL) {
        DBG("zipOpen fail!\n");
        return false;
    }

    zip_fileinfo zi;
    int r = -1;

    foreach(ZipEntry *entry, entries) {
        memset(&zi, 0, sizeof(zi));
        if((r = zipOpenNewFileInZip3_64(file, entry->name.toUtf8(), &zi, NULL, 0, NULL, 0, NULL,
                                        entry->compress_level != 0 ? Z_DEFLATED : 0, entry->compress_level, 0,
                                        -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
                                        NULL, 0, 0)) != ZIP_OK) {
            DBG("error when zipOpenNewFileInZip3_64 '%s', %d\n", entry->name.toLocal8Bit().data(), r);
            ret = false;
            continue;
        }

        int pos = 0;
        int left = entry->data.size();
        char *ptr = entry->data.data();

        //DBG("write entry '%s', size %d\n", TR_C(entry->name), left);
        while(left > 0) {
            int n = left > 4096 ? 4096 : left;
            if((r = zipWriteInFileInZip(file, ptr + pos, n)) < 0) {
                DBG("error when zipWriteInFileInZip '%s'\n", entry->name.toLocal8Bit().data());
                ret = false;
                break;
            }

            pos += n;
            left -= n;
        }

        zipCloseFileInZip(file);
    }

    zipClose(file, NULL);
    return ret;
}
