#ifndef ZCTOOLS_H
#define ZCTOOLS_H

unsigned long long getFreeSize(char *dir);
unsigned long long getMountSize(char *dir);
unsigned long long getFreeSizeForFile(char *path);
long long getFileSize(char *path);
bool mkdirForFile(char *path);
bool getFileExists(char *path);
bool getFileMd5(char *path, char *md5);
bool getMemInfoKB(long long &free, long long &total);

char *strBasename(char *path);
bool strBeginsWith(const char *str, const char *prefix);
bool strEndsWith(const char *str, const char *suffix);
void strChopTail(char *src);

bool dir_writable(char *path);
bool copy_file(char *from, char *to, int mode = 0644);
bool write_file(char *path, char *data, int size, int mode = 0644);
void rm_files(char *path, bool delself);

bool copy_apklibs(const char *lib_prefix, const char *path);
bool rm_apklibs(const char *lib_prefix, const char *path);

typedef char *security_context_t;

class SELinux {
    void *handle;
    int (*func_getfilecon)(const char *, security_context_t *);
    int (*func_setfilecon)(const char *, security_context_t);
    void (*func_freecon)(char *);
public:
    SELinux();
    ~SELinux();

    int getfilecon(const char *path, security_context_t *con);
    int setfilecon(const char *path, security_context_t con);
    void freecon(char * con);
};

template<typename T>
class QList {

    class NODE {
    public:
        T data;
        NODE *prev;
        NODE *next;
    };

    NODE *list_head;
    int list_size;

    NODE *lastNode() {
        NODE *p = list_head;
        if (p == 0) {
            return 0;
        }

        while (p->next != 0) {
            p = p->next;
        }
        return p;
    }

    NODE *nodeAt(int index) {
        NODE *p = list_head;
        int i = 0;
        while (p != 0) {
            if (i++ == index) {
                return p;
            }
            p = p->next;
        }
        return 0;
    }

    void append(NODE *node) {
        if (list_head == 0) {
            list_head = node;
        } else {
            NODE *last = lastNode();
            last->next = node;
            node->prev = last;
        }
        list_size++;
    }

    void remove(NODE *p) {
        if (p == list_head) {
            list_head = p->next;
            if (p->next != 0) {
                p->next->prev = 0;
            }
        } else {
            if (p->prev != 0) {
                p->prev->next = p->next;
            }
            if (p->next != 0) {
                p->next->prev = p->prev;
            }
        }
        list_size--;
        delete p;
    }
public:
    QList() {
        list_head = 0;
        list_size = 0;
    }

    ~QList() {
        clear();
    }

    inline bool isEmpty() {
        return list_size == 0;
    }

    void insert(int index, T data) {
        NODE *node = new NODE();
        node->data = data;

        if(index <= 0) {
            if(list_head != 0) {
                list_head->prev = node;
            }
            node->prev = 0;
            node->next = list_head;
            list_head = node;
            list_size += 1;
        } else if(index >= list_size) {
            append(node);
        } else {
            NODE *after = nodeAt(index);
            node->prev = after->prev;
            node->prev->next = node;
            after->prev = node;
            node->next = after;
            list_size += 1;
        }
    }

    void append(T data) {
        NODE *node = new NODE();
        node->data = data;
        node->prev = 0;
        node->next = 0;

        append(node);
    }

    void append(QList<T> list) {
        NODE *p = list.list_head;
        NODE *dest = lastNode();

        while (p != 0) {
            NODE *node = new NODE();
            node->data = p->data;
            node->prev = 0;
            node->next = 0;

            if (dest == 0) {
                list_head = node;
            } else {
                node->prev = dest;
                dest->next = node;
            }

            dest = node;
            list_size++;

            p = p->next;
        }
    }

    bool removeAt(int index) {
        NODE *p = nodeAt(index);

        if (p == 0) {
            return false;
        }

        remove(p);
        return true;
    }

    int indexOf(T data) {
        int i = -1;
        NODE *p = list_head;
        while (p != 0) {
            i++;
            if (p->data == data) {
                return i;
            }
            p = p->next;
        }
        return -1;
    }

    bool removeOne(T data) {
        bool found = false;
        NODE *p = list_head;
        while (p != 0) {
            if (p->data == data) {
                found = true;
                break;
            }
            p = p->next;
        }

        if (found) {
            remove(p);
        }
        return found;
    }

    T at(int index) {
        return nodeAt(index)->data;
    }

    T takeAt(int index) {
        NODE *p = nodeAt(index);
        T ret = p->data;
        remove(p);
        return ret;
    }

    inline T operator [](int index) {
        return at(index);
    }

    void clear() {
        NODE *p = list_head;
        NODE *q = 0;

        while (p != 0) {
            q = p;
            p = p->next;
            delete q;
        }

        list_head = 0;
        list_size = 0;
    }

    int size() {
        return list_size;
    }
};

template<typename KEY, typename T>
class QMap {

    class NODE {
    public:
        KEY key;
        T value;
        NODE *prev;
        NODE *next;
    };

    NODE *list_head;
    int list_size;

    NODE *lastNode() {
        NODE *p = list_head;
        if (p == 0) {
            return 0;
        }

        while (p->next != 0) {
            p = p->next;
        }
        return p;
    }

    NODE *nodeAt(int index) {
        NODE *p = list_head;
        int i = 0;
        while (p != 0) {
            if (i++ == index) {
                return p;
            }
            p = p->next;
        }
        return 0;
    }

    void append(NODE *node) {
        if (list_head == 0) {
            list_head = node;
        } else {
            NODE *last = lastNode();
            last->next = node;
            node->prev = last;
        }
        list_size++;
    }

    void remove(NODE *p) {
        if (p == list_head) {
            list_head = p->next;
            if (p->next != 0) {
                p->next->prev = 0;
            }
        } else {
            if (p->prev != 0) {
                p->prev->next = p->next;
            }
            if (p->next != 0) {
                p->next->prev = p->prev;
            }
        }
        list_size--;
        delete p;
    }

    NODE *nodeByKey(KEY key) {
        NODE *p = list_head;
        bool found = false;
        while (p != 0) {
            if (p->key == key) {
                return p;
            }

            p = p->next;
        }
        return 0;
    }

    NODE *nodeByValue(T value) {
        NODE *p = list_head;
        bool found = false;
        while (p != 0) {
            if (p->value == value) {
                return p;
            }

            p = p->next;
        }
        return 0;
    }
public:
    QMap() {
        list_head = 0;
        list_size = 0;
    }

    ~QMap() {
        clear();
    }

    inline bool isEmpty() {
        return list_size == 0;
    }

    KEY keyAt(int i) {
        NODE *p = nodeAt(i);
        return p->key;
    }

    T valueAt(int i) {
        NODE *p = nodeAt(i);
        return p->value;
    }

    QList<KEY> keys() {
        QList<KEY> keys;
        NODE *p = list_head;
        while (p != 0) {
            keys.append(p->key);
            p = p->next;
        }
        return keys;
    }

    QList<T> values() {
        QList<T> values;
        NODE *p = list_head;
        while (p != 0) {
            values.append(p->value);
            p = p->next;
        }
        return values;
    }

    void put(KEY key, T value) {
        NODE *p = list_head;
        bool found = false;
        while (p != 0) {
            if (p->key == key) {
                found = true;
                break;
            }

            p = p->next;
        }

        if (found) {
            p->value = value;
            return;
        }

        NODE *q = new NODE();
        q->key = key;
        q->value = value;
        q->prev = 0;
        q->next = 0;
        append(q);
    }

    inline void set(KEY key, T value) {
        put(key, value);
    }

    bool hasKey(KEY key) {
        NODE *p = nodeByKey(key);
        return p != 0;
    }

    T get(KEY key) {
        return nodeByKey(key)->value;
    }

    T get(KEY key, T defaultValue) {
        NODE *p = nodeByKey(key);
        if (p != 0) {
            return p->value;
        }
        return defaultValue;
    }

    inline T operator [](KEY key) {
        return get(key);
    }

    KEY keyOf(T value) {
        return nodeByValue(value)->key;
    }

    bool remove(KEY key) {
        NODE *q = nodeByKey(key);
        if (q == 0) {
            return false;
        }

        remove(q);
        return true;
    }

    void clear() {
        NODE *p = list_head;
        NODE *q = 0;

        while (p != 0) {
            q = p;
            p = p->next;
            delete q;
        }

        list_head = 0;
        list_size = 0;
    }

    int size() {
        return list_size;
    }
};

#endif // ZCTOOLS_H
