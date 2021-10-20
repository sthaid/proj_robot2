// xxx tbd later
// - msync
//
// xxx tbd maybe later
// - make list utils inline funcs

#include <utils.h>

//
// defines
//

#define MB        0x100000
#define GB        (1024 * MB)
#define PAGE_SIZE 4096

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

static_assert(sizeof(hdr_t) == PAGE_SIZE, "");
static_assert(sizeof(record_t) == 64, "");

// -----------------  DB INT AND CREATE   -------------------------------------------

void db_init(char *file_name, bool create, uint64_t file_len)
{
    int fd;
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
    // https://web.mit.edu/freebsd/head/sys/libkern/crc32.c
    static const uint32_t crc32_tab[] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
        0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
        0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
        0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
        0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
        0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
        0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
        0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
        0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
        0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
        0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
        0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
        0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
        0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
        0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
        0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
        0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
        0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
        0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
        0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
        0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
        0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d };
    /*
     * A function that calculates the CRC-32 based on the table above is
     * given below for documentation purposes. An equivalent implementation
     * of this function that's actually used in the kernel can be found
     * in sys/libkern.h, where it can be inlined.
     *
     *      uint32_t
     *      crc32(const void *buf, size_t size)
     *      {
     *              const uint8_t *p = buf;
     *              uint32_t crc;
     *
     *              crc = ~0U;
     *              while (size--)
     *                      crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
     *              return crc ^ ~0U;
     *      }
     */
    
    #define CALC_CRC(x) \
        do { \
            crc = crc32_tab[(crc ^ (x)) & 0xFF] ^ (crc >> 8); \
        } while (0)

    uint32_t crc = ~0U;
    CALC_CRC((uint8_t)keyid);
    for (uint8_t *p = (uint8_t*)keystr; *p; p++) {
        CALC_CRC(*p);
    }
    crc = crc ^ ~0U;

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
