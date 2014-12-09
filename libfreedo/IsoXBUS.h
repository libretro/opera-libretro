/*
  www.freedo.org
The first and only working 3DO multiplayer emulator.

The FreeDO licensed under modified GNU LGPL, with following notes:

*   The owners and original authors of the FreeDO have full right to develop closed source derivative work.
*   Any non-commercial uses of the FreeDO sources or any knowledge obtained by studying or reverse engineering
    of the sources, or any other material published by FreeDO have to be accompanied with full credits.
*   Any commercial uses of FreeDO sources or any knowledge obtained by studying or reverse engineering of the sources,
    or any other material published by FreeDO is strictly forbidden without owners approval.

The above notes are taking precedence over GNU LGPL in conflicting situations.

Project authors:

Alexander Troosh
Maxim Grishin
Allen Wright
John Sammons
Felix Lazarev
*/

#ifndef ISO_3DO_HEADER
#define ISO_3DO_HEADER

#define XBP_INIT		0	//plugin init, returns plugin version
#define XBP_RESET		1	//plugin reset with parameter(image path)
#define XBP_SET_COMMAND	2	//XBUS
#define XBP_FIQ			3	//check interrupt form device
#define XBP_SET_DATA	4	//XBUS
#define XBP_GET_DATA	5	//XBUS
#define XBP_GET_STATUS	6	//XBUS
#define XBP_SET_POLL	7	//XBUS
#define XBP_GET_POLL	8	//XBUS
#define XBP_SELECT		9   //selects device by Opera
#define XBP_RESERV		10  //reserved reading from device
#define XBP_DESTROY		11  //plugin destroy

#define XBP_GET_SAVESIZE	19	//save support from emulator side
#define XBP_GET_SAVEDATA	20
#define XBP_SET_SAVEDATA	21

#define STATDELAY 100
#define REQSIZE	2048

enum MEI_CDROM_Error_Codes {
   MEI_CDROM_no_error = 0x00,
   MEI_CDROM_recv_retry = 0x01,
   MEI_CDROM_recv_ecc = 0x02,
   MEI_CDROM_not_ready = 0x03,
   MEI_CDROM_toc_error = 0x04,
   MEI_CDROM_unrecv_error = 0x05,
   MEI_CDROM_seek_error = 0x06,
   MEI_CDROM_track_error = 0x07,
   MEI_CDROM_ram_error = 0x08,
   MEI_CDROM_diag_error = 0x09,
   MEI_CDROM_focus_error = 0x0A,
   MEI_CDROM_clv_error = 0x0B,
   MEI_CDROM_data_error = 0x0C,
   MEI_CDROM_address_error = 0x0D,
   MEI_CDROM_cdb_error = 0x0E,
   MEI_CDROM_end_address = 0x0F,
   MEI_CDROM_mode_error = 0x10,
   MEI_CDROM_media_changed = 0x11,
   MEI_CDROM_hard_reset = 0x12,
   MEI_CDROM_rom_error = 0x13,
   MEI_CDROM_cmd_error = 0x14,
   MEI_CDROM_disc_out = 0x15,
   MEI_CDROM_hardware_error = 0x16,
   MEI_CDROM_illegal_request = 0x17
};


#define POLSTMASK	0x01
#define POLDTMASK	0x02
#define POLMAMASK	0x04
#define POLREMASK	0x08
#define POLST		0x10
#define POLDT		0x20
#define POLMA		0x40
#define POLRE		0x80

#define CDST_TRAY  0x80
#define CDST_DISC  0x40
#define CDST_SPIN  0x20
#define CDST_ERRO  0x10
#define CDST_2X    0x02
#define CDST_RDY   0x01
#define CDST_TRDISC 0xC0
#define CDST_OK    CDST_RDY|CDST_TRAY|CDST_DISC|CDST_SPIN

//medium specific
#define CD_CTL_PREEMPHASIS      0x01
#define CD_CTL_COPY_PERMITTED   0x02
#define CD_CTL_DATA_TRACK       0x04
#define CD_CTL_FOUR_CHANNEL     0x08
#define CD_CTL_QMASK            0xF0
#define CD_CTL_Q_NONE           0x00
#define CD_CTL_Q_POSITION       0x10
#define CD_CTL_Q_MEDIACATALOG   0x20
#define CD_CTL_Q_ISRC           0x30

#define MEI_DISC_DA_OR_CDROM    0x00
#define MEI_DISC_CDI            0x10
#define MEI_DISC_CDROM_XA       0x20

#define CDROM_M1_D              2048
#define CDROM_DA                2352
#define CDROM_DA_PLUS_ERR       2353
#define CDROM_DA_PLUS_SUBCODE   2448
#define CDROM_DA_PLUS_BOTH      2449

//medium specific
//drive specific
#define MEI_CDROM_SINGLE_SPEED  0x00
#define MEI_CDROM_DOUBLE_SPEED  0x80

#define MEI_CDROM_DEFAULT_RECOVERY         0x00
#define MEI_CDROM_CIRC_RETRIES_ONLY        0x01
#define MEI_CDROM_BEST_ATTEMPT_RECOVERY    0x20

#define Address_Blocks    0
#define Address_Abs_MSF   1
#define Address_Track_MSF 2

#pragma pack(push,1)
//drive specific
//disc data
struct TOCEntry{
   unsigned char res0;
   unsigned char CDCTL;
   unsigned char TRKNUM;
   unsigned char res1;
   unsigned char mm;
   unsigned char ss;
   unsigned char ff;
   unsigned char res2;
};

//disc data
struct DISCStc{
   unsigned char curabsmsf[3]; //BIN form
   unsigned char curtrack;
   unsigned char nextmsf[3]; //BIN form
   unsigned char tempmsf[3]; //BIN form
   int  tempblk;
   int  templba;
   unsigned char currenterror;
   unsigned char currentxbus;
   unsigned int  currentoffset;
   unsigned int  currentblocksize;
   unsigned char currentspeed;
   unsigned char  totalmsf[3];//BIN form
   unsigned char  firsttrk;
   unsigned char  lasttrk;
   unsigned char  discid;
   unsigned char  sesmsf[3]; //BIN form
   TOCEntry DiscTOC[100];
};

struct cdrom_Device
{
      unsigned char Poll;
      unsigned char XbusStatus;
      unsigned char StatusLen;
      int  DataLen;
      int  DataPtr;
      unsigned int olddataptr;
      unsigned char CmdPtr;
      unsigned char Status[256];
      unsigned char Data[REQSIZE];
      unsigned char Command[7];
      char STATCYC;
      int Requested;
      MEI_CDROM_Error_Codes MEIStatus;
      DISCStc DISC;
      unsigned int curr_sector;
};

extern cdrom_Device *isodrive;

#endif 
