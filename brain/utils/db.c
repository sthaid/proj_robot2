#include <utils.h>

//
// defines
//

#define MIN_FILE_LEN MB

#define RECORD_BOUNDARY 32
#define MIN_RECORD_LEN  (sizeof(record_t) + RECORD_BOUNDARY)

#define MAX_KEYID  128

#define MAGIC_HDR          0x11111111
#define MAGIC_RECORD_FREE  0x22222222
#define MAGIC_RECORD_ENTRY 0x33333333

#define REC_LEN_AT_END(r)  (*(uint64_t*)((void*)(r) + (r)->len - sizeof(uint64_t)))
#define SET_REC_LEN(r,l) \
    do { \
        (r)->len = (l); \
        REC_LEN_AT_END(r) = (r)->len; \
    } while (0)

#define RW_INITLOCK   do { pthread_rwlock_init(&rwlock,NULL); } while (0)
#define RW_RDLOCK     do { pthread_rwlock_rdlock(&rwlock); } while (0)
#define RW_WRLOCK     do { pthread_rwlock_wrlock(&rwlock); } while (0)
#define RW_UNLOCK     do { pthread_rwlock_unlock(&rwlock); } while (0)

//
// typedefs
//

typedef struct {
    uint64_t next;
    uint64_t prev;
} node_t;

typedef struct {
    uint64_t magic;
    uint64_t file_len;
    uint64_t hdr_len;
    uint64_t hash_tbl_len;
    uint64_t data_len;
    uint64_t max_hash_tbl;
    node_t   free_head;
    node_t   keyid_head[MAX_KEYID];
    char     pad[1984];  // pad to 4096
} hdr_t;

typedef struct {
    uint64_t magic;
    uint64_t len;
    union {
        struct {
            node_t   node_keyid;
            node_t   node_hashtbl;
            uint32_t keyfull_offset;
            uint32_t value_offset;
            uint32_t value_len;
            uint32_t value_alloc_len;
        } entry;
        struct {
            node_t node;
        } free;
    };
} record_t;

//
// variables
//

static void       * mmap_addr;
static hdr_t      * hdr;
static node_t     * hash_tbl;
static void       * data;
static void       * data_end;
static node_t     * free_head;
static node_t     * keyid_head;
static unsigned int max_hash_tbl;

static pthread_rwlock_t rwlock;

//
// prototypes
//

static void create_db_file(char *file_name, uint64_t file_len);
static record_t *find(int keyid, char *keystr, int *htidx);
static record_t *alloc_record(uint64_t alloc_len);
static void combine_free(record_t *rec);
static unsigned int hash(int keyid, char *keystr);
static uint64_t round_up64(uint64_t x, uint64_t boundary);
static unsigned int round_up32(unsigned int x, unsigned int boundary);

//
// linked lists
//

#define NODE(offset)   ((node_t*)(mmap_addr + (offset)))
#define NODE_OFFSET(node)  ((uint64_t)((void*)(node) - mmap_addr))
#define CONTAINER(node,type,field)  ((type*)((void*)(node) - offsetof(type,field)))

static void init_list_head(node_t *n);
static void add_to_list_tail(node_t *head, node_t *new_tail);
static void add_to_list_head(node_t *head, node_t *new_tail);
static void remove_from_list(node_t *node);

//
// static asserts
//

static_assert(sizeof(hdr_t) == 4096, "");
static_assert(sizeof(record_t) == 64, "");

// -----------------  DB INT AND CREATE   -------------------------------------------

void db_init(char *file_name, bool create, uint64_t file_len)
{
    int fd, rc;
    hdr_t Hdr;
    struct stat buf;

    // if db file does not exist and the create flag is set then create it
    if (stat(file_name, &buf) < 0) {
        if (errno != ENOENT) {
            FATAL("file %s, unexpected stat errno, %s\n", file_name, strerror(errno));
        }
        if (!create) {
            FATAL("file %s does not exist\n", file_name);
        }
        create_db_file(file_name, file_len);
    }

    // open file and read hdr
    fd = open(file_name, O_RDWR, 0666);
    if (fd < 0) {
        FATAL("open %s, %s\n", file_name, strerror(errno));
    }
    if (read(fd, &Hdr, sizeof(Hdr)) != sizeof(Hdr)) {
        FATAL("read hdr %s, %s\n", file_name, strerror(errno));
    }

    // verify hdr magic
    if (Hdr.magic != MAGIC_HDR) {
        FATAL("file  %s, invalid hdr magic, 0x%llx should be 0x%x\n", file_name, Hdr.magic, MAGIC_HDR);
    }

    // stat file and validate st_size matches file_len stored in hdr
    if (fstat(fd, &buf) < 0) {
        FATAL("stat %s, %s\n", file_name, strerror(errno));
    }
    if (Hdr.file_len != buf.st_size) {
        FATAL("size %s, 0x%lx should be 0x%llx\n", file_name, buf.st_size, Hdr.file_len);
    }
    
    // mmap the file
    mmap_addr = mmap(NULL, Hdr.file_len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (mmap_addr == NULL) {
        FATAL("mmap %s, %s\n", file_name, strerror(errno));
    }

    // don't fork mmap'ed memory;
    // this seems a good idea, but did not reduce the fork/exec time
    rc = madvise(mmap_addr, Hdr.file_len, MADV_DONTFORK);
    if (rc < 0) {
        FATAL("madvice(%p,%lld), %s\n", mmap_addr, Hdr.file_len, strerror(errno));
    }

    // init globals
    hdr          = mmap_addr;
    hash_tbl     = mmap_addr + sizeof(hdr_t);
    data         = hash_tbl + hdr->max_hash_tbl;
    data_end     = data + hdr->data_len;
    free_head    = &hdr->free_head;
    keyid_head   = hdr->keyid_head;
    max_hash_tbl = hdr->max_hash_tbl;

    // asserts
    assert(data_end == mmap_addr + hdr->file_len);

    // init reader/writer lock
    RW_INITLOCK;
}

static void create_db_file(char *file_name, uint64_t file_len)
{
    int fd, rc, i;
    unsigned int max_ht;

    // verify file_len is a multiple of PAGE_SIZE
    if ((file_len % PAGE_SIZE) || (file_len < MIN_FILE_LEN)) {
        FATAL("file_len 0x%llx invalid\n", file_len);
    }

    // create empty file of size file_len
    fd = open(file_name, O_CREAT|O_EXCL|O_RDWR, 0666);
    if (fd < 0) {
        FATAL("open for create %s, %s\n", file_name, strerror(errno));
    }
    rc = ftruncate(fd, file_len);
    if (rc < 0) {
        FATAL("truncate %s, %s\n", file_name, strerror(errno));
    }

    // mmap the file
    mmap_addr = mmap(NULL, file_len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (mmap_addr == NULL) {
        FATAL("mmap %s, %s\n", file_name, strerror(errno));
    }

    // set max_ht so that the hash table size is 1/32 of file_len
    max_ht = (file_len / 32) / sizeof(node_t);
    max_ht = round_up64(max_ht, 256);

    // init hdr
    hdr = mmap_addr;
    hdr->magic        = MAGIC_HDR;
    hdr->file_len     = file_len;
    hdr->hdr_len      = sizeof(hdr_t);
    hdr->hash_tbl_len = max_ht * sizeof(node_t);
    hdr->data_len     = file_len - hdr->hdr_len - hdr->hash_tbl_len;
    hdr->max_hash_tbl = max_ht;
    init_list_head(&hdr->free_head);
    for (i = 0; i < MAX_KEYID; i++) {
        init_list_head(&hdr->keyid_head[i]);
    }

    // init globals
    hdr          = mmap_addr;
    hash_tbl     = mmap_addr + sizeof(hdr_t);
    data         = hash_tbl + hdr->max_hash_tbl;
    data_end     = data + hdr->data_len;
    free_head    = &hdr->free_head;
    keyid_head   = hdr->keyid_head;
    max_hash_tbl = hdr->max_hash_tbl;

    // asserts
    assert(data_end == mmap_addr + hdr->file_len);

    // init hash_tbl 
    for (i = 0; i < max_ht; i++) {
        init_list_head(&hash_tbl[i]);
    }

    // init data by placing a free record at the begining of data
    record_t *rec = (record_t*)data;
    rec->magic = MAGIC_RECORD_FREE;
    SET_REC_LEN(rec, hdr->data_len);
    add_to_list_head(free_head, &rec->free.node);

    // unmap and close
    munmap(mmap_addr, file_len);
    close(fd);
    mmap_addr = NULL;

    // print message
    INFO("created %s, size=%lld MB\n", file_name, file_len/MB);
}

// -----------------  DB ACCESS  ----------------------------------------------------

// caller must ensure this db entry is not removed or changed
// until after caller is done with val
int db_get(int keyid, char *keystr, void **val, unsigned int *val_len)
{
    record_t *rec;
    int htidx;

    RW_RDLOCK;

    // preset returns
    *val = NULL;
    *val_len = 0;

    // check keyid arg
    if (keyid < 0 || keyid >= MAX_KEYID) {
        ERROR("invalid keyid %d\n", keyid);
        RW_UNLOCK;
        return -1;
    }

    // find the record with keyid and keystr
    rec = find(keyid, keystr, &htidx);
    if (rec == NULL) {
        RW_UNLOCK;
        return -1;
    }

    // return ptr to value and the length of value
    *val = (void*)rec + rec->entry.value_offset;
    *val_len = rec->entry.value_len;

    RW_UNLOCK;
    return 0;
}

int db_set(int keyid, char *keystr, void *val, unsigned int val_len)
{
    record_t *rec;
    int htidx;

    RW_WRLOCK;

    // check val_len
    if (val_len == 0) {
        ERROR("invalid val_len %d\n", val_len);
        RW_UNLOCK;
        return -1;
    }

    // check keyid arg
    if (keyid < 0 || keyid >= MAX_KEYID) {
        ERROR("invalid keyid %d\n", keyid);
        RW_UNLOCK;
        return -1;
    }

    // determine if the record already exists; 
    // this call also returns the hash tbl idx, which is used later in this routine
    rec = find(keyid, keystr, &htidx);

    // if it already exists
    //   if its value_alloc_len is large enough
    //     replace the value
    //     return
    //   else
    //     remove the record from the lists, and
    //     add it to the head of the free list
    //   endif
    // endif
    if (rec) {
        if (rec->entry.value_alloc_len >= val_len) {
            memcpy((void*)rec+rec->entry.value_offset, val, val_len);
            rec->entry.value_len = val_len;
            RW_UNLOCK;
            return 0;
        } else {
            remove_from_list(&rec->entry.node_keyid);
            remove_from_list(&rec->entry.node_hashtbl);

            rec->magic = MAGIC_RECORD_FREE;
            add_to_list_head(free_head, &rec->free.node);
            combine_free(rec);
        }
    }

    // allocate a record from the free list
    unsigned int min_alloc_len, actual_alloc_len, spare_len, keyfull_len;

    keyfull_len = round_up32(1 + strlen(keystr) + 1, 8);
    min_alloc_len = sizeof(record_t) +
                    keyfull_len + 
                    val_len +
                    sizeof(uint64_t);
    actual_alloc_len = round_up32(min_alloc_len, RECORD_BOUNDARY);
    spare_len = actual_alloc_len - min_alloc_len;

    rec = alloc_record(actual_alloc_len);
    if (rec == NULL) {
        RW_UNLOCK;
        return -1;
    }

    // initialize record fields
    rec->entry.keyfull_offset = sizeof(record_t);
    rec->entry.value_offset = rec->entry.keyfull_offset + keyfull_len;
    rec->entry.value_len = val_len;
    rec->entry.value_alloc_len = val_len + spare_len;

    // copy the value and key to after the record_t
    char *keyfull = (char*)rec + rec->entry.keyfull_offset;
    keyfull[0] = keyid;
    strcpy(keyfull+1, keystr);

    void *rec_value = (char*)rec + rec->entry.value_offset;
    memcpy(rec_value, val, val_len);

    // add the record's list nodes to the lists
    add_to_list_tail(&hash_tbl[htidx], &rec->entry.node_hashtbl);
    add_to_list_tail(&keyid_head[(int)keyid], &rec->entry.node_keyid);

    // return success
    RW_UNLOCK;
    return 0;
}

int db_rm(int keyid, char *keystr)
{
    record_t *rec;
    int htidx;

    RW_WRLOCK;

    // check keyid arg
    if (keyid < 0 || keyid >= MAX_KEYID) {
        ERROR("invalid keyid %d\n", keyid);
        RW_UNLOCK;
        return -1;
    }

    // find the record that is to be removed
    rec = find(keyid, keystr, &htidx);
    if (rec == NULL) {
        RW_UNLOCK;
        return -1;
    }

    // unlink record from lists
    remove_from_list(&rec->entry.node_keyid);
    remove_from_list(&rec->entry.node_hashtbl);

    // add record to free list
    rec->magic = MAGIC_RECORD_FREE;
    add_to_list_head(free_head, &rec->free.node);
    combine_free(rec);

    // success
    RW_UNLOCK;
    return 0;
}

int db_get_keyid(int keyid, void (*callback)(int keyid, char *keystr, void *val, unsigned int val_len))
{
    node_t *head;
    uint64_t off;
    record_t *rec;

    RW_RDLOCK;

    // check keyid arg
    if (keyid < 0 || keyid >= MAX_KEYID) {
        ERROR("invalid keyid %d\n", keyid);
        RW_UNLOCK;
        return -1;
    }

    // loop over they keyid list, for the caller specified keyid, and
    // call callback proc for all db entries on this list
    head = &keyid_head[(int)keyid];
    for (off = head->next; off != NODE_OFFSET(head); off = NODE(off)->next) {
        rec = CONTAINER(NODE(off), record_t, entry.node_keyid);
        char *keyfull = (void*)rec + rec->entry.keyfull_offset;
        callback(keyfull[0], 
                 keyfull+1,
                 (void*)rec+rec->entry.value_offset, 
                 rec->entry.value_len);
    }

    // success
    RW_UNLOCK;
    return 0;
}

// -----------------  DB ACCESS UTILS  ----------------------------------------------

// these 2 routines set/get a numeric value to the db
void db_set_num(int keyid, char *keystr, double value)
{
    char val_str[50];
    int rc;

    sprintf(val_str, "%g", value);
    rc = db_set(keyid, keystr, val_str, strlen(val_str)+1);
    assert(rc == 0);
}

double db_get_num(int keyid, char *keystr, double default_value)
{
    char *val_str;
    unsigned int val_len;
    int rc;
    double val;

    rc = db_get(keyid, keystr, (void**)&val_str, &val_len);
    if (rc < 0) {
        db_set_num(keyid, keystr, default_value);
        return default_value;
    } else {
        rc = sscanf(val_str, "%lg", &val);
        assert(rc == 1);
        return val;
    }
}

// -----------------  GENERAL UTILS  ------------------------------------------------

static record_t *find(int keyid, char *keystr, int *htidx)
{
    uint64_t off;
    node_t *head;
    record_t *rec;
    char *keyfull;

    *htidx = hash(keyid, keystr);
    head = &hash_tbl[*htidx];

    for (off = head->next; off != NODE_OFFSET(head); off = NODE(off)->next) {
        rec = CONTAINER(NODE(off), record_t, entry.node_hashtbl);

        assert(rec->magic == MAGIC_RECORD_ENTRY);
        assert(rec->len >= MIN_RECORD_LEN);
        assert((rec->len & (RECORD_BOUNDARY-1)) == 0);
        assert(rec->len == REC_LEN_AT_END(rec));

        keyfull = (void*)rec + rec->entry.keyfull_offset;

        if (keyfull[0] == keyid && strcmp(keystr, &keyfull[1]) == 0) {
            return rec;
        }
    }

    return NULL;
}

static record_t *alloc_record(uint64_t alloc_len)
{
    uint64_t  off;
    record_t *rec;
    bool      found;
    
    // loop over free list until a record is found with adequate len
    found = false;
    for (off = free_head->next; off != NODE_OFFSET(free_head); off = NODE(off)->next) {
        rec = CONTAINER(NODE(off), record_t, free.node);

        assert(rec->magic == MAGIC_RECORD_FREE);
        assert(rec->len >= MIN_RECORD_LEN);
        assert((rec->len & (RECORD_BOUNDARY-1)) == 0);
        assert(rec->len == REC_LEN_AT_END(rec));

        if (rec->len >= alloc_len) {
            found = true;
            break;
        }
    }

    // if there are no records available that are large enough then return error
    if (!found) {
        return NULL;
    }

    // remove the record found from the free list
    remove_from_list(&rec->free.node);

    rec->magic = MAGIC_RECORD_ENTRY;

    // if the record found does not have enough length to be divided then
    // return the record
    if (rec->len - alloc_len < MIN_RECORD_LEN) {
        return rec;
    }

    // divide the record
    uint64_t rec_len_save = rec->len;
    SET_REC_LEN(rec, alloc_len);

    record_t *new_free_rec = (void*)rec + alloc_len;
    new_free_rec->magic = MAGIC_RECORD_FREE;
    SET_REC_LEN(new_free_rec, rec_len_save - alloc_len);

    if ((void*)new_free_rec + new_free_rec->len == data_end) {
        add_to_list_tail(free_head, &new_free_rec->free.node);
    } else {
        add_to_list_head(free_head, &new_free_rec->free.node);
    }

    // call combine_free to combine this new_free_rec with adjacent free records
    combine_free(new_free_rec);

    // return allocated record, with it's magic and len fields set
    return rec;
}

static void combine_free(record_t *rec)
{
    assert(rec->magic == MAGIC_RECORD_FREE);

    // if the next record in data memory exists and is free then
    //   remove the next record from the free list
    //   combine its space into the rec
    // endif
    if ((void*)rec + rec->len < data_end) {
        record_t * next_rec = (record_t*)((void*)rec + rec->len);

        assert(next_rec->magic == MAGIC_RECORD_FREE || next_rec->magic == MAGIC_RECORD_ENTRY);

        if (next_rec->magic == MAGIC_RECORD_FREE) {
            remove_from_list(&next_rec->free.node);
            SET_REC_LEN(rec, rec->len + next_rec->len);
        }
    }

    // if the prior record in data memory exists and is free then
    //   remove the prior record from the free list
    //   combine its space into the rec
    // endif
    if ((void*)rec > data) {
        uint64_t prior_rec_len = *(uint64_t*)((void*)rec - sizeof(uint64_t));
        record_t * prior_rec = (record_t*)((void*)rec - prior_rec_len);

        assert(prior_rec->magic == MAGIC_RECORD_FREE || prior_rec->magic == MAGIC_RECORD_ENTRY);

        if (prior_rec->magic == MAGIC_RECORD_FREE) {
            remove_from_list(&rec->free.node);
            SET_REC_LEN(prior_rec, prior_rec->len + rec->len);
        }
    }
}

static unsigned int hash(int keyid, char *keystr)
{
    unsigned int crc = crc32_multi_buff(2, &keyid, (size_t)1, keystr, strlen(keystr));
    unsigned int htidx = (crc % max_hash_tbl);

    return htidx;
}

// boundary must be power of 2
static uint64_t round_up64(uint64_t x, uint64_t boundary)
{
    return (x + (boundary-1)) & ~(boundary-1);
}

static unsigned int round_up32(unsigned int x, unsigned int boundary)
{
    return (x + (boundary-1)) & ~(boundary-1);
}

// -----------------  LIST UTILS  ---------------------------------------------------

static void init_list_head(node_t *n)
{
    n->next = n->prev = NODE_OFFSET(n);
}

static void add_to_list_tail(node_t *head, node_t *new_last)
{
    node_t *old_last = NODE(head->prev);
    new_last->next   = NODE_OFFSET(head);
    new_last->prev   = NODE_OFFSET(old_last);
    old_last->next   = NODE_OFFSET(new_last);
    head->prev       = NODE_OFFSET(new_last);
}

static void add_to_list_head(node_t *head, node_t *new_first)
{
    node_t *old_first = NODE(head->next);
    new_first->next   = NODE_OFFSET(old_first);
    new_first->prev   = NODE_OFFSET(head);
    old_first->prev   = NODE_OFFSET(new_first);
    head->next        = NODE_OFFSET(new_first);
}

static void remove_from_list(node_t *node)
{
    node_t *prev = NODE(node->prev);
    node_t *next = NODE(node->next);
    node->next = node->prev = 0;
    prev->next = NODE_OFFSET(next);
    next->prev = NODE_OFFSET(prev);
}

// -----------------  DEBUG / TEST UTILS  -------------------------------------------

// ---- free list ----

void db_print_free_list(void)
{
    uint64_t off;
    record_t *rec;
    int num_entries=0;

    RW_RDLOCK;

    INFO("FREE LIST ...\n");
    for (off = free_head->next; off != NODE_OFFSET(free_head); off = NODE(off)->next) {
        rec = CONTAINER(NODE(off), record_t, free.node);
        INFO("  node_offset = 0x%llx   magic = 0x%llx   len=%lld  %lld MB\n", 
             NODE_OFFSET(rec), rec->magic, rec->len, rec->len/MB);
        num_entries++;
    }
    INFO("  num_entries = %d\n", num_entries);
    INFO("\n");

    RW_UNLOCK;
}

unsigned int db_get_free_list_len(void)
{
    int num_entries=0;

    RW_RDLOCK;
    for (uint64_t off = free_head->next; off != NODE_OFFSET(free_head); off = NODE(off)->next) {
        num_entries++;
    }
    RW_UNLOCK;

    return num_entries;
}

// ---- db reset ----

void db_reset(void)
{
    unsigned int i;

    RW_WRLOCK;

    // reset list heads
    init_list_head(&hdr->free_head);
    for (i = 0; i < MAX_KEYID; i++) {
        init_list_head(&hdr->keyid_head[i]);
    }
    for (i = 0; i < max_hash_tbl; i++) {
        init_list_head(&hash_tbl[i]);
    }

    // init data by placing a free record at the begining of data
    record_t *rec = (record_t*)data;
    rec->magic = MAGIC_RECORD_FREE;
    SET_REC_LEN(rec, hdr->data_len);
    add_to_list_head(free_head, &rec->free.node);

    RW_UNLOCK;
}

// ---- db dump ----

static int last_keyid_dumped;
static void dump_cb(int keyid, char *keystr, void *val, unsigned int val_len);
static char *val_str(void *val_arg, unsigned int val_len);

void db_dump(void)
{
    RW_RDLOCK;

    last_keyid_dumped = -1;
    for (int keyid = 0; keyid < MAX_KEYID; keyid++) {
        db_get_keyid(keyid, dump_cb);
    }

    INFO("\n");
    db_print_free_list();

    RW_UNLOCK;
}

static void dump_cb(int keyid, char *keystr, void *val, unsigned int val_len)
{
    int i;
    char keystr2[33];

    if (keyid > last_keyid_dumped) {
        INFO("KEYID = %d\n", keyid);
        last_keyid_dumped = keyid;
    }

    strncpy(keystr2, keystr, sizeof(keystr2));
    keystr2[sizeof(keystr2)-1] = '\0';
    for (i = 0; keystr2[i]; i++) {
        if (keystr2[i] == '\n') keystr2[i] = '.';
    }

    INFO("  %-32s = %-39s  len=%d\n", keystr2, val_str(val,val_len), val_len);
}

static char *val_str(void *val_arg, unsigned int val_len)
{
    static char val_str[40];
    char *val = val_arg;

    assert(val_len > 0);
    if (val_len > sizeof(val_str)) val_len = sizeof(val_str);

    for (int i = 0; i < val_len; i++) {
        val_str[i] = (isprint(val[i]) ? val[i] : '.');
    }
    val_str[val_len-1] = '\0';

    return val_str;
}
