#define SYS_exit    1
#define SYS_read    3
#define SYS_write   4
#define SYS_open    5
#define SYS_close   6
#define SYS_lseek   19
#define SYS_ioctl   54

#define O_RDONLY    0
#define O_WRONLY    1
#define O_CREAT     64
#define O_TRUNC     512
#define SEEK_SET    0

#define MEMGETINFO  0x80204d01
#define MEMREADOOB  0xc00c4d04

typedef unsigned int  uint32_t;
typedef unsigned char uint8_t;

struct mtd_info_user {
    uint8_t  type;
    uint32_t flags;
    uint32_t size;
    uint32_t erasesize;
    uint32_t oobblock;
    uint32_t oobsize;
    uint32_t ecctype;
    uint32_t eccsize;
};

struct mtd_oob_buf {
    uint32_t start;
    uint32_t length;
    unsigned char *ptr;
};

static uint32_t my_div(uint32_t a, uint32_t b) {
    uint32_t result = 0;
    while (a >= b) { a -= b; result++; }
    return result;
}

static uint32_t my_mod(uint32_t a, uint32_t b) {
    while (a >= b) a -= b;
    return a;
}

static inline int sys_write(int fd, const void *buf, int len) {
    register int r0 asm("r0") = fd;
    register const void *r1 asm("r1") = buf;
    register int r2 asm("r2") = len;
    asm volatile("swi #0x900004" : "+r"(r0) : "r"(r1),"r"(r2) : "memory");
    return r0;
}

static inline int sys_read(int fd, void *buf, int len) {
    register int r0 asm("r0") = fd;
    register void *r1 asm("r1") = buf;
    register int r2 asm("r2") = len;
    asm volatile("swi #0x900003" : "+r"(r0) : "r"(r1),"r"(r2) : "memory");
    return r0;
}

static inline int sys_open(const char *path, int flags, int mode) {
    register const char *r0 asm("r0") = path;
    register int r1 asm("r1") = flags;
    register int r2 asm("r2") = mode;
    register int ret asm("r0");
    asm volatile("swi #0x900005" : "=r"(ret) : "r"(r0),"r"(r1),"r"(r2) : "memory");
    return ret;
}

static inline int sys_close(int fd) {
    register int r0 asm("r0") = fd;
    asm volatile("swi #0x900006" : "+r"(r0) : : "memory");
    return r0;
}

static inline int sys_lseek(int fd, uint32_t offset, int whence) {
    register int r0 asm("r0") = fd;
    register uint32_t r1 asm("r1") = offset;
    register int r2 asm("r2") = whence;
    asm volatile("swi #0x900013" : "+r"(r0) : "r"(r1),"r"(r2) : "memory");
    return r0;
}

static inline int sys_ioctl(int fd, uint32_t req, void *arg) {
    register int r0 asm("r0") = fd;
    register uint32_t r1 asm("r1") = req;
    register void *r2 asm("r2") = arg;
    asm volatile("swi #0x900036" : "+r"(r0) : "r"(r1),"r"(r2) : "memory");
    return r0;
}

static inline void sys_exit(int code) {
    register int r0 asm("r0") = code;
    asm volatile("swi #0x900001" : : "r"(r0) : "memory");
}

static int my_strlen(const char *s) {
    int i = 0;
    while (s[i]) i++;
    return i;
}

static void print(const char *s) {
    sys_write(1, s, my_strlen(s));
}

static void print_num(uint32_t n) {
    char buf[12];
    int i = 11;
    buf[i] = 0;
    if (n == 0) { print("0"); return; }
    while (n && i > 0) {
        buf[--i] = '0' + my_mod(n, 10);
        n = my_div(n, 10);
    }
    print(buf + i);
}

static void my_memset(void *p, int v, int n) {
    unsigned char *b = p;
    while (n--) *b++ = v;
}

static unsigned char page_buf[2048];
static unsigned char oob_buf[64];

void dump_mtd(int num) {
    char mtd_dev[]  = "/dev/mtd/mtd0";
    char out_file[] = "/sdcard/mtd0.img";
    mtd_dev[12]  = '0' + num;
    out_file[11] = '0' + num;

    /* діагностика */
    int test_fd = sys_open("/dev/null", O_RDONLY, 0);
    if (test_fd < 0) {
        print("sys_open broken\n");
        return;
    }
    print("sys_open OK\n");
    sys_close(test_fd);

    print("Opening ");
    print(mtd_dev);
    print("...\n");

    int fd = sys_open(mtd_dev, O_RDONLY, 0);
    if (fd < 0) {
        print("open failed\n");
        return;
    }

    struct mtd_info_user mtd;
    my_memset(&mtd, 0, sizeof(mtd));
    if (sys_ioctl(fd, MEMGETINFO, &mtd) < 0) {
        print("MEMGETINFO failed\n");
        sys_close(fd);
        return;
    }

    print("MTD size:  "); print_num(mtd.size);    print("\n");
    print("Page size: "); print_num(mtd.oobblock); print("\n");
    print("OOB size:  "); print_num(mtd.oobsize);  print("\n");

    int out = sys_open(out_file, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (out < 0) { print("open output failed\n"); sys_close(fd); return; }

    uint32_t page_size = mtd.oobblock;
    uint32_t oob_size  = mtd.oobsize;
    uint32_t num_pages = my_div(mtd.size, page_size);

    print("Dumping "); print_num(num_pages); print(" pages...\n");

    uint32_t i;
    for (i = 0; i < num_pages; i++) {
        uint32_t offset = i * page_size;

        sys_lseek(fd, offset, SEEK_SET);
        sys_read(fd, page_buf, page_size);

        struct mtd_oob_buf oob;
        oob.start  = offset;
        oob.length = oob_size;
        oob.ptr    = oob_buf;
        my_memset(oob_buf, 0xff, oob_size);
        sys_ioctl(fd, MEMREADOOB, &oob);

        sys_write(out, page_buf, page_size);
        sys_write(out, oob_buf,  oob_size);

        if (my_mod(i, 100) == 0) {
            print("Page "); print_num(i);
            print(" / ");   print_num(num_pages);
            print("\r");
        }
    }

    print("\nDone! Saved to ");
    print(out_file);
    print("\n");

    sys_close(fd);
    sys_close(out);
}

void dump_mem_region(int fd, uint32_t start, uint32_t size, const char *out_file) {
    print("Dumping ");
    print(out_file);
    print("...\n");

    int out = sys_open(out_file, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (out < 0) { print("open output failed\n"); return; }

    uint32_t offset;
    for (offset = 0; offset < size; offset += 4) {
        sys_lseek(fd, start + offset, SEEK_SET);
        uint32_t val = 0xDEADBEEF;
        sys_read(fd, &val, 4);
        sys_write(out, &val, 4);
    }

    print("Done -> ");
    print(out_file);
    print("\n");
    sys_close(out);
}

void dump_mem() {
    int fd = sys_open("/dev/mem", O_RDONLY, 0);
    if (fd < 0) { print("open /dev/mem failed\n"); return; }

    dump_mem_region(fd, 0xFFFE1800, 0x00000800, "/sdcard/chipid.img");

    sys_close(fd);
    print("All done!\n");
}

void _start() {
    print("Select dump:\n");
    print("  0 - mtd0 (00000000)\n");
    print("  1 - mtd1 (bootloader)\n");
    print("  2 - mtd2 (mfg_and_gsm)\n");
    print("  3 - mtd3 (recovery)\n");
    print("  4 - mtd4 (boot)\n");
    print("  5 - mtd5 (system)\n");
    print("  6 - mtd6 (userdata)\n");
    print("  7 - MEM  (chipid)\n");
    print("Enter choice: ");

    char choice;
    sys_read(0, &choice, 1);

    if (choice >= '0' && choice <= '6') {
        dump_mtd(choice - '0');
    } else if (choice == '7') {
        dump_mem();
    } else {
        print("Invalid choice\n");
    }

    sys_exit(0);
}
