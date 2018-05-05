#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char _binary_traceDb_yaml_start[];

typedef struct {
	unsigned int id;
	char *str;
} traceDb_t;

traceDb_t *traceDb = NULL;
unsigned int traceDb_cnt = 0;


int compar(const void *item_1_p, const void *item_2_p)
{
	traceDb_t *item_1 = (traceDb_t *)item_1_p;
	traceDb_t *item_2 = (traceDb_t *)item_2_p;

	if (item_1->id > item_2->id)
		return 1;
	if (item_1->id < item_2->id)
		return -1;
	return 0;
}

void free_traceDb(void)
{
	int i;

	if (!traceDb)
		return;

	for (i = 0; i < traceDb_cnt; i++)
		free(traceDb[i].str);

	free(traceDb);
	traceDb = NULL;
}

#define STR_DELIM "\n"

int loadDb(void)
{
	char *in_str = _binary_traceDb_yaml_start;
	char *delim;

	while ((delim = strstr(in_str, STR_DELIM)) != NULL) {

		char *one_line = malloc(delim - in_str + 1);
		if (!one_line) {
			printf("%s[%d]: can not allocate memory\n", __func__, __LINE__);

			free_traceDb();
			return -1;
		}

		memcpy(one_line, in_str, delim - in_str);
		one_line[delim - in_str] = 0;

		char *start_fmt_str = NULL;
		char *end_fmt_str = NULL;
		start_fmt_str = strstr(one_line, "\"");
		if (start_fmt_str) {
			start_fmt_str++;
			end_fmt_str = strstr(start_fmt_str, "\"");
			if (end_fmt_str) {
				char *target_str = malloc(end_fmt_str - start_fmt_str + 1);
				if (target_str) {
					memcpy(target_str, start_fmt_str, end_fmt_str - start_fmt_str);
					target_str[end_fmt_str - start_fmt_str] = 0;

					unsigned int id;
					sscanf(one_line, "0x%x", &id);

					traceDb_cnt++;
					traceDb_t *traceDb_new = realloc(traceDb, sizeof(traceDb_t) * traceDb_cnt);
					if (traceDb_new) {
						traceDb = traceDb_new;
					}
					else {
						printf("%s[%d]: can not allocate memory\n", __func__, __LINE__);

						free(one_line);
						free(target_str);
						free_traceDb();
						return -1;
					}
					
					char *pos;
					while ((pos = strstr(target_str, "\\n")) != NULL) {
						memmove(pos + 1, pos + 2, strlen(target_str) + 1 - (pos - target_str) - 1);
						*pos = '\n';
					}
					
					
					traceDb[traceDb_cnt - 1].id = id;
					traceDb[traceDb_cnt - 1].str = target_str;
				}
				else {
					printf("%s[%d]: can not allocate memory\n", __func__, __LINE__);

					free(one_line);
					free_traceDb();
					return -1;
				}
			}
		}

		free(one_line);

		in_str = delim + strlen(STR_DELIM);
	}

	qsort((void *)traceDb, sizeof(traceDb_t), traceDb_cnt, compar);

	return 0;
}

void print_traceDb(void)
{
	int i;
	for (i = 0; i < traceDb_cnt; i++)
		printf("%d - 0x%08x: %s\n", i, traceDb[i].id, traceDb[i].str);
}


char *traceDb_get_str(unsigned int id)
{
	unsigned int left = 0;
	unsigned int right = traceDb_cnt - 1;

	if (id > traceDb[right].id)
		return NULL;

	while (1) {
		if (traceDb[right].id == id)
			return traceDb[right].str;
		if (traceDb[left].id == id)
			return traceDb[left].str;

		if (right - left <= 1)
			break;

		unsigned int cur = (right - left) / 2;
		if (traceDb[left + cur].id < id)
			left += cur;
		else
			right -= cur;
	}
	
	return NULL;
}

