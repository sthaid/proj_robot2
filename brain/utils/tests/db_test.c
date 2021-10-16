#include <utils.h>

#define GB  (1024 * 0x100000)

#define KEYID1  1

void get_all_cb(void *val, unsigned int val_len)
{
    INFO("  '%s'  %d\n", (char*)val, val_len);
}

void print_free_list(void)
{
#if 0
    INFO("PRINTING FREE LIST\n");
    uint64_t off;
    record_t *rec;
    for (off = free_head->next; off != NODE_OFFSET(free_head); off = NODE(off)->next) {
        rec = CONTAINER(NODE(off), record_t, free.node);
        INFO(" 0x%llx :  magic=0x%llx  len=%lld  %lld MB\n", 
             NODE_OFFSET(rec), rec->magic, rec->len, rec->len/MB);
    }
#endif
}

int main(int argc, char **argv)
{
    int rc, i;
    //void *val;
    //unsigned int val_len;
    char keystr[100], valstr[100];

    logging_init(NULL, false);

    //INFO("calling db_create\n");
    //db_create("db_test.dat", GB);
    //INFO("done\n");

    INFO("calling db_init\n");
    db_init("db_test.dat", true, GB);
    INFO("done\n");

    // add 100 entries to db
    for (i = 0; i < 10; i++) {
        sprintf(keystr, "key_%d", i);
        sprintf(valstr, "value_%d", i);
        rc = db_set(1, keystr, valstr, strlen(valstr)+1);
        if (rc < 0) {
            FATAL("db_set failed\n");
        }
    }

    print_free_list();

    // remove the first 50
    //for (i = 0; i < 5; i++) 
    for (i = 4; i >= 0; i--) 
    {
        sprintf(keystr, "key_%d", i);
        rc = db_rm(1, keystr);
        if (rc < 0) {
            FATAL("db_rm failed\n");
        }
    }

    // print the free list
    print_free_list();

#if 0
    // get entries, and confirm
    for (i = 0; i < 10; i++) {
        void *val;
        unsigned int val_len;
        sprintf(keystr, "key_%d", i);
        rc =  db_get(KEYID1, keystr, &val, &val_len);
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
#endif

    // get all keyid==1 values
    //db_get_all_keyid(1, get_all_cb);


    return 0;
}
