#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#if defined(PACMAN)
#define COMMAND_FORMAT "pacman -Qi '%s' | grep Depends | cut -d: -f2"
#elif defined(APTITUDE)
#error TODO
#else
#error "No package manager defined!"
#endif

typedef struct
{
    char* output;
    bool success;
} Result;

static Result RunCommand(const char* command)
{
    FILE* pipe;
    int buffSz = 0;
    int buffCap = 1024;
    char* buff = malloc(buffCap);
    char chunk[4096];

    pipe = popen(command, "r");
    if (pipe == NULL)
        return (Result) { .success = false, .output = strerror(errno) };

    while (fgets(chunk, sizeof(chunk), pipe) != NULL) {
        int chunkSz = strlen(chunk);
        if (buffSz + chunkSz >= buffCap) {
            while (buffSz + chunkSz >= buffCap)
                buffCap *= 2;
            buff = realloc(buff, buffCap);
            assert(buff != NULL && "Buy more RAM!");
        }
        memcpy(buff + buffSz, chunk, chunkSz);
        buffSz += chunkSz;
    }

    int result = pclose(pipe);

    buff[buffSz] = 0;
    return (Result) {
        .success = result == 0,
        .output = buff
    };
}

typedef bool (*CallbackFunc)(const char* pkg, const char* dep);
static void FindAllPackages(const char* pkg, CallbackFunc KeepGoing)
{
    char command[1024];
    snprintf(command, sizeof(command), COMMAND_FORMAT, pkg);
    Result res = RunCommand(command);
    if (!res.success) {
        fprintf(stderr, "%s", res.output);
        exit(1);
    }
    char* saveptr;
    for (char* dep = res.output;
         NULL != (dep = strtok_r(dep, " \n", &saveptr));
         dep = NULL)
    {
        if (strcmp(dep, "None") == 0)
            break;
        if (KeepGoing(pkg, dep))
            FindAllPackages(dep, KeepGoing);
    }
    free(res.output);
}


struct
{
    char* key;
    bool value;
}* hashmap;

static bool AddEdge(const char* pkg, const char* dep)
{
    printf("  \"%s\" -> \"%s\";\n", pkg, dep);
    if (shgetp_null(hashmap, dep) != NULL)
        return false;
    shput(hashmap, dep, 1);
    return true;
}

static void ConstructGraph(const char* root)
{
    sh_new_arena(hashmap);
    printf("digraph {\n");
    FindAllPackages(root, AddEdge);
    printf("}\n");
    shfree(hashmap);
}

int main(int argc, char** argv)
{
    assert(argc >= 1);
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <package>\n", argv[0]);
        return 1;
    }
    ConstructGraph(argv[1]);
    return 0;
}

