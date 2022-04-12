# fleshutils

Install
-------

Run `python setup.py install` as root.


Use
---

- Install utilities or add `fleshutils/bin` to `PATH`.
- Run utilities with `--help` for documentation.

Examples
--------

1. Launch gzip in background
   ```
   # gzip < /dev/zero > /dev/null &
   [1] 3182
   ```

2. See gzip I/O status in `/proc/PID/io`
   ```
   # cat /proc/$(pidof gzip)/io
   rchar: 496142460
   wchar: 475136
   syscr: 15147
   syscw: 29
   read_bytes: 0
   write_bytes: 0
   cancelled_write_bytes: 0
   ```

3. `watch` gzip progress and speed with `numdelta` and `numhr`
   ```
   # watch "numdelta -t < /proc/$(pidof gzip)/io | numhr"
   Every 2.0s: numdelta -t < /proc/3182/io | numhr
   rchar: 12G (+248M/s)
   wchar: 12M (+242k/s)
   syscr: 374k (+8k/s)
   syscw: 725 (+15/s)
   read_bytes: 0 (+0/s)
   write_bytes: 0 (+0/s)
   cancelled_write_bytes: 0 (+0/s)
   ```
