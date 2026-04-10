#pragma once
#include <cstdlib>
#include <cstdio>
#include <sys/stat.h>

// openctp TTS .so requires zh_CN.GB18030 locale.
// Generate it under /tmp/locales if missing, then set LOCPATH.
inline void fix_locale() {
    const char *dir = "/tmp/locales";
    const char *loc = "/tmp/locales/zh_CN.GB18030";
    struct stat st;
    if (stat(loc, &st) != 0) {
        mkdir(dir, 0755);
        int rc = system("localedef -i zh_CN -f GB18030 /tmp/locales/zh_CN.GB18030");
        if (rc != 0) {
            fprintf(stderr, "Warning: failed to generate zh_CN.GB18030 locale\n");
        }
    }
    setenv("LOCPATH", dir, 0);
}
