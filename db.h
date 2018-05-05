#ifndef __DB_H__
#define __DB_H__

int loadDb(void);
void free_traceDb(void);
void print_traceDb(void);
char *traceDb_get_str(unsigned int id);

#endif
