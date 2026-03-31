#include "canvasos.h"
#include "cell.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

static FSNode  fs_nodes[MAX_FILES];
static int     fs_initialized = 0;

void fs_init(void) {
    memset(fs_nodes, 0, sizeof(fs_nodes));
    fs_initialized = 1;
}

static int fs_find(const char *path) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs_nodes[i].used) {
            int match = 1;
            for (int j = 0; j < 63; j++) {
                if (fs_nodes[i].name[j] != path[j]) { match = 0; break; }
                if (!path[j]) break;
            }
            if (match) return i;
        }
    }
    return -1;
}

static int fs_alloc(void) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs_nodes[i].used) return i;
    }
    return -1;
}

int fs_write(const char *path, const uint8_t *data, size_t size) {
    if (!path || !data) return -1;
    int idx = fs_find(path);
    if (idx < 0) idx = fs_alloc();
    if (idx < 0) return -1;

    FSNode *n = &fs_nodes[idx];
    n->used = true;
    int plen = 0;
    while (path[plen] && plen < 63) { n->name[plen] = path[plen]; plen++; }
    n->name[plen] = '\0';

    size_t sz = size > MAX_FS_DATA ? MAX_FS_DATA : size;
    memcpy(n->data, data, sz);
    n->size        = sz;
    n->modified_at = (uint64_t)time(NULL);
    if (n->created_at == 0) n->created_at = n->modified_at;
    return idx;
}

int fs_read(const char *path, uint8_t *buffer, size_t buf_size) {
    int idx = fs_find(path);
    if (idx < 0) return -1;
    FSNode *n = &fs_nodes[idx];
    size_t copy = n->size < buf_size ? n->size : buf_size;
    memcpy(buffer, n->data, copy);
    return (int)copy;
}

void fs_delete(const char *path) {
    int idx = fs_find(path);
    if (idx >= 0) {
        memset(&fs_nodes[idx], 0, sizeof(FSNode));
    }
}

void fs_list(const char *dir) {
    printf("Files in '%s':\n", dir ? dir : "/");
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs_nodes[i].used) {
            printf("  [%d] %s (%zu bytes)\n", i, fs_nodes[i].name, fs_nodes[i].size);
        }
    }
}

bool fs_exists(const char *path) {
    return fs_find(path) >= 0;
}
