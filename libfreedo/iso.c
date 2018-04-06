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

// Iso.cpp: implementation of the CIso class.
//
//////////////////////////////////////////////////////////////////////
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>

#include "iso.h"

#pragma pack(pop)

extern unsigned int _3do_DiscSize(void);
extern void _3do_Read2048(void *buff);
extern void _3do_OnSector(unsigned int sector);

void LBA2MSF(struct cdrom_Device *cd)
{
   cd->DISC.templba += 150;
   cd->DISC.tempmsf[0] = cd->DISC.templba/(60*75);
   cd->DISC.templba %= (60*75);
   cd->DISC.tempmsf[1] = cd->DISC.templba/75;
   cd->DISC.tempmsf[2] = cd->DISC.templba%75;
}

void cdrom_Init(struct cdrom_Device *cd)
{
   unsigned int filesize = 150;

   cd->DataPtr=0;
   cd->XbusStatus=0;
   //XBPOLL=POLSTMASK|POLDTMASK|POLMAMASK|POLREMASK;
   cd->Poll=0xf;
   cd->XbusStatus |= CDST_TRAY; //Inject the disc
   cd->XbusStatus |= CDST_RDY;
   cd->XbusStatus |= CDST_DISC;
   cd->XbusStatus |= CDST_SPIN;
   cd->MEIStatus = MEI_CDROM_no_error;

   cd->DISC.firsttrk = 1;
   cd->DISC.lasttrk = 1;
   cd->DISC.curabsmsf[0] = 0;
   cd->DISC.curabsmsf[1] = 2;
   cd->DISC.curabsmsf[2] = 0;

   cd->DISC.DiscTOC[1].CDCTL=CD_CTL_DATA_TRACK|CD_CTL_Q_NONE;//|CD_CTL_COPY_PERMITTED;
   cd->DISC.DiscTOC[1].TRKNUM = 1;
   cd->DISC.DiscTOC[1].mm = 0;
   cd->DISC.DiscTOC[1].ss = 2;
   cd->DISC.DiscTOC[1].ff = 0;

   cd->DISC.firsttrk=1;
   cd->DISC.lasttrk=1;
   cd->DISC.discid = MEI_DISC_DA_OR_CDROM;

   cd->DISC.templba = filesize;
   LBA2MSF(cd);
   cd->DISC.totalmsf[0] = cd->DISC.tempmsf[0];
   cd->DISC.totalmsf[1] = cd->DISC.tempmsf[1];
   cd->DISC.totalmsf[2] = cd->DISC.tempmsf[2];

   cd->DISC.templba = filesize-150;
   LBA2MSF(cd);
   cd->DISC.sesmsf[0] = cd->DISC.tempmsf[0];
   cd->DISC.sesmsf[1] = cd->DISC.tempmsf[1];
   cd->DISC.sesmsf[2] = cd->DISC.tempmsf[2];

   cd->STATCYC = STATDELAY;
}

unsigned int GetStatusFifo(struct cdrom_Device *cd)
{
   unsigned int res = 0;

   if(cd->StatusLen > 0)
   {
      res = cd->Status[0];
      cd->StatusLen--;
      if (cd->StatusLen>0)
         memmove(cd->Status, cd->Status + 1, cd->StatusLen);
      else
         cd->Poll &= ~POLST;
   }
   return res;
}

void MSF2LBA(struct cdrom_Device *cd)
{
   cd->DISC.templba = (cd->DISC.tempmsf[0] * 60 + cd->DISC.tempmsf[1]) * 75 + cd->DISC.tempmsf[2] - 150;
   if (cd->DISC.templba<0)
      cd->DISC.templba=0;
}

void DoCommand(struct cdrom_Device *cd)
{
   int i;

   /*for(i=0;i<=0x100000;i++)
     Data[i]=0;

     DataLen=0;*/
   cd->StatusLen=0;

   cd->Poll &= ~POLST;
   cd->Poll &= ~POLDT;
   cd->XbusStatus &= ~CDST_ERRO;
   cd->XbusStatus &= ~CDST_RDY;

   switch(cd->Command[0])
   {
      case 0x1:
         //seek
         //not used in opera
         //01 00 ll-bb-aa 00 00.
         //01 02 mm-ss-ff 00 00.
         //status 4 bytes
         //xx xx xx XS  (xs=xbus status)
         //		sprintf(str,"#CDROM 0x1 SEEK!!!\n");
         //		CDebug::DPrint(str);
         break;
      case 0x2:
         //spin up
         //opera status request = 0
         //status 4 bytes
         //xx xx xx XS  (xs=xbus status)
         if((cd->XbusStatus & CDST_TRAY) && (cd->XbusStatus & CDST_DISC))
         {
            cd->XbusStatus |= CDST_SPIN;
            cd->XbusStatus |= CDST_RDY;
            cd->MEIStatus = MEI_CDROM_no_error;
         }
         else
         {
            cd->XbusStatus |= CDST_ERRO;
            cd->XbusStatus &= ~CDST_RDY;
            cd->MEIStatus = MEI_CDROM_recv_ecc;

         }

         cd->Poll |= POLST; //status is valid

         cd->StatusLen=2;
         cd->Status[0]=0x2;
         //cd->Status[1]=0x0;
         //cd->Status[2]=0x0;
         cd->Status[1] = cd->XbusStatus;


         break;
      case 0x3:
         // spin down
         //opera status request = 0 // not used in opera
         //status 4 bytes
         //xx xx xx XS  (xs=xbus status)
         if((cd->XbusStatus & CDST_TRAY) && (cd->XbusStatus & CDST_DISC))
         {
            cd->XbusStatus &= ~CDST_SPIN;
            cd->XbusStatus |= CDST_RDY;
            cd->MEIStatus=MEI_CDROM_no_error;

         }
         else
         {
            cd->XbusStatus|=CDST_ERRO;
            cd->XbusStatus|=CDST_RDY;
            cd->MEIStatus=MEI_CDROM_recv_ecc;

         }

         cd->Poll|=POLST; //status is valid

         cd->StatusLen=2;
         cd->Status[0]=0x3;
         //cd->Status[1]=0x0;
         //cd->Status[2]=0x0;
         cd->Status[1] = cd->XbusStatus;

         break;
      case 0x4:
         //diagnostics
         // not used in opera
         //04 00 ll-bb-aa 00 00.
         //04 02 mm-ss-ff 00 00.
         //status 4 bytes
         //xx S1 S2 XS
         //		sprintf(str,"#CDROM 0x4 Diagnostic!!!\n");
         //		CDebug::DPrint(str);

         break;
      case 0x6:
         // eject disc
         //opera status request = 0
         //status 4 bytes
         //xx xx xx XS
         // 1b command of scsi
         //emulation ---
         // Execute EJECT command;
         // Check Sense, update PollRegister (if medium present)
         cd->XbusStatus&=~CDST_TRAY;
         cd->XbusStatus&=~CDST_DISC;
         cd->XbusStatus&=~CDST_SPIN;
         cd->XbusStatus&=~CDST_2X;
         cd->XbusStatus&=~CDST_ERRO;
         cd->XbusStatus|=CDST_RDY;
         cd->Poll|=POLST; //status is valid
         cd->Poll&=~POLMA;
         cd->MEIStatus=MEI_CDROM_no_error;

         cd->StatusLen=2;
         cd->Status[0]=0x6;
         //cd->Status[1]=0x0;
         //cd->Status[2]=0x0;
         cd->Status[1]=cd->XbusStatus;

         /*	ClearCDB();
            CDB[0]=0x1b;
            CDB[4]=0x2;
            CDBLen=12;
            */



         break;
      case 0x7:
         // inject disc
         //opera status request = 0
         //status 4 bytes
         //xx xx xx XS
         //1b command of scsi
         //		sprintf(str,"#CDROM 0x7 INJECT!!!\n");
         //		CDebug::DPrint(str);

         break;
      case 0x8:
         // abort !!!
         //opera status request = 31
         //status 4 bytes
         //xx xx xx XS
         //
         cd->StatusLen=33;
         cd->Status[0]=0x8;
         for(i=1;i<32;i++)
            cd->Status[i]=0;
         cd->Status[32]=cd->XbusStatus;

         cd->XbusStatus|=CDST_RDY;
         cd->MEIStatus=MEI_CDROM_no_error;


         break;
      case 0x9:
         // mode set
         //09 MM nn 00 00 00 00 // 2048 or 2340 transfer size
         // to be checked -- wasn't called even once
         // 2nd byte is type selector
         // MM = mode nn= value
         //opera status request = 0
         //status 4 bytes
         //xx xx xx XS
         // to check!!!

         //	if((XbusStatus&CDST_TRAY) && (XbusStatus&CDST_DISC))
         //	{
         cd->XbusStatus |= CDST_RDY;
         cd->MEIStatus = MEI_CDROM_no_error;

         //CDMode[Command[1]]=Command[2];
         //	}
         //	else
         //	{
         //		XbusStatus|=CDST_ERRO;
         //		XbusStatus&=~CDST_RDY;
         //	}

         cd->Poll |= POLST; //status is valid

         cd->StatusLen=2;
         cd->Status[0]=0x9;
         cd->Status[1] = cd->XbusStatus;
         break;
      case 0x0a:
         // reset
         //not used in opera
         //status 4 bytes
         //xx xx xx XS
         //		sprintf(str,"#CDROM 0xa RESET!!!\n");
         //		CDebug::DPrint(str);
         break;
      case 0x0b:
         // flush
         //opera status request = 31
         //status 4 bytes
         //xx xx xx XS
         //returns data
         //flush all internal buffer
         //1+31+1
         cd->XbusStatus|=CDST_RDY;
         cd->StatusLen=33;
         cd->Status[0]=0xb;
         for(i=1;i<32;i++)
            cd->Status[i]=0;
         cd->Status[32] = cd->XbusStatus;

         //cd->XbusStatus|=CDST_RDY;
         cd->MEIStatus=MEI_CDROM_no_error;

         break;
      case 0x10:
         //Read Data !!!
         //10 01 00 00 00 00 01 // read 0x0 blocks from MSF=1.0.0
         //10 xx-xx-xx nn-nn fl.
         //00 01 02 03 04 05 06
         //reads nn blocks from xx
         //fl=0 xx="lba"
         //fl=1 xx="msf"
         //block= 2048 bytes
         //opera status request = 0
         //status 4 bytes
         //xx xx xx XS
         //returns data
         // here we go


         //olddataptr=DataLen;
         if((cd->XbusStatus & CDST_TRAY) && (cd->XbusStatus & CDST_DISC) && (cd->XbusStatus & CDST_SPIN))
         {
            cd->XbusStatus |= CDST_RDY;
            //cd->CDMode[Command[1]] = cd->Command[2];
            cd->StatusLen = 2;
            cd->Status[0] = 0x10;
            //cd->Status[1]=0x0;
            //cd->Status[2]=0x0;
            cd->Status[1] = cd->XbusStatus;

            //if(cd->Command[6] == Address_Abs_MSF)
            {
               cd->DISC.curabsmsf[0] = (cd->Command[1]);
               cd->DISC.curabsmsf[1] = (cd->Command[2]);
               cd->DISC.curabsmsf[2] = (cd->Command[3]);
               cd->DISC.tempmsf[0] = cd->DISC.curabsmsf[0];
               cd->DISC.tempmsf[1] = cd->DISC.curabsmsf[1];
               cd->DISC.tempmsf[2] = cd->DISC.curabsmsf[2];
               MSF2LBA(cd);


               //if(fiso!=NULL)
               //	fseek(fiso,DISC.templba*2048+iso_off_from_begin,SEEK_SET);
               {
                  cd->curr_sector = cd->DISC.templba;
                  _3do_OnSector(cd->DISC.templba);
               }
               //fseek(fiso,DISC.templba*2048,SEEK_SET);
               //fseek(fiso,DISC.templba*2336,SEEK_SET);
            }





            cd->olddataptr = (cd->Command[5] << 8) + cd->Command[6];
            //cd->olddataptr = cd->olddataptr*2048; //!!!
            cd->Requested = cd->olddataptr;

            if (cd->Requested)
            {
               _3do_OnSector(cd->curr_sector++);
               _3do_Read2048(cd->Data);
               cd->DataLen = REQSIZE;
               cd->Requested--;
            }
            else
               cd->DataLen=0;

            cd->Poll |= POLDT;
            cd->Poll |= POLST;
            cd->MEIStatus = MEI_CDROM_no_error;
         }
         else
         {
            cd->XbusStatus|=CDST_ERRO;
            cd->XbusStatus&=~CDST_RDY;
            cd->Poll|=POLST; //status is valid
            cd->StatusLen=2;
            cd->Status[0]=0x10;
            //cd->Status[1]=0x0;
            //cd->Status[2]=0x0;
            cd->Status[1] = cd->XbusStatus;
            cd->MEIStatus = MEI_CDROM_recv_ecc;

         }


         break;
      case 0x80:
         // data path chech
         //opera status request = 2
         //MKE =2
         // status 4 bytes
         // 80 AA 55 XS
         cd->XbusStatus |= CDST_RDY;
         cd->StatusLen = 4;
         cd->Status[0]=0x80;
         cd->Status[1]=0xaa;
         cd->Status[2]=0x55;
         cd->Status[3] = cd->XbusStatus;
         cd->Poll|=POLST;
         cd->MEIStatus=MEI_CDROM_no_error;


         break;
      case 0x82:
         //read error (get last status???)
         //opera status request = 8 ---- tests status req=9?????
         //MKE = 8!!!
         //00
         //11
         //22   Current Status //MKE / Opera???
         //33
         //44
         //55
         //66
         //77
         //88   Current Status //TEST
         cd->Status[0]=0x82;
         cd->Status[1] = cd->MEIStatus;
         cd->Status[2] = cd->MEIStatus;
         cd->Status[3] = cd->MEIStatus;
         cd->Status[4] = cd->MEIStatus;
         cd->Status[5] = cd->MEIStatus;
         cd->Status[6] = cd->MEIStatus;
         cd->Status[7] = cd->MEIStatus;
         cd->Status[8] = cd->MEIStatus;
         cd->XbusStatus|=CDST_RDY;
         cd->Status[9] = cd->XbusStatus;
         //cd->Status[9] = cd->XbusStatus; // 1 == disc present
         cd->StatusLen=10;
         cd->Poll |= POLST;
         //Poll|=0x80; //MDACC



         break;
      case 0x83:
         //read id
         //opera status request = 10
         //status 12 bytes (3 words)
         //MEI text + XS
         //00 M E I 1 01 00 00 00 00 00 XS
         cd->XbusStatus|=CDST_RDY;
         cd->StatusLen=12;
         cd->Status[0]=0x83;
         cd->Status[1]=0x00;//manufacture id
         cd->Status[2]=0x10;//10
         cd->Status[3]=0x00;//MANUFACTURE NUM
         cd->Status[4]=0x01;//01
         cd->Status[5]=00;
         cd->Status[6]=00;
         cd->Status[7]=0;//REVISION NUMBER:
         cd->Status[8]=0;
         cd->Status[9]=0x00;//FLAG BYTES
         cd->Status[10]=0x00;
         cd->Status[11] = cd->XbusStatus;//DEV.DRIVER SIZE
         //cd->Status[11] = cd->XbusStatus;
         //cd->Status[12] = cd->XbusStatus;
         cd->Poll|=POLST;
         cd->MEIStatus=MEI_CDROM_no_error;

         break;
      case 0x84:
         //mode sense
         //not used in opera
         //84 mm 00 00 00 00 00.
         //status 4 bytes
         //xx S1 S2 XS
         //xx xx nn XS
         //
         cd->StatusLen = 4;
         cd->Status[0] = 0x0;
         cd->Status[1] = 0x0;
         cd->Status[2] = 0x0;

         if((cd->XbusStatus & CDST_TRAY) && (cd->XbusStatus & CDST_DISC))
         {
            cd->XbusStatus |= CDST_RDY;
            //CDMode[Command[1]]=Command[2];
            //cd->Status[2] = CDMode[Command[1]];
         }
         else
         {
            cd->XbusStatus |= CDST_ERRO;
            cd->XbusStatus &= ~CDST_RDY;
         }

         cd->Poll|=POLST; //status is valid

         cd->Status[3] = cd->XbusStatus;
         break;
      case 0x85:
         //read capacity
         //status 8 bytes
         //opera status request = 6
         //cc cc cc cc cc cc cc XS
         //data?
         //00 85
         //11 mm  total
         //22 ss  total
         //33 ff  total
         //44 ??
         //55 ??
         //66 ??
         if((cd->XbusStatus&CDST_TRAY) && (cd->XbusStatus&CDST_DISC)&&(cd->XbusStatus&CDST_SPIN))
         {
            cd->StatusLen=8;//CMD+status+DRVSTAT
            cd->Status[0]=0x85;
            cd->Status[1]=0;
            cd->Status[2]= cd->DISC.totalmsf[0]; //min
            cd->Status[3]= cd->DISC.totalmsf[1]; //sec
            cd->Status[4]= cd->DISC.totalmsf[2]; //fra
            cd->Status[5]=0x00;
            cd->Status[6]=0x00;
            cd->XbusStatus|=CDST_RDY;
            cd->Status[7]= cd->XbusStatus;
            cd->Poll|=POLST;
            cd->MEIStatus=MEI_CDROM_no_error;


         }
         else
         {
            cd->XbusStatus |= CDST_ERRO;
            cd->XbusStatus &= ~CDST_RDY;
            cd->StatusLen = 2;//CMD+status+DRVSTAT
            cd->Status[0] = 0x85;
            cd->Status[1] = cd->XbusStatus;
            cd->Poll |= POLST;
            cd->MEIStatus = MEI_CDROM_recv_ecc;

         }

         break;
      case 0x86:
         //read header
         // not used in opera
         // 86 00 ll-bb-aa 00 00.
         // 86 02 mm-ss-ff 00 00.
         // status 8 bytes
         // data?
         //		sprintf(str,"#CDROM 0x86 READ HEADER!!!\n");
         //		CDebug::DPrint(str);

         break;
      case 0x87:
         //read subq
         //opera status request = 10
         //87 fl 00 00 00 00 00
         //fl=0 "lba"
         //fl=1 "msf"
         //
         //11 00 (if !=00 then break)
         //22 Subq_ctl_adr=swapnibles(_11_)
         //33 Subq_trk = but2bcd(_22_)
         //44 Subq_pnt_idx=byt2bcd(_33_)
         //55 mm run tot
         //66 ss run tot
         //77 ff run tot
         //88 mm run trk
         //99 ss run trk
         //aa ff run trk

         if((cd->XbusStatus & CDST_TRAY) && (cd->XbusStatus & CDST_DISC) && (cd->XbusStatus & CDST_SPIN))
         {
            cd->StatusLen=12;//CMD+status+DRVSTAT
            cd->Status[0]=0x87;
            cd->Status[1]=0;//DISC.totalmsf[0]; //min
            cd->Status[2]=0; //sec
            cd->Status[3]=0; //fra
            cd->Status[4]=0;
            cd->Status[5]=0;
            cd->XbusStatus|=CDST_RDY;
            cd->Status[6]=0x0;
            cd->Status[7]=0x0;
            cd->Status[8]=0x0;
            cd->Status[9]=0x0;
            cd->Status[10]=0x0;
            cd->Status[11] = cd->XbusStatus;
            cd->Poll|=POLST;
            cd->MEIStatus=MEI_CDROM_no_error;
         }
         else
         {
            cd->XbusStatus|=CDST_ERRO;
            cd->XbusStatus&=~CDST_RDY;
            cd->StatusLen=2;//CMD+status+DRVSTAT
            cd->Status[0]=0x85;
            cd->Status[1] = cd->XbusStatus;
            cd->Poll |= POLST;
            cd->MEIStatus=MEI_CDROM_recv_ecc;

         }




         break;
      case 0x88:
         //read upc
         // not used in opera
         //88 00 ll-bb-aa 00 00
         //88 02 mm-ss-ff 00 00
         //status 20(16) bytes
         //data?
         //		sprintf(str,"#CDROM 0x88 READ UPC!!!\n");
         //		CDebug::DPrint(str);

         break;
      case 0x89:
         //read isrc
         // not used in opera
         //89 00 ll-bb-aa 00 00
         //89 02 mm-ss-ff 00 00
         //status 16(15) bytes
         //data?

         //		sprintf(str,"#CDROM 0x89 READ ISRC!!!\n");
         //		CDebug::DPrint(str);

         break;
      case 0x8a:
         //read disc code
         //ignore it yet...
         ////opera status request = 10
         // 8a 00 00 00 00 00 00
         //status 10 bytes
         //????? which code???
         if((cd->XbusStatus & CDST_TRAY) && (cd->XbusStatus & CDST_DISC) && (cd->XbusStatus & CDST_SPIN))
         {
            cd->StatusLen=12;//CMD+status+DRVSTAT
            cd->Status[0]=0x8a;
            cd->Status[1]=0;//DISC.totalmsf[0]; //min
            cd->Status[2]=0; //sec
            cd->Status[3]=0; //fra
            cd->Status[4]=0;
            cd->Status[5]=0;
            cd->XbusStatus|=CDST_RDY;
            cd->Status[6]=0x0;
            cd->Status[7]=0x0;
            cd->Status[8]=0x0;
            cd->Status[9]=0x0;
            cd->Status[10]=0x0;
            cd->Status[11] = cd->XbusStatus;
            cd->Poll|=POLST;
            cd->MEIStatus=MEI_CDROM_no_error;
         }
         else
         {
            cd->XbusStatus|=CDST_ERRO;
            cd->XbusStatus&=~CDST_RDY;
            cd->StatusLen=2;//CMD+status+DRVSTAT
            cd->Status[0] = 0x85;
            cd->Status[1] = cd->XbusStatus;
            cd->Poll |= POLST;
            cd->MEIStatus = MEI_CDROM_recv_ecc;

         }



         break;
      case 0x8b:
         //MKE !!!v the same
         //read disc information
         //opera status request = 6
         //8b 00 00 00 00 00 00
         //status 8(6) bytes
         //read the toc descritor
         //00 11 22 33 44 55 XS
         //00= 8b //command code
         //11= Disc ID /// XA_BYTE
         //22= 1st track#
         //33= last track#
         //44= minutes
         //55= seconds
         //66= frames


         cd->StatusLen = 8;//6+1 + 1 for what?
         cd->Status[0] = 0x8b;
         if(cd->XbusStatus&(CDST_TRAY|CDST_DISC|CDST_SPIN))
         {
            cd->Status[1] = cd->DISC.discid;
            cd->Status[2] = cd->DISC.firsttrk;
            cd->Status[3] = cd->DISC.lasttrk;
            cd->Status[4] = cd->DISC.totalmsf[0]; //minutes
            cd->Status[5] = cd->DISC.totalmsf[1]; //seconds
            cd->XbusStatus |= CDST_RDY;
            cd->Status[6] = cd->DISC.totalmsf[2]; //frames
            cd->MEIStatus = MEI_CDROM_no_error;
            cd->Status[7] = cd->XbusStatus;
         }
         else
         {
            cd->StatusLen=2;//6+1 + 1 for what?
            cd->XbusStatus|=CDST_ERRO;
            cd->MEIStatus=MEI_CDROM_recv_ecc;
            cd->Status[1] = cd->XbusStatus;
         }

         cd->Poll |= POLST; //status is valid

         break;
      case 0x8c:
         //read toc
         //MKE !!!v the same
         //opera status request = 8
         //8c fl nn 00 00 00 00 // reads nn entry
         //status 12(8) bytes
         //00 11 22 33 44 55 66 77 XS
         //00=8c
         //11=reserved0; // NIX BYTE
         //22=addressAndControl; //TOCENT_CTL_ADR=swapnibbles(11) ??? UPCCTLADR=_10_ | x02 (_11_ &F0 = _10_)
         //33=trackNumber;  //TOC_ENT NUMBER
         //44=reserved3;    //TOC_ENT FORMAT
         //55=minutes;     //TOCENT ADRESS == 0x00445566
         //66=seconds;
         //77=frames;
         //88=reserved7;
         cd->StatusLen = 10;//CMD+status+DRVSTAT
         cd->Status[0] = 0x8c;

         if(cd->XbusStatus & (CDST_TRAY|CDST_DISC|CDST_SPIN))
         {
            cd->Status[1] = cd->DISC.DiscTOC[cd->Command[2]].res0;
            cd->Status[2] = cd->DISC.DiscTOC[cd->Command[2]].CDCTL;
            cd->Status[3] = cd->DISC.DiscTOC[cd->Command[2]].TRKNUM;
            cd->Status[4] = cd->DISC.DiscTOC[cd->Command[2]].res1;
            cd->Status[5] = cd->DISC.DiscTOC[cd->Command[2]].mm; //min
            cd->XbusStatus |= CDST_RDY;
            cd->Status[6] = cd->DISC.DiscTOC[cd->Command[2]].ss; //sec
            cd->Status[7] = cd->DISC.DiscTOC[cd->Command[2]].ff; //frames
            cd->Status[8] = cd->DISC.DiscTOC[cd->Command[2]].res2;
            cd->MEIStatus = MEI_CDROM_no_error;
            cd->Status[9] = cd->XbusStatus;

         }
         else
         {
            cd->StatusLen = 2;
            cd->XbusStatus |= CDST_ERRO;
            cd->MEIStatus = MEI_CDROM_recv_ecc;
            cd->Status[1] = cd->XbusStatus;
         }

         cd->Poll |= POLST;
         break;
      case 0x8d:
         //read session information
         //MKE !!!v the same
         //opera status request = 6
         //status 8(6)
         //00 11 22 33 44 55 XS ==
         //00=8d
         //11=valid;  // 0x80 = MULTISESS
         //22=minutes;
         //33=seconds;
         //44=frames;
         //55=rfu1; //ignore
         //66=rfu2  //ignore

         cd->StatusLen = 8;//CMD+status+DRVSTAT
         cd->Status[0] = 0x8d;
         if((cd->XbusStatus & CDST_TRAY) && (cd->XbusStatus & CDST_DISC))
         {
            cd->Status[1] = 0x00;
            cd->Status[2] = 0x0;//DISC.sesmsf[0];//min
            cd->Status[3] = 0x2;//DISC.sesmsf[1];//sec
            cd->Status[4] = 0x0;//DISC.sesmsf[2];//fra
            cd->Status[5] = 0x00;
            cd->XbusStatus |= CDST_RDY;
            cd->Status[6] = 0x00;
            cd->Status[7] = cd->XbusStatus;
            cd->MEIStatus = MEI_CDROM_no_error;

         }
         else
         {
            cd->StatusLen = 2;//CMD+status+DRVSTAT
            cd->XbusStatus |= CDST_ERRO;
            cd->Status[1] = cd->XbusStatus;
            cd->MEIStatus = MEI_CDROM_recv_ecc;
         }

         cd->Poll|=POLST;
         break;
      case 0x8e:
         //read device driver
         break;
      case 0x93:
         //?????
         cd->StatusLen=4;
         cd->Status[0]=0x0;
         cd->Status[1]=0x0;
         cd->Status[2]=0x0;

         if((cd->XbusStatus&CDST_TRAY) && (cd->XbusStatus & CDST_DISC))
         {
            cd->XbusStatus|=CDST_RDY;
            //CDMode[Command[1]]=Command[2];
            //			Status[2]=CDMode[Command[1]];
         }
         else
         {
            cd->XbusStatus|=CDST_ERRO;
            cd->XbusStatus|=CDST_RDY;
         }

         cd->Poll |= POLST; //status is valid

         cd->Status[3] = cd->XbusStatus;

         break;
      default:
         // error!!!
         //sprintf(str,"#CDROM %x!!!\n",Command[0]);
         //CDebug::DPrint(str);
         break;
   }
}

void  SendCommand(struct cdrom_Device *cd, uint8_t val)
{
   if (cd->CmdPtr < 7)
   {
      cd->Command[cd->CmdPtr] = (uint8_t)val;
      cd->CmdPtr++;
   }

   if((cd->CmdPtr >= 7) || (cd->Command[0] == 0x8))
   {
      //Poll&=~0x80; ???
      DoCommand(cd);
      cd->CmdPtr=0;
   }
}

bool TestFIQ(struct cdrom_Device *cd)
{
   if(((cd->Poll & POLST) && (cd->Poll & POLSTMASK)) ||
         ((cd->Poll & POLDT) && (cd->Poll & POLDTMASK)))
      return true;
   return false;
}

void SetPoll(struct cdrom_Device *cd, unsigned int val)
{
   cd->Poll &= 0xF0;
   val &= 0xf;
   cd->Poll |= val;
}

unsigned int GetDataFifo(struct cdrom_Device *cd)
{
   unsigned int res = 0;
   //int i;

   if(cd->DataLen > 0)
   {
      res= (uint8_t)cd->Data[cd->DataPtr];
      cd->DataLen--;
      cd->DataPtr++;

      if (cd->DataLen==0)
      {
         cd->DataPtr=0;

         if(cd->Requested)
         {
            _3do_OnSector(cd->curr_sector++);
            _3do_Read2048(cd->Data);
            cd->Requested--;
            cd->DataLen = REQSIZE;
         }
         else
         {
            cd->Poll &= ~POLDT;
            cd->Requested = 0;
            cd->DataLen = 0;
            cd->DataPtr = 0;
         }

      }
   }

   return res;
}


#define BCD2BIN(in) (((in >> 4) * 10 + (in & 0x0F)))

#define BIN2BCD(in) (((in / 10)<<4) | (in % 10))

void MSF2BLK(struct cdrom_Device *cd)
{
   cd->DISC.tempblk = (cd->DISC.tempmsf[0] * 60 + cd->DISC.tempmsf[1]) * 75 + cd->DISC.tempmsf[2] - 150;
   if (cd->DISC.tempblk<0)
      cd->DISC.tempblk=0; //??
}

void BLK2MSF(struct cdrom_Device *cd)
{
   unsigned int mm;
   cd->DISC.tempmsf[0] = (cd->DISC.tempblk+150) / (60*75);
   mm= (cd->DISC.tempblk + 150) % (60*75);
   cd->DISC.tempmsf[1] = mm / 75;
   cd->DISC.tempmsf[2] = mm % 75;
}


void ClearDataPoll(struct cdrom_Device *cd, unsigned int len)
{
   if((int)len <= cd->DataLen)
   {
      if(cd->DataLen > 0)
      {
         cd->DataLen -= len;
         if (cd->DataLen > 0)
            memmove(cd->Data,cd->Data + 4,len);
         else
            cd->Poll &= ~POLDT;
      }
   }
   else
      cd->Poll &= ~POLDT;
}

bool InitCD(struct cdrom_Device *cd)
{
   unsigned int filesize=0;

   //fseek(fiso,0,SEEK_END);
   cd->curr_sector=0;
   _3do_OnSector(0);
   //filesize=800000000;//ftell(fiso);

   filesize=_3do_DiscSize()+150;


   //sprintf(str,"FILESIZE=0x%x\n",filesize);
   //CDebug::DPrint(str);

   cd->XbusStatus=0;
   //XBPOLL=POLSTMASK|POLDTMASK|POLMAMASK|POLREMASK;
   cd->Poll=0xf;
   cd->XbusStatus|=CDST_TRAY; //Inject the disc
   cd->XbusStatus|=CDST_RDY;
   cd->XbusStatus|=CDST_DISC;
   cd->XbusStatus|=CDST_SPIN;

   cd->MEIStatus=MEI_CDROM_no_error;

   cd->DISC.firsttrk=1;
   cd->DISC.lasttrk=1;
   cd->DISC.curabsmsf[0]=0;
   cd->DISC.curabsmsf[1]=2;
   cd->DISC.curabsmsf[2]=0;

   cd->DISC.DiscTOC[1].CDCTL = CD_CTL_DATA_TRACK|CD_CTL_Q_NONE;//|CD_CTL_COPY_PERMITTED;
   cd->DISC.DiscTOC[1].TRKNUM=1;
   cd->DISC.DiscTOC[1].mm=0;
   cd->DISC.DiscTOC[1].ss=2;
   cd->DISC.DiscTOC[1].ff=0;

   cd->DISC.firsttrk = 1;
   cd->DISC.lasttrk  = 1;
   cd->DISC.discid   = MEI_DISC_DA_OR_CDROM;

   cd->DISC.templba=filesize;
   LBA2MSF(cd);
   cd->DISC.totalmsf[0] = cd->DISC.tempmsf[0];
   cd->DISC.totalmsf[1] = cd->DISC.tempmsf[1];
   cd->DISC.totalmsf[2] = cd->DISC.tempmsf[2];

   //sprintf(str,"##ISO M=0x%x  S=0x%x  F=0x%x\n",DISC.totalmsf[0],DISC.totalmsf[1],DISC.totalmsf[2]);
   //CDebug::DPrint(str);

   cd->DISC.templba = filesize-150;
   LBA2MSF(cd);
   cd->DISC.sesmsf[0] = cd->DISC.tempmsf[0];
   cd->DISC.sesmsf[1] = cd->DISC.tempmsf[1];
   cd->DISC.sesmsf[2] = cd->DISC.tempmsf[2];
   return false;
}

unsigned int GedWord(struct cdrom_Device *cd)
{
   unsigned int res = 0;

   if(cd->DataLen > 0)
   {
      //res=(uint8_t)Data[0];
      //res
      res=(cd->Data[0]<<24) + (cd->Data[1]<<16) + (cd->Data[2]<<8) + cd->Data[3];
      if(cd->DataLen<3)
      {
         cd->DataLen--;
         if(cd->DataLen>0)
            memmove(cd->Data,cd->Data+1,cd->DataLen);
         else
         {
            cd->Poll &= ~POLDT;
            cd->DataLen=0;
         }
         cd->DataLen--;
         if(cd->DataLen>0)
            memmove(cd->Data,cd->Data+1,cd->DataLen);
         else
         {
            cd->Poll&=~POLDT;
            cd->DataLen=0;
         }
         cd->DataLen--;
         if(cd->DataLen > 0)
            memmove(cd->Data,cd->Data+1,cd->DataLen);
         else
         {
            cd->Poll&=~POLDT;
            cd->DataLen=0;
         }
      }
      else
      {
         //DataLen-=4;
         {
            memmove(cd->Data,cd->Data+4,cd->DataLen-4);
            cd->DataLen-=4;
         }

         if(cd->DataLen<=0)
         {
            cd->DataLen=0;
            cd->Poll &= ~POLDT;
         }
      }

   }

   return res;
}

//plugins----------------------------------------------------------------------------------
struct cdrom_Device *isodrive;

void* _xbplug_MainDevice(int proc, void* data)
{
   uint32_t tmp;

   switch(proc)
   {
      case XBP_INIT:
         isodrive = (struct cdrom_Device*)calloc(1, sizeof(*isodrive));
         cdrom_Init(isodrive);
         return (void*)true;
      case XBP_RESET:
         cdrom_Init(isodrive);
         if(_3do_DiscSize())
            InitCD(isodrive);
         break;
      case XBP_SET_COMMAND:
         SendCommand(isodrive, (intptr_t)data);
         break;
      case XBP_FIQ:
         return (void*)TestFIQ(isodrive);
      case XBP_GET_DATA:
         return (void*)(uintptr_t)GetDataFifo(isodrive);
      case XBP_GET_STATUS:
         return (void*)(uintptr_t)GetStatusFifo(isodrive);
      case XBP_SET_POLL:
         SetPoll(isodrive, (intptr_t)data);
         break;
      case XBP_GET_POLL:
         return (void*)(uintptr_t)isodrive->Poll;
      case XBP_DESTROY:
         break;
      case XBP_GET_SAVESIZE:
         tmp = sizeof(struct cdrom_Device);
         return (void*)(uintptr_t)tmp;
      case XBP_GET_SAVEDATA:
         memcpy(data, isodrive, sizeof(struct cdrom_Device));
         break;
      case XBP_SET_SAVEDATA:
         memcpy(isodrive, data, sizeof(struct cdrom_Device));
         return (void*)1;
   };

   return NULL;
}
