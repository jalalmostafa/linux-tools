#define _GNU_SOURCE
#include <getopt.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <linux/limits.h>

#define PAGE_FLAGS_PATH "/proc/kpageflags"

unsigned long long page2pfn(pid_t pid, void* addr)
{
    char path[PATH_MAX];
    unsigned long long pfn = 0;

    snprintf(path, PATH_MAX, "/proc/%d/pagemap", pid);

    int mapfd = open(path, O_RDONLY);
    if (mapfd < 0) {
        fprintf(stderr, "Could not open %s: %s\n", path, strerror(errno));
        return -1;
    }

    unsigned long long target = ((unsigned long long)addr) / sysconf(_SC_PAGESIZE);
    off_t err = lseek(mapfd, target * 8, SEEK_SET);
    if (err != target * 8) {
        fprintf(stderr, "Error lseek pagemap: %s\n", strerror(errno));
        goto exit;
    }

    err = read(mapfd, &pfn, 8);
    if (err < 0) {
        fprintf(stderr, "Error reading pagemap: %s\n", strerror(errno));
        goto exit;
    }

    if (err != 8) {
        fprintf(stderr, "Could only read %ld bytes: %s\n", err, strerror(errno));
        goto exit;
    }

    if (pfn == 0) {
        printf("error: %s\n", strerror(errno));
    }

exit:
    close(mapfd);
    return pfn;
}

long long get_page_flags(unsigned long long physaddr)
{

    long long pf = -1;
    int pageflags = open(PAGE_FLAGS_PATH, O_RDONLY);
    if (pageflags < 0) {
        fprintf(stderr, "Could not open %s: %s\n", PAGE_FLAGS_PATH, strerror(errno));
        return -1;
    }

    // Is the virtual page present ?
    if (physaddr & 0x8000000000000000LL) {
        unsigned long long pfn = physaddr & 0x3fffffffffffffLL;
        off_t err = lseek(pageflags, pfn * 8, SEEK_SET);
        if (err != pfn * 8) {
            fprintf(stderr, "Error lseek pagemap: %s\n", strerror(errno));
            goto exit;
        }

        err = read(pageflags, &pf, 8);
        if (err < 0) {
            fprintf(stderr, "Error reading pagemap: %s\n", strerror(errno));
            goto exit;
        }

        if (err != 8) {
            fprintf(stderr, "Could only read %ld bytes: %s\n", err, strerror(errno));
            goto exit;
        }
    }

exit:
    close(pageflags);
    return pf;
}

int run_cow_example()
{
    // allocate a shared data structure
    char* buf = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    strcpy(buf, "hello");

    pid_t pid = fork();
    unsigned long long v2a;
    if (pid == 0) {
        v2a = page2pfn(getpid(), buf);
        printf("Child: Before madvise %p(%llx)=%s, flags=0x%llx\n", buf, v2a, buf, get_page_flags(v2a));
        // child: force unshare for just this buffer
        madvise(buf, 4096, MADV_POPULATE_WRITE);
        v2a = page2pfn(getpid(), buf);
        printf("Child: After madvise %p(0x%llx)=%s, flags=0x%llx\n", buf, v2a, buf, get_page_flags(v2a));
        strcpy(buf, "world");
        v2a = page2pfn(getpid(), buf);
        printf("Child: After write %p(0x%llx)=%s, flags=0x%llx\n", buf, v2a, buf, get_page_flags(v2a));
        exit(0);
    } else {
        v2a = page2pfn(getpid(), buf);
        printf("Parent: Waiting for child to finish %p(0x%llx)=%s, flags=0x%llx\n", buf, v2a, buf, get_page_flags(v2a));
        wait(NULL);
        v2a = page2pfn(getpid(), buf);
        printf("Parent: Finished %p(0x%llx)=%s, flags=0x%llx\n", buf, v2a, buf, get_page_flags(v2a));
    }

    munmap(buf, 4096);
    return 0;
}

void usage(const char* prog)
{
    fprintf(stdout, "%s -p <pid> -a <virtual-address>\n", prog);
    fprintf(stdout, "%s -e\n", prog);
    fprintf(stdout, "Arguments:\n");
    fprintf(stdout, "  -h\t\tShow this help text.\n");
    fprintf(stdout, "  -e\t\tRun a Copy-on-Write example.\n");
    fprintf(stdout, "  -p\t\tProcess ID.\n");
    fprintf(stdout, "  -a\t\tVirtual Address as hexadecimal.\n");
    exit(0);
}

int main(int argc, char* argv[])
{
    int ret = 0;
    int example = 0;
    char arg;
    pid_t pid = 0;
    unsigned long long vaddr = 0;

    while ((arg = getopt(argc, argv, "hp:a:e")) != -1) {
        switch (arg) {
        case 'h':
            usage(argv[0]);
            break;

        case 'p':
            pid = atoi(optarg);
            if (pid <= 0) {
                fprintf(stderr, "Invalid PID: %s\n", optarg);
                return -1;
            }
            break;
        case 'a':
            ret = sscanf(optarg, "%llx", &vaddr);
            if (!ret) {
                fprintf(stderr, "Invalid virtual address: %llX\n", vaddr);
                return -1;
            }
            break;
        case 'e':
            example = 1;
            break;
        default:
            usage(argv[0]);
            break;
        }
    }

    if (getuid() != 0) {
        fprintf(stdout, "Root user is required to read page flags\n");
        return -1;
    }

    if (example && !pid && !vaddr) {
        run_cow_example();
        return 0;
    }

    if (!pid || !vaddr) {
        fprintf(stdout, "PID and virtual address are required arguments\n");
        return -1;
    }

    unsigned long long virt2phy = page2pfn(pid, (void*)vaddr);
    if (!virt2phy) {
        fprintf(stderr, "Failed to retrieve page frame number\n");
        return -1;
    }

    long long flags = get_page_flags(virt2phy);
    if (flags < 0) {
        fprintf(stderr, "Failed to retrieve page flags\n");
        return -1;
    }

    fprintf(stdout, "Page Frame Number of 0x%llX: 0x%llX with Flags 0x%llX\n", vaddr, virt2phy, flags);

    return 0;
}
