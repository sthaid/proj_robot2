#include <utils.h>

// xxx doc usage
//  test1, test2, set, rm, get, get_keyid, print_free_list

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
            rc = db_set(1, keystr, value, strlen(value)+1);
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
            rc = db_get(1, keystr, &value, &value_len);
            if (rc < 0) {
                ERROR("db_get ret %d\n", rc);
            } else if (strlen(value)+1 != value_len) {
                ERROR("value_len=%d should be %d\n", value_len, strlen(value)+1);
            } else {
                INFO("value='%s'\n", (char*)value);
            }
        } else if (strcmp(cmd, "rm") == 0) {
            char *keystr = arg1;
            if (keystr == NULL) {
                goto error;
            }
            rc = db_rm(1, keystr);
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
    INFO("  %d:%s  = %s  %s\n", keyid, keystr, (char*)val, errstr);
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

// prototypes
void *test2_thread(void *cx);
void print_stats(int secs);
void get_random_keystr(unsigned int keyid, char *keystr, int *keystr_idx);
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

    // clear all values from the test database
    db_reset();

    // create test2 threads
    for (int i = 0; i < MAX_TEST2_THREADS; i++) {
        pthread_create(&tid[i], NULL, test2_thread, (void*)i);
    }

    // wait until sigint
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
        usleep(10000);
    }

    // join with exitting test2_threads
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
    
    INFO("%4d: db_set %d / %d    db_get %d / %d    db_rm %d / %d\n",
         secs,
         total.db_set_okay, total.db_set_notok,
         total.db_get_okay, total.db_get_notok,
         total.db_rm_okay, total.db_rm_notok);
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

    INFO("test2_thread %d starting\n", keyid);
    
    while (!sigint) {
        // 10 db get
        for (int i = 0; i < 10; i++) {
            get_random_keystr(keyid, keystr, &keystr_idx);
            rc = db_get(keyid, keystr, &val, &val_len);
            if ((rc == 0) != (exp_val_tbl[keystr_idx].val != NULL)) {
                ERROR("XXX\n");
            } else if (rc == 0 && exp_val_tbl[keystr_idx].val_len != val_len) {
                ERROR("XXX\n");
            } else if (rc == 0 && memcmp(exp_val_tbl[keystr_idx].val, val, val_len) != 0) {
                ERROR("XXX\n");
            } else {  // okay
                // xxx incr stat
            }
            if (rc == 0) my_stats->db_get_okay++; else my_stats->db_get_notok++;
        }

        // 1 db set
        get_random_keystr(keyid, keystr, &keystr_idx);
        get_random_val(&val, &val_len);
        rc = db_set(keyid, keystr, val, val_len);
        if (rc < 0) {
            ERROR("XXX\n");
        }
        exp_val_tbl[keystr_idx].val = val;
        exp_val_tbl[keystr_idx].val_len = val_len;
        if (rc == 0) my_stats->db_set_okay++; else my_stats->db_set_notok++;

        // 1 db rm
        get_random_keystr(keyid, keystr, &keystr_idx);
        rc = db_rm(keyid, keystr);
        if ((rc == 0) != (exp_val_tbl[keystr_idx].val != NULL)) {
            ERROR("XXX\n");
        }
        if (rc == 0) {
            exp_val_tbl[keystr_idx].val = NULL;
            exp_val_tbl[keystr_idx].val_len = 0;
        }
        if (rc == 0) my_stats->db_rm_okay++; else my_stats->db_rm_notok++;
    }

    INFO("test2_thread %d terminating\n", keyid);

    return NULL;
}

// - - - - - - - - - - - - - - 

void get_random_keystr(unsigned int keyid, char *keystr, int *keystr_idx)
{
    int idx = random_range(0,1000-1);
    sprintf(keystr, "%d:keystr=%d", keyid, idx);
    if (keystr_idx) *keystr_idx = idx;
}

// - - - - - - - - - - - - - - 

unsigned char random_val_data[1000000];

void get_random_val_init(void)
{
    int *x = (int *)random_val_data;

    if (x[1] == 1) {
        INFO("XXX already init\n");
        return;
    }

    for (int i = 0; i < sizeof(random_val_data)/sizeof(int); i++) {
        x[i] = i;
    }
}

void get_random_val(void **val, unsigned int *val_len)
{
    *val_len = random_range(100,10000);
    *val = random_val_data + random_range(0,900000);
}

// - - - - - - - - - - - - - - 

int random_range(int min, int max)
{
    int span = max - min + 1;
    return (random() % span) + min;
}
