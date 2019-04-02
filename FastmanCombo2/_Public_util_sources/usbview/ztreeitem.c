#include "ztreeitem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ZTreeItem ZTreeItemInsert(ZTreeItem parent, char *name, void *param) {
    ZTreeItem node = (ZTreeItem) malloc(sizeof(struct _ZTreeItem));
    if(node == NULL) {
        return node;
    }

    memset(node->name, 0, sizeof(node->name));
    strncpy(node->name, name, sizeof(node->name)-1);
    node->param = param;

    node->parent = parent;
    node->child = NULL;
    node->next = NULL;

    if(parent != NULL) {
        ZTreeItem p = parent->child;
        if(p == NULL) {
            parent->child = node;
        } else {
            while(p->next != NULL) {
                p = p->next;
            }
            p->next = node;
        }
    }
    return node;
}

void ZTreeItemPrint(ZTreeItem node, int depth) {
    char indent[256] = {0};
    int i = 0;
    for(; i<depth; i++){
        strcat(indent, "  ");
    }

    if(node != NULL) {
        ZTreeItem p = node->child;
        printf("%s %s\n", indent, node, node->name);

        while(p != NULL) {
            ZTreeItemPrint(p, depth+1);
            p = p->next;
        }
    }
}

void ZTreeItemFree(ZTreeItem node) {
    ZTreeItem p = node->child;
    while(p != NULL) {
        ZTreeItem q = p;
        p = p->next;
        ZTreeItemFree(q);
    }
    //printf("free %p\n", node);
    free(node);
}
