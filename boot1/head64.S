#include <inc/mmu.h>

# Translation: cr3->pml4->pdpe->pgdir
# We set the PTE_MBZ bit in pgdir to skip the last step	
.set pml4,  0x8000
.set pdpt,  pml4 + 0x1000
.text
.globl _head64
_head64 :
.code32
    call verify_cpu   #check if CPU supports long mode
    movl $CR4_PAE,%eax
    movl %eax,%cr4

# build an early boot pml4 at 0x8000

    #initializing the page tables
    movl $pml4,%edi
    xorl %eax,%eax
    movl $((4096*6)/4),%ecx  # moving these many words to the 6 pages with 4 second level pages + 1 3rd level + 1 4th level pages 
    rep stosl

    # creating a 4G boot page table
    # setting the 4th level page table only the first entry needed (PML4)
    movl $pml4,%eax
    leal pdpt, %ebx
    orl $PTE_P,%ebx
    orl $PTE_W,%ebx
    movl %ebx,(%eax)  


    # setting the 3rd level page table (PDPE)
    # 4 entries (counter in ecx), point to the next four physical pages (pgdirs)
    # pgdirs in 0xa0000--0xd000
    leal pdpt,%edi
    movl $0x4,%ecx  
    addl $0x1000,%ebx
    pushl %ebx
  1:
    movl %ebx,(%edi)
    addl $0x8,%edi
    addl $0x1000,%ebx
    decl %ecx
    jnz 1b

    
    # setting the pgdir so that the LA=PA
    # 3 pages * 512 entries/page = 1536
    movl $1536,%ecx
    # Start at the end and work backwards
    leal (pml4 + 5*0x1000 - 0x8),%edi
    # PTE_P|PTE_W|PTE_MBZ
    movl $0x00000183,%eax
  1:
    decl %ecx
    movl %ecx,%ebx
    # left shift 21 bits = 2 MB
    shl  $21,%ebx
    orl $PTE_P,%ebx
    orl $PTE_W,%ebx
    orl $PTE_MBZ,%ebx
    movl %ebx,(%edi)
    subl $0x8,%edi
    #addl $0x00200000,%eax
    #decl %ecx
    cmp $0x0,%ecx
    jne 1b

    #setting the last 2nd level page table such that LA - KERNBASE = PA
    leal (pml4 + 5*0x1000 + 8*384),%edi  #start with the 384th entry
    movl $128,%ecx  # 512 - 384 = 128
    movl $0x00000183,%eax
 1:
    movl %eax, (%edi)
    addl $0x8, %edi
    addl $0x00200000, %eax
    decl %ecx
    jnz 1b




    # set the cr3 register
    movl $pml4,%eax
    movl %eax, %cr3
    
    # enable the long mode in MSR
    movl $EFER_MSR,%ecx
    rdmsr
    btsl $EFER_LME,%eax
    wrmsr
    
    # enable paging 
    movl %cr0,%eax
    orl $CR0_PE,%eax
    orl $CR0_PG,%eax
    orl $CR0_AM,%eax
    orl $CR0_WP,%eax
    orl $CR0_MP,%eax
    movl %eax,%cr0
    #jump to long mode with CS=0 and
    lgdt gdtdesc_64 
    pushl $0x8
    leal bootmain,%eax
    push %eax
    
    .globl jumpto_longmode
    .type jumpto_longmode,@function
jumpto_longmode:
    lret




verify_cpu:
    pushfl                   # get eflags in eax -- standardard way to check for cpuid
    popl %eax
    movl %eax,%ecx
    xorl $0x200000, %eax
    pushl %eax
    popfl
    pushfl
    popl %eax
    cmpl %eax,%ebx
    jz verify_cpu_no_longmode   # no cpuid -- no long mode

    movl $0x0,%eax              # see if cpuid 1 is implemented
    cpuid
    cmpl $0x1,%eax
    jb verify_cpu_no_longmode    # cpuid 1 is not implemented


    mov $0x80000001, %eax
    cpuid                 
    test $(1 << 29),%edx                 #Test if the LM-bit, is set or not.
    jz verify_cpu_no_longmode

    ret

verify_cpu_no_longmode:
    jmp verify_cpu_no_longmode
    
.data 

    .p2align 2

gdt_64:
    SEG_NULL
    .quad  0x00af9a000000ffff            #64 bit CS
    .quad  0x00cf92000000ffff            #64 bit DS

gdtdesc_64:
    .word 0x17 
    .long gdt_64 
