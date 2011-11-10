/*
 * libc/stdlib/clearenv.c
 */

#include <runtime.h>
#include <stdlib.h>

int clearenv(void)
{
	struct environ_t * environ = &(__get_runtime()->__environ);
	struct environ_t * p, * q;

	if (!environ)
		return -1;

	for(p = environ->next; p != environ; )
	{
		q = p;
		p = p->next;

		q->next->prev = q->prev;
		q->prev->next = q->next;
		free(q->content);
		free(q);
	}

	return 0;
}
