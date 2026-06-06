#include "vfs.h"

#define TMPFS_MAX_FILE_NAME 15
#define TMPFS_MAX_DIR_ENTRY 16
#define TMPFS_MAX_FILE_SIZE 4096

enum fsnode_type { FS_DIR, FS_FILE };

struct tmpfs_vnode {
    enum fsnode_type type;
    char name[TMPFS_MAX_FILE_NAME];
    struct vnode* entry[TMPFS_MAX_DIR_ENTRY];
    char* data;
    size_t size;
};

struct file_operations tmpfs_file_ops = {.open = tmpfs_open,
                                         .close = tmpfs_close,
                                         .read = tmpfs_read,
                                         .write = tmpfs_write};

struct vnode_operations tmpfs_vnode_ops = {.lookup = tmpfs_lookup,
                                           .create = tmpfs_create};

struct vnode* tmpfs_create_vnode(enum fsnode_type type) {
    // TODO: Implement this function
    struct tmpfs_vnode* inode = malloc(sizeof(struct tmpfs_vnode));
    inode->type = type;
    inode->name[0] = '\0';
    for (int i = 0; i < TMPFS_MAX_DIR_ENTRY; i++)
        inode->entry[i] = (void*)0;
    inode->data = (void*)0;
    inode->size = 0;

    struct vnode* vnode = malloc(sizeof(struct vnode));
    vnode->mount = (void*)0;
    vnode->v_ops = &tmpfs_vnode_ops;
    vnode->f_ops = &tmpfs_file_ops;
    vnode->internal = inode;

    return vnode;
}

int tmpfs_setup_mount(struct filesystem* fs, struct mount* mnt) {
    mnt->root = tmpfs_create_vnode(FS_DIR);
    mnt->fs = fs;
    return 0;
}

int tmpfs_open(struct vnode* file_node, struct file** target) {
    (*target)->vnode = file_node;
    (*target)->f_ops = &tmpfs_file_ops;
    (*target)->f_pos = 0;
    return 0;
}

int tmpfs_close(struct file* file) {
    free(file);
    return 0;
}

int tmpfs_read(struct file* file, void* buf, size_t len) {
    // TODO: Implement this function
    struct tmpfs_vnode* inode = file->vnode->internal;
    if (!inode || inode->type != FS_FILE)
        return -1;
    if (file->f_pos >= inode->size)
        return 0;
    size_t available = inode->size - file->f_pos;
    size_t len_read = (len > available) ? available : len;
    char* start_address = inode->data + file->f_pos;
    memcpy(buf, start_address, len_read);
    file->f_pos += len_read;
    return len_read;
}

int tmpfs_write(struct file* file, const void* buf, size_t len) {
    // TODO: Implement this function
    struct tmpfs_vnode* inode = file->vnode->internal;
    if (!inode || inode->type != FS_FILE)
        return -1;
    if (inode->data == (void*)0) {
        inode->data = calloc(1, TMPFS_MAX_FILE_SIZE);
    }
    size_t available = TMPFS_MAX_FILE_SIZE - file->f_pos;
    size_t len_write = (len > available) ? available : len;
    char* start_address = inode->data + file->f_pos;
    memcpy(start_address, buf, len_write);
    file->f_pos += len_write;
    if (file->f_pos > inode->size) {
        inode->size = file->f_pos;
    }
    return len_write;
}

int tmpfs_lookup(struct vnode* dir_node,
                 struct vnode** target,
                 const char* component_name) {
    struct tmpfs_vnode* dentry = dir_node->internal;
    for (int i = 0; i < TMPFS_MAX_DIR_ENTRY; i++) {
        if (!dentry->entry[i])
            return -1;
        struct tmpfs_vnode* inode = dentry->entry[i]->internal;
        if (!strcmp(inode->name, component_name)) {
            *target = dentry->entry[i];
            return 0;
        }
    }
    return -1;
}

int tmpfs_create(struct vnode* dir_node,
                 struct vnode** target,
                 const char* component_name) {
    struct tmpfs_vnode* parent_node = (struct tmpfs_vnode*)dir_node->internal;
    if (parent_node->type != FS_DIR)
        return -1;

    // 1. 先找目錄裡有沒有空的插槽 (slot)
    int slot = -1;
    for (int i = 0; i < TMPFS_MAX_DIR_ENTRY; i++) {
        if (parent_node->entry[i] == NULL) {
            slot = i;
            break;  // 找到第一個空的就停下來
        }
    }

    // 目錄滿了
    if (slot == -1)
        return -1;

    // 2. 建立新檔案的 vnode
    struct vnode* new_vnode = tmpfs_create_vnode(FS_FILE);
    if (!new_vnode)
        return -1;

    struct tmpfs_vnode* target_node = (struct tmpfs_vnode*)new_vnode->internal;

    // 3. 設定檔名
    strncpy(target_node->name, component_name, TMPFS_MAX_FILE_NAME - 1);
    target_node->name[TMPFS_MAX_FILE_NAME - 1] = '\0';

    // 4. 註冊到父目錄中
    parent_node->entry[slot] = new_vnode;

    // 5. 傳回給呼叫者
    *target = new_vnode;
    return 0;
}