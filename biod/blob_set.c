/*
 *  Synopsis:
 *	Simple functions to get/put small blobs in memory
 */
#include "biod.h"

/*
 *  Size of hash table.  Really ought to grow dynamically.
 */
#define HASH_TABLE_SIZE		99991

/*
 *  Attributed to Dan Berstein from a usenet comp.lang.c newsgroup.
 */
static unsigned int
djb(unsigned char *buf, int nbytes)
{
	unsigned int hash = 5381;
	unsigned char *p;

	p = buf;
	while (--nbytes >= 0)
		 hash = ((hash << 5) + hash) + *p++;
	return hash;
}

struct hash_set_element
{
	unsigned char		*value;
	int			size;		

	struct hash_set_element	*next;
};

struct hash_set
{
	int			size;
	struct hash_set_element	**table;
	int			for_each_level;
};

/*
 *  Invoke a callback on each element in a set.
 *  No elements may be added or deleted during the traversal.
 */
int
blob_set_for_each(void *set,
		 int (*each_callback)(unsigned char *, int size, void *),
		 void *context_data
		)
{
	struct hash_set *s;
	struct hash_set_element *e;
	int status, i;

	s = (struct hash_set *)set;
	s->for_each_level++;

	/*
	 *  For each element, invoke the callback.
	 */
	status = 0;
	for (i = 0;  i < s->size;  i++)
		if ((e = s->table[i]))
			while (e) {
				if ((status = (*each_callback)(
						e->value,
						e->size,
						context_data
				)))
					goto bye;
				e = e->next;
			}
bye:
	--s->for_each_level;
	return status;
}

void
blob_set_alloc(void **set)
{
	struct hash_set *s;
	static char nm[] = "blob_set_alloc";

	s = malloc(sizeof *s);
	if (s == NULL)
		panic3(nm, "malloc(hash_set) failed", strerror(errno));
	s->size = HASH_TABLE_SIZE;
	s->table = calloc(s->size * sizeof *s->table, sizeof *s->table);
	if (s->table == NULL)
		panic3(nm, "calloc(table) failed", strerror(errno));
	s->for_each_level = 0;
	*set = (void *)s;
}

/*
 *  Does an element exist in a set?
 */
int
blob_set_exists(void *set, unsigned char *value, int size)
{
	struct hash_set *s = (struct hash_set *)set;
	struct hash_set_element *e;
	unsigned int hash;

	hash = djb(value, size) % s->size;
	e = s->table[hash];
	if (e) do {
		if (size == e->size && memcmp(e->value, value, size) == 0)
			return 1;
	} while ((e = e->next));

	return 0;
}

/*
 *  Synopsis:
 *	Add an element to the hash table.
 *  Returns:
 *	0	added, did not exist
 *	1	already exists
 *	-1	failed.
 *  Note:
 *	Calling blob_set_put() while in the middle of an unfinished
 *	blob_set_for_each() is a system panic.
 */
int
blob_set_put(void *set, unsigned char *value, int size)
{
	unsigned int hash;
	struct hash_set *s;
	struct hash_set_element *e, *e_new;
	static char nm[] = "blob_set_put";

	s = (struct hash_set *)set;
	if (s->for_each_level > 0)
		panic2(nm, "blob_set_put() called in for each");
	if (blob_set_exists(set, value, size))
		return 1;

	/*
	 *  Allocate new hash entry.
	 */
	e_new = malloc(sizeof *e);
	if (e_new == NULL)
		panic3(nm, "malloc() failed", strerror(errno));
	e_new->value = malloc(size);
	if (e_new->value == NULL)
		panic3(nm, "malloc(element) failed", strerror(errno));
	memcpy(e_new->value, value, size);
	e_new->size = size;
	e_new->next = 0;
	hash = djb(value, size) % s->size;
	if ((e = s->table[hash]))
		e->next = e_new;
	else
		s->table[hash] = e_new;
	return 0;
}

static void
free_element(struct hash_set_element *e)
{
	if (e) {
		free_element(e->next);
		free(e->value);
		free(e);
	}
}

/*
 *  Free a blob set.
 *  Returns 0 on success, non 0 otherwise.
 *
 *  Note:
 *  	Why would a free() fail other than because memory is corrupted?
 */
int
blob_set_free(void *set)
{
	struct hash_set *s = (struct hash_set *)set;
	int i;

	for (i = 0;  i < s->size;  i++)
		free_element(s->table[i]);
	free(s->table);
	free(s);
	return 0;
}
