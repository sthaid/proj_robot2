#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <jsmn.h>

#define MAX_JS    1000000
#define MAX_TOKEN 10

#define JSMN_ERROR_STR(x) \
    ((x) == JSMN_ERROR_NOMEM ? "JSMN_ERROR_NOMEM" : \
     (x) == JSMN_ERROR_INVAL ? "JSMN_ERROR_INVAL" : \
     (x) == JSMN_ERROR_PART  ? "JSMN_ERROR_PART"   : \
                               "JSMN_ERROR_INVALID_ERROR")

#define JSMN_TYPE_STR(x) \
    ((x) == JSMN_UNDEFINED ? "JSMN_UNDEFINED" : \
     (x) == JSMN_OBJECT    ? "JSMN_OBJECT"    : \
     (x) == JSMN_ARRAY     ? "JSMN_ARRAY"     : \
     (x) == JSMN_STRING    ? "JSMN_STRING"    : \
     (x) == JSMN_PRIMITIVE ? "JSMN_PRIMITIVE" : \
                             "JSMN_INVALID_TYPE")

#define TOKEN_LEN(t) ((t)->end - (t)->start)

char        js[MAX_JS];
int         jslen;
jsmn_parser p;
jsmntok_t   tokens[MAX_TOKEN];

// caller should free the returned string
char *token_str(char *js, jsmntok_t *t)
{
    if (t->type != JSMN_STRING) {
        fprintf(stderr, "ERROR: not JSMN_STRING\n");
        exit(1);
    }
    return strndup(js+t->start, TOKEN_LEN(t));
}
    
int main(int argc, char **argv)
{
    int rc;

    // read js from stdin
    jslen = 0;
    while (true) {
        rc = read(0, js+jslen, MAX_JS-jslen);
        if (rc == 0) {
            break;
        } 
        if (rc < 0) {
            fprintf(stderr, "ERROR: read stdin, %s\n", strerror(errno));
            return 1;
        }
        jslen += rc;
        if (jslen == MAX_JS) {
            fprintf(stderr, "ERROR: js buffer is full\n");
            return 1;
        }
    }

    // parse the json
    jsmn_init(&p);
    rc = jsmn_parse(&p, js, jslen, tokens, MAX_TOKEN);
    if (rc < 0) {
        fprintf(stderr, "ERROR: failed to parse json, %s\n", JSMN_ERROR_STR(rc));
        return 1;
    }

#if 0
    // debug prints
    fprintf(stderr, "jsmn_parse rc=%d\n", rc);
    for (int i = 0; i < rc; i++) {
        fprintf(stderr, "DEBUG: %d %s start=%d end=%d size=%d\n",
               i, JSMN_TYPE_STR(tokens[i].type), tokens[i].start, tokens[i].end, tokens[i].size);
    }
    fprintf(stderr, "DEBUG: token[1] string = '%s'\n", token_str(js, &tokens[1]));
#endif

    // verify 
    if (rc != 3 ||
        tokens[0].type != JSMN_OBJECT ||
        tokens[1].type != JSMN_STRING ||
        tokens[2].type != JSMN_STRING ||
        strcmp(token_str(js,&tokens[1]), "audioContent") != 0)
    {
        fprintf(stderr, "ERROR: invalid json input\n");
        return 1;
    }

    // write token2 string to stdout
    jsmntok_t *t = &tokens[2];
    rc = write(1, js+t->start, TOKEN_LEN(t));
    if (rc != TOKEN_LEN(t)) {
        fprintf(stderr, "ERROR: write rc=%d exp=%d, %s\n", rc, TOKEN_LEN(t), strerror(errno));
        return 1;
    }

    // done
    fprintf(stderr, "success\n");
    return 0;
}
