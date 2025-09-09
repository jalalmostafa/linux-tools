# linux-tools
A bunch of my Linux tools for diagnostics and maybe more

## `mem-diag`
A tool to diagnose page allocations and check its flags.

```bash
./mem-diag -p <pid> -a <virtual-address>
./mem-diag -e
Arguments:
  -h            Show this help text.
  -e            Run a Copy-on-Write example.
  -p            Process ID.
  -a            Virtual Address as hexadecimal.
```
