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

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#define MAXCMD 65535
#define MAXPATH 65535

const char *ic_cc = NULL;
const char *ic_cc_default = "cc";
const char *ic_cflags = NULL;
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

int _last_f = 0;
char _last_fpath[MAXPATH];
char _last_cmd[MAXCMD];

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

int add_f(const char* fname, char* s) {
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
    systemf("%s -O0 -g -fPIC %s -shared -rdynamic %s -o %s %s", ic_cc, ic_cflags, ic_ldflags, fpathso, fpath);
    void* handle = dlopen(fpathso, RTLD_NOW|RTLD_GLOBAL);
    dlerror();
    return handle;
}

void add_include(char* s) {
    add_f("include.h", s);
    systemf("( echo '#ifndef __IC_INCLUDES_H'; echo '#define __IC_INCLUDES_H'; ) > %s/includes.h; for f in %s/*include.h; do echo \"#include \\\"$f\\\"\" >> %s/includes.h; done; echo '#endif' >> %s/includes.h", ic_workdir, ic_workdir, ic_workdir, ic_workdir);
}

void add_type(char* s) {
    add_f("type.h", s);
    systemf("( echo '#ifndef __IC_TYPES_H'; echo '#define __IC_TYPES_H'; ) > %s/types.h; for f in %s/*type.h; do echo \"#include \\\"$f\\\"\" >> %s/types.h; done; echo '#endif' >> %s/types.h", ic_workdir, ic_workdir, ic_workdir, ic_workdir);
}

void add_var(char* s) {
    char *code = (char *)malloc(strlen(s) + MAXPATH + 1024);
    sprintf(code,
            "#include  \"%s/includes.h\"\n"
            "#include  \"%s/types.h\"\n"
            "%s", ic_workdir, ic_workdir, s);
    add_f("var.c", code);
    void  *handle = compile_load_f(_last_fpath);
    if (handle == NULL) {
        /* Bad variable declaration. Do not create a header for this. */
        unlink(_last_fpath);
        free(code);
        return;
    }
    /* Variable is now loaded in memory. Make it accessible to user
     * code via new include. */
    sprintf(code,
            "#include  \"%s/includes.h\"\n"
            "#include  \"%s/types.h\"\n"
            "extern %s", ic_workdir, ic_workdir, s);
    add_f("var.h", code);
    free(code);
    systemf("echo -n '' > %s/vars.h; for f in %s/*var.h; do echo \"#include \\\"$f\\\"\" >> %s/vars.h; done", ic_workdir, ic_workdir, ic_workdir);
}

void run(char* s) {
    char code[65536] = "";
    sprintf(code,
            "#include \"%s/includes.h\"\n"
            "#include \"%s/vars.h\"\n"
            "void runline(void){\n"
            "%s"
            "}",
            ic_workdir, ic_workdir, s);
    add_f("run.c", code);
    void* handle = compile_load_f(_last_fpath);
    if (handle == NULL) {
        return;
    }
    void (*runline)(void) = dlsym(handle, "runline");
    if (runline == NULL) {
        return;
    }
    runline();
    dlclose(handle);
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
        if (strncmp(cmdbuf, "#", 1) == 0) {
            add_include(cmdbuf);
            continue;
        }
        if (strncmp(cmdbuf, "t:", 2) == 0) {
            add_type(cmdbuf+2);
            continue;
        }
        if (strncmp(cmdbuf, "v:", 2) == 0) {
            add_var(cmdbuf+2);
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
