#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "dump.h"
#include "types.h"
#include "db.h"
#include "print.h"

/* 7                    0 15                      0 7          0    len bytes      7                   0
 * [ 0xAD - frame start ] [ frame len: (Msb->Lsb) ] [ frame id ] [ ... data .... ] [ next frame offset ]
 * data:
 * 15   0
 * [ id ] [ fmt - 4bytes or fmt str]
 * id:
 * 15 13   12        11         10        9     8        5 4     0
 * [   ] [ TBD ] [ TRAISE ] [ TABORT ] [ TIDU ] [ TLEVEL ] [ TID ]
 * TID    - 5 bits for user Id.
 * TLEVEL - 4 bits for level.
 * TIDU   - Do not display id.
 * TABORT - Abort indicator for abort exception.
 * TRAISE - Raise indicator for catched exception.
 * TBD    - Data base indicator: format string to be found in data base.
 * if TBD == 1 then fmt = 4bytes number from traceDb.yaml else fmt = fmt_str
*/

#define SXS_RMT_START_FRM   0xAD

#define SXS_RMC_YIELD    0x80

enum
{
	SXS_SPY_RMC,
	SXS_TRACE_RMC = SXS_RMC_YIELD, // 0x80
	SXS_DUMP_RMC,
	SXS_RPC_RSP_RMC,
	SXS_TIME_STAMP_RMC,

	SXS_READ_RMC, //0x84
	SXS_WRITE_RMC,
	SXS_TRACE_LEVEL_RMC,
	SXS_RPC_RMC,
	SXS_DWL_START_RMC, // 0x88
	SXS_DWL_RMC,
	SXS_MBX_RMC,
	SXS_RAZ_RMC,
	SXS_INIT_RMC, // 0x8c
	SXS_SERIAL_RMC,
	SXS_WIN_RMC,
	SXS_MEM_RMC,
	SXS_AT_RMC, // 0x90
	SXS_DATA_RMC,
	SXS_DATACTRL_RMC,

	SXS_EXIT_RMC, // 0x93

	SXS_PROFILE_RMC, // 0x94

	SXS_WCPU_RMC, // 0x95
};

char *frame_id_str[] =
{
	[SXS_SPY_RMC] = "SXS_SPY_RMC",
	[SXS_TRACE_RMC] = "SXS_TRACE_RMC",
	[SXS_DUMP_RMC] = "SXS_DUMP_RMC",
	[SXS_RPC_RSP_RMC] = "SXS_RPC_RSP_RMC",
	[SXS_TIME_STAMP_RMC] = "SXS_TIME_STAMP_RMC",

	[SXS_READ_RMC] = "SXS_READ_RMC",
	[SXS_WRITE_RMC] = "SXS_WRITE_RMC",
	[SXS_TRACE_LEVEL_RMC] = "SXS_TRACE_LEVEL_RMC",
	[SXS_RPC_RMC] = "SXS_RPC_RMC",
	[SXS_DWL_START_RMC] = "SXS_DWL_START_RMC",
	[SXS_DWL_RMC] = "SXS_DWL_RMC",
	[SXS_MBX_RMC] = "SXS_MBX_RMC",
	[SXS_RAZ_RMC] = "SXS_RAZ_RMC",
	[SXS_INIT_RMC] = "SXS_INIT_RMC",
	[SXS_SERIAL_RMC] = "SXS_SERIAL_RMC",
	[SXS_WIN_RMC] = "SXS_WIN_RMC",
	[SXS_MEM_RMC] = "SXS_MEM_RMC",
	[SXS_AT_RMC] = "SXS_AT_RMC",
	[SXS_DATA_RMC] = "SXS_DATA_RMC",
	[SXS_DATACTRL_RMC] = "SXS_DATACTRL_RMC",

	[SXS_EXIT_RMC] = "SXS_EXIT_RMC",

	[SXS_PROFILE_RMC] = "SXS_PROFILE_RMC",

	[SXS_WCPU_RMC] = "SXS_WCPU_RMC",
};

#define FRAME_ID_TO_STR(id) (frame_id_str[id] ? frame_id_str[id] : "unknown")

typedef enum {__SXR, __PAL, __L1A, __L1S, __LAP, __RLU, __RLD,
			__LLC, __MM,  __CC,  __SS,  __SMS,  __SM, __SND, __API,
			__MMI, __SIM, __AT , __M2A, __STT, __RRI, __RRD, __RLP,
			__HAL, __BCPU, __CSW, __EDRV, __MCI, __SVC1, __SVC2, __WCPU,
			SXS_NB_ID} sxs_TraceId_t;

const ascii * const sxs_TraceDesc [SXS_NB_ID] =
{
 "SXR", "PAL", "L1A", "L1S", "LAP", "RLU", "RLD",
 "LLC", "MM ", "CC ", "SS ", "SMS", "SM ", "SND", "API",
 "MMI", "SIM", "AT ", "M2A", "STT", "RRI", "RRD", "RLP",
 "HAL", "BCPU", "CSW", "EDRV", "MCI", "SV1", "SV2", "WCPU"
};

#define TID_POS         0
#define TID_MSK         (0x1F << TID_POS)               /* 5 bits for user Id.  */
#define TID(Id)         ((u8)((Id << TID_POS) & TID_MSK))
#define TGET_ID(Id)     (u8)((Id & TID_MSK) >> TID_POS)
#define TLEVEL_POS      5
#define TLEVEL_MSK      (0x0F << TLEVEL_POS)            /* 4 bits for level.    */
#define TLEVEL(Lev)     (((Lev-1) << TLEVEL_POS) & TLEVEL_MSK)
#define TGET_LEVEL(Id)  (u8)((Id & TLEVEL_MSK) >> TLEVEL_POS)
#define TIDU_POS        9
#define TIDU            (1 << TIDU_POS)                 /* Do not display id.*/
#define TABORT_POS      10
#define TABORT          (1 << TABORT_POS)               /* Abort indicator for abort exception. */
#define TRAISE_POS      11
#define TRAISE          (1 << TRAISE_POS)               /* Raise indicator for catched exception. */
#define TDB_POS         12
#define TDB             (1 << TDB_POS)                  /* Data base indicator: format string to be found in data base. */

#if 0
80 14 80 3c 2d 2d 20 3c 25 73 3e 20 6f 6e 20 41 ...<-- <%s> on A
72 66 63 6e 20 30 78 25 78 0a 00 52 52 49 5f 53 rfcn 0x%x..RRI_S
59 53 49 4e 46 4f 5f 49 4e 44 00 51 00 00 00 c4 YSINFO_IND.Q....
e3 24 82 03 00 00 00 e4 45 29 82 44 7d 27 82    .$......E).D}'.

char fmt_arg[] = {0x3c, 0x2d, 0x2d, 0x20, 0x3c, 0x25, 0x73, 0x3e, 0x20, 0x6f, 0x6e, 0x20, 0x41, 0x72, 0x66, 0x63, 0x6e, 0x20, 0x30, 0x78, 0x25, 0x78, 0x0a, 0x00};
char data_arg[] = {0x52, 0x52, 0x49, 0x5f, 0x53, 0x59, 0x53, 0x49, 0x4e, 0x46, 0x4f, 0x5f, 0x49, 0x4e, 0x44, 0x00, 0x51, 0x00, 0x00, 0x00};
#endif

#define DBG_FILE "/dev/modem1"
//#define DBG_FILE "log.bin"

#define DBG_EN_FILE "/sys/devices/virtual/tty/modem1/bp_trace"

void dbg_enable(void)
{
	FILE *fd = fopen(DBG_EN_FILE, "w");
	if (!fd) {
		printf("can not open dbg enable file "DBG_EN_FILE"\n");
		return;
	}
	fprintf(fd, "1\n");
	fclose(fd);
}

void main(void)
{
	int fd = open(DBG_FILE , O_RDONLY);
	if (fd < 0) {
		printf("can not open debug file: "DBG_FILE"\n");
		return; 
	}

	if (loadDb()) {
		printf("can not load traceDb\n");
		return;
	}

	dbg_enable();

	while (1) {
		u8 start_frm;
		int read_len;

		read_len = read(fd, &start_frm, 1);

		if (read_len < 1 || start_frm != SXS_RMT_START_FRM)
			continue;

		u16 data_size;

		u8 *data_buf = NULL;
		u16 Size;
		u8 *Data = NULL;

		read_len = read(fd, &data_size, sizeof(unsigned short));

		if (read_len < sizeof(unsigned short))
			continue;

		u16 tmp = data_size;
		data_size >>= 8;
		data_size |= tmp << 8;
		
		Size = data_size;

		data_buf = malloc(data_size);
		if (!data_buf)
			continue;

		Data = data_buf;

		read_len = read(fd, data_buf, data_size);

		if (read_len < data_size)
			continue;

		//printf("data_size: %d\n", data_size);

		u8 frame_id = data_buf[0];

		Data += sizeof(frame_id);
		Size -= sizeof(frame_id);

		//printf("frame id: 0x%02hhx (%s)\n", frame_id, FRAME_ID_TO_STR(frame_id));

		//hex_dump(data_buf, data_size);
		//printf("---\n");

		switch (frame_id) {
			case SXS_TRACE_RMC:
			{
				u16  TId = *((u16 *)Data);
				u16  FmtSize = strlen((ascii *)&Data [2]);
				u32 *StrBuffer;
				u8   PCnt = 0, i = 0, SMap = 0;
				u8  *Fmt = &Data[2];

				if (!(TId & TIDU))
					printf("%s %2i: ", sxs_TraceDesc [TGET_ID(TId)], TGET_LEVEL(TId) + 1);

				if (TId & TDB) {
					Fmt = traceDb_get_str(*(u32 *)(Data + 2));
					if (!Fmt)
						printf("unknown traceDb id: 0x%08x\n", *(u32 *)(Data + 2));
					Data += 4 + 2;
				}
				else {
					Data += FmtSize + 2 + 1;
				}
				if (Fmt) {
					char *fmt_new = strdup(Fmt);

					int i = strlen(fmt_new) - 1;
					while (fmt_new[i] == '\n') {
						fmt_new[i] = 0;
						i--;
					}

					print(fmt_new, Data);
					
					free(fmt_new);

					printf("\n");

				}

				break;
			}
			case SXS_TIME_STAMP_RMC:
			{
				u32 Fn = *((u32 *)Data);
				printf ("Fn %i T1 %i T2 %i T3 %i\n", Fn, Fn/1326, Fn%26, Fn%51);
				break;
			}
			default:
				printf("not supported frame_id: 0x%02x(%s)\n", frame_id, FRAME_ID_TO_STR(frame_id));
		}

		free(data_buf);

		unsigned char next_frame_offset;
		read(fd, &next_frame_offset, 1);

	}

	free_traceDb();
	close(fd);
}
