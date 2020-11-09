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

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

// LAB 1: add your command to here...
static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
        { "show", "Show a dope neofetch pretty-print", mon_show }
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
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

int mon_show(int argc, char **argv, struct Trapframe *tf) {
    cprintf("\x1b[?25l\x1b[?7l\x1b[0m\x1b[36m\x1b[1m                   -`\n                  .o+`\n                 `ooo/\n                `+oooo:\n               `+oooooo:\n               -+oooooo+:\n             `/:-:++oooo+:\n            `/++++/+++++++:\n           `/++++++++++++++:\n          `/+++o\x1b[0m\x1b[36m\x1b[1moooooooo\x1b[0m\x1b[36m\x1b[1moooo/`\n\x1b[0m\x1b[36m\x1b[1m         \x1b[0m\x1b[36m\x1b[1m./\x1b[0m\x1b[36m\x1b[1mooosssso++osssssso\x1b[0m\x1b[36m\x1b[1m+`\n\x1b[0m\x1b[36m\x1b[1m        .oossssso-````/ossssss+`\n       -osssssso.      :ssssssso.\n      :osssssss/        osssso+++.\n     /ossssssss/        +ssssooo/-\n   `/ossssso+/:-        -:/+osssso+-\n  `+sso+:-`                 `.-/+oso:\n `++:.                           `-/+/\n .`                                 `/\x1b[0m\n\x1b[19A\x1b[9999999D\x1b[41C\x1b[0m\x1b[1m\x1b[36m\x1b[1maaron\x1b[0m@\x1b[36m\x1b[1maaron\x1b[0m \n\x1b[41C\x1b[0m-----------\x1b[0m \n\x1b[41C\x1b[0m\x1b[36m\x1b[1mOS\x1b[0m\x1b[0m:\x1b[0m Arch Linux\x1b[0m \n\x1b[41C\x1b[0m\x1b[36m\x1b[1mHost\x1b[0m\x1b[0m:\x1b[0m ThinkPad X1 Extreme (Gen 2)\x1b[0m \n\x1b[41C\x1b[0m\x1b[36m\x1b[1mKernel\x1b[0m\x1b[0m:\x1b[0m 5.8.14-arch1-1\x1b[0m \n\x1b[41C\x1b[0m\x1b[36m\x1b[1mUptime\x1b[0m\x1b[0m:\x1b[0m 3 days, 19 hours, 40 mins\x1b[0m \n\x1b[41C\x1b[0m\x1b[36m\x1b[1mPackages\x1b[0m\x1b[0m:\x1b[0m 2259 (pacman)\x1b[0m \n\x1b[41C\x1b[0m\x1b[36m\x1b[1mShell\x1b[0m\x1b[0m:\x1b[0m zsh (+omz, theunraveler theme)\x1b[0m \n\x1b[41C\x1b[0m\x1b[36m\x1b[1mResolution\x1b[0m\x1b[0m:\x1b[0m 3840x2160\x1b[0m \n\x1b[41C\x1b[0m\x1b[36m\x1b[1mTerminal\x1b[0m\x1b[0m:\x1b[0m kitty\x1b[0m \n\x1b[41C\x1b[0m\x1b[36m\x1b[1mTerminal Font\x1b[0m\x1b[0m:\x1b[0m Operator Mono Lig Book\x1b[0m \n\x1b[41C\x1b[0m\x1b[36m\x1b[1mCPU\x1b[0m\x1b[0m:\x1b[0m Intel i7-9750H (12) @ 4.500GHz\x1b[0m \n\x1b[41C\x1b[0m\x1b[36m\x1b[1mGPU\x1b[0m\x1b[0m:\x1b[0m NVIDIA GeForce GTX 1650 Mobile / Max-Q\x1b[0m \n\x1b[41C\x1b[0m\x1b[36m\x1b[1mGPU\x1b[0m\x1b[0m:\x1b[0m Intel UHD Graphics 630\x1b[0m \n\x1b[41C\x1b[0m\x1b[36m\x1b[1mMemory\x1b[0m\x1b[0m:\x1b[0m 7543MiB / 39769MiB\x1b[0m \n\n\x1b[41C\x1b[30m\x1b[40m   \x1b[31m\x1b[41m   \x1b[32m\x1b[42m   \x1b[33m\x1b[43m   \x1b[34m\x1b[44m   \x1b[35m\x1b[45m   \x1b[36m\x1b[46m   \x1b[37m\x1b[47m   \x1b[m\n\x1b[41C\x1b[38;5;8m\x1b[48;5;8m   \x1b[38;5;9m\x1b[48;5;9m   \x1b[38;5;10m\x1b[48;5;10m   \x1b[38;5;11m\x1b[48;5;11m   \x1b[38;5;12m\x1b[48;5;12m   \x1b[38;5;13m\x1b[48;5;13m   \x1b[38;5;14m\x1b[48;5;14m   \x1b[38;5;15m\x1b[48;5;15m   \x1b[m\n\n\n\x1b[?25h\x1b[?7h");
    cprintf("extra credit plz\n");
    return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// LAB 1: Your code here.
        // HINT 1: use read_ebp().
        // HINT 2: print the current ebp on the first line (not current_ebp[0])
        cprintf("Stack backtrace:\n");

        size_t size = sizeof(uint32_t);

        uint32_t *ebp = (uint32_t *) read_ebp();
        uint32_t *last_ebp = ebp;
        uint32_t *eip;
        uint32_t *args;

        char info_data[4096];
        struct Eipdebuginfo *info = (struct Eipdebuginfo *) &info_data;

        while (1) {
            eip = (uint32_t *) (*last_ebp + size);
            args = (uint32_t *) (*last_ebp + (2 * size));
            
            // lookup symbols

            debuginfo_eip(*eip, info);

            // print the backtrace line

            cprintf("  ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n", ebp, *eip, *args, *(args+size), *(args+2*size), *(args+3*size), *(args+4*size));
            cprintf("         %s:%d: %.*s+%d\n", info->eip_file, info->eip_line, info->eip_fn_namelen, info->eip_fn_name, (*eip) - (info->eip_fn_addr));

            last_ebp = ebp;
            if (*ebp == 0) break;
            ebp = (uint32_t *) (*ebp);
        }

	return 0;
}



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
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
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
        cprintf("percent o test (should print [1337]): [%o]\n", 735);


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
