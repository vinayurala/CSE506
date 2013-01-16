#include <inc/mmu.h>

# Start the CPU: relocate itself to 0x600 and load the second stage bootloader
# The BIOS loads this code from the first sector of the hard disk into
# memory at physical address 0x7c00 and starts executing in real mode
# with %cs=0 %ip=7c00.
# This code loads the following two sectors from disk and then
# jumps to the second stage bootloader (in boot1).

.set PROT_MODE_CSEG, 0x8         # kernel code segment selector
.set PROT_MODE_DSEG, 0x10        # kernel data segment selector
.set CR0_PE_ON,      0x1         # protected mode enable flag

.globl start
start:
  .code16                     # Assemble for 16-bit mode
  cli                         # Disable interrupts
  cld                         # String operations increment

  #relocate the the MBR from 0x7c00 to 0x0600
relocate_mbr:
  xorw    %ax,%ax
  movw    %ax,%ss    #setup the stack segment
  movw    $0x7c00,%sp  # set the stack pointer to 0x7c00
  movw    %sp,%si    # si is set to 0x7c00
  push    %ax
  pop     %es        # now at 0:0x7c00
  push    %ax 
  pop     %ds
  sti
  cld
  movw    $0x600 , %di
  movw    $0x100, %cx  # copy 256 words = 512 bytes
  repnz  
  movsw
  # test for mbr signature
  movw     $0x7FE, %di
  movw     (%di), %ax
  cmpw     $0xAA55,%ax
  jnz     relocate_mbr    # check for bios signature other go back to relocate_mbr
  
  ljmp    $0x0,$0x628  # jump to reset

  
 
 
read_disk:
  call disk_ready_loop     # wait for disk to be ready
  movb $0x2,%al            # Read 2 sectors
  movw $0x1f2,%dx
  outb %al,%dx
  incw %dx
  movb $0x1,%al            # start reading at offset 1 
  outb %al,%dx
  shrb  $8,%al            
  incw %dx
  outb %al,%dx
  shrb  $8,%al
  incw %dx
  outb %al,%dx
  shrb  $8,%al
  orb   $0xe0,%al        
  incw  %dx
  outb %al,%dx
  movb $0x20,%al           # command to read sectors
  incw  %dx
  outb %al,%dx # command to read sectors
  call disk_ready_loop

  movw $0x1f0,%dx  # port to read from
  movw $0x200,%cx  # read 200 words
  xorw %ax,%ax
  movw %ax,%es
  movl $0x1000,%edi  # es:edi = 0:0x1000 .. copy to this memory location
  rep  insw
  ljmp $0x0,$0x1000  # jump to the beginning of the second stage that got loaded just now

disk_ready_loop:
  movw    $0x1f7, %dx
  movb    $0xc0, %bl
  inb     %dx,%al
  andb    %bl,%al
  cmpb     $0x40,%al
  jne     disk_ready_loop
  ret

displayError:    #just to test the 2nd stage is getting copied
  movw    (%esp),%si
loopmsg:
  subw    $0x7c00, %si
  addw    $0x600, %si
  lodsb
  cmp    $0x35,%al
  jz    hang
  push   %si
  movw   $0x7,%bx
  movb   $0x0e, %ah
  int    $0x10
  pop    %si
  subw   $0x600, %si
  addw   $0x7c00, %si
  jmp    loopmsg

displaySuccess:    #just to test the 2nd stage is getting copies
  movw   2(%esp),%si
loop:
  addw    $0x600, %si
  subw    $0x7c00,%si
  lodsb
  cmp    $0x35,%al
  jz     success
  push   %si
  movw   $0x7,%bx
  movb   $0x0e, %ah
  int    $0x10
  pop    %si
  subw   $0x600, %si
  addw   $0x7c00, %si
  jmp    loop
success:
  ret


hang:
    jmp hang

.data

sectorReadError:
 .string "Can't read the sector"
diskresetsuccess:
 .string "Disk reset success5"
sectorReadSuccess:
 .string "Sector read success5"
