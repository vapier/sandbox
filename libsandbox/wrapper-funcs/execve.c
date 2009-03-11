/*
 * execve() wrapper.
 *
 * Copyright 1999-2008 Gentoo Foundation
 * Licensed under the GPL-2
 *
 *  Partly Copyright (C) 1998-9 Pancrazio `Ezio' de Mauro <p@demauro.net>,
 *  as some of the InstallWatch code was used.
 */

#define WRAPPER_ARGS_PROTO const char *filename, char *const argv[], char *const envp[]
#define WRAPPER_ARGS filename, argv, envp
extern int EXTERN_NAME(WRAPPER_ARGS_PROTO);
static int (*WRAPPER_TRUE_NAME)(WRAPPER_ARGS_PROTO) = NULL;
static FILE *tty_fp = NULL;

/* See to see if this an ELF and if so, is it static which we can't wrap */
static void check_exec(const char *filename, char *const argv[])
{
	int fd;
	unsigned char *elf;
	struct stat st;

	if (!tty_fp)
		tty_fp = fopen("/dev/tty", "ae");
	if (!tty_fp)
		return;

#ifdef __linux__
	/* Filter some common safe static things */
	if (!strncmp(argv[0], "/lib", 4) && strstr(argv[0], ".so.")) {
		/* Packages often run `ldd /some/binary` which will in
		 * turn run `/lib/ld-linux.so.2 --verify /some/binary`
		 */
		if (!strcmp(argv[1], "--verify"))
			return;
	}
#endif

	fd = open(filename, O_RDONLY);
	if (fd == -1)
		return;
	if (stat(filename, &st))
		goto out_fd;
	if (st.st_size < EI_NIDENT)
		goto out_fd;
	elf = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (elf == MAP_FAILED)
		goto out_fd;

	if (elf[EI_MAG0] != ELFMAG0 &&
	    elf[EI_MAG1] != ELFMAG1 &&
	    elf[EI_MAG2] != ELFMAG2 &&
	    elf[EI_MAG3] != ELFMAG3 &&
	    !(elf[EI_CLASS] != ELFCLASS32 ||
	      elf[EI_CLASS] != ELFCLASS64))
		goto out_mmap;

#define PARSE_ELF(n) \
({ \
	Elf##n##_Ehdr *ehdr = (void *)elf; \
	Elf##n##_Phdr *phdr = (void *)(elf + ehdr->e_phoff); \
	uint16_t p; \
	if (st.st_size < sizeof(*ehdr)) \
		goto out_mmap; \
	if (st.st_size < ehdr->e_phoff + ehdr->e_phentsize * ehdr->e_phnum) \
		goto out_mmap; \
	for (p = 0; p < ehdr->e_phnum; ++p) \
		if (phdr[p].p_type == PT_INTERP) \
			goto done; \
})
	if (elf[EI_CLASS] == ELFCLASS32)
		PARSE_ELF(32);
	else
		PARSE_ELF(64);

	/* Write to tty_fd because stderr is not always 100% safe.  If running
	 * tests and validating output, this may break things.  #261957
	 * Writing to /dev/tty directly might annoy some people ... perhaps
	 * we should attempt to hijack the log fd from portage ...
	 */
	sb_fprintf(tty_fp, "QA: Static ELF: %s: ", filename);
	size_t i;
	for (i = 0; argv[i]; ++i)
		if (strchr(argv[i], ' '))
			sb_fprintf(tty_fp, "'%s' ", argv[i]);
		else
			sb_fprintf(tty_fp, "%s ", argv[i]);
	sb_fprintf(tty_fp, "\n");

 done:

 out_mmap:
	munmap(elf, st.st_size);
 out_fd:
	close(fd);
}

int WRAPPER_NAME(WRAPPER_ARGS_PROTO)
{
	char **my_env = NULL;
	char *entry;
	char *ld_preload = NULL;
	char *old_ld_preload = NULL;
	int old_errno = errno;
	int result = -1;
	int count;

	if (!FUNCTION_SANDBOX_SAFE(filename))
		return result;

	check_exec(filename, argv);

	str_list_for_each_item(envp, entry, count) {
		if (strstr(entry, LD_PRELOAD_EQ) != entry)
			continue;

		/* Check if we do not have to do anything */
		if (NULL != strstr(entry, sandbox_lib)) {
			/* Use the user's envp */
			my_env = (char **)envp;
			goto do_execve;
		} else {
			old_ld_preload = entry;
			/* No need to continue, we have to modify LD_PRELOAD */
			break;
		}
	}

	/* Ok, we need to create our own envp, as we need to add LD_PRELOAD,
	 * and we should not touch the user's envp.  First we add LD_PRELOAD,
	 * and just all the rest. */
	count = strlen(LD_PRELOAD_EQ) + strlen(sandbox_lib) + 1;
	if (NULL != old_ld_preload)
		count += strlen(old_ld_preload) - strlen(LD_PRELOAD_EQ) + 1;
	ld_preload = xmalloc(count * sizeof(char));
	if (NULL == ld_preload)
		goto error;
	snprintf(ld_preload, count, "%s%s%s%s", LD_PRELOAD_EQ, sandbox_lib,
		 (old_ld_preload) ? " " : "",
		 (old_ld_preload) ? old_ld_preload + strlen(LD_PRELOAD_EQ) : "");
	str_list_add_item(my_env, ld_preload, error);

	str_list_for_each_item(envp, entry, count) {
		if (strstr(entry, LD_PRELOAD_EQ) != entry) {
			str_list_add_item(my_env, entry, error);
			continue;
		}
	}

do_execve:
	errno = old_errno;
	check_dlsym(WRAPPER_TRUE_NAME, WRAPPER_SYMNAME,
		    WRAPPER_SYMVER);
	result = WRAPPER_TRUE_NAME(filename, argv, my_env);

	if ((NULL != my_env) && (my_env != envp))
		/* We do not use str_list_free(), as we did not allocate the
		 * entries except for LD_PRELOAD. */
		free(my_env);
	if (NULL != ld_preload)
		free(ld_preload);

	return result;

error:
	if ((NULL != my_env) && (my_env != envp))
		/* We do not use str_list_free(), as we did not allocate the
		 * entries except for LD_PRELOAD. */
		free(my_env);
	if (NULL != ld_preload)
		free(ld_preload);

	return -1;
}
