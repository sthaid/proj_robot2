#include <utils.h>

// usage
// - test1:                quick test of db functions
// - test2:                multitheaded duration test
// - set <keystr> <value>: call db_set
// - get <keystr>:         call db_get
// - get_keyid:            call db_get_keyid
// - rm:                   call db_rm
// - print_free_list, pfl: call db_print_free_list
//
// the set, get and rm commands use keyid=0

//
// defines
//

#define GB  (1024 * 0x100000)

//
// variables
//

bool sigint;

//
// prototypes
//

void sigint_hndlr(int sig);
void get_keyid_cb(int keyid, char *keystr, void *val, unsigned int val_len);
char *strtrunc(char *s);
void test1(void);
void test2(void);

// -----------------  MAIN  ------------------------------------------------

int main(int argc, char **argv)
{
    char *cmd, *arg1, *arg2;
    char s[1000], s_orig[1000];
    int rc;

    static struct sigaction act;
    act.sa_handler = sigint_hndlr;
    sigaction(SIGINT, &act, NULL);
    
    logging_init(NULL, false, true);

    db_init("db_test.dat", true, GB);

    while (printf("> "), fgets(s, sizeof(s), stdin) != NULL) {
        s[strcspn(s,"\n")] = '\0';
        strcpy(s_orig, s);

        cmd = strtok(s, " ");
        if (cmd == NULL) continue;
        arg1 = strtok(NULL, " ");
        arg2 = strtok(NULL, " ");

        sigint = false;

        if (strcmp(cmd, "test1") == 0) {
            test1();
        } else if (strcmp(cmd, "test2") == 0) {
            test2();
        } else if (strcmp(cmd, "set") == 0) {
            char *keystr = arg1;
            char *value  = arg2;
            if (keystr == NULL || value == NULL) {
                goto error;
            }
            rc = db_set(0, keystr, value, strlen(value)+1);
            if (rc < 0) {
                ERROR("db_set ret %d\n", rc);
            }
        } else if (strcmp(cmd, "get") == 0) {
            char *keystr = arg1;
            void *value;
            unsigned int value_len;
            if (keystr == NULL) {
                goto error;
            }
            rc = db_get(0, keystr, &value, &value_len);
            if (rc < 0) {
                ERROR("db_get ret %d\n", rc);
            } else if (strlen(value)+1 != value_len) {
                ERROR("value_len=%d should be %d\n", value_len, strlen(value)+1);
            } else {
                INFO("value='%s'\n", strtrunc(value));
            }
        } else if (strcmp(cmd, "rm") == 0) {
            char *keystr = arg1;
            if (keystr == NULL) {
                goto error;
            }
            rc = db_rm(0, keystr);
            if (rc < 0) {
                ERROR("db_get ret %d\n", rc);
            }
        } else if (strcmp(cmd, "get_keyid") == 0) {
            char *keyidstr = arg1;
            int keyid;
            if (keyidstr == NULL) {
                goto error;
            }
            if (sscanf(keyidstr, "%d", &keyid) != 1) {
                goto error;
            }
            rc = db_get_keyid(keyid, get_keyid_cb);
            if (rc < 0) {
                ERROR("db_get_keyid ret %d\n", rc);
            }
        } else if (strcmp(cmd, "print_free_list") == 0 || strcmp(cmd, "pfl") == 0) {
            db_print_free_list();
        } else if (strcmp(cmd, "q") == 0) {
            break;
        } else {
            goto error;
        }
        continue;

error:
        ERROR("invalid input: %s\n", s_orig);
    }

    return 0;
}

void sigint_hndlr(int sig)
{
    sigint = true;
}

void get_keyid_cb(int keyid, char *keystr, void *val, unsigned int val_len)
{
    char errstr[100] = {0};

    if (strlen(val)+1 != val_len) {
        sprintf(errstr, "  *** ERROR val_len=%d should be %d ***", val_len, strlen(val)+1);
    }
    INFO("  %d:%s  = %s  %s\n", keyid, keystr, strtrunc(val), errstr);
}

char *strtrunc(char *s)
{
    static char str[50];

    strncpy(str, s, 32);
    if (str[31] != '\0') {
        strcpy(str+32, " ...");
    } else {
        str[32] = '\0';
    }
    return str;
}

// -----------------  TEST1  -----------------------------------------------

void test1(void)
{
    int rc, i;
    char keystr[100], valstr[100];
    void *val;
    unsigned int val_len;

    // add 10 entries to db
    for (i = 0; i < 10; i++) {
        sprintf(keystr, "key_%d", i);
        sprintf(valstr, "value_%d", i);
        rc = db_set(1, keystr, valstr, strlen(valstr)+1);
        if (rc < 0) {
            FATAL("db_set failed\n");
        }
    }

    // remove the first 5
    for (i = 4; i >= 0; i--) {
        sprintf(keystr, "key_%d", i);
        rc = db_rm(1, keystr);
        if (rc < 0) {
            FATAL("db_rm failed\n");
        }
    }

    // get entries, and confirm
    for (i = 0; i < 10; i++) {
        sprintf(keystr, "key_%d", i);
        rc =  db_get(1, keystr, &val, &val_len);
        if (rc < 0) {
            if (i >= 5) {
                FATAL("db_get failed but should have succeeded, i=%d\n", i);
            }
        } else {
            sprintf(valstr, "value_%d", i);
            if (i < 5 || strcmp(val, valstr) || val_len != strlen(val)+1) {
                FATAL("db_get succeeded but should have failed, i=%d\n", i);
            }
        }
    }

    INFO("test1 passed\n");
}

// -----------------  TEST2  -----------------------------------------------

// defines
#define MAX_TEST2_THREADS 4

// variables
struct stats_s {
    unsigned int db_set_okay;
    unsigned int db_set_notok;
    unsigned int db_get_okay;
    unsigned int db_get_notok;
    unsigned int db_rm_okay;
    unsigned int db_rm_notok;
} stats[MAX_TEST2_THREADS];

bool terminate_threads;
unsigned int db_set_okay_last;

// prototypes
void *test2_thread(void *cx);
void print_stats(int secs);
void get_random_keystr(char *keystr, int *keystr_idx);
void get_random_val_init(void);
void get_random_val(void **val, unsigned int *val_len);
int random_range(int min, int max);

// - - - - - - - - - - - - - - 

void test2(void)
{
    pthread_t tid[MAX_TEST2_THREADS];
    uintptr_t i;
    int count=0;

    // initialize
    get_random_val_init();
    memset(stats, 0, sizeof(stats));
    terminate_threads = false;
    db_set_okay_last = 0;

    // clear all values from the test database
    db_reset();

    // create test2 threads
    for (int i = 0; i < MAX_TEST2_THREADS; i++) {
        pthread_create(&tid[i], NULL, test2_thread, (void*)i);
    }

    // poll until it is time to stop the test;
    // and print stats 
    uint64_t start = time(NULL);
    while (true) {
        if (sigint) {
            INFO("*** SIGINT ***\n");
            break;
        }
        if (++count == 100) {
            print_stats(time(NULL)-start);
            count = 0;
        }
        usleep(100000);  // 0.1 secs
    }

    // join with exitting test2_threads
    terminate_threads = true;    
    for (i = 0; i < MAX_TEST2_THREADS; i++) {
        pthread_join(tid[i], NULL);
    }

    // print final stats
    print_stats(time(NULL)-start);
}

void print_stats(int secs)
{
    struct stats_s total;

    memset(&total, 0, sizeof(total));

    for (int i = 0; i < MAX_TEST2_THREADS; i++) {
        total.db_set_okay += stats[i].db_set_okay;
        total.db_set_notok += stats[i].db_set_notok;
        total.db_get_okay += stats[i].db_get_okay;
        total.db_get_notok += stats[i].db_get_notok;
        total.db_rm_okay += stats[i].db_rm_okay;
        total.db_rm_notok += stats[i].db_rm_notok;
    }
    
    INFO("%4d: db_set %d / %d    db_get %d / %d    db_rm %d / %d    free_list_len %d    num_db_set %d\n",
         secs,
         total.db_set_okay, total.db_set_notok,
         total.db_get_okay, total.db_get_notok,
         total.db_rm_okay, total.db_rm_notok,
         db_get_free_list_len(),
         total.db_set_okay - db_set_okay_last);

    db_set_okay_last = total.db_set_okay;
}

// - - - - - - - - - - - - - - 

void *test2_thread(void *cx)
{
    unsigned int threadid = (uintptr_t)cx;
    unsigned int keyid = threadid;
    int keystr_idx;
    char keystr[100];
    void *val;
    unsigned int val_len;
    int rc;
    struct stats_s * my_stats = &stats[threadid];

    struct {
        void *val;
        unsigned int val_len;
    } exp_val_tbl[1000];

    memset(exp_val_tbl, 0, sizeof(exp_val_tbl));

    while (!terminate_threads) {
        // 10 db get
        for (int i = 0; i < 10; i++) {
            get_random_keystr(keystr, &keystr_idx);
            rc = db_get(keyid, keystr, &val, &val_len);
            if ((rc == 0) != (exp_val_tbl[keystr_idx].val != NULL)) {
                ERROR("db_get(%d,%s), rc=%d exp_val_tbl[%d].val=%p\n", 
                      keyid, keystr, rc, keystr_idx, exp_val_tbl[keystr_idx].val);
            } else if (rc == 0 && exp_val_tbl[keystr_idx].val_len != val_len) {
                ERROR("db_get(%d,%s), rc=%d exp_val_tbl[%d].val_len=%d val_len=%d\n", 
                      keyid, keystr, rc, keystr_idx, exp_val_tbl[keystr_idx].val_len, val_len);
            } else if (rc == 0 && memcmp(exp_val_tbl[keystr_idx].val, val, val_len) != 0) {
                ERROR("db_get(%d,%s), rc=%d exp_val_tbl[%d].val failed memcmp\n",
                      keyid, keystr, rc, keystr_idx);
            }
            if (rc == 0) my_stats->db_get_okay++; else my_stats->db_get_notok++;
        }

        // 1 db set
        get_random_keystr(keystr, &keystr_idx);
        get_random_val(&val, &val_len);
        rc = db_set(keyid, keystr, val, val_len);
        if (rc < 0) {
            ERROR("db_set(%d,%s), rc=%d val_len=%d\n",
                  keyid, keystr, rc, val_len);
        }
        exp_val_tbl[keystr_idx].val = val;
        exp_val_tbl[keystr_idx].val_len = val_len;
        if (rc == 0) my_stats->db_set_okay++; else my_stats->db_set_notok++;

        // 1 db rm
        get_random_keystr(keystr, &keystr_idx);
        rc = db_rm(keyid, keystr);
        if ((rc == 0) != (exp_val_tbl[keystr_idx].val != NULL)) {
            ERROR("db_rm(%d,%s), rc=%d exp_val_tbl[%d].val=%p\n",
                  keyid, keystr, rc, keystr_idx, exp_val_tbl[keystr_idx].val);
        }
        if (rc == 0) {
            exp_val_tbl[keystr_idx].val = NULL;
            exp_val_tbl[keystr_idx].val_len = 0;
        }
        if (rc == 0) my_stats->db_rm_okay++; else my_stats->db_rm_notok++;
    }

    return NULL;
}

// - - - - - - - - - - - - - - 

void get_random_keystr(char *keystr, int *keystr_idx)
{
    int idx = random_range(0,1000-1);
    sprintf(keystr, "keystr=%d", idx);
    if (keystr_idx) *keystr_idx = idx;
}

// - - - - - - - - - - - - - - 

char *random_val[1000];
unsigned int random_val_len[1000];

void get_random_val_init(void)
{
    static bool initialized;
    int i,j;

    if (initialized) {
        return;
    }

    for (i = 0; i < 1000; i++) {
        random_val_len[i] = random_range(100,10000);
        random_val[i] = malloc(random_val_len[i]);
        for (j = 0; j < random_val_len[i]-1; j++) {
            random_val[i][j] = random_range('A', 'Z');
        }
        random_val[i][j] = '\0';
    }

    initialized = true;
}

void get_random_val(void **val, unsigned int *val_len)
{
    int idx = random_range(0, 1000-1);
    *val = random_val[idx];
    *val_len = random_val_len[idx];
}

// - - - - - - - - - - - - - - 

int random_range(int min, int max)
{
    int span = max - min + 1;
    return (random() % span) + min;
}
