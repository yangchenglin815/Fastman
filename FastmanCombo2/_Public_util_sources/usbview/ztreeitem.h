#ifndef ZTREEITEM_H
#define ZTREEITEM_H

#ifdef __cplusplus
extern "C" {
#endif

struct _ZTreeItem {
    char name[512];
    void *param;

    struct _ZTreeItem* next;
    struct _ZTreeItem* parent;
    struct _ZTreeItem* child;
};

typedef struct _ZTreeItem *ZTreeItem;

ZTreeItem ZTreeItemInsert(ZTreeItem parent, char *name, void *param);
void ZTreeItemPrint(ZTreeItem node, int depth);
void ZTreeItemFree(ZTreeItem node);

#ifdef __cplusplus
}
#endif
#endif
