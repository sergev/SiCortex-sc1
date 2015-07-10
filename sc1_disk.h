/* Simple Disk
 * Revision $Id: sc1_disk.h 50164 2008-01-28 17:02:43Z denney $
 */

#define DISKBUFSIZE 1024

typedef struct {
    t_uint64 size;
    t_uint64 diskaddress;
    t_uint64 memaddress;
    t_uint64 count;
    t_uint64 command;
    t_uint64 status;
    t_uint64 read_cmds;
    t_uint64 read_bytes;
    t_uint64 write_cmds;
    t_uint64 write_bytes;
    t_uint64 reserved[6];
    t_uint64 buffer[DISKBUFSIZE >> 3];
} DiskRegs;

#define CMD_READ 0x100
#define CMD_WRITE 0x200
#define CMD_READBUFFER 0x400
#define CMD_WRITEBUFFER 0x800
#define CMD_CMDMASK (CMD_READ | CMD_WRITE | CMD_READBUFFER | CMD_WRITEBUFFER)
#define CMD_UNITMASK 7
#define STATUS_GOOD 1
#define STATUS_SEEKERROR 2
#define STATUS_READERROR 4
#define STATUS_WRITEERROR 8
#define STATUS_NODEV 16
#define STATUS_IDLE 32


#define DISKBASE    SIM_ULL(0xEB0000000)                     /* Disk base */
#define DISKSIZE	(sizeof(DiskRegs))
#define DISKAMASK 63
