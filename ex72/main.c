#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH      4096
#define MAX_COMPONENT 256

const char* curr_working_dir = "/path/to/current/directory";

/**
 * Resolve a relative or absolute filepath
 */
char* my_realpath(const char* path, char* resolved_path) {
    if (!path || !resolved_path)
        return NULL;

    // 用來儲存路徑每一層名稱的 Stack
    char tokens[MAX_COMPONENT][MAX_COMPONENT];
    int top = 0;  // Stack 的指標

    // 1. 如果是相對路徑，先把當前工作目錄的各層目錄放進 Stack
    if (path[0] != '/') {
        char cwd_copy[MAX_PATH];
        strcpy(cwd_copy, curr_working_dir);
        // strtok用法，遇到第一個/停下改成'\0'，回傳指標開頭
        char* token = strtok(cwd_copy, "/");
        while (token != NULL) {
            strcpy(tokens[top++], token);
            // 如果是NULL，會從上次的static pointer繼續往下執行
            token = strtok(NULL, "/");
        }
    }

    // 2. 複製一份 path，避免修改到唯讀的字串常數 (.rodata)
    char path_copy[MAX_PATH];
    strcpy(path_copy, path);

    // 3. 開始切分並處理輸入的路徑
    char* token = strtok(path_copy, "/");
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            // 遇到 "." -> 待在原地，不做任何事
        } else if (strcmp(token, "..") == 0) {
            // 遇到 ".." -> 回到上一層，把 Stack 頂端彈出 (Pop)
            if (top > 0) {
                top--;
            }
        } else if (strlen(token) > 0) {
            // 普通目錄或檔案名稱 -> 壓入 Stack (Push)
            strcpy(tokens[top++], token);
        }
        token = strtok(NULL, "/");
    }

    // 4. 將 Stack 中的目錄重新組合回 resolved_path
    resolved_path[0] = '\0';  // 初始化為空字串

    if (top == 0) {
        // 如果 Stack 是空的，代表是根目錄 "/"
        strcpy(resolved_path, "/");
    } else {
        for (int i = 0; i < top; i++) {
            strcat(resolved_path, "/");
            strcat(resolved_path, tokens[i]);
        }
    }

    return resolved_path;  // 記得回傳指標，main 函式的 if 判斷才會過
}

int main() {
    char resolved[MAX_PATH];
    const char* test_paths[] = {
        ".",
        "..",
        "./test",
        "../parent",
        "dir1/dir2/../../dir3",
        "/absolute/path",
        "relative/./path",
        NULL,
    };
    for (int i = 0; test_paths[i] != NULL; i++) {
        printf("[%d] \"%s\"", i, test_paths[i]);
        if (my_realpath(test_paths[i], resolved))
            printf(" --> \"%s\"\n", resolved);
    }
    return 0;
}
