/*
 *  Synopsis:
 *	Digest module list for biod.
 *  Blame:
 *  	jmscott@setspace.com
 *  	setspace@gmail.com
 */
#include "biod.h"

#ifdef SHA_FS_MODULE
extern struct digest_module		sha_fs_module;
#endif

#ifdef SK_FS_MODULE
extern struct digest_module		sk_fs_module;
#endif

/*
 *  List of all digest modules.
 *  The module list should really be in a separate file, not here.
 */
static struct digest_module		*modules[] =
{
#ifdef SHA_FS_MODULE 
	&sha_fs_module,
#endif

#ifdef SK_FS_MODULE
	&sk_fs_module,
#endif
};
static int module_count;

int
module_boot()
{
	int i;
	static char nm[] = "module_boot";
	char buf[MSG_SIZE];

	module_count = sizeof modules / sizeof *modules;
	snprintf(buf, sizeof buf, "%d compiled signature modules",module_count);
	info2(nm, buf);

	if (module_count == 0)
		panic2(nm, "no compiled signature modules");

	/*
	 *  Double check that modules are stored lexical order by name.
	 */
	for (i = 1;  i < module_count;  i++) {
		if (strcmp(modules[i - 1]->name, modules[i]->name) >= 0)
			panic3("module_init: modules out of order",
				modules[i - 1]->name, modules[i]->name);
	}

	for (i = 0;  i < module_count;  i++) {
		struct digest_module *mp = modules[i];

		info2("booting signature digest module", mp->name);
		if (mp->boot) {
			int status;

			status = (*mp->boot)();
			if (status != 0)
				panic3(nm, mp->name, "boot() failed");
		}
	}
	return 0;
}

void
module_leave()
{
	int i;
	static char nm[] = "module_leave";

	for (i = 0;  i < module_count;  i++) {
		struct digest_module *mp = modules[i];

		if (mp->shutdown) {
			if ((*mp->shutdown)())
				error3(nm, mp->name, "callback failed");
		} else
			warn3(nm, mp->name, "no shutdown() callback");
	}
}

struct digest_module *
module_get(char *name)
{
	size_t i;

	for (i = 0;  i < sizeof modules / sizeof *modules;  i++)
		if (strcmp(name, modules[i]->name) == 0)
			return modules[i];
	return (struct digest_module *)0;
}
