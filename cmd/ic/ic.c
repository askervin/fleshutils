/*
Copyright (c) 2022 Antti Kervinen <antti.kervinen@gmail.com>

License (MIT):

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation files
(the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXCMD 65535
#define MAXPATH 65535
#define MAXCODE (1024*1024)

const char *ic_cc = NULL;
const char *ic_cc_default = "cc";
const char *ic_cflags = "-g -O0";
const char *ic_cflags_default = "";
const char *ic_ldflags = NULL;
const char *ic_ldflags_default = "";
const char *ic_debug = NULL;
const char *ic_debug_default = NULL;
const char *ic_workdir = NULL;
FILE* ic_outfile = NULL;

const char* interactive_help = (
    "Interactive C prompt-of-concept\n"
    "\n"
    "Environment variables:\n"
    "  IC_ECHO            if set, echo commands to output\n"
    "  IC_DEBUG           if set, print compiler commands\n"
    "  IC_CC              C compiler, the default is \"cc\"\n"
    "  IC_CFLAGS          C compiler flags\n"
    "  IC_LDFLAGS         linker flags\n"
    "  IC_WORKDIR         existing directory for temporary files\n"
    "\n"
    "Commands:\n"
    "  help               print help.\n"
    "  quit or q          quit.\n"
    "  #INCLUDE           include a header.\n"
    "  t: TYPE-DECL       define a type.\n"
    "  v: VAR-DECL        compile and load a variable.\n"
    "  x: SHORTHAND       expand SHORTHAND\n"
    "  CODE               compile and run CODE.\n"
    "\n"
    "Example:\n"
    "ic> t: typedef struct {int x,y;} mypoint;\n"
    "ic> v: mypoint p;\n"
    "ic> p.x=4; p.y=2;\n"
    "ic> #include <stdio.h>\n"
    "ic> printf(\"xy: %d%d\\n\", p.x, p.y);\n"
    "\n"
    );

int  _last_f = 0;
char _last_fpath[MAXPATH];
char _last_cmd[MAXCMD];
char _code[MAXCODE];
int  _last_errno = 0;

typedef struct {
    const char* shorthand;
    const char *description;
    char* expanded;
} macro_t;

macro_t macros[2] = {
    { .shorthand = "iposix",
      .description = "include posix headers",
      .expanded =
      "#include <aio.h>\n"
      "#include <arpa/inet.h>\n"
      "#include <assert.h>\n"
      "#include <complex.h>\n"
      "#include <cpio.h>\n"
      "#include <ctype.h>\n"
      "#include <dirent.h>\n"
      "#include <dlfcn.h>\n"
      "#include <errno.h>\n"
      "#include <fcntl.h>\n"
      "#include <fenv.h>\n"
      "#include <float.h>\n"
      "#include <fmtmsg.h>\n"
      "#include <fnmatch.h>\n"
      "#include <ftw.h>\n"
      "#include <glob.h>\n"
      "#include <grp.h>\n"
      "#include <iconv.h>\n"
      "#include <inttypes.h>\n"
      "#include <iso646.h>\n"
      "#include <langinfo.h>\n"
      "#include <libgen.h>\n"
      "#include <limits.h>\n"
      "#include <locale.h>\n"
      "#include <math.h>\n"
      "#include <monetary.h>\n"
      "#include <mqueue.h>\n"
      "#include <net/if.h>\n"
      "#include <netdb.h>\n"
      "#include <netinet/in.h>\n"
      "#include <netinet/tcp.h>\n"
      "#include <nl_types.h>\n"
      "#include <poll.h>\n"
      "#include <pthread.h>\n"
      "#include <pwd.h>\n"
      "#include <regex.h>\n"
      "#include <sched.h>\n"
      "#include <search.h>\n"
      "#include <semaphore.h>\n"
      "#include <setjmp.h>\n"
      "#include <signal.h>\n"
      "#include <spawn.h>\n"
      "#include <stdarg.h>\n"
      "#include <stdbool.h>\n"
      "#include <stddef.h>\n"
      "#include <stdint.h>\n"
      "#include <stdio.h>\n"
      "#include <stdlib.h>\n"
      "#include <string.h>\n"
      "#include <strings.h>\n"
      "#include <sys/ipc.h>\n"
      "#include <sys/mman.h>\n"
      "#include <sys/msg.h>\n"
      "#include <sys/resource.h>\n"
      "#include <sys/select.h>\n"
      "#include <sys/sem.h>\n"
      "#include <sys/shm.h>\n"
      "#include <sys/socket.h>\n"
      "#include <sys/stat.h>\n"
      "#include <sys/statvfs.h>\n"
      "#include <sys/time.h>\n"
      "#include <sys/times.h>\n"
      "#include <sys/types.h>\n"
      "#include <sys/uio.h>\n"
      "#include <sys/un.h>\n"
      "#include <sys/utsname.h>\n"
      "#include <sys/wait.h>\n"
      "#include <syslog.h>\n"
      "#include <tar.h>\n"
      "#include <termios.h>\n"
      "#include <tgmath.h>\n"
      "#include <time.h>\n"
      "#include <ulimit.h>\n"
      "#include <unistd.h>\n"
      "#include <utime.h>\n"
      "#include <utmpx.h>\n"
      "#include <wchar.h>\n"
      "#include <wctype.h>\n"
      "#include <wordexp.h>\n"
    },
    { .shorthand = NULL },
};

void out(const char* fmt, ...) {
    if (ic_outfile == NULL) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    vfprintf(ic_outfile, fmt, args);
    va_end(args);
    fflush(ic_outfile);
}

int add_f(const char* fname, const char* s) {
    char *fpath = _last_fpath;
    _last_f += 1;
    sprintf(fpath, "%s/f%d-%s", ic_workdir, _last_f, fname);
    FILE *f = fopen(fpath, "w");
    if (f == NULL) {
        perror(fpath);
        return -1;
    }
    fprintf(f, "%s\n", s);
    fclose(f);
    return _last_f;
}

int systemf(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsprintf(_last_cmd, fmt, args);
  va_end(args);
  if (ic_debug != NULL) {
      out("%s\n", _last_cmd);
  }
  return system(_last_cmd);
}

void* compile_load_f(char* fpath) {
    char fpathso[MAXPATH] = "";
    systemf("touch %s/includes.h %s/types.h %s/vars.h", ic_workdir, ic_workdir, ic_workdir);
    strcat(fpathso, fpath);
    strcat(fpathso, ".so");
    systemf("%s -fPIC %s -shared -rdynamic %s -o %s %s", ic_cc, ic_cflags, ic_ldflags, fpathso, fpath);
    void* handle = dlopen(fpathso, RTLD_NOW|RTLD_GLOBAL);
    dlerror();
    return handle;
}

void add_include(const char* s) {
    add_f("include.h", s);
    systemf("( echo '#ifndef __IC_INCLUDES_H'; echo '#define __IC_INCLUDES_H'; ) > %s/includes.h; for f in %s/*include.h; do echo \"#include \\\"$f\\\"\" >> %s/includes.h; done; echo '#endif' >> %s/includes.h", ic_workdir, ic_workdir, ic_workdir, ic_workdir);
    /* TODO: test that include compiles, otherwise delete it. */
}

void add_type(const char* s) {
    add_f("type.h", s);
    systemf("( echo '#ifndef __IC_TYPES_H'; echo '#define __IC_TYPES_H'; ) > %s/types.h; for f in %s/*type.h; do echo \"#include \\\"$f\\\"\" >> %s/types.h; done; echo '#endif' >> %s/types.h", ic_workdir, ic_workdir, ic_workdir, ic_workdir);
    /* TODO: test that type compiles, otherwise delete it. */
}

void add_var(const char* s) {
    sprintf(_code,
            "#include  \"%s/includes.h\"\n"
            "#include  \"%s/types.h\"\n"
            "%s", ic_workdir, ic_workdir, s);
    add_f("var.c", _code);
    void  *handle = compile_load_f(_last_fpath);
    if (handle == NULL) {
        /* Bad variable declaration. Do not create a header for this. */
        unlink(_last_fpath);
        return;
    }
    /* Variable is now loaded in memory. Make it accessible to user
     * code via new include. */
    sprintf(_code,
            "#include  \"%s/includes.h\"\n"
            "#include  \"%s/types.h\"\n"
            "extern %s", ic_workdir, ic_workdir, s);
    add_f("var.h", _code);
    systemf("echo -n '' > %s/vars.h; for f in %s/*var.h; do echo \"#include \\\"$f\\\"\" >> %s/vars.h; done", ic_workdir, ic_workdir, ic_workdir);
}

void run(const char* s) {
    int runline_id = _last_f + 1;
    sprintf(_code,
            "#include \"%s/includes.h\"\n"
            "#include \"%s/vars.h\"\n"
            "void runline%d(void){\n"
            "%s"
            "}",
            ic_workdir, ic_workdir, runline_id, s);
    add_f("run.c", _code);
    void* handle = compile_load_f(_last_fpath);
    if (handle == NULL) {
        return;
    }
    sprintf(_code, "runline%d", runline_id);
    void (*runline)(void) = dlsym(handle, _code);
    if (runline == NULL) {
        return;
    }
    errno = _last_errno;
    runline();
    _last_errno = errno;
}

char* trim(const char* s) {
    if (s == NULL) {
        return NULL;
    }
    int start;
    for (start=0; start<strlen(s)-1 && isspace(s[start]); start++);
    int end;
    for (end=strlen(s)-1; end>=0 && isspace(s[end]); end--);
    if (end<start) end=start-1;
    char* trimmed = strndup(&(s[start]), end-start+1);
    return trimmed;
}

int handle_cmd(const char *);
void expand(const char* shorthand) {
    if (shorthand == NULL || strlen(shorthand) == 0) {
        return;
    }
    char* trimmed_ts = trim(shorthand);
    for (int i=0; macros[i].shorthand != NULL; i++) {
        if (strcmp(macros[i].shorthand, trimmed_ts) == 0) {
            char* exp = strdup(macros[i].expanded);
            char* line = strtok(exp, "\n");
            while (line) {
                printf("x> %s\n", line);
                handle_cmd(line);
                line = strtok(NULL, "\n");
            }
            free(exp);
            goto finish;
        }
    }
    out("expansions:\n");
    for (int i=0; macros[i].shorthand != NULL; i++) {
        out("   '%s': %s\n", macros[i].shorthand, macros[i].description);
    }
    out("no expansion for shorthand '%s'\n", trimmed_ts);
finish:
    free(trimmed_ts);
}

int handle_cmd(const char *cmd) {
    if (strncmp(cmd, "#", 1) == 0) {
        add_include(cmd);
        return 1;
    }
    if (strncmp(cmd, "t:", 2) == 0) {
        add_type(cmd+2);
        return 1;
    }
    if (strncmp(cmd, "v:", 2) == 0) {
        add_var(cmd+2);
        return 1;
    }
    if (strncmp(cmd, "x:", 2) == 0) {
        expand(cmd+2);
        return 1;
    }
    return 0;
}

int interactive(FILE* input) {
    char *cmdbuf = NULL;
    size_t cmdlen;
    int n = 1;
    int opt_echo = (getenv("IC_ECHO") != NULL);
    add_include("");
    while (1) {
        if (cmdbuf != NULL) {
            free(cmdbuf);
            cmdbuf = NULL;
        }
        out("ic> ");
        n = getline(&cmdbuf, &cmdlen, input);
        if (n <= 0 || cmdbuf == NULL) {
            break;
        }
        if (opt_echo) {
            out("%s", cmdbuf);
        }
        if (strlen(cmdbuf) == 0) {
            continue;
        }
        if (strncmp(cmdbuf, "help", 4) == 0) {
            out("%s", interactive_help);
            continue;
        }
        if (handle_cmd(cmdbuf)) {
            continue;
        }
        if (strncmp(cmdbuf, "q", 1) == 0 ||
            strncmp(cmdbuf, "quit", 4) == 0) {
            break;
        }
        run(cmdbuf);
    }
    if (cmdbuf != NULL) {
        free(cmdbuf);
        cmdbuf = NULL;
    }
    return 0;
}

int main(int argc, char** argv)
{
    ic_outfile = stdout;
    if (NULL == (ic_cc = getenv("IC_CC"))) {
        ic_cc = ic_cc_default;
    }
    if (NULL == (ic_cflags = getenv("IC_CFLAGS"))) {
        ic_cflags = ic_cflags_default;
    }
    if (NULL == (ic_ldflags = getenv("IC_LDFLAGS"))) {
        ic_ldflags = ic_ldflags_default;
    }
    if (NULL == (ic_debug = getenv("IC_DEBUG"))) {
        ic_debug = ic_debug_default;
    }
    ic_workdir = getenv("IC_WORKDIR");
    if (ic_workdir == NULL) {
        char template[128] = "/tmp/ic.XXXXXX";
        ic_workdir = mkdtemp(template);
        if (ic_workdir == NULL) {
            perror("ic");
            exit(1);
        }
    }
    if (argc == 1) {
        interactive(stdin);
    } else {
        FILE *input = fopen(argv[1], "r");
        if (input == NULL) {
            perror("cannot open input file");
            exit(1);
        }
        interactive(input);
    }
    return 0;
}
