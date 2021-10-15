// xxx tbd
// - msync
// - rwlock
// - unit test pgm
// - routine to get all for a keyid
// - add_to_free_list should combine with free neighbors
// - make list utils inline funcs
// - remove printf

#include <utils.h>

#include <sys/mman.h>
#include <stddef.h>

//
// defines
//

#define MB        0x100000
#define GB        (1024 * MB)
#define PAGE_SIZE 4096

#define MIN_FILE_LEN   MB
#define MIN_RECORD_LEN (sizeof(record_t) + 32)

#define MAX_KEYID  128

#define MAGIC_HDR          0x11111111
#define MAGIC_RECORD_FREE  0x22222222
#define MAGIC_RECORD_ENTRY 0x33333333

//
// defines for lists
//

#define NODE(offset)   ((node_t*)(mmap_addr + (offset)))
#define NODE_OFFSET(node)  ((uint64_t)((void*)(node) - mmap_addr))

#define CONTAINER(node,type,field)  ((type*)((void*)(node) - offsetof(type,field)))

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

static_assert(sizeof(hdr_t) == PAGE_SIZE, "");
static_assert(sizeof(record_t) == 64, "");

//
// variables
//

static void   * mmap_addr;
static hdr_t  * hdr;
static node_t * hash_tbl;
static void   * data;

static node_t * free_head;
static node_t * keyid_head;

//
// prototypes
//
static record_t *find(char keyid, char *keystr, int *htidx);
static record_t *alloc(uint64_t keyfull_len, uint64_t val_len);
static void add_to_free_list(record_t *rec, uint64_t len);
static int hash(char keyid, char *keystr);
static uint64_t round_up64(uint64_t x, uint64_t boundary);

void init_list_head(node_t *n);
void add_to_list_tail(node_t *head, node_t *new_tail);
void add_to_list_head(node_t *head, node_t *new_tail);
void remove_from_list(node_t *node);

// -----------------  DB CREATE AND INIT  -------------------------------------------

void db_create(char *file_name, uint64_t file_len)
{
    int fd, rc, i;
    uint64_t max_hash_tbl;

    printf("hdr size    = %zd\n", sizeof(hdr_t));
    printf("record size = %zd\n", sizeof(record_t));

    // verify file_len is a multiple of PAGE_SIZE
    if ((file_len % PAGE_SIZE) || (file_len < MIN_FILE_LEN)) {
        FATAL("file_len 0x%llx invalid\n", file_len);
    }

    // create empty file of size file_len
    fd = open(file_name, O_CREAT|O_EXCL|O_RDWR, 0666);
    if (fd < 0) {
        FATAL("create %s, %s\n", file_name, strerror(errno));
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

    // set max_hash_tbl so that the hash table size is 1/32 of file_len
    max_hash_tbl = (file_len / 32) / sizeof(node_t);
    max_hash_tbl = round_up64(max_hash_tbl, 256);
    printf("max_hash_tbl %lld  0x%llx\n", max_hash_tbl, max_hash_tbl);
    printf("hash_tbl len %lld  0x%llx\n", max_hash_tbl * sizeof(node_t), max_hash_tbl * sizeof(node_t));
    printf("file len     %lld  0x%llx\n", file_len, file_len);

    // init pointers to the 3 sections of the file, and convenience pointers
    hdr       = mmap_addr;
    hash_tbl  = mmap_addr + sizeof(hdr_t);
    data      = hash_tbl + max_hash_tbl;
    free_head = &hdr->free_head;
    keyid_head = hdr->keyid_head;

    printf("hdr=%p  hash_tbl=%p  data=%p\n", hdr, hash_tbl, data);

    // init hdr
    hdr->magic        = MAGIC_HDR;
    hdr->file_len     = file_len;
    hdr->hdr_len      = sizeof(hdr_t);
    hdr->hash_tbl_len = max_hash_tbl * sizeof(node_t);
    hdr->data_len     = file_len - hdr->hdr_len - hdr->hash_tbl_len;
    hdr->max_hash_tbl = max_hash_tbl;
    init_list_head(&hdr->free_head);
    for (i = 0; i < MAX_KEYID; i++) {
        init_list_head(&hdr->keyid_head[i]);
    }
    printf("datalen = %lld  0x%llx\n", hdr->data_len, hdr->data_len);
        
    // init hash_tbl 
    for (i = 0; i < max_hash_tbl; i++) {
        init_list_head(&hash_tbl[i]);
    }

    // init data by placing a free record at the begining of data
    add_to_free_list((record_t*)data, hdr->data_len);

    // xxx
    munmap(mmap_addr, file_len);
    close(fd);
}

void db_init(char *file_name)
{
    int fd;
    hdr_t Hdr;
    struct stat buf;

    // open file and read hdr
    fd = open(file_name, O_RDWR, 0666);
    if (fd < 0) {
        FATAL("create %s, %s\n", file_name, strerror(errno));
    }
    if (read(fd, &Hdr, sizeof(Hdr)) != sizeof(Hdr)) {
        FATAL("read hdr %s, %s\n", file_name, strerror(errno));
    }

    // verify hdr magic
    if (Hdr.magic != MAGIC_HDR) {
        FATAL("invalid hdr magic, 0x%llx should be 0x%x\n", Hdr.magic, MAGIC_HDR);
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

    // init pointers to the 3 sections of the file, and convenience pointers
    hdr        = mmap_addr;
    hash_tbl   = mmap_addr + sizeof(hdr_t);
    data       = hash_tbl + hdr->max_hash_tbl;
    free_head  = &hdr->free_head;
    keyid_head = hdr->keyid_head;

    printf("db_init max_hash_tbl  0x%llx\n", hdr->max_hash_tbl);
}

// -----------------  DB ACCESS  ----------------------------------------------------

// caller must ensure this db entry is not removed or changed
// until after caller is done with val
int db_get(char keyid, char *keystr, void **val, unsigned int *val_len)
{
    record_t *rec;
    int htidx;

    // find the record with keyid and keystr
    rec = find(keyid, keystr, &htidx);
    if (rec == NULL) {
        return -1;
    }

    // return ptr to value and the length of value
    *val = (void*)rec + rec->entry.value_offset;
    *val_len = rec->entry.value_len;
    return 0;
}

int db_set(char keyid, char *keystr, void *val, unsigned int val_len)
{
    record_t *rec;
    int htidx;

    // check keyid arg
    if (keyid <= 0 || keyid >= MAX_KEYID) {
        ERROR("invalid keyid %d\n", keyid);
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
            return 0;
        } else {
            remove_from_list(&rec->entry.node_keyid);
            remove_from_list(&rec->entry.node_hashtbl);
            add_to_free_list(rec, rec->len);
        }
    }

    // allocate a record from the free list
    unsigned int keyfull_len = (1 + strlen(keystr) + 1);
    rec = alloc(keyfull_len, val_len);
    if (rec == NULL) {
        return -1;
    }

    // initialize record fields
    rec->entry.value_offset = sizeof(record_t); //xxx needed ?
    rec->entry.keyfull_offset = sizeof(record_t) + val_len;  //xxx needed?0
    rec->entry.value_len = val_len;
    rec->entry.value_alloc_len = val_len;

    // copy the value and key to after the record_t
    void *rec_value = (char*)rec + rec->entry.value_offset;
    memcpy(rec_value, val, val_len);

    char *keyfull = (char*)rec + rec->entry.keyfull_offset;
    keyfull[0] = keyid;
    strcpy(keyfull+1, keystr);

    // add the record's list nodes to the lists
    add_to_list_tail(&hash_tbl[htidx], &rec->entry.node_hashtbl);
    add_to_list_tail(&keyid_head[(int)keyid], &rec->entry.node_keyid);

    // return success
    return 0;
}

int db_rm(char keyid, char *keystr)
{
    record_t *rec;
    int htidx;

    // check keyid arg
    if (keyid <= 0 || keyid >= MAX_KEYID) {
        ERROR("invalid keyid %d\n", keyid);
        return -1;
    }

    // find the record that is to be removed
    rec = find(keyid, keystr, &htidx);
    if (rec == NULL) {
        return -1;
    }

    // unlink record from lists
    remove_from_list(&rec->entry.node_keyid);
    remove_from_list(&rec->entry.node_hashtbl);

    // add record to free list
    add_to_free_list(rec, rec->len);

    // success
    return 0;
}

int db_get_all_keyid(char keyid, void (*callback)(void *val, unsigned int val_len))
{
    node_t *head;
    uint64_t off;
    record_t *rec;

    // check keyid arg
    if (keyid <= 0 || keyid >= MAX_KEYID) {
        ERROR("invalid keyid %d\n", keyid);
        return -1;  // xxx these could be assert or fatal  and then some of these routines could ret void
    }

    head = &keyid_head[(int)keyid];

    for (off = head->next; off != NODE_OFFSET(head); off = NODE(off)->next) {
        rec = CONTAINER(NODE(off), record_t, entry.node_keyid);
        callback((void*)rec+rec->entry.value_offset, rec->entry.value_len);
    }

    return 0;
}

// -----------------  GENERAL UTILS  ------------------------------------------------

static record_t *find(char keyid, char *keystr, int *htidx)
{
    uint64_t off;
    node_t *head;
    record_t *rec;
    char *keyfull;

    *htidx = hash(keyid, keystr);
    head = &hash_tbl[*htidx];

    for (off = head->next; off != NODE_OFFSET(head); off = NODE(off)->next) {
        rec = CONTAINER(NODE(off), record_t, entry.node_hashtbl);
        keyfull = (void*)rec + rec->entry.keyfull_offset;

        if (keyfull[0] == keyid && strcmp(keystr, &keyfull[1]) == 0) {
            return rec;
        }
    }

    return NULL;
}

static record_t *alloc(uint64_t keyfull_len, uint64_t val_len)
{
    uint64_t  len_to_alloc, off;
    record_t *rec;
    bool      found;

    // xxx
    len_to_alloc = round_up64(
                    sizeof(record_t) + val_len + keyfull_len + sizeof(uint64_t),
                    32); // xxx define for 32
    if (len_to_alloc < MIN_RECORD_LEN) {
        len_to_alloc = MIN_RECORD_LEN;
    }
    
    // loop over free list until a record is found with adequate len
    found = false;
    for (off = free_head->next; off != NODE_OFFSET(free_head); off = NODE(off)->next) {
        rec = CONTAINER(NODE(off), record_t, free.node);

        assert(rec->magic == MAGIC_RECORD_FREE);
        assert(rec->len >= MIN_RECORD_LEN);
        assert((rec->len & (32-1)) == 0);
        // xxx assert the len at the end too

        if (rec->len >= len_to_alloc) {
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

    // if the record found does not have enough length to be divided then
    // return the record
    if (rec->len - len_to_alloc < MIN_RECORD_LEN) {
        rec->magic = MAGIC_RECORD_ENTRY;
        return rec;
    }

    // divide the record
    uint64_t new_free_rec_len = rec->len - len_to_alloc;
    record_t *new_free_rec = (void*)rec + len_to_alloc;
    add_to_free_list(new_free_rec, new_free_rec_len);

    // return allocated record, with it's magic and len fields set
    rec->magic = MAGIC_RECORD_ENTRY;
    rec->len = len_to_alloc;
    return rec;
}

static void add_to_free_list(record_t *rec, uint64_t len)
{
    printf("add to free list   %lld  0x%llx\n", len,len);
    assert(len >= MIN_RECORD_LEN);
    assert((len & (32-1)) == 0);  //xxx 32 

    // init the record
    rec->magic = MAGIC_RECORD_FREE;
    rec->len = len;

    // add rec to the head of the free list
    add_to_list_head(free_head, &rec->free.node);
}

static int hash(char keyid, char *keystr)
{
    return 7; // xxx later  - remoember mod hash_tbl_len
}

// boundary must be power of 2
static uint64_t round_up64(uint64_t x, uint64_t boundary)
{
    return (x + (boundary-1)) & ~(boundary-1);
}

// -----------------  LIST UTILS  ---------------------------------------------------

void init_list_head(node_t *n)
{
    n->next = n->prev = NODE_OFFSET(n);
}

void add_to_list_tail(node_t *head, node_t *new_last)
{
    node_t *old_last = NODE(head->prev);
    new_last->next   = NODE_OFFSET(head);
    new_last->prev   = NODE_OFFSET(old_last);
    old_last->next   = NODE_OFFSET(new_last);
    head->prev       = NODE_OFFSET(new_last);
}

void add_to_list_head(node_t *head, node_t *new_first)
{
    node_t *old_first = NODE(head->next);
    new_first->next   = NODE_OFFSET(old_first);
    new_first->prev   = NODE_OFFSET(head);
    old_first->prev   = NODE_OFFSET(new_first);
    head->next        = NODE_OFFSET(new_first);
}

void remove_from_list(node_t *node)
{
    node_t *prev = NODE(node->prev);
    node_t *next = NODE(node->next);
    node->next = node->prev = 0;
    prev->next = NODE_OFFSET(next);
    next->prev = NODE_OFFSET(prev);
}

// -----------------  DB TOOL  ------------------------------------------------------

#ifdef DB_TOOL

#define KEYID1  1

void get_all_cb(void *val, unsigned int val_len)
{
    INFO("  '%s'  %d\n", (char*)val, val_len);
}

int main(int argc, char **argv)
{
    int rc, i;
    //void *val;
    //unsigned int val_len;
    char keystr[100], valstr[100];

    logging_init(NULL, false);

    INFO("calling db_create\n");
    db_create("db_test.dat", GB);
    INFO("done\n");

    INFO("calling db_init\n");
    db_init("db_test.dat");
    INFO("done\n");

    // add 100 entries to db
    for (i = 0; i < 100; i++) {
        sprintf(keystr, "key_%d", i);
        sprintf(valstr, "value_%d", i);
        rc = db_set(1, keystr, valstr, strlen(valstr)+1);
        if (rc < 0) {
            FATAL("db_set failed\n");
        }
    }

    // remove every other one
    for (i = 0; i < 100; i+=2) {
        sprintf(keystr, "key_%d", i);
        rc = db_rm(1, keystr);
        if (rc < 0) {
            FATAL("db_rm failed\n");
        }
    }

    // get entries, and confirm
    for (i = 0; i < 100; i++) {
        void *val;
        unsigned int val_len;
        sprintf(keystr, "key_%d", i);
        rc =  db_get(KEYID1, keystr, &val, &val_len);
        if (rc < 0) {
            INFO("db_get i=%d rc=%d\n", i, rc);
            if ((i & 1) == 1) {
                FATAL("xxx\n");
            }
        } else {
            INFO("db_get i=%d rc=%d: '%s'  %d\n", i, rc, (char*)val, val_len);
            sprintf(valstr, "value_%d", i);
            if ((i & 1) == 0 || strcmp(val, valstr) || val_len != strlen(val)+1) {
                FATAL("xxx\n");
            }
        }
    }

    // get all keyid==1 values
    db_get_all_keyid(1, get_all_cb);

    // print the free list
    uint64_t off;
    record_t *rec;
    for (off = free_head->next; off != NODE_OFFSET(free_head); off = NODE(off)->next) {
        rec = CONTAINER(NODE(off), record_t, free.node);
        INFO(" 0x%llx :  magic=0x%llx  len=%lld  %lld MB\n", 
             NODE_OFFSET(rec), rec->magic, rec->len, rec->len/MB);
    }



#if 0
    val = "steve"; 
    rc = db_set(KEYID1, "name", val, strlen(val)+1);
    if (rc < 0) {
        FATAL("db_set failed\n");
    }

    INFO("dbget return val='%s', val_len=%d\n", (char*)val, val_len);
#endif


    return 0;
}

// XXX later, test
//  db_init("dbxxx.dat");
#endif

