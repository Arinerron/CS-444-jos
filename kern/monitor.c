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
#include <inc/types.h>

#include <kern/pmap.h>
#include <inc/memlayout.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line

extern pde_t *kern_pgdir;

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
        { "show", "Show a dope neofetch pretty-print", mon_show },
        { "dbg", "Debug memory", mon_dbg }
};

struct Flag {
    const char *name;
    const int val;
};

static struct Flag flagdict[] = {
    { "PTE_P", PTE_P },
    { "PTE_W", PTE_W },
    { "PTE_U", PTE_U },
    { "PTE_PWT", PTE_PWT },
    { "PTE_PCD", PTE_PCD },
    { "PTE_A", PTE_A },
    { "PTE_D", PTE_D },
    { "PTE_PS", PTE_PS },
    { "PTE_G", PTE_G }
};

/***** Implementations of basic kernel monitor commands *****/

int mon_dbg(int argc, char **argv, struct Trapframe *tf) {
    if (argc == 1) {
        char *targv[] = {"help", "dbg"};
        mon_help(2, targv, tf);
    } else {
        if (!strcmp(argv[1], "dump")) {
            if (argc < 4) {
                cprintf("Incorrect number of arguments.\n");
                return 0;
            }

            uint32_t start = ROUNDUP(b16to10(argv[3]), PGSIZE) - PGSIZE;
            uint32_t end;
            if (argc == 4)
                end = start + PGSIZE;
            else
                end = ROUNDUP(b16to10(argv[4]), PGSIZE);

            if (end < start) {
                cprintf("End cannot be before start address.\n");
                return 0;
            }

            hexdump((void *)start, (int)(end - start));
        } else if (!strcmp(argv[1], "mappings")) {
            if (argc < 4) {
                cprintf("Incorrect number of arguments.\n");
            } else {
                if (!strcmp(argv[2], "show")) {
                    uint32_t start = ROUNDUP(b16to10(argv[3]), PGSIZE) - PGSIZE;
                    uint32_t end;
                    if (argc == 4)
                        end = start + PGSIZE;
                    else
                        end = ROUNDUP(b16to10(argv[4]), PGSIZE);

                    cprintf("Showing mappings between 0x%x and 0x%x.\n", start, end);
                    cprintf("VIRTUAL \tPHYSICAL\tFLAGS\n");

                    int showed_mappings = 0;
                    for (void *va = (void *)start; va < (void *)end; va += PGSIZE) {
                        pte_t *pte = pgdir_walk(kern_pgdir, va, 0);
                        if (!pte) continue;
                        physaddr_t pa = (physaddr_t) *pte;
                        
                        cprintf("%p\t%p\t%s\n", va, pa & ~0xfff, (char *) format_flags((uint32_t)pa));

                        showed_mappings += 1;
                    }

                    if (!showed_mappings)
                        cprintf("(no mappings in specified range)\n");
                } else if (!strcmp(argv[2], "set") || !strcmp(argv[2], "create")) {
                    if (argc < 5) {
                        cprintf("Incorrect number of arguments.\n");
                        return 0;
                    }

                    void *va = (void *)(ROUNDUP(b16to10(argv[3]), PGSIZE) - PGSIZE);
                    physaddr_t pa = (physaddr_t)(ROUNDUP(b16to10(argv[4]), PGSIZE) - PGSIZE);
                    int size = PGSIZE;
                    int perms = 0; // 0 means don't change

                    if (argc >= 6)
                        size = ROUNDUP(b16to10(argv[5]), PGSIZE) - PGSIZE;
                    if (argc >= 7)
                        perms = parse_perms(&argv[6], argc - 6) | PTE_P;

                    cprintf("Mapping virtual address range %p->%p to physical address range %p->%p (size 0x%x)\n", va, va + size, pa, pa + size, size);
                    for (int i = 0; i < size; i += PGSIZE) {
                        pte_t *pte = pgdir_walk(kern_pgdir, va + i, perms || PTE_P);
                        if (!pte) {
                            cprintf("Could not alloc PTE for va %p.\n", va + i);
                            return 0;
                        }

                        *pte = (pa + i) | (perms || ((*pte & 0xfff) | PTE_P)); // reuse permissions
                        cprintf("... virtual %p => physical %p\n", va + i, pa + i);
                    }
                } else if (!strcmp(argv[2], "unset") || !strcmp(argv[2], "delete")) {
                    if (argc < 4) {
                        cprintf("Incorrect number of arguments.\n");
                        return 0;
                    }

                    void *va = (void *)(ROUNDUP(b16to10(argv[3]), PGSIZE) - PGSIZE);
                    int size = PGSIZE;
                    int perms = PTE_P;

                    if (argc >= 5)
                        size = ROUNDUP(b16to10(argv[4]), PGSIZE) - PGSIZE;

                    cprintf("Deleting pages from %p to %p\n", va, va + size);
                    for (int i = 0; i < size; i += PGSIZE) {
                        pte_t *pte = pgdir_walk(kern_pgdir, va + i, 0);
                        if (pte && *pte & PTE_P) {
                            physaddr_t pa = (physaddr_t) *pte;
                            cprintf("... %p virtual => %p physical deleted\n", va + i, pa & ~0xfff);
                        } else {
                            cprintf("... %p is not mapped\n", va + i);
                        }
                    }
                } else if (!strcmp(argv[2], "setperms")) {
                    if (argc < 4) {
                        cprintf("Incorrect number of arguments.\n");
                        return 0;
                    }

                    void *va = (void *)(ROUNDUP(b16to10(argv[3]), PGSIZE) - PGSIZE);
                    int perms = PTE_P;

                    if (argc >= 5)
                        perms = parse_perms(&argv[4], argc - 4);
                    
                    pte_t *pte = pgdir_walk(kern_pgdir, va, perms);
                    if (pte && *pte & PTE_P) {
                        *pte = (*pte & ~0xfff) | perms;
                        cprintf("Applied flags %s to physical address %p in page table\n", format_flags(perms), *pte & ~0xfff);
                    } else {
                        cprintf("%p is not mapped\n", va);
                    }
                } else {
                    cprintf("%s %s subcommand '%s' not found.\n", argv[0], argv[1], argv[2]);
                }
            }
        } else {
            cprintf("%s subcommand '%s' not found.\n", argv[0], argv[1]);
        }
    }
    return 0;
}

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
    if (argc == 1) {
        int i;

        for (i = 0; i < ARRAY_SIZE(commands); i++)
                cprintf("%s - %s\n", commands[i].name, commands[i].desc);
        return 0;
    } else {
        for (int i = 1; i < argc; i++) {
            // print out the generic help
            int found_cmd = 0;
            for (int i2 = 0; i2 < ARRAY_SIZE(commands); i2++) {
                if (!strcmp(commands[i2].name, argv[i])) {
                    cprintf("%s - %s\n", commands[i2].name, commands[i2].desc);
                }

                found_cmd = 1;
            }

            if (found_cmd) {
                cprintf("\n");
            } else {
                cprintf("Command '%s' not found.\n");
                continue;
            }

            if (!strcmp(argv[i], "dbg")) {
                cprintf("> %s mappings show <start> <end=start+PGSIZE>\n\tShows the physical page mappings that apply for the specified range of address space.\n", argv[i]);
                cprintf("> %s mappings setperms <virtual> [flags...=PTE_P]\n\tSets the permission flags for a given virtual address mapping. Flags may be any of [PTE_W, PTE_U, PTE_PWT, PTE_PCD, PTE_A, PTE_D, PTE_PS, PTE_G].\n", argv[i]);
                cprintf("> %s mappings set <virtual> <physical> <size=PGSIZE> [flags...=PTE_P]\n\tDefines a new physical page mapping of a certain size.\n", argv[i]);
                cprintf("> %s mappings unset <start> <end=start+PGSIZE>\n\tDeletes the physical page mappings that apply for the specified range of address space.\n", argv[i]);
                cprintf("> %s dump <start> <end>\n", argv[i]);
            } else {
                cprintf("No additional help is available for the command '%s'.\n", argv[i]);
            }

            cprintf("\n");
        }
    }

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


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}

uint32_t b16to10(const char *str) {
    uint32_t res = 0;

    if (str[0] == '0' && str[1] == 'x') {
        str = str + 2;
    }

    res = (uint32_t) strtol(str, NULL, 16);

    return res;
}

int parse_perms(char **argv, int argc) {
    int flags = PTE_P;
    if (!argc) return flags;

    for (int i = 0; i < argc; i++) {
        int found = 0;
        for (int i2 = 0; i2 < ARRAY_SIZE(flagdict); i2++) {
            if (!strcmp(argv[i], flagdict[i2].name)) {
                flags |= flagdict[i2].val;
                found = 1;
            }
        }

        if (!found) {
            // try as int
            cprintf("Flag '%s' invalid; attempting to interpret as integer.\n", argv[i]);
            flags |= strtol(argv[i], NULL, 10);
        }
    }

    return flags;
}

// TODO: refactor to take in a &buffer
char flags_output[4096]; // XXX: potential bof
char * format_flags(uint32_t flags) {
    flags_output[0] = 0;

    int first = 1;
    for (int i = 0; i < ARRAY_SIZE(flagdict); i++) {
        if (flags & flagdict[i].val) {
            if (!first) {
                strcat(flags_output, "|");
            }

            strcat(flags_output, flagdict[i].name);
            first = 0;
        }
    }

    return flags_output;
    //for (int i = 0; i < 12; i++) cprintf("%d", (pa & (1 << i)) ? 1 : 0);
}

void hexdump(void *start, int size) {
    const int HEXDUMP_COLS = 16;
    const int HEXDUMP_SHOW_BYTES = 1;

    void *ptr = start;
    while (ptr < (start + size)) {
        cprintf("0x%08x:  ", ptr);
        int encountered_unmapped_region = 0;
        
        for (int column = 0; column < HEXDUMP_COLS; column++) {
            cprintf("%02x ", ((unsigned char *)ptr)[0]);
            ptr += 1;

            int first = 1;
            while (1) {
                pte_t *pte = pgdir_walk(kern_pgdir, (void *)(ROUNDUP(ptr, PGSIZE) - PGSIZE), 0);
                if (!(pte && *pte & PTE_P)) {
                    if (first) {
                        // XXX: what about HEXDUMP_SHOW_BYTES?
                        cprintf("\n<unmapped memory region, skipping...>");
                    }
                    
                    encountered_unmapped_region = 1;
                    first = 0;

                    ptr = ROUNDUP(ptr, PGSIZE); // jump to next page
                } else {
                    break;
                }
            }

            if (encountered_unmapped_region) break;
        }

        if (HEXDUMP_SHOW_BYTES && !encountered_unmapped_region) {
            cprintf("  %.*s", HEXDUMP_COLS, ptr - HEXDUMP_COLS - 1);
        }

        cprintf("\n");
    }
}
