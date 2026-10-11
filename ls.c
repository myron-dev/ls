#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <time.h>
#include <limits.h>

struct FileInfo {
    char *permissions;
    char *user_name;
    char *group_name;
    char *date;
    char *name;
    long int size_b;
    long int link_n;
};

struct ColumnWidth {
    int permissions;
    int link_n;
    int user_name;
    int group_name;
    int size_b;
    int date;
    int name;
};

struct FileList {
    struct FileInfo *items;
    size_t count;
    size_t capacity;
    struct ColumnWidth widths;
};

void output(struct ColumnWidth *width, struct FileInfo *file, int *params_bin) {
    if (params_bin[0] == 1) {
        if (params_bin[1] || file->name[0] != '.' ) {
            printf("%*s ",width->permissions, file->permissions);
            printf("%*ld ", width->link_n, file->link_n);
            printf("%*s ", width->user_name, file->user_name);
            printf("%*s ", width->group_name, file->group_name);
            printf("%*ld ", width->size_b, file->size_b);
            printf("%*s ", width->date, file->date);
            printf("%-*s", width->date, file->name);
            printf("\n");
        }
    } else if (params_bin[0] != 1) {
        if (params_bin[1] || file->name[0] != '.' ) {
            printf("%s  ", file->name); 
        }
    }
}

void filelist_init(struct FileList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    memset(&list->widths, 0, sizeof list->widths);
}

static void update_max(int *w, int len) {
    if (len > *w) *w = len;
}

int filelist_add(struct FileList *list, const struct FileInfo *fi) {
    if (list->count == list->capacity) {
        size_t new_cap = list->capacity ? list->capacity * 2: 8;
        struct FileInfo *tmp = realloc(list->items, new_cap * sizeof *tmp);
        if (!tmp) return -1;
        list->items = tmp;
        list->capacity = new_cap;
    }

    list->items[list->count] = *fi;
    list->count++;

    update_max(&list->widths.user_name, (int)strlen(fi->user_name));
    update_max(&list->widths.permissions, (int)strlen(fi->permissions));
    update_max(&list->widths.group_name, (int)strlen(fi->group_name));
    update_max(&list->widths.link_n, snprintf(NULL, 0, "%ld", fi->link_n));
    update_max(&list->widths.size_b, snprintf(NULL, 0, "%ld", fi->size_b));
    update_max(&list->widths.name, (int)strlen(fi->name));
    update_max(&list->widths.date, (int)strlen(fi->date));
  
    return 0;
}

void filelist_free(struct FileList *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i].name);
        free(list->items[i].group_name);
        free(list->items[i].user_name);
        free(list->items[i].date);
        free(list->items[i].permissions);
    }
    free(list->items);
    list->items = NULL;
    list->count = list->capacity = 0;
}

struct FileInfo parse_file(struct stat *info, char *name) {
    char type;
    
    if (S_ISREG(info->st_mode)) type = '-';
    else if (S_ISDIR(info->st_mode)) type = 'd';
    else if (S_ISLNK(info->st_mode)) type = 'l';
    else type = '?';

    char permissions[11];

    permissions[0] = type;
    
    permissions[1] = (info->st_mode & S_IRUSR) ? 'r' : '-';
    permissions[2] = (info->st_mode & S_IWUSR) ? 'w' : '-';
    permissions[3] = (info->st_mode & S_IXUSR) ? 'x' : '-';

    permissions[4] = (info->st_mode & S_IRGRP) ? 'r' : '-';
    permissions[5] = (info->st_mode & S_IWGRP) ? 'w' : '-';
    permissions[6] = (info->st_mode & S_IXGRP) ? 'x' : '-';

    permissions[7] = (info->st_mode & S_IROTH) ? 'r' : '-';
    permissions[8] = (info->st_mode & S_IWOTH) ? 'w' : '-';
    permissions[9] = (info->st_mode & S_IXOTH) ? 'x' : '-';

    permissions[10] = '\0';
    
    struct passwd *user = getpwuid(info->st_uid);
    struct group *group = getgrgid(info->st_gid);
    
    char date[20];
    strftime(
        date,
        sizeof(date),
        "%b %d %H:%M",
        localtime(&info->st_mtime)
    );
    
    struct FileInfo fileinfo = {strdup(permissions), strdup(user->pw_name), strdup(group->gr_name), strdup(date), strdup(name), (long int)info->st_size, (long int)info->st_nlink};

    return fileinfo;

}

int compare_strings(const void *a, const void *b) {
    const char *name_a = *(const char * const *)a;
    const char *name_b = *(const char * const *)b;

    return strcmp(name_a, name_b);
}

int compare_strings_r(const void *a, const void *b) {
    const char *name_a = *(const char * const *)a;
    const char *name_b = *(const char * const *)b;

    return strcmp(name_b, name_a);
}

void sort_names(char **names, int *params_bin, int count) {
    if (params_bin[2] == 1) {
        qsort(names, count, sizeof(*names), compare_strings_r);
    } else {
        qsort(names, count, sizeof(*names), compare_strings);
    }
}

int get_blocks(struct stat info) {
    return info.st_blocks;
}

void compare_params(char *pos_params, char *params, int params_bin[]) {
    for (int i = 0; i < strlen(pos_params); i++) {
        if (strchr(params, pos_params[i]) != NULL) {
            params_bin[i] = 1;
        } else {
            params_bin[i] = 0;
        }
    }
}

int main(int argc, char *argv[]) {
    
    char params[4] = "";

    char pos_params[4];
    pos_params[0] = 'l';
    pos_params[1] = 'a';
    pos_params[2] = 'r';
    pos_params[3] = '\0';

    char *path = NULL;

    if (argc > 1) {
        for (int a = 1; a < argc; a++) {
            if (argv[a][0] == '-') {
                size_t p_index = 0;
                for (size_t i = 0; argv[a][i] != '\0'; i++) {
                    if (i == 0) continue;
                    if (strchr(pos_params, argv[a][i]) == NULL) {
                        printf("Invalid param: %c", argv[a][i]);
                        return 1;
                    }
                    params[p_index] = argv[a][i];
                    p_index++;
                }
                params[p_index] = '\0';
            } else if (!path) {
                path = argv[a];
            } else {
                printf("Invalid option: %s", argv[a]);
                return 1;
            }

        }
    }
   
    if (!path) { 
        path = ".";
    }
    
    int params_bin[3];
    compare_params(pos_params, params, params_bin);

    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir");
        return 1;
    }

    struct dirent *entry;
    
    long total_blocks = 0;
    if (params_bin[0]) {        
        while ((entry = readdir(dir)) != NULL) {
            char full_path[PATH_MAX];

            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

            struct stat info;

            if (!(params_bin[1] == 1 || entry->d_name[0] != '.')) 
                continue;

            if (stat(full_path, &info) == -1) {
                perror("stat");
                continue;
            }
            long blocks = get_blocks(info);
            total_blocks += blocks;
        }
        total_blocks = (int)(total_blocks / 2); 
                
        rewinddir(dir);
    }

    if (params_bin[0]) 
        printf("Total %ld\n", total_blocks);
    
    size_t count = 0;
    size_t capacity = 16;
    
    char **names = malloc(capacity * sizeof *names);

    if (names == NULL) {
        perror("malloc");
        closedir(dir);
        return 1;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (count == capacity) {
            capacity *= 2;

            char **tmp = realloc(names, capacity * sizeof *names);
            if (tmp == NULL) {
                perror("malloc");

                for (size_t i = 0; i < count; i++) {
                    free(names[i]);
                }

                free(names);
                closedir(dir);
                return 1;
            }
            names = tmp;
        }
        names[count] = strdup(entry->d_name);

        if (names[count] == NULL) {
            perror("strdup");

            for (size_t i = 0; i < count; i++) {
                free(names[i]);
            }

            free(names);
            closedir(dir);
            return 1;
        }
        count++;
    }

    sort_names(names, params_bin, count);
    
    struct FileList list;
    filelist_init(&list);

    for (int i = 0; i < count; i++) {
        struct stat info;

        char full_path[PATH_MAX];

        snprintf(full_path, sizeof(full_path), "%s/%s", path, names[i]);
        
        if (stat(full_path, &info) == -1) {
            perror("stat");
            continue;
        }

        struct FileInfo file = parse_file(&info, names[i]);

        int result = filelist_add(&list, &file);
        if (result == -1) {
            perror("malloc");
            filelist_free(&list);
            closedir(dir);
            return result;
        }
    }

    for (int i = 0; i < list.count; i++) {
        output(&list.widths, &list.items[i], params_bin);
    }

    closedir(dir);

    return 0;
}
