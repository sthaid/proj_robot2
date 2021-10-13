// xxx tbd
// - msync
// - rwlock
// - unit test pgm
// - routine to get all for a keyid
#include <utils.h>

#include <sys/mman.h>
#include <stddef.h>

//
// defines
//

#define MB        0x100000
#define PAGE_SIZE 4096
#define MAX_KEYID  128

#define MAGIC_HDR 0x11111111
#define MAGIC_RECORD_FREE 0x22222222
#define MAGIC_RECORD_ENTRY 0x33333333

//
// defines for lists
//

#define NODE(offset)   ((node_t*)(addr + (offset)))
#define NODE_OFFSET(node)  ((void*)(node) - addr)

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
    int      max_hash_tbl;
    node_t   free_head;
    node_t   keyid_head[MAX_KEYID];
    char     pad[1984];  // pad to 4096
} hdr_t;

typedef struct {
    int magic;
    int len;
    union {
        struct {
            node_t node_keyid;
            node_t node_keyfull;
            int    keyfull_offset;
            int    value_offset;
            int    value_len;
            int    value_alloc_len;
            int    pad[2];
        } entry;
        struct {
            node_t node;
        } free;
    };
} record_t;

//
// variables
//

static void   * addr;  // xxx name
static hdr_t  * hdr;
static node_t * hash_tbl;
static void   * data;

static node_t * free_head;
static node_t * keyid_head;

//
// prototypes
//
static record_t *find(char keyid, char *keystr);
static record_t *alloc(int len);
static void add_to_free_list(record_t *rec, int len);
static int hash(char keyid, char *keystr);

void init_list_head(node_t *n);
void add_to_list_tail(node_t *head, node_t *new_tail);
void add_to_list_head(node_t *head, node_t *new_tail);
void remove_from_list(node_t *node);

// -----------------  DB CREATE AND INIT  -------------------------------------------

void db_create(char *file_name, size_t file_len)
{
    int fd, rc, max_hash_tbl, i;

    printf("hdr size    = %zd\n", sizeof(hdr_t));
    printf("record size = %zd\n", sizeof(record_t));

    // xxx use ct assert
    assert(sizeof(hdr_t) == PAGE_SIZE);
    assert(sizeof(record_t) == 64);

    // verify file_len is a multiple of PAGE_SIZE
    // xxx also minimum size
    if (file_len % PAGE_SIZE) {
        FATAL("file_len 0x%zx invalid\n", file_len);
    }

    // create empty file of size file_len
    fd = open(file_name, O_CREAT|O_EXCL|O_RDWR, 0666);
    if (fd < 0) {
        FATAL("create %s, %s\n", file_name, strerror(errno));
    }
    rc = ftruncate(fd, file_len);
    if (rc < 0) {
        FATAL("truncate, %s\n", strerror(errno));
    }

    // mmap the file
    addr = mmap(NULL, file_len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == NULL) {
        FATAL("mmap failed, %s\n", strerror(errno));
    }

    // set max_hash_tbl so that the hash table size is 1/32 of file_len
    max_hash_tbl = (file_len / 32) / sizeof(node_t);
    printf("max_hash_tbl %d\n", max_hash_tbl);
    printf("hash_tbl len %zd\n", max_hash_tbl * sizeof(node_t));
    printf("hash_tbl len 0x%zx\n", max_hash_tbl * sizeof(node_t));
    printf("file len 0x%zx\n", file_len);

    // init pointers to the 3 sections of the file, and convenience pointers
    hdr       = addr;
    hash_tbl  = addr + sizeof(hdr_t);
    data      = hash_tbl + max_hash_tbl;
    free_head = &hdr->free_head;
    keyid_head = hdr->keyid_head;

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
    printf("datalen = %lld\n", hdr->data_len);
        
    // init hash_tbl 
    for (i = 0; i < max_hash_tbl; i++) {
        init_list_head(&hash_tbl[i]);
    }

    // init data by placing a free record at the begining of data
    add_to_free_list((record_t*)data, hdr->data_len);
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
        FATAL("read hdr, %s\n", strerror(errno));
    }

    // verify hdr magic
    if (Hdr.magic != MAGIC_HDR) {
        FATAL("invalid hdr magic\n");
    }

    // stat file and validate st_size matches file_len stored in hdr
    if (fstat(fd, &buf) < 0) {
        FATAL("stat, %s\n", strerror(errno));
    }
    if (Hdr.file_len != buf.st_size) {
        FATAL("size\n");  // xxx review these prints
    }
    
    // mmap the file
    addr = mmap(NULL, Hdr.file_len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == NULL) {
        FATAL("mmap failed, %s\n", strerror(errno));
    }

    // init pointers to the 3 sections of the file, and convenience pointers
    hdr        = addr;
    hash_tbl   = addr + sizeof(hdr_t);
    data       = hash_tbl + hdr->max_hash_tbl;
    free_head  = &hdr->free_head;
    keyid_head = hdr->keyid_head;
}

// -----------------  DB ACCESS  ----------------------------------------------------

// xxx XXX review from here
int db_get(char keyid, char *keystr, void **val, int *val_len)
{
    record_t *rec;

    rec = find(keyid, keystr);
    if (rec == NULL) {
        return -1;
    }

    *val = (void*)rec + rec->entry.value_offset;
    *val_len = rec->entry.value_len;
    return 0;
}

int db_set(char keyid, char *keystr, void *val, int val_len)
{
    record_t *rec;

    // xxx check keyid arg and others?

    // determine if the record already exists
    rec = find(keyid, keystr);

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
            remove_from_list(&rec->entry.node_keyfull);
            add_to_free_list(rec, rec->len);
        }
    }

    // allocate a record from the free list
    int keyfull_len, rec_len;
    keyfull_len = (1 + strlen(keystr) + 1);
    rec_len = sizeof(record_t) + keyfull_len + val_len;
    rec = alloc(rec_len);
    if (rec == NULL) {
        return -1;
    }

    // initialize the record entry fields
    rec->entry.keyfull_offset = sizeof(record_t);
    rec->entry.value_offset = sizeof(record_t) + keyfull_len;   // xxx align to 8 byte, and add exte len
    rec->entry.value_len = val_len;
    rec->entry.value_alloc_len = val_len;

    // copy the key and value to after the record_t
    char *keyfull = (char*)rec + rec->entry.keyfull_offset;
    keyfull[0] = keyid;
    strcpy(keyfull+1, keystr);

    void *value = (char*)rec + rec->entry.value_offset;
    memcpy(value, val, val_len);

    add_to_list_tail(&hash_tbl[hash(keyid,keystr)], &rec->entry.node_keyfull);
    add_to_list_tail(&keyid_head[(int)keyid], &rec->entry.node_keyid);

    return 0;
}

// -----------------  UTILS  --------------------------------------------------------

static record_t *find(char keyid, char *keystr)
{
    int idx;
    uint64_t off;
    node_t *head;
    record_t *rec;
    char *keyfull;

    idx = 123;
    head = &hash_tbl[idx];

    for (off = head->next; off != NODE_OFFSET(head); off = NODE(off)->next) {
        rec = CONTAINER(NODE(off), record_t, entry.node_keyfull);
        keyfull = (void*)rec + rec->entry.keyfull_offset;

        if (keyfull[0] == keyid && strcmp(keystr, &keyfull[1]) == 0) {
            return rec;
        }
    }

    return NULL;
}

// xxx
#define MIN 128
static record_t *alloc(int len)
{
    int len_required;
    uint64_t off;
    record_t *rec;
    bool found;

    len_required = ROUND_UP(len,64); //xxx or use 16

    // loop over free list until a record is found with adequate len
    found = false;
    for (off = free_head->next; off != NODE_OFFSET(free_head); off = NODE(off)->next) {
        rec = CONTAINER(NODE(off), record_t, entry.node_keyfull);
        //xxx assert magic
        //xxx assert len multiple of 64

        if (rec->len >= len_required) {
            found = true;
            break;
        }
    }

    // xxx
    if (!found) {
        return NULL;
    }

    // if the record found has enough length to be divided then do so
    if (rec->len - len_required >= MIN) {
        rec->magic = MAGIC_RECORD_ENTRY;
        rec->len = len_required;

        record_t *new_free_rec = (void*)rec + len_required;
        add_to_free_list(new_free_rec, rec->len - len_required); // xxx
    } else {
        rec->magic = MAGIC_RECORD_ENTRY;
    }

    // return the allocated record, with it's magic and len fields set
    return rec;
}

static void add_to_free_list(record_t *rec, int len)
{
// xxx should this zero the rec struct first

    // init the record
    rec->magic = MAGIC_RECORD_FREE;
    rec->len = len;

    // add it to the head of the free list
    add_to_list_head(free_head, &rec->free.node);
}

static int hash(char keyid, char *keystr)
{
    return 7; // xxx
}

// xxx make these inline funcs,  don't reeval

// xxx asserts in these
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

#if 0
// -----------------  DB TOOL  ------------------------------------------------------

#ifdef DB_TOOL
int main(int argc, char **argv)
{
    logging_init(NULL, false);

#if 0
    db_create("dbxxx.dat", 1024*MB);
#else
    db_init("dbxxx.dat");
#endif
    return 0;
}
#endif

#endif

// xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
