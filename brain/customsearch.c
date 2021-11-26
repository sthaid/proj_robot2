#include <common.h>

static void readline(char *s, int slen, FILE *fp, bool *eof);
static void cleanup_url(char *url);
static void cleanup_description(char *description);

// ----------------------------------------------------------------

int customsearch(char *transcript)
{
    FILE       *fp;
    char        title[1000], description[10000];
    char        cmd[2500], url[1000], wikipedia_page[1000], filename[1100];
    struct stat statbuf;
    int         rc, len;
    bool        eof;

    // print search request transcript
    INFO("transcript = '%s'\n", transcript);

    // perform google customsearch of  www.en.wikipedia.org/*, and
    // extract the best match url from the result
    sprintf(cmd, "./customsearch.py \"%s\" | grep htmlFormattedUrl | awk -F\\' '{ print $4}' | head -1", transcript);
    INFO("customsearch cmd = '%s'\n", cmd);
    fp = popen(cmd, "r");
    if (fp == NULL) {
        ERROR("popen customsearch.py failed, %s\n", strerror(errno));
        return -1;
    }
    readline(url, sizeof(url), fp, &eof);
    pclose(fp);
    if (eof) {
        ERROR("failed to read url from customsearch.py\n");
        return -1;
    }
    cleanup_url(url);
    INFO("url = '%s'\n", url);

    // generate scrape filename from url
    if (sscanf(url, "https://en.wikipedia.org/wiki/%s", wikipedia_page) != 1) {
        ERROR("bad url '%s'\n", url);
        return -1;
    }
    sprintf(filename, "scrape/%s", wikipedia_page);
    INFO("filename = '%s'\n", filename);

    // if file scrape file does not exist
    //   run beautifulsoup web scraper on the url
    // endif
    if (stat(filename, &statbuf) < 0) {
        sprintf(cmd, "./beautifulsoup.py '%s' > '%s'", url, filename);
        INFO("beautifysoup cmd = '%s'\n", cmd);
        if ((rc = system(cmd)) != 0) {
            ERROR("beautifysoup.py rc=0x%xx\n", rc);
            return -1;
        }
    } else {
        INFO("file %s already exists\n", filename);
    }

    // open the scrape file
    fp = fopen(filename, "r");
    if (fp == NULL) {
        ERROR("failed to open %s, %s\n", filename, strerror(errno));
        return -1;
    }

    // get title from the scrape file
    readline(title, sizeof(title), fp, &eof);

    // get description from the scrape file
    while (true) {
        // get candidate for description
        readline(description, sizeof(description), fp, &eof);

        // if eof then we have failed to get a description
        if (eof) break;

        // if description begins with "Coordinates:", or is shorter than 100 chars
        // then we won't use this candidate
        if (strncmp(description, "Coordinates:", 12) == 0) continue;
        if ((len=strlen(description)) < 100) continue;

        // this description will be used; also append the contents of the
        // next line to description
        readline(description+len, sizeof(description)-len, fp, &eof);
        break;
    }
    if (description[0] == '\0') {
        strcpy(description, "no information available");
    }

    // close scrape file
    fclose(fp);

    // play title
    t2s_play("title: %s", title);

    // cleanup and play description
    cleanup_description(description);
    INFO("description len=%zd - '%s'\n", strlen(description), description);
    t2s_play("%s", description);

    // done
    return 0;
}

void readline(char *s, int slen, FILE *fp, bool *eof)
{
    if (fgets(s, slen, fp) == NULL) {
        s[0] = '\0';
        *eof = true;
        return;
    }

    s[strcspn(s, "\n")] = '\0';
    *eof = false;
}

void cleanup_url(char *url)
{
    char *p;

    // remove <b> from url
    while (true) {
        p = strstr(url, "<b>");
        if (p == NULL) break;
        memmove(p, p+3, strlen(p+3)+1);
    }

    // remove </b> from url
    while (true) {
        p = strstr(url, "</b>");
        if (p == NULL) break;
        memmove(p, p+4, strlen(p+4)+1);
    }
}

void cleanup_description(char *description)
{
    char *p, *end, *start;
    int level;

    // remove double quote char
    p = description;
    while (true) {
        p = strchr(p, '"');
        if (p == NULL) break;
        memmove(p, p+1, strlen(p+1)+1);
    }

    // remove [...]
    p = description;
    while (true) {
        p = strchr(p, '[');
        if (p == NULL) break;
        end = strchr(p, ']');
        if (end == NULL) break;
        memmove(p, end+1, strlen(end+1)+1);
    }

    // remove {...}
    p = description;
    while (true) {
        p = strchr(p, '{');
        if (p == NULL) break;
        end = strchr(p, '}');
        if (end == NULL) break;
        memmove(p, end+1, strlen(end+1)+1);
    }

    // remove (...(...)...)
    p = description;
    level = 0;
    start = NULL;
    while (*p != '\0') {
        if (*p == '(') {
            if (level == 0) start = p;
            level++;
        }
        if (*p == ')') {
            level--;
            if ((level < 0) || (level == 0 && start == NULL)) {
                break;
            }
            if (level == 0) {
                memmove(start, p+1, strlen(p+1)+1);
                p = start-1;
                start = NULL;
            }
        }
        p++;
    }

    // remove these strings
    static char *strs[] = {
        ".mw-parser-output", 
        ".frac", 
        ".num", 
        ".den", 
        ".sr-only", 
                };
    for (int i = 0; i < sizeof(strs)/sizeof(strs[0]); i++) {
        p = description;
        while (true) {
            p = strstr(p, strs[i]);
            if (p == NULL) break;
            int len = strlen(strs[i]);
            memmove(p, p+len, strlen(p+len)+1);
        }
    }

    // if descriptin len is > 500 then shorten it by starting at 500 and
    // null terminating at the first period found
    if (strlen(description) > 500) {
        for (p = description+500; *p; p++) {
            if (*p == '.' && *(p+1) == ' ') {
                *(p+1) = '\0';
                break;
            }
        }
    }
}
