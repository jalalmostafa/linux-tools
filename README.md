# My Linux Tools
A bunch of my Linux tools for diagnostics and maybe more

## `mem-diag`
A tool to diagnose page allocations and check its flags.
It fetches the Page Frame Number (PFN) of a virtual address in a process and its kernel page flags.
It uses `/proc/[pid]/pagemap` and `/proc/kpageflags` to retrieve the information.

The tool provides an example of the Copy-on-Write mechanism to show how PFN and page flags change in parent and child processes
before and after populating (using `madvise(MADV_POPULATE_WRITE)`) or writing to the page.

```bash
./mem-diag -p <pid> -a <virtual-address>
./mem-diag -e
Arguments:
  -h            Show this help text.
  -e            Run a Copy-on-Write example.
  -p            Process ID.
  -a            Virtual Address as hexadecimal.
```
