#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h> // 디렉토리 작업을 위한 헤더
#include <sys/stat.h> // 파일 상태 정보를 위한 헤더
#include <unistd.h> // getopt를 위한 헤더
#include <pwd.h> // 사용자 ID를 위한 헤더
#include <grp.h> // 그룹 ID를 위한 헤더
#include <time.h> // 시간 형식을 위한 헤더
#include <getopt.h> // getopt_long을 위한 헤더
#include <limits.h> // 현재 파일경로를 가져오기 위한 헤더
#include <libgen.h> // 현재 파일경로를 가져오기 위한 헤더

// 옵션을 위한 enum 정의
enum Options {
    OPT_NONE = 0,
    OPT_SHOW_ALL = 1 << 0,     // -a
    OPT_LONG_FORMAT = 1 << 1,  // -l
    OPT_TREE_FORMAT = 1 << 2,  // -R
    OPT_SORT_ALPHA = 1 << 3    // -b
};

// 함수 선언
void list_directory(const char *dir_name, int options, int depth);

void print_file_info(const char *path, const char *name, int options);

void print_permissions(mode_t mode);

void print_help(char *program_name);

int main(int argc, char *argv[]) {
    int opt;
    int options = OPT_NONE;

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"tree", no_argument, 0, 'R'},  // -R 옵션을 --tree로 사용
        {0, 0, 0, 0}
    };

    // 명령줄 옵션 파싱
    while ((opt = getopt_long(argc, argv, "alRhb", long_options, NULL)) != -1) {
        switch (opt) {
            case 'a':
                options |= OPT_SHOW_ALL;
                break;
            case 'l':
                options |= OPT_LONG_FORMAT;
                break;
            case 'R':
                options |= OPT_TREE_FORMAT;
                break;
            case 'b':
                options |= OPT_SORT_ALPHA;
                break;
            case 'h':
                print_help(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                print_help(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // 디렉토리가 지정되지 않은 경우 현재 디렉토리를 나열
    if (optind == argc) {
        char *path = (char *)malloc(sizeof(char) * 1024);;
        ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
        path[count] = '\0';
        char *dir = dirname(path); // 현재 컴파일된 파일의 경로에서 디렉토리 경로만을 가지고옴
        printf("%s", dir);
        list_directory(dir, options, 0);
    } else {
        // 지정된 각 디렉토리를 나열
        for (int i = optind; i < argc; i++) {
            printf("%s:\n", argv[i]);
            list_directory(argv[i], options, 0);
            if (i < argc - 1) {
                printf("\n");
            }
        }
    }

    return 0;
}

void list_directory(const char *dir_name, int options, int depth) {
    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(dir_name))) {
        perror(dir_name);
        return;
    }

    // 파일 이름들을 저장할 배열
    struct dirent **namelist;
    int n;

    if (options & OPT_SORT_ALPHA) {
        // 알파벳 순으로 정렬
        n = scandir(dir_name, &namelist, NULL, alphasort);
    } else {
        // 정렬하지 않음
        n = scandir(dir_name, &namelist, NULL, NULL);
    }

    if (n < 0) {
        perror("scandir");
        closedir(dir);
        return;
    }

    for (int i = 0; i < n; i++) {
        entry = namelist[i];

        // 숨김 파일을 -a 옵션이 없으면 건너뜀
        if (!(options & OPT_SHOW_ALL) && entry->d_name[0] == '.') {
            free(entry);
            continue;
        }

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir_name, entry->d_name);

        // 트리 형식인 경우 들여쓰기 적용
        if (options & OPT_TREE_FORMAT) {
            for (int j = 0; j < depth; j++) {
                printf("  ");
            }
            printf("|-- ");
        }

        if (options & OPT_LONG_FORMAT) {
            print_file_info(path, entry->d_name, options);
        } else {
            printf("%s\n", entry->d_name);
        }

        // 디렉토리인 경우 재귀적으로 호출
        struct stat st;
        if (stat(path, &st) == -1) {
            perror(path);
            free(entry);
            continue;
        }

        if (S_ISDIR(st.st_mode) && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            list_directory(path, options, depth + 1);
        }

        free(entry);
    }
    free(namelist);

    closedir(dir);
}

void print_file_info(const char *path, const char *name, int options) {
    struct stat st;
    if (stat(path, &st) == -1) {
        perror(path);
        return;
    }

    print_permissions(st.st_mode);
    printf(" %ld", (long)st.st_nlink);

    struct passwd *pwd = getpwuid(st.st_uid);
    struct group *grp = getgrgid(st.st_gid);

    printf(" %s %s", pwd ? pwd->pw_name : "-", grp ? grp->gr_name : "-");

    printf(" %5ld", (long)st.st_size);

    char timebuf[80];
    struct tm *tm_info = localtime(&st.st_mtime);
    strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", tm_info);

    printf(" %s", timebuf);

    printf(" %s\n", name);
}

void print_permissions(mode_t mode) {
    printf( (S_ISDIR(mode)) ? "d" : "-");
    printf( (mode & S_IRUSR) ? "r" : "-");
    printf( (mode & S_IWUSR) ? "w" : "-");
    printf( (mode & S_IXUSR) ? "x" : "-");
    printf( (mode & S_IRGRP) ? "r" : "-");
    printf( (mode & S_IWGRP) ? "w" : "-");
    printf( (mode & S_IXGRP) ? "x" : "-");
    printf( (mode & S_IROTH) ? "r" : "-");
    printf( (mode & S_IWOTH) ? "w" : "-");
    printf( (mode & S_IXOTH) ? "x" : "-");
}

void print_help(char *program_name) {
    printf("사용법: %s [옵션] [디렉토리...]\n", program_name);
    printf("옵션:\n");
    printf("  -a            숨김 파일을 포함하여 모든 파일을 표시합니다.\n");
    printf("  -l            자세한 정보와 함께 파일을 나열합니다.\n");
    printf("  -R, --tree    디렉토리를 트리 형식으로 재귀적으로 나열합니다.\n");
    printf("  -b            파일을 알파벳 순으로 정렬하여 출력합니다.\n");
    printf("  -h, --help    이 도움말 메시지를 표시하고 종료합니다.\n");
}
