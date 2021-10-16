#include <utils.h>

#define GB  (1024 * 0x100000)

void test1(void);
void test2(void);
void get_keyid_cb(int keyid, char *keystr, void *val, unsigned int val_len);

// xxx doc usage
//  test1, test2, set, rm, get, get_keyid, print_free_list

// -----------------  MAIN  ------------------------------------------------

int main(int argc, char **argv)
{
    char *cmd, *arg1, *arg2;
    char s[1000], s_orig[1000];
    int rc;

    logging_init(NULL, false);

    db_init("db_test.dat", true, GB);

    while (printf("> "), fgets(s, sizeof(s), stdin) != NULL) {
        s[strcspn(s,"\n")] = '\0';
        strcpy(s_orig, s);

        cmd = strtok(s, " ");
        if (cmd == NULL) continue;
        arg1 = strtok(NULL, " ");
        arg2 = strtok(NULL, " ");

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
            rc = db_set(3, keystr, value, strlen(value)+1);
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
            rc = db_get(3, keystr, &value, &value_len);
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
            rc = db_rm(3, keystr);
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
            INFO("db_get i=%d rc=%d\n", i, rc);
            if (i >= 5) {
                FATAL("xxx\n");
            }
        } else {
            INFO("db_get i=%d rc=%d: '%s'  %d\n", i, rc, (char*)val, val_len);
            sprintf(valstr, "value_%d", i);
            if (i < 5 || strcmp(val, valstr) || val_len != strlen(val)+1) {
                FATAL("xxx\n");
            }
        }
    }

    INFO("test1 passed\n");
}

// -----------------  TEST2  -----------------------------------------------

void test2(void)
{
    // xxx multiple threads each thread randomly gets, sets, removes, and uses its own keyid
}
