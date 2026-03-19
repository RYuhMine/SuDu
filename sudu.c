#define SYS_exit      1
#define SYS_read      3
#define SYS_write     4
#define SYS_open      5
#define SYS_close     6
#define SYS_lseek     19
#define SYS_ioctl     54
#define SYS_statfs    99
#define SYS_unlink    10

#define O_RDWR      2
#define O_RDONLY    0
#define O_WRONLY    1
#define O_CREAT     64
#define O_TRUNC     512
#define SEEK_SET    0

#define MEMGETINFO  0x80204d01
#define MEMREADOOB  0xc00c4d04

typedef unsigned int   uint32_t;
typedef unsigned char  uint8_t;
typedef unsigned long  uint64_t;

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

struct statfs_t {
    uint32_t f_type;
    uint32_t f_bsize;
    uint32_t f_blocks;
    uint32_t f_bfree;
    uint32_t f_bavail;
    uint32_t f_files;
    uint32_t f_ffree;
    uint32_t f_fsid[2];
    uint32_t f_namelen;
    uint32_t f_frsize;
    uint32_t f_spare[5];
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

static inline int sys_statfs(const char *path, struct statfs_t *buf) {
    register const char *r0 asm("r0") = path;
    register struct statfs_t *r1 asm("r1") = buf;
    register int ret asm("r0");
    asm volatile("swi #0x900063" : "=r"(ret) : "r"(r0),"r"(r1) : "memory");
    return ret;
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

static uint32_t read_cp15(int op1, int crn, int crm, int op2) {
    uint32_t val = 0;
    if (op1==0 && crn==0 && crm==0 && op2==0)
        asm volatile("mrc p15, 0, %0, c0, c0, 0" : "=r"(val));
    else if (op1==0 && crn==0 && crm==0 && op2==1)
        asm volatile("mrc p15, 0, %0, c0, c0, 1" : "=r"(val));
    else if (op1==0 && crn==0 && crm==0 && op2==2)
        asm volatile("mrc p15, 0, %0, c0, c0, 2" : "=r"(val));
    else if (op1==0 && crn==0 && crm==0 && op2==3)
        asm volatile("mrc p15, 0, %0, c0, c0, 3" : "=r"(val));
    else if (op1==0 && crn==0 && crm==1 && op2==0)
        asm volatile("mrc p15, 0, %0, c0, c1, 0" : "=r"(val));
    else if (op1==0 && crn==0 && crm==1 && op2==1)
        asm volatile("mrc p15, 0, %0, c0, c1, 1" : "=r"(val));
    return val;
}

void show_sdcard_space() {
    struct statfs_t st;
    my_memset(&st, 0, sizeof(st));
    int r = sys_statfs("/sdcard", &st);
    if (r < 0) {
        print("Cannot get SD card info\n");
        return;
    }
    uint32_t free_mb  = my_div(st.f_bavail * my_div(st.f_bsize, 1024), 1024);
    uint32_t total_mb = my_div(st.f_blocks * my_div(st.f_bsize, 1024), 1024);
    print("SD card: ");
    print_num(free_mb);
    print("MB free / ");
    print_num(total_mb);
    print("MB total\n");
}

void dump_cpu_regs() {
    print("Dumping CPU/SoC registers...\n");

    int out = sys_open("/sdcard/cpu-regs.img", O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (out < 0) { print("open output failed\n"); return; }

    uint32_t val;

    print("Reading CP15 registers...\n");

    val = read_cp15(0,0,0,0);
    sys_write(out, &val, 4);
    print("MIDR: "); print_num(val); print("\n");

    val = read_cp15(0,0,0,1);
    sys_write(out, &val, 4);
    print("Cache Type: "); print_num(val); print("\n");

    val = read_cp15(0,0,0,2);
    sys_write(out, &val, 4);
    print("TCM Type: "); print_num(val); print("\n");

    val = read_cp15(0,0,0,3);
    sys_write(out, &val, 4);
    print("TLB Type: "); print_num(val); print("\n");

    val = read_cp15(0,0,1,0);
    sys_write(out, &val, 4);
    print("PFR0: "); print_num(val); print("\n");

    val = read_cp15(0,0,1,1);
    sys_write(out, &val, 4);
    print("DFR0: "); print_num(val); print("\n");

    print("Reading OMAP850 Die ID...\n");
    int mem = sys_open("/dev/mem", O_RDONLY, 0);
    if (mem >= 0) {
        sys_lseek(mem, 0xFFFE1800, SEEK_SET);
        val = 0xDEADBEEF;
        sys_read(mem, &val, 4);
        sys_write(out, &val, 4);
        print("Die ID 0: "); print_num(val); print("\n");

        sys_lseek(mem, 0xFFFE1804, SEEK_SET);
        val = 0xDEADBEEF;
        sys_read(mem, &val, 4);
        sys_write(out, &val, 4);
        print("Die ID 1: "); print_num(val); print("\n");

        sys_close(mem);
    } else {
        print("Cannot open /dev/mem\n");
        val = 0xDEADBEEF;
        sys_write(out, &val, 4);
        sys_write(out, &val, 4);
    }

    sys_close(out);
    print("Done! Saved to /sdcard/cpu-regs.img\n");
}

void dump_mtd(int num) {
    char mtd_dev[]  = "/dev/mtd/mtd0";
    char out_file[] = "/sdcard/mtd0.img";
    mtd_dev[12]  = '0' + num;
    out_file[11] = '0' + num;

    print("Opening ");
    print(mtd_dev);
    print("...\n");

    int fd = sys_open(mtd_dev, O_RDONLY, 0);
    if (fd < 0) { print("open failed\n"); return; }

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

void dump_all_mtd() {
    int i;
    print("Dumping all MTD partitions (0-7)...\n");
    show_sdcard_space();
    for (i = 0; i <= 7; i++) {
        print("\n--- MTD"); print_num(i); print(" ---\n");
        dump_mtd(i);
        show_sdcard_space();
    }
    print("\nAll MTD partitions dumped!\n");
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

        if (my_mod(offset, 1024*64) == 0) {
            print_num(my_div(offset, 1024));
            print("KB\r");
        }
    }

    print("\nDone -> ");
    print(out_file);
    print("\n");
    sys_close(out);
}

void dump_ram() {
    print("Opening /dev/mem for RAM dump...\n");
    int fd = sys_open("/dev/mem", O_RDONLY, 0);
    if (fd < 0) { print("open /dev/mem failed\n"); return; }
    dump_mem_region(fd, 0x10000000, 60*1024*1024, "/sdcard/ram.img");
    sys_close(fd);
    print("RAM dump done!\n");
}

void dump_sram() {
    print("Opening /dev/mem for SRAM dump...\n");
    int fd = sys_open("/dev/mem", O_RDONLY, 0);
    if (fd < 0) { print("open /dev/mem failed\n"); return; }
    dump_mem_region(fd, 0x20000000, 0x00100000, "/sdcard/sram.img");
    sys_close(fd);
    print("SRAM dump done!\n");
}

void dump_gsm_ffs() {
    print("Dumping GSM FFS...\n");

    int fd = sys_open("/dev/omap_csmi_ffs", O_RDONLY, 0);
    if (fd < 0) { print("open failed\n"); return; }

    int out = sys_open("/sdcard/gsm-ffs.img", O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (out < 0) { print("open output failed\n"); sys_close(fd); return; }

    unsigned char buf[512];
    int r;
    uint32_t total = 0;
    while (1) {
        r = sys_read(fd, buf, 512);
        if (r <= 0) break;
        sys_write(out, buf, r);
        total += r;
        print_num(total);
        print(" bytes\r");
    }

    print("\nDone! Saved to /sdcard/gsm-ffs.img\n");
    sys_close(fd);
    sys_close(out);
}

void at_interactive() {
    print("GSM AT Interactive Mode\n");
    print("Type AT commands, empty line to exit.\n");
    print("Results saved to /sdcard/GSM-AT.txt\n\n");

    /* зупиняємо ril-daemon */
    print("Note: stop ril-daemon before using this!\n\n");

    int tty = sys_open("/dev/omap_csmi_tty1", O_RDWR, 0);
    if (tty < 0) {
        print("Cannot open /dev/omap_csmi_tty1\n");
        print("Try: stop ril-daemon\n");
        return;
    }

    int log = sys_open("/sdcard/GSM-AT.txt", O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (log < 0) {
        print("Cannot open log file\n");
        sys_close(tty);
        return;
    }

    /* заголовок у файл */
    sys_write(log, "=== SuDu GSM AT Log ===\n", 23);

    unsigned char cmd[128];
    unsigned char resp[512];
    int cmd_len;
    int r;

    while (1) {
        /* читаємо команду від користувача */
        print("> ");
        cmd_len = 0;
        my_memset(cmd, 0, sizeof(cmd));

        while (cmd_len < 126) {
            unsigned char c;
            r = sys_read(0, &c, 1);
            if (r <= 0) break;
            if (c == '\r' || c == '\n') break;
            cmd[cmd_len++] = c;
        }
        cmd[cmd_len] = 0;

        /* порожній рядок — вихід */
        if (cmd_len == 0) {
            print("Exiting AT mode\n");
            break;
        }

        /* записуємо команду у файл */
        sys_write(log, "> ", 2);
        sys_write(log, cmd, cmd_len);
        sys_write(log, "\n", 1);

        /* надсилаємо команду на GSM чіп */
        sys_write(tty, cmd, cmd_len);
        sys_write(tty, "\r", 1);

        /* чекаємо відповідь — читаємо поки є дані */
        my_memset(resp, 0, sizeof(resp));
        int total = 0;
        int attempts = 0;

        while (attempts < 100 && total < 511) {
            r = sys_read(tty, resp + total, 512 - total);
            if (r > 0) {
                total += r;
                attempts = 0;
            } else {
                attempts++;
            }
        }

        if (total > 0) {
            /* записуємо у файл */
            sys_write(log, resp, total);
            sys_write(log, "\n", 1);
            /* виводимо відповідь на екран */
            sys_write(1, resp, total);
            print("\n");
        } else {
            print("(no response)\n");
            sys_write(log, "(no response)\n", 14);
        }
    }

    sys_close(tty);
    sys_close(log);
    print("Saved to /sdcard/GSM-AT.txt\n");
}

static inline int sys_unlink(const char *path) {
    register const char *r0 asm("r0") = path;
    register int ret asm("r0");
    asm volatile("swi #0x90000a" : "=r"(ret) : "r"(r0) : "memory");
    return ret;
}

void delete_dumps() {
    print("WARNING: This will delete ALL dump files from /sdcard/!\n");
    print("Are you sure? (y/n): ");

    char c;
    sys_read(0, &c, 1);
    print("\n");

    if (c != 'y' && c != 'Y') {
        print("Cancelled.\n");
        return;
    }

    print("Deleting dump files...\n");

    const char *files[] = {
        "/sdcard/mtd0.img",
        "/sdcard/mtd1.img",
        "/sdcard/mtd2.img",
        "/sdcard/mtd3.img",
        "/sdcard/mtd4.img",
        "/sdcard/mtd5.img",
        "/sdcard/mtd6.img",
        "/sdcard/mtd7.img",
        "/sdcard/ram.img",
        "/sdcard/sram.img",
        "/sdcard/cpu-regs.img",
        "/sdcard/gsm-ffs.img",
        "/sdcard/GSM-AT.txt",
        0
    };

    int i = 0;
    int deleted = 0;
    while (files[i] != 0) {
        int r = sys_unlink(files[i]);
        if (r == 0) {
            print("Deleted: ");
            print(files[i]);
            print("\n");
            deleted++;
        }
        i++;
    }

    print("Done! Deleted ");
    print_num(deleted);
    print(" files.\n");
}

void show_about() {
    print("\n");
    print("===============================================================\n");
    print("  SuDu - Hardware Dump Utility\n");
    print("  Version: v1.4\n");
    print("  Target: Google Sooner (HTC EXCA300)\n");
    print("  CPU: ARM926EJ-S / OMAP850\n");
    print("  Built with: arm-linux-gnueabi-gcc\n");
    print("  Author: RYuh (aka RYuhMine, OstupBurtik or Hakurei Reimu)\n");
    print("  GitHub: https://github.com/RYuhMine/SuDu\n");
    print("  Licensed under GNU GPL v2.\n");
    print("===============================================================\n");
    print("\n");
}

void print_menu() {
    print("\nSuDu v1.4 - Google Sooner Dump Utility\n");
    print("============================================\n");
    show_sdcard_space();
    print("--------------------------------------------\n");
    print("Select dump:\n");
    print("      htc-29386.0.9.0.0 | htc-20645.0.8.0.0\n");
    print("  0 - mtd0 (00000000)   | (00000000)\n");
    print("  1 - mtd1 (bootloader) | (bootloader)\n");
    print("  2 - mtd2 (mfg_and_gsm)| (mfg_and_gsm)\n");
    print("  3 - mtd3 (recovery)   | (kernel)\n");
    print("  4 - mtd4 (boot)       | (2ndstage)\n");
    print("  5 - mtd5 (system)     | (ramdisk)\n");
    print("  6 - mtd6 (userdata)   | (system)\n");
    print("  7 - mtd7  --------    | (userdata)\n");
    print("  8 - CPU/SoC registers dump\n");
    print("  9 - RAM dump (60MB)\n");
    print("  a - SRAM dump (1MB)\n");
    print("  b - GSM FFS metadata dump\n");
    print("  p - GSM AT interactive mode\n");
    print("  d - Dump all MTD partitions (0-7)\n");
    print("  r - Delete all dump files\n");
    print("  i - About\n");
    print("  q - Exit\n");
    print("--------------------------------------------\n");
    print("Enter choice: ");
}

void _start() {
    char choice;

    while (1) {
        print_menu();
        sys_read(0, &choice, 1);
        print("\n");

        if (choice >= '0' && choice <= '7') {
            dump_mtd(choice - '0');
        } else if (choice == '8') {
            dump_cpu_regs();
        } else if (choice == '9') {
            dump_ram();
        } else if (choice == 'a') {
            dump_sram();
        } else if (choice == 'b') {
            dump_gsm_ffs();
        } else if (choice == 'p') {
            at_interactive();
        } else if (choice == 'd') {
            dump_all_mtd();
        } else if (choice == 'r') {
            delete_dumps();
        } else if (choice == 'i') {
            show_about();
        } else if (choice == 'q') {
            print("Thank you for using SuDu!\n");
            sys_exit(0);
        } else if (choice == '\r' || choice == '\n') {
            /* ігноруємо enter */
        } else {
            print("Invalid choice: '");
            sys_write(1, &choice, 1);
            print("'\n");
        }
    }
}
