/*---------------------------------------------------------------------------/
/  Configuration of FatFs R0.16 for PicoCalc FX-CG50 AI
/  (based on the ffconf.h template of the FatFs distribution, see LICENSE.txt)
/
/  - long file names: ".pcgraph" / ".pcpict" have extensions longer than 8.3
/  - exFAT: SDXC cards (64 GB and more) are sold formatted with exFAT
/  - tiny buffers (one 512-byte sector window shared by the files)
/  - no real-time clock on the PicoCalc: fixed time stamp
/  - f_mkfs only in the host build (tests format a RAM disk)
/---------------------------------------------------------------------------*/

#define FFCONF_DEF	80386	/* Revision ID */

/*---------------------------------------------------------------------------/
/ Function Configurations
/---------------------------------------------------------------------------*/

#define FF_FS_READONLY	0
#define FF_FS_MINIMIZE	0
#define FF_USE_FIND		0
#ifdef FXAI_FATFS_MKFS
#define FF_USE_MKFS		1
#else
#define FF_USE_MKFS		0
#endif
#define FF_USE_FASTSEEK	0
#define FF_USE_EXPAND	0
#define FF_USE_CHMOD	0
#define FF_USE_LABEL	0
#define FF_USE_FORWARD	0
#define FF_USE_STRFUNC	0
#define FF_PRINT_LLI	0
#define FF_PRINT_FLOAT	0
#define FF_STRF_ENCODE	0

/*---------------------------------------------------------------------------/
/ Locale and Namespace Configurations
/---------------------------------------------------------------------------*/

#define FF_CODE_PAGE	437		/* U.S. */
#define FF_USE_LFN		1		/* LFN with a static working buffer (single thread) */
#define FF_MAX_LFN		255
#define FF_LFN_UNICODE	0		/* ANSI/OEM in char (UTF-8 is not needed: names are A-Z 0-9 _ -) */
#define FF_LFN_BUF		255
#define FF_SFN_BUF		12
#define FF_FS_RPATH		0
#define FF_PATH_DEPTH	10

/*---------------------------------------------------------------------------/
/ Drive/Volume Configurations
/---------------------------------------------------------------------------*/

#define FF_VOLUMES		1
#define FF_STR_VOLUME_ID	0
#define FF_VOLUME_STRS		"SD"
#define FF_MULTI_PARTITION	0
#define FF_MIN_SS		512
#define FF_MAX_SS		512
#define FF_LBA64		1		/* required by exFAT */
#define FF_MIN_GPT		0x10000000
#define FF_USE_TRIM		0

/*---------------------------------------------------------------------------/
/ System Configurations
/---------------------------------------------------------------------------*/

#define FF_FS_TINY		1
#define FF_FS_EXFAT		1
#define FF_FS_NORTC		1
#define FF_NORTC_MON	1
#define FF_NORTC_MDAY	1
#define FF_NORTC_YEAR	2026
#define FF_FS_CRTIME	0
#define FF_FS_NOFSINFO	0
#define FF_FS_LOCK		0
#define FF_FS_REENTRANT	0
#define FF_FS_TIMEOUT	1000

/*--- End of configuration options ---*/
