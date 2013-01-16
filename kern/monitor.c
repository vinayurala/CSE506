// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <inc/disk_crypt.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "backtrace", "Display Stack backtrace", mon_backtrace}, 
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	// { "encrypt", "Encrypt disk", mon_encrypt_disk}, 
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Backtrace code.
	uint64_t rbp_value;
	uint64_t *rbp_pointer;
	int res, i;
	long int rip_value;
	rbp_pointer = (uint64_t *)read_rbp();
       	struct Ripdebuginfo debuginfo_instance;
	while(rbp_pointer)
	{
		rip_value = *((int*)rbp_pointer + 2);
		rip_value &= 0x00000000FFFFFFFF;
		//rip_value = rip_value & 0x00000000FFFFFFFF;
      		res = debuginfo_rip((uintptr_t)rip_value, &debuginfo_instance);
		if(res == -1)
		{
			cprintf("\nCannot find bounds of current function\n");
			return -1;
	       	}
		cprintf("rbp %016x rip %016x args %016x %016x %016x %016x\n", rbp_pointer, (int)rip_value, *((int *)rbp_pointer - 1), *((int *)rbp_pointer - 2), *((int *)rbp_pointer - 3), *((int *)rbp_pointer - 4));
		cprintf("\t%s:%d: ", debuginfo_instance.rip_file, debuginfo_instance.rip_line);
       		cprintf("%.*s+%016x\n", debuginfo_instance.rip_fn_namelen, debuginfo_instance.rip_fn_name,((int)rip_value - debuginfo_instance.rip_fn_addr));
		rbp_pointer = (uint64_t *)*rbp_pointer;
      	}
	return 0;
}

/* int */
/* mon_encrypt_disk() */
/* { */
/* 	disk_encrypt(); */
/* 	return 0; */
/* } */

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
