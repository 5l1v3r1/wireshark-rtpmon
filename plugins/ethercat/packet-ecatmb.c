/* packet-ecatmb.c
 * Routines for ethercat packet disassembly
 *
 * $Id$
 *
 * Copyright (c) 2007 by Beckhoff Automation GmbH
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/* Include files */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <string.h>

#include <glib.h>

#include <epan/packet.h>
#include <epan/addr_resolv.h>
#include <epan/prefs.h>
#include <epan/strutil.h>
#include <epan/emem.h>

#include "packet-ecatmb.h"

void proto_reg_handoff_ecat_mailbox(void);


#define BIT2BYTE(x) ((x+7)/8)
#define ENDOF(p) ((p)+1) /* pointer to end of *p */

static dissector_handle_t eth_handle;
static dissector_handle_t ams_handle;

/* Define the EtherCAT mailbox proto */
int proto_ecat_mailbox  = -1;

static int ett_ecat_mailbox = -1;
static int ett_ecat_mailbox_eoe = -1;
static int ett_ecat_mailbox_eoe_init = -1;
static int ett_ecat_mailbox_eoe_macfilter = -1;
static int ett_ecat_mailbox_eoe_macfilter_filter = -1;
static int ett_ecat_mailbox_eoe_macfilter_filtermask = -1;
static int ett_ecat_mailbox_coe = -1;
static int ett_ecat_mailbox_sdo = -1;
static int ett_ecat_mailbox_coe_sdoccs = -1;
static int ett_ecat_mailbox_coe_sdoscs = -1;
static int ett_ecat_mailbox_foe = -1;
static int ett_ecat_mailbox_foe_efw = -1;
static int ett_ecat_mailbox_soeopmode = -1;
static int ett_ecat_mailbox_soeflag = -1;
static int ett_ecat_mailbox_soe = -1;
static int ett_ecat_mailbox_fraghead = -1;

static int hf_ecat_mailbox = -1;
static int hf_ecat_mailboxlength = -1;
static int hf_ecat_mailboxaddress = -1;
static int hf_ecat_mailboxtype = -1;
static int hf_ecat_mailboxcounter = -1;
static int hf_ecat_mailbox_eoe = -1;
static int hf_ecat_mailbox_eoe_fraghead = -1;
static int hf_ecat_mailbox_eoe_type = -1;
static int hf_ecat_mailbox_eoe_fragno = -1;
static int hf_ecat_mailbox_eoe_offset = -1;
static int hf_ecat_mailbox_eoe_frame = -1;
static int hf_ecat_mailbox_eoe_last = -1;
static int hf_ecat_mailbox_eoe_timestampreq = -1;
static int hf_ecat_mailbox_eoe_timestampapp = -1;
static int hf_ecat_mailbox_eoe_fragment = -1;
static int hf_ecat_mailbox_eoe_init = -1;
static int hf_ecat_mailbox_eoe_init_contains_macaddr = -1;
static int hf_ecat_mailbox_eoe_init_contains_ipaddr = -1;
static int hf_ecat_mailbox_eoe_init_contains_subnetmask = -1;
static int hf_ecat_mailbox_eoe_init_contains_defaultgateway = -1;
static int hf_ecat_mailbox_eoe_init_contains_dnsserver = -1;
static int hf_ecat_mailbox_eoe_init_contains_dnsname = -1;
static int hf_ecat_mailbox_eoe_init_append_timestamp = -1;
static int hf_ecat_mailbox_eoe_init_macaddr = -1;
static int hf_ecat_mailbox_eoe_init_ipaddr = -1;
static int hf_ecat_mailbox_eoe_init_subnetmask = -1;
static int hf_ecat_mailbox_eoe_init_defaultgateway = -1;
static int hf_ecat_mailbox_eoe_init_dnsserver = -1;
static int hf_ecat_mailbox_eoe_init_dnsname = -1;
static int hf_ecat_mailbox_eoe_macfilter = -1;
static int hf_ecat_mailbox_eoe_macfilter_macfiltercount = -1;
static int hf_ecat_mailbox_eoe_macfilter_maskcount = -1;
static int hf_ecat_mailbox_eoe_macfilter_nobroadcasts = -1;
static int hf_ecat_mailbox_eoe_macfilter_filter;
static int hf_ecat_mailbox_eoe_macfilter_filters[16] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static int hf_ecat_mailbox_eoe_macfilter_filtermask = -1;
static int hf_ecat_mailbox_eoe_macfilter_filtermasks[4] = {-1,-1,-1,-1};
static int hf_ecat_mailbox_eoe_timestamp = -1;
static int hf_ecat_mailbox_coe = -1;
static int hf_ecat_mailbox_coe_number = -1;
static int hf_ecat_mailbox_coe_type = -1;
static int hf_ecat_mailbox_coe_sdoreq = -1;
static int hf_ecat_mailbox_coe_sdoccsid = -1;
static int hf_ecat_mailbox_coe_sdoccsid_sizeind = -1;
static int hf_ecat_mailbox_coe_sdoccsid_expedited = -1;
static int hf_ecat_mailbox_coe_sdoccsid_size0= -1;
static int hf_ecat_mailbox_coe_sdoccsid_size1= -1;
static int hf_ecat_mailbox_coe_sdoccsid_complete = -1;
static int hf_ecat_mailbox_coe_sdoccsds = -1;
static int hf_ecat_mailbox_coe_sdoccsds_lastseg = -1;
static int hf_ecat_mailbox_coe_sdoccsds_size = -1;
static int hf_ecat_mailbox_coe_sdoccsds_toggle = -1;
static int hf_ecat_mailbox_coe_sdoccsus = -1;
static int hf_ecat_mailbox_coe_sdoccsus_toggle = -1;
static int hf_ecat_mailbox_coe_sdoccsiu = -1;
static int hf_ecat_mailbox_coe_sdoccsiu_complete = -1;
static int hf_ecat_mailbox_coe_sdoidx = -1;
static int hf_ecat_mailbox_coe_sdosub = -1;
static int hf_ecat_mailbox_coe_sdodata = -1;
static int hf_ecat_mailbox_coe_sdodata1 = -1;
static int hf_ecat_mailbox_coe_sdodata2 = -1;
static int hf_ecat_mailbox_coe_sdoldata = -1;
static int hf_ecat_mailbox_coe_sdolength = -1;
static int hf_ecat_mailbox_coe_sdoerror = -1;
static int hf_ecat_mailbox_coe_sdores = -1;
static int hf_ecat_mailbox_coe_sdoscsds = -1;
static int hf_ecat_mailbox_coe_sdoscsds_toggle = -1;
static int hf_ecat_mailbox_coe_sdoscsiu = -1;
static int hf_ecat_mailbox_coe_sdoscsiu_sizeind = -1;
static int hf_ecat_mailbox_coe_sdoscsiu_expedited = -1;
static int hf_ecat_mailbox_coe_sdoscsiu_size0 = -1;
static int hf_ecat_mailbox_coe_sdoscsiu_size1 = -1;
static int hf_ecat_mailbox_coe_sdoscsiu_complete = -1;
static int hf_ecat_mailbox_coe_sdoscsus = -1;
static int hf_ecat_mailbox_coe_sdoscsus_lastseg = -1;
static int hf_ecat_mailbox_coe_sdoscsus_bytes = -1;
static int hf_ecat_mailbox_coe_sdoscsus_toggle = -1;
static int hf_ecat_mailbox_coe_sdoinfoopcode = -1;
static int hf_ecat_mailbox_coe_sdoinfofrag = -1;
static int hf_ecat_mailbox_coe_sdoinfolisttype = -1;
static int hf_ecat_mailbox_coe_sdoinfolist = -1;
static int hf_ecat_mailbox_coe_sdoinfoindex = -1;
static int hf_ecat_mailbox_coe_sdoinfosubindex = -1;
static int hf_ecat_mailbox_coe_sdoinfovalueinfo = -1;
static int hf_ecat_mailbox_coe_sdoinfoerrorcode = -1;
static int hf_ecat_mailbox_coe_sdoinfodatatype = -1;
static int hf_ecat_mailbox_coe_sdoinfomaxsub = -1;
static int hf_ecat_mailbox_coe_sdoinfoobjcode = -1;
static int hf_ecat_mailbox_coe_sdoinfoname = -1;
static int hf_ecat_mailbox_coe_sdoinfobitlen = -1;
static int hf_ecat_mailbox_coe_sdoinfoobjaccess = -1;
static int hf_ecat_mailbox_coe_sdoinfounittype = -1;
static int hf_ecat_mailbox_coe_sdoinfodefaultvalue = -1;
static int hf_ecat_mailbox_coe_sdoinfominvalue = -1;
static int hf_ecat_mailbox_coe_sdoinfomaxvalue = -1;
static int hf_ecat_mailboxdata = -1;
static int hf_ecat_mailbox_foe = -1;
static int hf_ecat_mailbox_foe_opmode = -1;
static int hf_ecat_mailbox_foe_filelength = -1;
static int hf_ecat_mailbox_foe_filename = -1;
static int hf_ecat_mailbox_foe_packetno = -1;
static int hf_ecat_mailbox_foe_errcode = -1;
static int hf_ecat_mailbox_foe_errtext = -1;
static int hf_ecat_mailbox_foe_busydone = -1;
static int hf_ecat_mailbox_foe_busyentire = -1;
static int hf_ecat_mailbox_foe_data = -1;
static int hf_ecat_mailbox_foe_efw = -1;
static int hf_ecat_mailbox_foe_efw_cmd = -1;
static int hf_ecat_mailbox_foe_efw_size = -1;
static int hf_ecat_mailbox_foe_efw_addresslw = -1;
static int hf_ecat_mailbox_foe_efw_addresshw = -1;
static int hf_ecat_mailbox_foe_efw_data = -1;
static int hf_ecat_mailbox_soe = -1;
static int hf_ecat_mailbox_soe_header = -1;

static int hf_ecat_mailbox_soe_header_opcode = -1;
static int hf_ecat_mailbox_soe_header_incomplete = -1;
static int hf_ecat_mailbox_soe_header_error = -1;
static int hf_ecat_mailbox_soe_header_driveno = -1;
static int hf_ecat_mailbox_soe_header_datastate = -1;
static int hf_ecat_mailbox_soe_header_name = -1;
static int hf_ecat_mailbox_soe_header_attribute = -1;
static int hf_ecat_mailbox_soe_header_unit = -1;
static int hf_ecat_mailbox_soe_header_min = -1;
static int hf_ecat_mailbox_soe_header_max = -1;
static int hf_ecat_mailbox_soe_header_value = -1;
static int hf_ecat_mailbox_soe_header_reserved = -1;
static int hf_ecat_mailbox_soe_idn = -1;
static int hf_ecat_mailbox_soe_data = -1;
static int hf_ecat_mailbox_soe_frag = -1;
static int hf_ecat_mailbox_soe_error = -1;

static const value_string EcMBoxType[] =
{
   {   0, "Invalid", },
   {   1, "AoE", },
   {   2, "EoE", },
   {   3, "CoE", },
   {   4, "FoE", },
   {   5, "SoE", },
   {   0x80+1, "AoE - Err", },
   {   0x80+2, "EoE - Err", },
   {   0x80+3, "CoE - Err", },
   {   0x80+4, "FoE - Err", },
   {   0x80+5, "SoE - Err", },
   {   0, NULL }
};

static const value_string FoEOpMode[] =
{
   {   1, "RRQ", },
   {   2, "WRQ", },
   {   3, "DATA", },
   {   4, "ACK", },
   {   5, "ERROR", },
   {   6, "BUSY", },
   {   0,  NULL }
};

static const value_string FoEEfwCmd[] =
{
   {   1, "Memory Transfer", },
   {   2, "Write Code", },
   {   3, "Check device id", },
   {   4, "Checksum", },
   {   5, "Write code checksum", },
   {   6, "Set device id", },
   {   8, "Set code id", },
   {   9, "NOP", },
   {  10, "Checksum checksum", },
   {  11, "boot checksum", },
   { 0, NULL }
};

static const value_string SoeOpcode[] =
{
   {   0, "unused" },
   {   1, "readReq" },
   {   2, "readRes"},
   {   3, "writeReq"},
   {   4, "writeRes" },
   {   5, "notification" },
   {   6, "emergency"},
   {   0, NULL }
};

static const true_false_string tfs_complete =
{
   "Complete", "Legacy"
};

void init_mbx_header(PETHERCAT_MBOX_HEADER pMbox, tvbuff_t *tvb, gint offset)
{
   pMbox->Length = tvb_get_letohs(tvb, offset); offset+=sizeof(guint16);
   pMbox->Address = tvb_get_letohs(tvb, offset); offset+=sizeof(guint16);
   pMbox->aControlUnion.Control = tvb_get_letohs(tvb, offset);
}

void init_eoe_header(PETHERCAT_EOE_HEADER pEoE, tvbuff_t *tvb, gint offset)
{
   pEoE->anEoeHeaderInfoUnion.Info = tvb_get_letohs(tvb, offset); offset+=sizeof(guint16);
   pEoE->anEoeHeaderDataUnion.Result = tvb_get_letohs(tvb, offset);
}

void init_foe_header(PETHERCAT_FOE_HEADER pFoE, tvbuff_t *tvb, gint offset)
{
   pFoE->OpMode = tvb_get_guint8(tvb, offset++);
   pFoE->Reserved1 = tvb_get_guint8(tvb, offset++);
   pFoE->aFoeHeaderDataUnion.FileLength = tvb_get_letohl(tvb, offset);
}

void init_soe_header(PETHERCAT_SOE_HEADER pSoE, tvbuff_t *tvb, gint offset)
{
   pSoE->anSoeHeaderControlUnion.Control = tvb_get_guint8(tvb, offset++);
   pSoE->anSoeHeaderControlUnion.Element = tvb_get_guint8(tvb, offset++);
   pSoE->anSoeHeaderDataUnion.FragmentsLeft = tvb_get_letohs(tvb, offset);
}

void init_coe_header(PETHERCAT_COE_HEADER pCoE, tvbuff_t *tvb, gint offset)
{
   pCoE->header = tvb_get_letohs(tvb, offset);
}

void init_sdo_header(PETHERCAT_SDO_HEADER pSdo, tvbuff_t *tvb, gint offset)
{
   pSdo->anSdoHeaderUnion.CS = tvb_get_guint8(tvb, offset++);
   pSdo->Index = tvb_get_letohs(tvb, offset);offset+=sizeof(guint16);
   pSdo->SubIndex = tvb_get_guint8(tvb, offset++);
   pSdo->Data = tvb_get_letohl(tvb, offset);
}

void init_sdo_info_header(PETHERCAT_SDO_INFO_HEADER pInfo, tvbuff_t *tvb, gint offset)
{
   pInfo->anSdoControlUnion.Control = tvb_get_guint8(tvb, offset++);
   pInfo->Reserved = tvb_get_guint8(tvb, offset++);
   pInfo->FragmentsLeft = sizeof(guint16);
}


static void MailboxTypeFormater(PETHERCAT_MBOX_HEADER pMbx, char *szText, gint nMax)
{
   guint32 i;

   for(i = 0; i<sizeof(EcMBoxType)/sizeof(value_string); i++ )
   {
      if( EcMBoxType[i].value == pMbx->aControlUnion.Type )
      {
         g_snprintf(szText, nMax, "Type    : %s (0x%x)", EcMBoxType[i].strptr, pMbx->aControlUnion.Type);
         return;
      }
   }
   g_snprintf ( szText, nMax,"Type    : %d", pMbx->aControlUnion.Type);
}

static void EoETypeFormater(PETHERCAT_EOE_HEADER pEoE, char *szText, gint nMax)
{
   switch (pEoE->anEoeHeaderInfoUnion.Type)
   {
   case EOE_TYPE_FRAME_FRAG:
      g_snprintf ( szText, nMax, "Type(%d)    : Fragment", pEoE->anEoeHeaderInfoUnion.Type);
      break;
   case EOE_TYPE_TIMESTAMP_RES:
      g_snprintf ( szText, nMax, "Type(%d)    : TimeStamp", pEoE->anEoeHeaderInfoUnion.Type);
      break;
   case EOE_TYPE_INIT_REQ:
      g_snprintf ( szText, nMax, "Type(%d)    : Init Req", pEoE->anEoeHeaderInfoUnion.Type);
      break;
   case EOE_TYPE_INIT_RES:
      g_snprintf ( szText, nMax, "Type(%d)    : Init Res", pEoE->anEoeHeaderInfoUnion.Type);
      break;
   case EOE_TYPE_MACFILTER_REQ:
      g_snprintf ( szText, nMax, "Type(%d)    : MAC Req", pEoE->anEoeHeaderInfoUnion.Type);
      break;
   case EOE_TYPE_MACFILTER_RES:
      g_snprintf ( szText, nMax, "Type(%d)    : MAC Res", pEoE->anEoeHeaderInfoUnion.Type);
      break;
   default:
      g_snprintf ( szText, nMax, "Type(%d)    : Unknown", pEoE->anEoeHeaderInfoUnion.Type);
      break;
   }
}

static void EoEFragNoFormater(PETHERCAT_EOE_HEADER pEoE, char *szText, gint nMax)
{
   g_snprintf ( szText, nMax, "FragNo     : %d", pEoE->anEoeHeaderDataUnion.Fragment);
}

static void EoEOffsetFormater(PETHERCAT_EOE_HEADER pEoE, char *szText, gint nMax)
{
   if ( pEoE->anEoeHeaderDataUnion.Fragment == 0 )
      g_snprintf ( szText, nMax, "BufferSize : %d", 32*pEoE->anEoeHeaderDataUnion.OffsetBuffer);
   else
      g_snprintf ( szText, nMax, "Offset     : %d", 32*pEoE->anEoeHeaderDataUnion.OffsetBuffer);
}

static void EoEFrameFormater(PETHERCAT_EOE_HEADER pEoE, char *szText, gint nMax)
{
   g_snprintf ( szText, nMax, "FrameNo    : %d", pEoE->anEoeHeaderDataUnion.FrameNo);
}

static void EoELastFormater(PETHERCAT_EOE_HEADER pEoE, char *szText, gint nMax)
{
   if ( pEoE->anEoeHeaderInfoUnion.LastFragment != 0 )
      g_snprintf ( szText, nMax, "Last Frag");
   else
      g_snprintf ( szText, nMax, "More Frags...");
}

static void CANopenNumberFormater(PETHERCAT_COE_HEADER pCoE, char *szText, gint nMax)
{
   g_snprintf( szText, nMax, "Number  : %d", pCoE->Number);
}

static void CANopenTypeFormater(PETHERCAT_COE_HEADER pCoE, char *szText, gint nMax)
{
   switch ( pCoE->Type)
   {
   case ETHERCAT_COE_TYPE_EMERGENCY:
      g_snprintf ( szText, nMax, "Type    : EMERGENCY(%d)", pCoE->Type);
      break;
   case ETHERCAT_COE_TYPE_SDOREQ:
      g_snprintf ( szText, nMax, "Type    : SDO Req(%d)", pCoE->Type);
      break;
   case ETHERCAT_COE_TYPE_SDORES:
      g_snprintf ( szText, nMax, "Type    : SDO Res(%d)", pCoE->Type);
      break;
   case ETHERCAT_COE_TYPE_TXPDO:
      g_snprintf ( szText, nMax, "Type    : TxPDO(%d)", pCoE->Type);
      break;
   case ETHERCAT_COE_TYPE_RXPDO:
      g_snprintf ( szText, nMax, "Type    : RxPDO(%d)", pCoE->Type);
      break;
   case ETHERCAT_COE_TYPE_TXPDO_RTR:
      g_snprintf ( szText, nMax, "Type    : TxPDO_RTR(%d)", pCoE->Type);
      break;
   case ETHERCAT_COE_TYPE_RXPDO_RTR:
      g_snprintf ( szText, nMax, "Type    : RxPDO_RTR(%d)", pCoE->Type);
      break;
   default:
      g_snprintf ( szText, nMax, "Type    :%d", pCoE->Type);
   }
}

static void CANopenSdoReqFormater(PETHERCAT_SDO_HEADER pSdo, char *szText, gint nMax)
{
   switch ( pSdo->anSdoHeaderUnion.Idq.Ccs )
   {
   case SDO_CCS_INITIATE_DOWNLOAD:
      g_snprintf ( szText, nMax, "SDO Req : 'Initiate Download' (%d) Idx=0x%x Sub=%d", pSdo->anSdoHeaderUnion.Idq.Ccs, pSdo->Index,  pSdo->SubIndex);
      break;
   case SDO_CCS_INITIATE_UPLOAD:
      g_snprintf ( szText, nMax, "SDO Req : 'Initiate Upload' (%d) Idx=0x%x Sub=%d", pSdo->anSdoHeaderUnion.Idq.Ccs, pSdo->Index,  pSdo->SubIndex);
      break;
   case SDO_CCS_DOWNLOAD_SEGMENT:
      g_snprintf ( szText, nMax, "SDO Req : 'Download Segment' (%d)", pSdo->anSdoHeaderUnion.Idq.Ccs);
      break;
   case SDO_CCS_UPLOAD_SEGMENT:
      g_snprintf ( szText, nMax, "SDO Req : 'Upload Segment' (%d)", pSdo->anSdoHeaderUnion.Idq.Ccs);
      break;
   case SDO_CCS_ABORT_TRANSFER:
      g_snprintf ( szText, nMax, "SDO Req : 'Abort Transfer' (%d)", pSdo->anSdoHeaderUnion.Idq.Ccs);
      break;
   default:
      g_snprintf ( szText, nMax, "SDO Req : Ccs %d", pSdo->anSdoHeaderUnion.Idq.Ccs);
   }
}

static void CANopenSdoResFormater(PETHERCAT_SDO_HEADER pSdo, char *szText, gint nMax)
{
   g_snprintf ( szText, nMax, "SDO Res : Scs %d", pSdo->anSdoHeaderUnion.Ids.Scs);
}

static void CANopenSdoInfoFormater(PETHERCAT_SDO_INFO_HEADER pHead, char *szText, gint nMax)
{
   guint8 opCode = pHead->anSdoControlUnion.OpCode&0x7F;
   char* txt2 = "";
   if ( (pHead->anSdoControlUnion.OpCode&0x80) != 0 )
      txt2 = " - More Follows";
   switch (opCode)
   {
   case ECAT_COE_INFO_OPCODE_LIST_Q:
      g_snprintf ( szText, nMax, "CoE SDO Info, OpCode: 'List Req' %s", txt2);
      break;
   case ECAT_COE_INFO_OPCODE_LIST_S:
      g_snprintf ( szText, nMax, "CoE SDO Info, OpCode: 'List Res' %s", txt2);
      break;
   case ECAT_COE_INFO_OPCODE_OBJ_Q:
      g_snprintf ( szText, nMax, "CoE SDO Info, OpCode: 'Obj Req' %s", txt2);
      break;
   case ECAT_COE_INFO_OPCODE_OBJ_S:
      g_snprintf ( szText, nMax, "CoE SDO Info, OpCode: 'Obj Res' %s", txt2);
      break;
   case ECAT_COE_INFO_OPCODE_ENTRY_Q:
      g_snprintf ( szText, nMax, "CoE SDO Info, OpCode: 'Entry Req' %s", txt2);
      break;
   case ECAT_COE_INFO_OPCODE_ENTRY_S:
      g_snprintf ( szText, nMax, "CoE SDO Info, OpCode: 'Entry Res' %s", txt2);
      break;
   case ECAT_COE_INFO_OPCODE_ERROR_S:
      g_snprintf ( szText, nMax, "CoE SDO Info, OpCode: 'Error Res' %s", txt2);
      break;
   default:
      g_snprintf ( szText, nMax, "CoE SDO Info, OpCode: %d %s", opCode, txt2);
   }
}

static void FoeFormater(tvbuff_t *tvb, gint offset, char *szText, gint nMax, guint foe_length)
{
   ETHERCAT_FOE_HEADER foe;
   char tmp[50];
   memset(tmp, 0, sizeof(tmp));

   init_foe_header(&foe, tvb, offset);

   switch ( foe.OpMode )
   {
   case ECAT_FOE_OPMODE_RRQ:
   case ECAT_FOE_OPMODE_WRQ:
   case ECAT_FOE_OPMODE_ERR:
      if ( foe_length > ETHERCAT_FOE_HEADER_LEN )
         tvb_memcpy(tvb, tmp, offset+ETHERCAT_FOE_HEADER_LEN, MIN(foe_length-ETHERCAT_FOE_HEADER_LEN, sizeof(tmp)-1));
      break;
   }

   switch ( foe.OpMode )
   {
   case ECAT_FOE_OPMODE_RRQ:
      g_snprintf ( szText, nMax, "FoE RRQ (%d) : '%s'", foe.aFoeHeaderDataUnion.FileLength, tmp);
      break;
   case ECAT_FOE_OPMODE_WRQ:
      g_snprintf ( szText, nMax, "FoE WRQ (%d) : '%s'", foe.aFoeHeaderDataUnion.FileLength, tmp);
      break;
   case ECAT_FOE_OPMODE_DATA:
      g_snprintf ( szText, nMax, "FoE DATA (%d) : %d Bytes", foe.aFoeHeaderDataUnion.PacketNo, foe_length-ETHERCAT_FOE_HEADER_LEN);
      break;
   case ECAT_FOE_OPMODE_ACK:
      g_snprintf ( szText, nMax, "FoE ACK (%d)", foe.aFoeHeaderDataUnion.PacketNo);
      break;
   case ECAT_FOE_OPMODE_ERR:
      g_snprintf ( szText, nMax, "FoE ERR (%d) : '%s'", foe.aFoeHeaderDataUnion.ErrorCode, tmp);
      break;
   case ECAT_FOE_OPMODE_BUSY:
      if ( foe.aFoeHeaderDataUnion.Entire > 0 )
         g_snprintf ( szText, nMax, "FoE BUSY (%d%%)", ((guint32)foe.aFoeHeaderDataUnion.Done*100)/foe.aFoeHeaderDataUnion.Entire);
      else
         g_snprintf ( szText, nMax, "FoE BUSY (%d/%d)", foe.aFoeHeaderDataUnion.Done, foe.aFoeHeaderDataUnion.Entire);
      break;
   }
}

static void SoEIdToString( char* txt, guint16 id, int nMax)
{
   if ( id & 0x8000 )
      g_snprintf(txt, nMax, "P-%d-%04d", (id>>12) & 0x0007, id & 0x0FFF );
   else
      g_snprintf(txt, nMax, "S-%d-%04d", id>>12, id & 0x0FFF );
}

static void SoeFormater(tvbuff_t *tvb, gint offset, char *szText, gint nMax, guint soe_length)
{
   ETHERCAT_SOE_HEADER soe;
   char tmp[50];
   char elm[50];
   memset(tmp, 0, sizeof(tmp));

   init_soe_header(&soe, tvb, offset);
   offset+=ETHERCAT_SOE_HEADER_LEN;

   if ( !soe.anSoeHeaderControlUnion.Error )
   {
      if ( !soe.anSoeHeaderControlUnion.InComplete )
      {
         SoEIdToString(tmp, soe.anSoeHeaderDataUnion.IDN, sizeof(tmp)-1);
         elm[0] = 0;
         if ( soe.anSoeHeaderControlUnion.DataState )
            strcat(elm, "D");
         if ( soe.anSoeHeaderControlUnion.Name )
            strcat(elm, "N");
         if ( soe.anSoeHeaderControlUnion.Attribute )
            strcat(elm, "A");
         if ( soe.anSoeHeaderControlUnion.Unit )
            strcat(elm, "U");
         if ( soe.anSoeHeaderControlUnion.Min )
            strcat(elm, "I");
         if ( soe.anSoeHeaderControlUnion.Max )
            strcat(elm, "X");
         if ( soe.anSoeHeaderControlUnion.Value )
            strcat(elm, "V");
         switch ( soe.anSoeHeaderControlUnion.OpCode )
         {
         case ECAT_SOE_OPCODE_RRQ:
            g_snprintf ( szText, nMax, "SoE: RRQ (%s, '%s')", tmp, elm);
            break;
         case ECAT_SOE_OPCODE_RRS:
            g_snprintf ( szText, nMax, "SoE: RRS (%s, '%s') : %u Bytes", tmp, elm, (guint)(soe_length-ETHERCAT_SOE_HEADER_LEN));
            break;
         case ECAT_SOE_OPCODE_WRS:
            g_snprintf ( szText, nMax, "SoE: WRS (%s, '%s')", tmp, elm);
            break;
         case ECAT_SOE_OPCODE_WRQ:
            g_snprintf ( szText, nMax, "SoE: WRQ (%s, '%s') : %u Bytes", tmp, elm, (guint)(soe_length-ETHERCAT_SOE_HEADER_LEN));
            break;
         case ECAT_SOE_OPCODE_NFC:
            g_snprintf ( szText, nMax, "SoE: NFC (%s, '%s') : %u Bytes", tmp, elm, (guint)(soe_length-ETHERCAT_SOE_HEADER_LEN));
            break;
         case 6:
            g_snprintf ( szText, nMax, "SoE: EMGCY");
         default:
            g_snprintf ( szText, nMax, "SoE:");
         }
      }
      else
         g_snprintf ( szText, nMax, "SoE: FragmentsLeft %d", soe.anSoeHeaderDataUnion.FragmentsLeft);
   }
   else
      g_snprintf ( szText, nMax, "SoE: Error %04x", tvb_get_letohs(tvb, offset));
}


/*ethercat mailbox*/
static void dissect_ecat_coe(tvbuff_t *tvb, gint offset, packet_info *pinfo, proto_tree *tree)
{
   proto_tree *ecat_coe_tree = NULL, *ecat_sdo_tree, *ecat_coe_sdoccs_tree, *ecat_coe_sdoscs_tree;

   proto_item *aitem = NULL, *aparent = NULL;
   char szText[200];
   int nMax = sizeof(szText)-1;

   guint coe_length = tvb_reported_length(tvb)-offset;
   guint16 len;

   if( tree )
   {
      aitem = proto_tree_add_item(tree, hf_ecat_mailbox_coe, tvb, offset, coe_length, TRUE);
      proto_item_set_text(aitem,"CoE");
      aparent = proto_item_get_parent(aitem);
      proto_item_append_text(aparent,":CoE ");
   }

   if (check_col(pinfo->cinfo, COL_INFO))
      col_append_str(pinfo->cinfo, COL_INFO, "CoE ");

   if( coe_length >= ETHERCAT_COE_HEADER_LEN )
   {
      ETHERCAT_COE_HEADER coe;
      init_coe_header(&coe, tvb, offset);
      if( tree )
      {
         ecat_coe_tree = proto_item_add_subtree(aitem, ett_ecat_mailbox_coe);

         CANopenNumberFormater(&coe, szText, nMax);
         aitem = proto_tree_add_uint(ecat_coe_tree, hf_ecat_mailbox_coe_number, tvb, offset, ETHERCAT_COE_HEADER_LEN, coe.Number);
         proto_item_set_text(aitem, szText);

         CANopenTypeFormater(&coe, szText, nMax);
         aitem = proto_tree_add_uint(ecat_coe_tree, hf_ecat_mailbox_coe_type, tvb, offset, ETHERCAT_COE_HEADER_LEN, coe.Type);
         proto_item_set_text(aitem, szText);
      }

      offset += ETHERCAT_COE_HEADER_LEN;

      switch (coe.Type)
      {
      case ETHERCAT_COE_TYPE_SDOREQ:
         {
            ETHERCAT_SDO_HEADER sdo;

            if( coe_length < ETHERCAT_COE_HEADER_LEN + ETHERCAT_SDO_HEADER_LEN )
            {
               if (check_col(pinfo->cinfo, COL_INFO))
                  col_append_str(pinfo->cinfo, COL_INFO, "Sdo Req - invalid length");
               break;
            }

            init_sdo_header(&sdo, tvb, offset);

            CANopenSdoReqFormater(&sdo, szText, nMax);
            if (check_col(pinfo->cinfo, COL_INFO))
               col_append_str(pinfo->cinfo, COL_INFO, szText);

            if( tree )
            {
               proto_item_append_text(aparent, szText);

               aitem = proto_tree_add_uint(ecat_coe_tree, hf_ecat_mailbox_coe_sdoreq, tvb, offset, 1, sdo.anSdoHeaderUnion.Idq.Ccs);
               proto_item_set_text(aitem, szText);
               ecat_sdo_tree = proto_item_add_subtree(aitem, ett_ecat_mailbox_sdo);

               switch ( sdo.anSdoHeaderUnion.Idq.Ccs )
               {
               case SDO_CCS_INITIATE_DOWNLOAD:
                  aitem = proto_tree_add_item(ecat_sdo_tree, hf_ecat_mailbox_coe_sdoccsid, tvb, offset, 1, TRUE);
                  ecat_coe_sdoccs_tree = proto_item_add_subtree(aitem, ett_ecat_mailbox_coe_sdoccs);
                  aitem = proto_tree_add_item(ecat_coe_sdoccs_tree, hf_ecat_mailbox_coe_sdoccsid_sizeind, tvb, offset, 1, TRUE);
                  aitem = proto_tree_add_item(ecat_coe_sdoccs_tree, hf_ecat_mailbox_coe_sdoccsid_expedited, tvb, offset, 1, TRUE);
                  aitem = proto_tree_add_item(ecat_coe_sdoccs_tree, hf_ecat_mailbox_coe_sdoccsid_size0, tvb, offset, 1, TRUE);
                  aitem = proto_tree_add_item(ecat_coe_sdoccs_tree, hf_ecat_mailbox_coe_sdoccsid_size1, tvb, offset, 1, TRUE);
                  aitem = proto_tree_add_item(ecat_coe_sdoccs_tree, hf_ecat_mailbox_coe_sdoccsid_complete, tvb, offset, 1, TRUE);

                  aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoidx, tvb, offset+1, 2, TRUE);
                  aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdosub, tvb, offset+3, 1, TRUE);
                  if ( sdo.anSdoHeaderUnion.Idq.SizeInd && !sdo.anSdoHeaderUnion.Idq.Expedited )
                  {
                     len = coe_length - ETHERCAT_COE_HEADER_LEN - ETHERCAT_SDO_HEADER_LEN;
                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdolength, tvb, offset+4, 4, TRUE);
                     offset+=ETHERCAT_SDO_HEADER_LEN;
                     if ( len > 0 )
                        aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoldata, tvb, offset, len, TRUE);
                  }
                  else
                  {
                     if ( sdo.anSdoHeaderUnion.Idq.Size == 3 )
                        aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdodata1, tvb, offset+4, 1, TRUE);
                     else if ( sdo.anSdoHeaderUnion.Idq.Size == 2 )
                        aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdodata2, tvb, offset+4, 2, TRUE);
                     else
                        aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdodata, tvb, offset+4, 4, TRUE);
                  }
                  break;
               case SDO_CCS_INITIATE_UPLOAD:
                  aitem = proto_tree_add_item(ecat_sdo_tree, hf_ecat_mailbox_coe_sdoccsiu, tvb, offset, 1, TRUE);
                  ecat_coe_sdoccs_tree = proto_item_add_subtree(aitem, ett_ecat_mailbox_coe_sdoccs);
                  aitem = proto_tree_add_item(ecat_coe_sdoccs_tree, hf_ecat_mailbox_coe_sdoccsid_complete, tvb, offset, 1, TRUE);

                  aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoidx, tvb, offset+1, 2, TRUE);
                  aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdosub, tvb, offset+3, 1, TRUE);

                  break;
               case SDO_CCS_DOWNLOAD_SEGMENT:
                  aitem = proto_tree_add_item(ecat_sdo_tree, hf_ecat_mailbox_coe_sdoccsds, tvb, offset, 1, TRUE);
                  ecat_coe_sdoccs_tree = proto_item_add_subtree(aitem, ett_ecat_mailbox_coe_sdoccs);
                  aitem = proto_tree_add_item(ecat_coe_sdoccs_tree, hf_ecat_mailbox_coe_sdoccsds_lastseg, tvb, offset, 1, TRUE);
                  aitem = proto_tree_add_item(ecat_coe_sdoccs_tree, hf_ecat_mailbox_coe_sdoccsds_size, tvb, offset, 1, TRUE);
                  aitem = proto_tree_add_item(ecat_coe_sdoccs_tree, hf_ecat_mailbox_coe_sdoccsds_toggle, tvb, offset, 1, TRUE);
                  offset+=1;

                  if ( coe_length-offset > 0 )
                  {
                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoldata, tvb, offset, coe_length-offset, TRUE);
                     proto_item_append_text(aitem, "(len = %d)", coe_length-offset);
                  }
                  break;
               case SDO_CCS_UPLOAD_SEGMENT:
                  aitem = proto_tree_add_item(ecat_sdo_tree, hf_ecat_mailbox_coe_sdoccsus, tvb, offset, 1, TRUE);
                  ecat_coe_sdoccs_tree = proto_item_add_subtree(aitem, ett_ecat_mailbox_coe_sdoccs);
                  aitem = proto_tree_add_item(ecat_coe_sdoccs_tree, hf_ecat_mailbox_coe_sdoccsus_toggle, tvb, offset, 1, TRUE);
                  break;
               case SDO_CCS_ABORT_TRANSFER:
                  aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoidx, tvb, offset+4, 4, TRUE);
                  break;
               }
            }
         }
         break;

      case ETHERCAT_COE_TYPE_SDORES:
         {
            ETHERCAT_SDO_HEADER sdo;
            if( coe_length < ETHERCAT_COE_HEADER_LEN + ETHERCAT_SDO_HEADER_LEN )
            {
               if (check_col(pinfo->cinfo, COL_INFO))
                  col_append_str(pinfo->cinfo, COL_INFO, "Sdo Res - invalid length");
               break;
            }

            init_sdo_header(&sdo, tvb, offset);

            CANopenSdoResFormater(&sdo, szText, nMax);
            if (check_col(pinfo->cinfo, COL_INFO))
               col_append_str(pinfo->cinfo, COL_INFO, szText);

            if( tree )
            {
               aitem = proto_tree_add_uint(ecat_coe_tree, hf_ecat_mailbox_coe_sdores, tvb, offset, 1, sdo.anSdoHeaderUnion.Ids.Scs);
               proto_item_set_text(aitem, szText);
               ecat_sdo_tree = proto_item_add_subtree(aitem, ett_ecat_mailbox_sdo);

               switch ( sdo.anSdoHeaderUnion.Ids.Scs )
               {
               case SDO_SCS_INITIATE_DOWNLOAD:
                  aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoidx, tvb, offset+1, 2, TRUE);
                  aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdosub, tvb, offset+3, 1, TRUE);
                  break;
               case SDO_SCS_INITIATE_UPLOAD:
                  aitem = proto_tree_add_item(ecat_sdo_tree, hf_ecat_mailbox_coe_sdoscsiu, tvb, offset, 1, TRUE);
                  ecat_coe_sdoscs_tree = proto_item_add_subtree(aitem, ett_ecat_mailbox_coe_sdoscs);
                  aitem = proto_tree_add_item(ecat_coe_sdoscs_tree, hf_ecat_mailbox_coe_sdoscsiu_sizeind, tvb, offset, 1, TRUE);
                  aitem = proto_tree_add_item(ecat_coe_sdoscs_tree, hf_ecat_mailbox_coe_sdoscsiu_expedited, tvb, offset, 1, TRUE);
                  aitem = proto_tree_add_item(ecat_coe_sdoscs_tree, hf_ecat_mailbox_coe_sdoscsiu_size0, tvb, offset, 1, TRUE);
                  aitem = proto_tree_add_item(ecat_coe_sdoscs_tree, hf_ecat_mailbox_coe_sdoscsiu_size1, tvb, offset, 1, TRUE);
                  aitem = proto_tree_add_item(ecat_coe_sdoscs_tree, hf_ecat_mailbox_coe_sdoscsiu_complete, tvb, offset, 1, TRUE);

                  aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoidx, tvb, offset+1, 2, TRUE);
                  aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdosub, tvb, offset+3, 1, TRUE);
                  if ( sdo.anSdoHeaderUnion.Ius.SizeInd && !sdo.anSdoHeaderUnion.Ius.Expedited )
                  {
                     len = coe_length - ETHERCAT_COE_HEADER_LEN - ETHERCAT_SDO_HEADER_LEN;
                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdolength, tvb, offset+4, 4, TRUE);
                     offset+=ETHERCAT_SDO_HEADER_LEN;
                     if ( len > 0 )
                        aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoldata, tvb, offset, len, TRUE);
                  }
                  else if ( sdo.anSdoHeaderUnion.Ius.SizeInd && sdo.anSdoHeaderUnion.Ius.Expedited && sdo.anSdoHeaderUnion.Ius.Size == 3 )
                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdodata1, tvb, offset+4, 1, TRUE);
                  else if ( sdo.anSdoHeaderUnion.Ius.SizeInd && sdo.anSdoHeaderUnion.Ius.Expedited && sdo.anSdoHeaderUnion.Ius.Size == 2 )
                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdodata2, tvb, offset+4, 2, TRUE);
                  else
                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdodata, tvb, offset+4, 4, TRUE);
                  break;
               case SDO_SCS_DOWNLOAD_SEGMENT:
                  aitem = proto_tree_add_item(ecat_sdo_tree, hf_ecat_mailbox_coe_sdoscsds, tvb, offset, 1, TRUE);
                  ecat_coe_sdoscs_tree = proto_item_add_subtree(aitem, ett_ecat_mailbox_coe_sdoscs);
                  aitem = proto_tree_add_item(ecat_coe_sdoscs_tree, hf_ecat_mailbox_coe_sdoscsds_toggle, tvb, offset, 1, TRUE);
                  break;
               case SDO_SCS_UPLOAD_SEGMENT:
                  aitem = proto_tree_add_item(ecat_sdo_tree, hf_ecat_mailbox_coe_sdoscsus, tvb, offset, 1, TRUE);
                  ecat_coe_sdoscs_tree = proto_item_add_subtree(aitem, ett_ecat_mailbox_coe_sdoscs);
                  aitem = proto_tree_add_item(ecat_coe_sdoscs_tree, hf_ecat_mailbox_coe_sdoscsus_lastseg, tvb, offset, 1, TRUE);
                  aitem = proto_tree_add_item(ecat_coe_sdoscs_tree, hf_ecat_mailbox_coe_sdoscsus_bytes, tvb, offset, 1, TRUE);
                  aitem = proto_tree_add_item(ecat_coe_sdoscs_tree, hf_ecat_mailbox_coe_sdoscsus_toggle, tvb, offset, 1, TRUE);
                  offset+=1;

                  if ( coe_length-offset> 0 )
                  {
                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoldata, tvb, offset, coe_length-offset, TRUE);
                     proto_item_append_text(aitem, "(len = %d)", coe_length-offset);
                  }
                  break;
               }
            }
         }
         break;

      case ETHERCAT_COE_TYPE_SDOINFO:
         {
            ETHERCAT_SDO_INFO_HEADER info;

            if( coe_length < ETHERCAT_COE_HEADER_LEN + ETHERCAT_SDO_INFO_LISTREQ_LEN )
            {
               if (check_col(pinfo->cinfo, COL_INFO))
                  col_append_str(pinfo->cinfo, COL_INFO, "Sdo Info - invalid length");
               break;
            }

            init_sdo_info_header(&info, tvb, offset);

            CANopenSdoInfoFormater(&info, szText, nMax);
            if (check_col(pinfo->cinfo, COL_INFO))
               col_append_str(pinfo->cinfo, COL_INFO, szText);

            if( tree )
            {
               aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfoopcode, tvb, offset++, 1, TRUE);
               offset++; /*Reserved*/

               aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfofrag, tvb, offset, 2, TRUE);
               offset+=2;

               switch ( info.anSdoControlUnion.OpCode )
               {
               case ECAT_COE_INFO_OPCODE_LIST_Q:
                  {
                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfolisttype, tvb, offset, 2, TRUE);
                  }
                  break;
               case ECAT_COE_INFO_OPCODE_LIST_S:
                  {
                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfolisttype, tvb, offset, 2, TRUE);
                     offset+=2;

                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfolist, tvb, offset, coe_length-offset, TRUE);
                  }
                  break;
               case ECAT_COE_INFO_OPCODE_OBJ_Q:
                  aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfoindex, tvb, offset, 2, TRUE);
                  break;
               case ECAT_COE_INFO_OPCODE_OBJ_S:
                  {
                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfoindex, tvb, offset, 2, TRUE);
                     offset+=2;

                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfodatatype, tvb, offset, 2, TRUE);
                     offset+=2;

                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfomaxsub, tvb, offset++, 1, TRUE);
                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfoobjcode, tvb, offset++, 1, TRUE);

                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfoname, tvb, offset, coe_length-offset, TRUE);
                  }
                  break;
               case ECAT_COE_INFO_OPCODE_ENTRY_Q:
                  {
                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfoindex, tvb, offset, 2, TRUE);
                     offset+=2;

                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfosubindex, tvb, offset++, 1, TRUE);
                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfovalueinfo, tvb, offset, 1, TRUE);
                  }
                  break;
               case ECAT_COE_INFO_OPCODE_ENTRY_S:
                  {
                     guint16 objlen;

                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfoindex, tvb, offset, 2, TRUE);
                     offset+=2;

                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfosubindex, tvb, offset++, 1, TRUE);
                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfovalueinfo, tvb, offset++, 1, TRUE);

                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfodatatype, tvb, offset, 2, TRUE);
                     offset+=2;

                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfobitlen, tvb, offset, 2, TRUE);
                     offset+=2;

                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfoobjaccess, tvb, offset, 2, TRUE);
                     offset+=2;

                     if ( (info.anSdoInfoUnion.Entry.ValueInfo & 0x08) != 0 )
                     {
                        aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfounittype, tvb, offset, 2, TRUE);
                        offset+=2;
                     }
                     if ( (info.anSdoInfoUnion.Entry.ValueInfo & 0x10) != 0 )
                     {
                        objlen = BIT2BYTE(info.anSdoInfoUnion.Entry.Res.BitLen);
                        aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfodefaultvalue, tvb, offset, objlen, TRUE);
                        offset+=objlen;
                     }
                     if ( (info.anSdoInfoUnion.Entry.ValueInfo & 0x20) != 0 )
                     {
                        objlen = BIT2BYTE(info.anSdoInfoUnion.Entry.Res.BitLen);
                        aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfominvalue, tvb, offset, objlen, TRUE);
                        offset+=objlen;
                     }
                     if ( (info.anSdoInfoUnion.Entry.ValueInfo & 0x40) != 0 )
                     {
                        objlen = BIT2BYTE(info.anSdoInfoUnion.Entry.Res.BitLen);
                        aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfomaxvalue, tvb, offset, objlen, TRUE);
                        offset+=objlen;
                     }
                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfoname, tvb, offset, coe_length-offset, TRUE);
                  }
                  break;
               case ECAT_COE_INFO_OPCODE_ERROR_S:
                  {
                     aitem = proto_tree_add_item(ecat_coe_tree, hf_ecat_mailbox_coe_sdoinfoerrorcode, tvb, offset, 4, TRUE);
                  }
                  break;
               }
            }
         }
         break;
      }
   }
   else
   {
      if (check_col(pinfo->cinfo, COL_INFO))
         col_append_str(pinfo->cinfo, COL_INFO, "- invalid length");
   }
}

static void dissect_ecat_soe(tvbuff_t *tvb, gint offset, packet_info *pinfo, proto_tree *tree)
{
   proto_tree *ecat_soeflag_tree, *ecat_soe_tree;

   proto_item *aitem = NULL ,*aparent = NULL;
   char szText[200];
   int nMax = sizeof(szText)-1;

   guint soe_length = tvb_reported_length(tvb)-offset;

   if( tree )
   {
      aitem = proto_tree_add_item(tree, hf_ecat_mailbox_soe, tvb, offset, soe_length, TRUE);

      aparent = proto_item_get_parent(aitem);
      proto_item_append_text(aparent,":SoE ");
   }

   if( soe_length >= ETHERCAT_SOE_HEADER_LEN )
   {
      SoeFormater(tvb, offset, szText, nMax, soe_length);
      if (check_col(pinfo->cinfo, COL_INFO))
         col_append_str(pinfo->cinfo, COL_INFO, szText);

      if( tree )
      {
         ETHERCAT_SOE_HEADER soe;
         init_soe_header(&soe, tvb, offset);

         proto_item_append_text(aparent, szText);
         proto_item_set_text(aitem, szText);

         ecat_soe_tree = proto_item_add_subtree(aitem, ett_ecat_mailbox_soe);
         aitem = proto_tree_add_item(ecat_soe_tree, hf_ecat_mailbox_soe_header, tvb, offset , 2, TRUE);

         ecat_soeflag_tree = proto_item_add_subtree(aitem, ett_ecat_mailbox_soeflag);
         aitem = proto_tree_add_item(ecat_soeflag_tree, hf_ecat_mailbox_soe_header_opcode, tvb, offset, 2, TRUE);
         aitem = proto_tree_add_item(ecat_soeflag_tree, hf_ecat_mailbox_soe_header_incomplete, tvb, offset, 2, TRUE);
         aitem = proto_tree_add_item(ecat_soeflag_tree, hf_ecat_mailbox_soe_header_error, tvb, offset, 2, TRUE);
         aitem = proto_tree_add_item(ecat_soeflag_tree, hf_ecat_mailbox_soe_header_driveno, tvb, offset, 2, TRUE);
         aitem = proto_tree_add_item(ecat_soeflag_tree, hf_ecat_mailbox_soe_header_datastate, tvb, offset, 2, TRUE);
         aitem = proto_tree_add_item(ecat_soeflag_tree, hf_ecat_mailbox_soe_header_name, tvb, offset, 2, TRUE);
         aitem = proto_tree_add_item(ecat_soeflag_tree, hf_ecat_mailbox_soe_header_attribute, tvb, offset, 2, TRUE);
         aitem = proto_tree_add_item(ecat_soeflag_tree, hf_ecat_mailbox_soe_header_unit, tvb, offset, 2, TRUE);
         aitem = proto_tree_add_item(ecat_soeflag_tree, hf_ecat_mailbox_soe_header_min, tvb, offset, 2, TRUE);
         aitem = proto_tree_add_item(ecat_soeflag_tree, hf_ecat_mailbox_soe_header_max, tvb, offset, 2, TRUE);
         aitem = proto_tree_add_item(ecat_soeflag_tree, hf_ecat_mailbox_soe_header_value, tvb, offset, 2, TRUE);
         aitem = proto_tree_add_item(ecat_soeflag_tree, hf_ecat_mailbox_soe_header_reserved, tvb, offset, 2, TRUE);
         offset+=2;

         if ( !soe.anSoeHeaderControlUnion.Error )
         {
            if ( !soe.anSoeHeaderControlUnion.InComplete )
            {
               switch (soe.anSoeHeaderControlUnion.OpCode)
               {
               case ECAT_SOE_OPCODE_RRQ:
               case ECAT_SOE_OPCODE_WRS:
                  aitem = proto_tree_add_item(ecat_soe_tree, hf_ecat_mailbox_soe_idn, tvb, offset, 2, TRUE);
                  break;
               case ECAT_SOE_OPCODE_RRS:
               case ECAT_SOE_OPCODE_WRQ:
               case ECAT_SOE_OPCODE_NFC:
                  aitem = proto_tree_add_item(ecat_soe_tree, hf_ecat_mailbox_soe_idn, tvb, offset, 2, TRUE);
                  offset+=2;
                  aitem = proto_tree_add_item(tree, hf_ecat_mailbox_soe_data, tvb, offset, soe_length-offset, TRUE);
                  break;
               }
            }
            else
            {
               aitem = proto_tree_add_item(ecat_soe_tree, hf_ecat_mailbox_soe_frag, tvb, offset, 2, TRUE);
               offset+=2;

               aitem = proto_tree_add_item(tree, hf_ecat_mailbox_soe_data, tvb, offset, soe_length-offset, TRUE);
            }
         }
         else
         {
            aitem = proto_tree_add_item(ecat_soe_tree, hf_ecat_mailbox_soe_idn, tvb, offset, 2, TRUE);
            aitem = proto_tree_add_item(tree, hf_ecat_mailbox_soe_error, tvb, offset, 2, TRUE);
         }
      }
   }
   else
   {
      if (check_col(pinfo->cinfo, COL_INFO))
         col_append_str(pinfo->cinfo, COL_INFO, "SoE - invalid length");
   }
}

static void dissect_ecat_eoe(tvbuff_t *tvb, gint offset, packet_info *pinfo, proto_tree *tree)
{
   proto_tree *ecat_eoe_tree = 0, *ecat_fraghead_tree, *ecat_eoe_init_tree, *ecat_eoe_macfilter_tree,
      *ecat_eoe_macfilter_filter_tree, *ecat_eoe_macfilter_filtermask_tree;
   tvbuff_t *next_tvb;
   proto_item *aitem = NULL, *aparent = NULL;
   char szText[200];
   int nMax = sizeof(szText)-1;
   int nCnt;

   guint eoe_length = tvb_reported_length(tvb)-offset;

   if( tree )
   {
      aitem = proto_tree_add_item(tree, hf_ecat_mailbox_eoe, tvb, offset, eoe_length, TRUE);
      proto_item_set_text(aitem, "EoE Fragment");

      aparent = proto_item_get_parent(aitem);
      proto_item_append_text(aparent,":EoE ");
   }

   if( eoe_length >= ETHERCAT_EOE_HEADER_LEN )
   {
      ETHERCAT_EOE_HEADER eoe;
      init_eoe_header(&eoe, tvb, offset);
      if (check_col(pinfo->cinfo, COL_INFO))
      {
         if ( eoe.anEoeHeaderInfoUnion.Type == EOE_TYPE_FRAME_FRAG )
            g_snprintf ( szText, nMax, "EoE-Frag %d", eoe.anEoeHeaderDataUnion.Fragment);
         else
            g_snprintf ( szText, nMax, "EoE");
         col_append_str(pinfo->cinfo, COL_INFO, szText);
      }

      if( tree )
      {
         ecat_eoe_tree = proto_item_add_subtree(aitem, ett_ecat_mailbox_eoe);

         aitem = proto_tree_add_item(ecat_eoe_tree, hf_ecat_mailbox_eoe_fraghead, tvb, offset, 4, TRUE);
         proto_item_set_text(aitem, "Header");
         ecat_fraghead_tree = proto_item_add_subtree(aitem, ett_ecat_mailbox_fraghead);

         aitem = proto_tree_add_uint(ecat_fraghead_tree, hf_ecat_mailbox_eoe_type, tvb, offset, 4, eoe.anEoeHeaderInfoUnion.Type);
         EoETypeFormater(&eoe, szText, nMax);
         proto_item_set_text(aitem,szText);

         switch ( eoe.anEoeHeaderInfoUnion.Type )
         {
         case EOE_TYPE_FRAME_FRAG:
            aitem = proto_tree_add_uint(ecat_fraghead_tree, hf_ecat_mailbox_eoe_fragno, tvb, offset, 4, eoe.anEoeHeaderDataUnion.Fragment);
            EoEFragNoFormater(&eoe, szText, nMax);
            proto_item_set_text(aitem,szText);

            aitem = proto_tree_add_uint(ecat_fraghead_tree, hf_ecat_mailbox_eoe_offset, tvb, offset, 4, 32*eoe.anEoeHeaderDataUnion.OffsetBuffer);
            EoEOffsetFormater(&eoe, szText, nMax);
            proto_item_set_text(aitem,szText);

            aitem = proto_tree_add_uint(ecat_fraghead_tree, hf_ecat_mailbox_eoe_frame, tvb, offset, 4, eoe.anEoeHeaderDataUnion.FrameNo);
            EoEFrameFormater(&eoe, szText, nMax);
            proto_item_set_text(aitem,szText);

            aitem = proto_tree_add_uint(ecat_fraghead_tree, hf_ecat_mailbox_eoe_last, tvb, offset, 4, eoe.anEoeHeaderInfoUnion.LastFragment);
            EoELastFormater(&eoe, szText, nMax);
            proto_item_set_text(aitem,szText);

            if ( eoe.anEoeHeaderInfoUnion.TimeStampRequested )
            {
               aitem = proto_tree_add_uint(ecat_fraghead_tree, hf_ecat_mailbox_eoe_timestampreq, tvb, offset, 4, eoe.anEoeHeaderInfoUnion.TimeStampRequested);
               proto_item_set_text(aitem, "Time Stamp Requested");
            }

            if ( eoe.anEoeHeaderInfoUnion.TimeStampAppended )
            {
               aitem = proto_tree_add_uint(ecat_fraghead_tree, hf_ecat_mailbox_eoe_timestampapp, tvb, offset, 4, eoe.anEoeHeaderInfoUnion.TimeStampAppended);
               proto_item_set_text(aitem, "Time Stamp Appended");
            }

            offset+=ETHERCAT_EOE_HEADER_LEN;
            aitem = proto_tree_add_item(ecat_eoe_tree, hf_ecat_mailbox_eoe_fragment, tvb, offset, eoe_length-offset, TRUE);

            if ( eoe.anEoeHeaderDataUnion.Fragment == 0 )
            {
               next_tvb = tvb_new_subset(tvb, offset, eoe_length-offset, eoe_length-offset);
               call_dissector( eth_handle, next_tvb, pinfo, ecat_eoe_tree);
            }

            if ( eoe.anEoeHeaderInfoUnion.TimeStampAppended )
            {
               aitem = proto_tree_add_item(ecat_eoe_tree, hf_ecat_mailbox_eoe_timestamp, tvb, eoe_length-ETHERCAT_EOE_TIMESTAMP_LEN, ETHERCAT_EOE_TIMESTAMP_LEN, TRUE);
            }
            break;

         case EOE_TYPE_TIMESTAMP_RES:
            aitem = proto_tree_add_item(ecat_eoe_tree, hf_ecat_mailbox_eoe_timestamp, tvb, offset+ETHERCAT_EOE_HEADER_LEN, ETHERCAT_EOE_TIMESTAMP_LEN, TRUE);
            break;

         case EOE_TYPE_INIT_REQ:
            offset+=ETHERCAT_EOE_HEADER_LEN;
            aitem = proto_tree_add_item(ecat_fraghead_tree, hf_ecat_mailbox_eoe_init, tvb, offset, MIN(eoe_length-offset,ETHERCAT_EOE_INIT_LEN), TRUE);
            if( eoe_length-offset >= ETHERCAT_EOE_INIT_LEN )
            {
               ecat_eoe_init_tree = proto_item_add_subtree(aitem, ett_ecat_mailbox_eoe_init);

               aitem = proto_tree_add_item(ecat_eoe_init_tree, hf_ecat_mailbox_eoe_init_contains_macaddr, tvb, offset, 4, TRUE);
               aitem = proto_tree_add_item(ecat_eoe_init_tree, hf_ecat_mailbox_eoe_init_contains_ipaddr, tvb, offset, 4, TRUE);
               aitem = proto_tree_add_item(ecat_eoe_init_tree, hf_ecat_mailbox_eoe_init_contains_subnetmask, tvb, offset, 4, TRUE);
               aitem = proto_tree_add_item(ecat_eoe_init_tree, hf_ecat_mailbox_eoe_init_contains_defaultgateway, tvb, offset, 4, TRUE);
               aitem = proto_tree_add_item(ecat_eoe_init_tree, hf_ecat_mailbox_eoe_init_contains_dnsserver, tvb, offset, 4, TRUE);
               aitem = proto_tree_add_item(ecat_eoe_init_tree, hf_ecat_mailbox_eoe_init_contains_dnsname, tvb, offset, 4, TRUE);
               aitem = proto_tree_add_item(ecat_eoe_init_tree, hf_ecat_mailbox_eoe_init_append_timestamp, tvb, offset, 4, TRUE);
               offset+=4;

               aitem = proto_tree_add_item(ecat_eoe_init_tree, hf_ecat_mailbox_eoe_init_macaddr, tvb, offset, ETHERNET_ADDRESS_LEN, TRUE);
               offset+=ETHERNET_ADDRESS_LEN;

               aitem = proto_tree_add_item(ecat_eoe_init_tree, hf_ecat_mailbox_eoe_init_ipaddr, tvb, offset, 4, TRUE);
               offset+=4;

               aitem = proto_tree_add_item(ecat_eoe_init_tree, hf_ecat_mailbox_eoe_init_subnetmask, tvb, offset, 4, TRUE);
               offset+=4;

               aitem = proto_tree_add_item(ecat_eoe_init_tree, hf_ecat_mailbox_eoe_init_defaultgateway, tvb, offset, 4, TRUE);
               offset+=4;

               aitem = proto_tree_add_item(ecat_eoe_init_tree, hf_ecat_mailbox_eoe_init_dnsserver, tvb, offset, 4, TRUE);
               offset+=4;

               aitem = proto_tree_add_item(ecat_eoe_init_tree, hf_ecat_mailbox_eoe_init_dnsname, tvb, offset, 32, TRUE);
            }
            else
               proto_item_append_text(aitem, " - Invalid length!");
            break;

         case EOE_TYPE_MACFILTER_REQ:
            {
               EoeMacFilterOptionsUnion options;
               offset+=ETHERCAT_EOE_HEADER_LEN;
               aitem = proto_tree_add_item(ecat_fraghead_tree, hf_ecat_mailbox_eoe_macfilter, tvb, offset, MIN(eoe_length-offset, ETHERCAT_EOE_MACFILTER_LEN), TRUE);
               if( eoe_length-offset >= ETHERCAT_EOE_MACFILTER_LEN )
               {
                  ecat_eoe_macfilter_tree = proto_item_add_subtree(aitem, ett_ecat_mailbox_eoe_macfilter);
                  aitem = proto_tree_add_item(ecat_eoe_macfilter_tree, hf_ecat_mailbox_eoe_macfilter_macfiltercount, tvb, offset, 4, TRUE);
                  aitem = proto_tree_add_item(ecat_eoe_macfilter_tree, hf_ecat_mailbox_eoe_macfilter_maskcount, tvb, offset, 4, TRUE);
                  aitem = proto_tree_add_item(ecat_eoe_macfilter_tree, hf_ecat_mailbox_eoe_macfilter_nobroadcasts, tvb, offset, 4, TRUE);
                  options.Options = tvb_get_letohs(tvb, offset);
                  offset+=4;

                  aitem = proto_tree_add_item(ecat_eoe_macfilter_tree, hf_ecat_mailbox_eoe_macfilter_filter, tvb, offset, 16*ETHERNET_ADDRESS_LEN, TRUE);
                  ecat_eoe_macfilter_filter_tree = proto_item_add_subtree(aitem, ett_ecat_mailbox_eoe_macfilter_filter);
                  for( nCnt=0; nCnt<options.MacFilterCount; nCnt++)
                     aitem = proto_tree_add_item(ecat_eoe_macfilter_filter_tree, hf_ecat_mailbox_eoe_macfilter_filters[nCnt], tvb, offset+nCnt*ETHERNET_ADDRESS_LEN, ETHERNET_ADDRESS_LEN, TRUE);
                  offset+=16*ETHERNET_ADDRESS_LEN;

                  aitem = proto_tree_add_item(ecat_eoe_macfilter_tree, hf_ecat_mailbox_eoe_macfilter_filtermask, tvb, offset, 4*sizeof(guint32), TRUE);
                  ecat_eoe_macfilter_filtermask_tree = proto_item_add_subtree(aitem, ett_ecat_mailbox_eoe_macfilter_filtermask);
                  for( nCnt=0; nCnt<options.MacFilterMaskCount; nCnt++)
                     aitem = proto_tree_add_item(ecat_eoe_macfilter_tree, hf_ecat_mailbox_eoe_macfilter_filtermasks[nCnt], tvb, offset+nCnt*sizeof(guint32), sizeof(guint32), TRUE);
               }
               else
                  proto_item_append_text(aitem, " - Invalid length!");
            }
            break;

         case EOE_TYPE_INIT_RES:
         case EOE_TYPE_MACFILTER_RES:
            break;
         }
      }

      if (check_col(pinfo->cinfo, COL_INFO))
         col_prepend_fstr(pinfo->cinfo, COL_INFO, "EoE(");

      if (check_col(pinfo->cinfo, COL_PROTOCOL))
         col_prepend_fstr(pinfo->cinfo, COL_PROTOCOL, "EoE-");
   }
   else
   {
      if (check_col(pinfo->cinfo, COL_INFO))
         col_append_str(pinfo->cinfo, COL_INFO, "EoE - invalid length!");
   }
}

static void dissect_ecat_foe(tvbuff_t *tvb, gint offset, packet_info *pinfo, proto_tree *tree)
{
   proto_tree *ecat_foe_tree,*ecat_foe_efw_tree;

   proto_item *aitem= NULL,*aparent = NULL;
   char szText[200];
   int nMax = sizeof(szText)-1;

   guint foe_length = tvb_reported_length(tvb)-offset;

   if( tree )
   {
      aitem = proto_tree_add_item(tree, hf_ecat_mailbox_foe, tvb, offset, foe_length, TRUE);
      proto_item_set_text(aitem, ":Foe");

      aparent = proto_item_get_parent(aitem);
      proto_item_append_text(aparent,"FoE ");
   }

   if( foe_length >= ETHERCAT_FOE_HEADER_LEN )
   {
      FoeFormater(tvb, offset, szText, nMax, foe_length);
      if (check_col(pinfo->cinfo, COL_INFO))
         col_append_str(pinfo->cinfo, COL_INFO, szText);

      if( tree )
      {
         ETHERCAT_FOE_HEADER foe;
         init_foe_header(&foe, tvb, offset);

         ecat_foe_tree = proto_item_add_subtree(aitem, ett_ecat_mailbox_foe);
         aitem = proto_tree_add_item(ecat_foe_tree, hf_ecat_mailbox_foe_opmode, tvb, offset++, 1, TRUE);
         offset++; /*Reserved1;*/

         switch (foe.OpMode)
         {
         case ECAT_FOE_OPMODE_RRQ:
         case ECAT_FOE_OPMODE_WRQ:
            aitem = proto_tree_add_item(ecat_foe_tree, hf_ecat_mailbox_foe_filelength, tvb, offset, 4, TRUE);
            offset+=4;

            aitem = proto_tree_add_item(ecat_foe_tree, hf_ecat_mailbox_foe_filename, tvb, offset, foe_length-offset, TRUE);
            break;

         case ECAT_FOE_OPMODE_DATA:
            {
               aitem = proto_tree_add_item(ecat_foe_tree, hf_ecat_mailbox_foe_packetno, tvb, offset, 2, TRUE);
               offset+=4; /*+2 for Reserved2*/

               aitem = proto_tree_add_item(ecat_foe_tree, hf_ecat_mailbox_foe_data, tvb, offset, foe_length-offset, TRUE);

               if( foe_length-offset >= sizeof(TEFWUPDATE_HEADER) )
               {
                  aitem = proto_tree_add_item(ecat_foe_tree, hf_ecat_mailbox_foe_efw, tvb, offset, foe_length-offset, TRUE);
                  ecat_foe_efw_tree = proto_item_add_subtree(aitem, ett_ecat_mailbox_foe_efw);
                  aitem = proto_tree_add_item(ecat_foe_efw_tree, hf_ecat_mailbox_foe_efw_cmd, tvb, offset, 2, TRUE);
                  offset+=2;

                  aitem = proto_tree_add_item(ecat_foe_efw_tree, hf_ecat_mailbox_foe_efw_size, tvb, offset, 2, TRUE);
                  offset+=2;

                  aitem = proto_tree_add_item(ecat_foe_efw_tree, hf_ecat_mailbox_foe_efw_addresslw, tvb, offset, 2, TRUE);
                  offset+=2;

                  aitem = proto_tree_add_item(ecat_foe_efw_tree, hf_ecat_mailbox_foe_efw_addresshw, tvb, offset, 2, TRUE);
                  offset+=2;

                  aitem = proto_tree_add_item(ecat_foe_efw_tree, hf_ecat_mailbox_foe_efw_data, tvb, offset, foe_length-offset, TRUE);
               }
            }
            break;

         case ECAT_FOE_OPMODE_ACK:
            aitem = proto_tree_add_item(ecat_foe_tree, hf_ecat_mailbox_foe_packetno, tvb, offset, 2, TRUE);
            break;

         case ECAT_FOE_OPMODE_ERR:
            aitem = proto_tree_add_item(ecat_foe_tree, hf_ecat_mailbox_foe_errcode, tvb, offset, 4, TRUE);
            offset+=4;

            aitem = proto_tree_add_item(ecat_foe_tree, hf_ecat_mailbox_foe_errtext, tvb, offset, foe_length-offset, TRUE);
            break;

         case ECAT_FOE_OPMODE_BUSY:
            aitem = proto_tree_add_item(ecat_foe_tree, hf_ecat_mailbox_foe_busydone, tvb, offset, 2, TRUE);
            offset+=2;

            aitem = proto_tree_add_item(ecat_foe_tree, hf_ecat_mailbox_foe_busyentire, tvb, offset, 2, TRUE);
            break;
         }
      }
   }
   else
   {
      if (check_col(pinfo->cinfo, COL_INFO))
         col_append_str(pinfo->cinfo, COL_INFO, "FoE - invalid length");
   }
}

static void dissect_ecat_mailbox(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
   proto_tree *ecat_mailbox_tree=NULL;
   tvbuff_t *next_tvb;
   proto_item *aitem;
   gint offset = 0;
   char szText[200];
   int nMax = sizeof(szText)-1;

   guint mailbox_length = tvb_reported_length(tvb);

   if( mailbox_length >= ETHERCAT_MBOX_HEADER_LEN )
   {
      ETHERCAT_MBOX_HEADER hdr;

      init_mbx_header(&hdr, tvb, offset);
      pinfo->private_data = &hdr;

      if( mailbox_length >= ETHERCAT_MBOX_HEADER_LEN + hdr.Length )
      {
         if (check_col(pinfo->cinfo, COL_INFO))
            col_append_str(pinfo->cinfo, COL_INFO, " Mbx(");

         if( tree )
         {
            aitem = proto_tree_add_item(tree, proto_ecat_mailbox, tvb, 0,  ETHERCAT_MBOX_HEADER_LEN+hdr.Length, TRUE);
            ecat_mailbox_tree = proto_item_add_subtree(aitem, ett_ecat_mailbox);

            aitem = proto_tree_add_item(ecat_mailbox_tree, hf_ecat_mailboxlength, tvb, offset, sizeof(hdr.Length), TRUE);
            offset+=sizeof(hdr.Length);

            aitem = proto_tree_add_item(ecat_mailbox_tree, hf_ecat_mailboxaddress, tvb, offset, sizeof(hdr.Address), TRUE);
            offset+=sizeof(hdr.Address)+sizeof(guint8);

            aitem = proto_tree_add_item(ecat_mailbox_tree, hf_ecat_mailboxtype, tvb, offset, 1, TRUE);
            MailboxTypeFormater(&hdr, szText, nMax);
            proto_item_set_text(aitem,szText);

            aitem = proto_tree_add_item(ecat_mailbox_tree, hf_ecat_mailboxcounter, tvb, offset, 1,TRUE);
            proto_item_set_text(aitem,"Counter : %d",hdr.aControlUnion.Counter);
            offset++;
         }
         else
            offset+=ETHERCAT_MBOX_HEADER_LEN;

         next_tvb = tvb_new_subset (tvb, offset, hdr.Length, hdr.Length);
         switch ( hdr.aControlUnion.Type )
         {
         case ETHERCAT_MBOX_TYPE_ADS:
            call_dissector(ams_handle, next_tvb, pinfo, ecat_mailbox_tree);
            break;

         case ETHERCAT_MBOX_TYPE_EOE:
            dissect_ecat_eoe(next_tvb, 0, pinfo, ecat_mailbox_tree);
            break;

         case ETHERCAT_MBOX_TYPE_COE:
            dissect_ecat_coe(next_tvb, 0, pinfo, ecat_mailbox_tree);
            break;

         case ETHERCAT_MBOX_TYPE_FOE:
            dissect_ecat_foe(next_tvb, 0, pinfo, ecat_mailbox_tree);
            break;

         case ETHERCAT_MBOX_TYPE_SOE:
            dissect_ecat_soe(next_tvb, 0, pinfo, ecat_mailbox_tree);
            break;

         default:
            aitem = proto_tree_add_item(ecat_mailbox_tree, hf_ecat_mailboxdata, tvb, offset, hdr.Length, TRUE);
         }

         if (check_col(pinfo->cinfo, COL_INFO))
            col_append_str(pinfo->cinfo, COL_INFO, ")");
      }
   }
}

void proto_register_ecat_mailbox(void)
{
   static const true_false_string flags_set_truth =
   {
      "Set",
      "Not set"
   };

   static hf_register_info hf[] =
   {
      { &hf_ecat_mailbox,
      { "Mbx: ", "ecat_mailbox",
      FT_BYTES, BASE_NONE, NULL, 0x0,
      "", HFILL }
      },
      { &hf_ecat_mailboxlength,
      { "Length  ", "ecat_mailbox.length",
      FT_UINT16, BASE_DEC, NULL, 0x0,
      "", HFILL }
      },
      { &hf_ecat_mailboxaddress,
      { "Address ", "ecat_mailbox.address",
      FT_UINT16, BASE_HEX, NULL, 0x0,
      "", HFILL }
      },
      { &hf_ecat_mailboxtype,
      { "Type    ", "ecat_mailbox.type",
      FT_UINT8, BASE_HEX, VALS(EcMBoxType), 0x0F,
      "", 0x0F, 0, 0, 0, NULL, NULL }
      },
      { &hf_ecat_mailboxcounter,
      { "Counter    ", "ecat_mailbox.counter",
      FT_UINT8, BASE_HEX, NULL, 0x70,
      "", 0x70, 0, 0, 0, NULL, NULL }
      },
      { &hf_ecat_mailbox_eoe,
      { "EoE Fragment", "ecat_mailbox.eoe",
      FT_BYTES, BASE_NONE, NULL, 0x0,
      "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_fraghead,
      { "Eoe Frag Header", "ecat_mailbox.eoe.fraghead",
      FT_BYTES, BASE_NONE, NULL, 0x0,
      "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_type,
      { "EoE"/*"Type*/, "ecat_mailbox.eoe.type",
      FT_UINT32, BASE_DEC, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_fragno,
      { "EoE"/*"FragNo*/, "ecat_mailbox.eoe.fragno",
      FT_UINT32, BASE_DEC, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_offset,
      { "EoE"/*"Offset"*/, "ecat_mailbox.eoe.offset",
      FT_UINT32, BASE_DEC, NULL, 0x0, "", HFILL}
      },
      { &hf_ecat_mailbox_eoe_frame,
      { "EoE"/*"FrameNo"*/, "ecat_mailbox.eoe.frame",
      FT_UINT32, BASE_DEC, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_last,
      { "Last Fragment"/*"Last Fragment"*/, "ecat_mailbox.eoe.last",
      FT_UINT32, BASE_DEC, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_timestampapp,
      { "Last Fragment"/*"Last Fragment"*/, "ecat_mailbox.eoe.timestampapp",
      FT_UINT32, BASE_DEC, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_timestampreq,
      { "Last Fragment"/*"Last Fragment"*/, "ecat_mailbox.eoe.timestampreq",
      FT_UINT32, BASE_DEC, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_fragment,
      { "EoE Frag Data", "ecat_mailbox.eoe.fragment",
      FT_BYTES, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_init,
      { "Init", "ecat_mailbox.eoe.init",
      FT_NONE, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_init_contains_macaddr,
      { "MacAddr", "ecat_mailbox.eoe.init.contains_macaddr",
      FT_BOOLEAN, 32, TFS(&flags_set_truth), 0x00000001, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_init_contains_ipaddr,
      { "IpAddr", "ecat_mailbox.eoe.init.contains_ipaddr",
      FT_BOOLEAN, 32, TFS(&flags_set_truth), 0x00000002, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_init_contains_subnetmask,
      { "SubnetMask", "ecat_mailbox.eoe.init.contains_subnetmask",
      FT_BOOLEAN, 32, TFS(&flags_set_truth), 0x00000004, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_init_contains_defaultgateway,
      { "DefaultGateway", "ecat_mailbox.eoe.init.contains_defaultgateway",
      FT_BOOLEAN, 32, TFS(&flags_set_truth), 0x00000008, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_init_contains_dnsserver,
      { "DnsServer", "ecat_mailbox.eoe.init.contains_dnsserver",
      FT_BOOLEAN, 32, TFS(&flags_set_truth), 0x00000010, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_init_contains_dnsname,
      { "DnsName", "ecat_mailbox.eoe.init.contains_dnsname",
      FT_BOOLEAN, 32, TFS(&flags_set_truth), 0x00000020, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_init_append_timestamp,
      { "AppendTimeStamp", "ecat_mailbox.eoe.init.append_timestamp",
      FT_BOOLEAN, 32, TFS(&flags_set_truth), 0x00010000, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_init_macaddr,
      { "Mac Addr", "ecat_mailbox.eoe.init.macaddr",
      FT_ETHER, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_init_ipaddr,
      { "Ip Addr", "ecat_mailbox.eoe.init.ipaddr",
      FT_IPv4, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_init_subnetmask,
      { "Subnet Mask", "ecat_mailbox.eoe.init.subnetmask",
      FT_IPv4, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_init_defaultgateway,
      { "Default Gateway", "ecat_mailbox.eoe.init.defaultgateway",
      FT_IPv4, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_init_dnsserver,
      { "Dns Server", "ecat_mailbox.eoe.init.dnsserver",
      FT_IPv4, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_init_dnsname,
      { "Dns Name", "ecat_mailbox.eoe.init.dnsname",
      FT_STRING, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter,
      { "Mac Filter", "ecat_mailbox.eoe.macfilter",
      FT_BYTES, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_macfiltercount,
      { "Mac Filter Count", "ecat_mailbox.eoe.macfilter.macfiltercount",
      FT_UINT8, 16, NULL, 0x0, "", 0x0000000F, 0, 0, 0, NULL, NULL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_maskcount,
      { "Mac Filter Mask Count", "ecat_mailbox.eoe.macfilter.maskcount",
      FT_UINT8, 16, NULL, 0x0, "", 0x00000030, 0, 0, 0, NULL, NULL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_nobroadcasts,
      { "No Broadcasts", "ecat_mailbox.eoe.macfilter.nobroadcasts",
      FT_BOOLEAN, 16,  TFS(&flags_set_truth), 0x0, "", 0x00000080, 0, 0, 0, NULL, NULL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_filter,
      { "Filter", "ecat_mailbox.eoe.macfilter.filter",
      FT_BYTES, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_filters[0],
      { "Filter 0", "ecat_mailbox.eoe.macfilter.filter0",
      FT_ETHER, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_filters[1],
      { "Filter 1", "ecat_mailbox.eoe.macfilter.filter1",
      FT_ETHER, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_filters[2],
      { "Filter 2", "ecat_mailbox.eoe.macfilter.filter2",
      FT_ETHER, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_filters[3],
      { "Filter 3", "ecat_mailbox.eoe.macfilter.filter3",
      FT_ETHER, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_filters[4],
      { "Filter 4", "ecat_mailbox.eoe.macfilter.filter4",
      FT_ETHER, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_filters[5],
      { "Filter 5", "ecat_mailbox.eoe.macfilter.filter5",
      FT_ETHER, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_filters[6],
      { "Filter 6", "ecat_mailbox.eoe.macfilter.filter6",
      FT_ETHER, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_filters[7],
      { "Filter 7", "ecat_mailbox.eoe.macfilter.filter7",
      FT_ETHER, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_filters[8],
      { "Filter 8", "ecat_mailbox.eoe.macfilter.filter8",
      FT_ETHER, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_filters[9],
      { "Filter 9", "ecat_mailbox.eoe.macfilter.filter9",
      FT_ETHER, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_filters[10],
      { "Filter 10", "ecat_mailbox.eoe.macfilter.filter10",
      FT_ETHER, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_filters[11],
      { "Filter 11", "ecat_mailbox.eoe.macfilter.filter11",
      FT_ETHER, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_filters[12],
      { "Filter 12", "ecat_mailbox.eoe.macfilter.filter12",
      FT_ETHER, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_filters[13],
      { "Filter 13", "ecat_mailbox.eoe.macfilter.filter13",
      FT_ETHER, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_filters[14],
      { "Filter 14", "ecat_mailbox.eoe.macfilter.filter14",
      FT_ETHER, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_filters[15],
      { "Filter 15", "ecat_mailbox.eoe.macfilter.filter15",
      FT_ETHER, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_filtermask,
      { "Filter Mask", "ecat_mailbox.eoe.macfilter.filtermask",
      FT_BYTES, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_filtermasks[0],
      { "Mask 0", "ecat_mailbox.eoe.macfilter.filtermask0",
      FT_ETHER, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_filtermasks[1],
      { "Mask 1", "ecat_mailbox.eoe.macfilter.filtermask1",
      FT_ETHER, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_filtermasks[2],
      { "Mask 2", "ecat_mailbox.eoe.macfilter.filtermask2",
      FT_ETHER, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_macfilter_filtermasks[3],
      { "Mask 3", "ecat_mailbox.eoe.macfilter.filtermask3",
      FT_ETHER, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_eoe_timestamp,
      { "Time Stamp", "ecat_mailbox.eoe.timestamp",
      FT_UINT32, BASE_DEC, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_coe,
      { "CoE", "ecat_mailbox.coe",
      FT_BYTES, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_coe_number,
      { "Number", "ecat_mailbox.coe.number",
      FT_UINT16, BASE_DEC, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_coe_type,
      { "Type  ", "ecat_mailbox.coe.type",
      FT_UINT16, BASE_DEC, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoreq,
      { "SDO Req", "ecat_mailbox.coe.sdoreq",
      FT_UINT8, BASE_DEC, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoccsid,
      { "Initiate Download", "ecat_mailbox.coe.sdoccsid",
      FT_UINT8, BASE_HEX, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoccsid_sizeind,
      { "Size Ind.", "ecat_mailbox.coe.sdoccsid.sizeind",
      FT_BOOLEAN, 8, TFS(&flags_set_truth), 0x00000001,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoccsid_expedited,
      { "Expedited", "ecat_mailbox.coe.sdoccsid.expedited",
      FT_BOOLEAN, 8, TFS(&flags_set_truth), 0x00000002,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoccsid_size0,
      { "Bytes", "ecat_mailbox.coe.sdoccsid.size0",
      FT_BOOLEAN, 8, TFS(&flags_set_truth), 0x00000004,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoccsid_size1,
      { "Bytes", "ecat_mailbox.coe.sdoccsid.size1",
      FT_BOOLEAN, 8, TFS(&flags_set_truth), 0x00000008,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoccsid_complete,
      { "Access", "ecat_mailbox.coe.sdoccsid.complete",
      FT_BOOLEAN, 8, TFS(&tfs_complete), 0x00000010,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoccsds,
      { "Download Segment", "ecat_mailbox.coe.sdoccsds",
      FT_UINT8, BASE_HEX, NULL, 0x0,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoccsds_lastseg,
      { "Last Segment", "ecat_mailbox.coe.sdoccsds.lastseg",
      FT_BOOLEAN, 8, TFS(&flags_set_truth), 0x00000001,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoccsds_size,
      { "Size", "ecat_mailbox.coe.sdoccsds.size",
      FT_UINT8, BASE_DEC, NULL, 0x0000000E,
      "", 0x0000000E, 0, 0, 0, NULL, NULL }
      },
      { &hf_ecat_mailbox_coe_sdoccsds_toggle,
      { "Toggle Bit", "ecat_mailbox.coe.sdoccsds.toggle",
      FT_BOOLEAN, 8, TFS(&flags_set_truth), 0x00000010,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoccsiu,
      { "Init Upload", "ecat_mailbox.coe.sdoccsiu",
      FT_UINT8, BASE_HEX, NULL, 0x0,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoccsiu_complete,
      { "Toggle Bit", "ecat_mailbox.coe.sdoccsiu_complete",
      FT_BOOLEAN, 8, TFS(&flags_set_truth), 0x00000010,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoccsus,
      { "Upload Segment", "ecat_mailbox.coe.sdoccsus",
      FT_UINT8, BASE_HEX, NULL, 0x0,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoccsus_toggle,
      { "Toggle Bit", "ecat_mailbox.coe.sdoccsus_toggle",
      FT_BOOLEAN, 8, TFS(&flags_set_truth), 0x00000010,
      "", HFILL }
      },

      { &hf_ecat_mailbox_coe_sdoidx,
      { "Index   ", "ecat_mailbox.coe.sdoidx",
      FT_UINT16, BASE_HEX, NULL, 0x0,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdosub,
      { "SubIndex", "ecat_mailbox.coe.sdosub",
      FT_UINT8, BASE_HEX, NULL, 0x0,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdodata,
      { "Data    ", "ecat_mailbox.coe.sdodata",
      FT_UINT32, BASE_HEX, NULL, 0x0,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdodata1,
      { "Data    ", "ecat_mailbox.coe.sdodata",
      FT_UINT8, BASE_HEX, NULL, 0x0,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdodata2,
      { "Data    ", "ecat_mailbox.coe.sdodata",
      FT_UINT16, BASE_HEX, NULL, 0x0,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoldata,
      { "Data    ", "ecat_mailbox.coe.dsoldata",
      FT_BYTES, BASE_NONE, NULL, 0x0,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdolength,
      { "Length  ", "ecat_mailbox.coe.sdolength",
      FT_UINT32, BASE_HEX, NULL, 0x0,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoerror,
      { "SDO Error   ", "ecat_mailbox.coe.sdoerror",
      FT_UINT32, BASE_HEX, NULL, 0x0,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdores,
      { "SDO Res     ", "ecat_mailbox.coe.sdores",
      FT_UINT8, BASE_DEC, NULL, 0x0,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoscsiu,
      { "Initiate Upload Response", "ecat_mailbox.coe.sdoscsiu",
      FT_UINT8, BASE_HEX, NULL, 0x0,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoscsiu_sizeind,
      { "Size Ind.", "ecat_mailbox.coe.sdoscsiu_sizeind",
      FT_BOOLEAN, 8, TFS(&flags_set_truth), 0x00000001,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoscsiu_expedited,
      { "Expedited", "ecat_mailbox.coe.sdoscsiu_expedited",
      FT_BOOLEAN, 8, TFS(&flags_set_truth), 0x00000002,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoscsiu_size0,
      { "Bytes", "ecat_mailbox.coe.sdoscsiu_size0",
      FT_BOOLEAN, 8, TFS(&flags_set_truth), 0x00000004,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoscsiu_size1,
      { "Bytes", "ecat_mailbox.coe.sdoscsiu_size1",
      FT_BOOLEAN, 8, TFS(&flags_set_truth), 0x00000008,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoscsiu_complete,
      { "Access", "ecat_mailbox.coe.sdoscsiu_complete",
      FT_BOOLEAN, 8, TFS(&tfs_complete), 0x00000010,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoscsds,
      { "Download Segment Response", "ecat_mailbox.coe.sdoscsds",
      FT_UINT8, BASE_HEX, NULL, 0x0,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoscsds_toggle,
      { "Toggle Bit", "ecat_mailbox.coe.sdoscsds_toggle",
      FT_BOOLEAN, 8, TFS(&flags_set_truth), 0x00000010,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoscsus,
      { "Upload Segment", "ecat_mailbox.coe.sdoscsus",
      FT_UINT8, BASE_HEX, NULL, 0x0,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoscsus_lastseg,
      { "Last Segment", "ecat_mailbox.coe.sdoscsus_lastseg",
      FT_BOOLEAN, 8, TFS(&flags_set_truth), 0x00000001,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoscsus_bytes,
      { "Bytes", "ecat_mailbox.coe.sdoscsus_bytes",
      FT_UINT8, BASE_DEC, NULL, 0x0000000E,
      "", 0x0000000E, 0, 0, 0, NULL, NULL }
      },
      { &hf_ecat_mailbox_coe_sdoscsus_toggle,
      { "Toggle Bit", "ecat_mailbox.coe.sdoscsus_toggle",
      FT_BOOLEAN, 8, TFS(&flags_set_truth), 0x00000010,
      "", HFILL }
      },
      { &hf_ecat_mailbox_coe_sdoinfoopcode,
      { "Info OpCode     ", "ecat_mailbox.coe.sdoinfoopcode",
      FT_UINT8, BASE_HEX, NULL, 0x0,
      "", HFILL },
      },
      { &hf_ecat_mailbox_coe_sdoinfofrag,
      { "Info Frag Left  ", "ecat_mailbox.coe.sdoinfofrag",
      FT_UINT16, BASE_HEX, NULL, 0x0, "", HFILL },
      },
      { &hf_ecat_mailbox_coe_sdoinfolisttype,
      { "Info List Type  ", "ecat_mailbox.coe.sdoinfolisttype",
      FT_UINT16, BASE_HEX, NULL, 0x0, "", HFILL },
      },
      { &hf_ecat_mailbox_coe_sdoinfolist,
      { "Info List       ", "ecat_mailbox.coe.sdoinfolist",
      FT_NONE, BASE_NONE, NULL, 0x0, "", HFILL },
      },
      { &hf_ecat_mailbox_coe_sdoinfoindex,
      { "Info Obj Index  ", "ecat_mailbox.coe.sdoinfoindex",
      FT_UINT16, BASE_HEX, NULL, 0x0, "", HFILL },
      },
      { &hf_ecat_mailbox_coe_sdoinfosubindex,
      { "Info Obj SubIdx ", "ecat_mailbox.coe.sdoinfosubindex",
      FT_UINT8, BASE_HEX, NULL, 0x0, "", HFILL },
      },
      { &hf_ecat_mailbox_coe_sdoinfovalueinfo,
      { "Info Obj SubIdx ", "ecat_mailbox.coe.sdoinfovalueinfo",
      FT_UINT8, BASE_HEX, NULL, 0x0, "", HFILL },
      },
      { &hf_ecat_mailbox_coe_sdoinfoerrorcode,
      { "Info Error Code ", "ecat_mailbox.coe.sdoinfoerrorcode",
      FT_UINT32, BASE_HEX, NULL, 0x0, "", HFILL },
      },
      { &hf_ecat_mailbox_coe_sdoinfodatatype,
      { "Info Data Type  ", "ecat_mailbox.coe.sdoinfodatatype",
      FT_UINT16, BASE_HEX, NULL, 0x0, "", HFILL },
      },
      { &hf_ecat_mailbox_coe_sdoinfomaxsub,
      { "Info Max SubIdx ", "ecat_mailbox.coe.sdoinfomaxsub",
      FT_UINT8, BASE_HEX, NULL, 0x0, "", HFILL },
      },
      { &hf_ecat_mailbox_coe_sdoinfoobjcode,
      { "Info Obj Code   ", "ecat_mailbox.coe.sdoinfoobjcode",
      FT_UINT8, BASE_HEX, NULL, 0x0, "", HFILL },
      },
      { &hf_ecat_mailbox_coe_sdoinfoname,
      { "Info Name       ", "ecat_mailbox.coe.sdoinfoname",
      FT_STRING, BASE_NONE, NULL, 0x0, "", HFILL },
      },
      { &hf_ecat_mailbox_coe_sdoinfobitlen,
      { "Info Bit Len    ", "ecat_mailbox.coe.sdoinfobitlen",
      FT_UINT16, BASE_HEX, NULL, 0x0, "", HFILL },
      },
      { &hf_ecat_mailbox_coe_sdoinfoobjaccess,
      { "Info Obj Access ", "ecat_mailbox.coe.sdoinfoobjaccess",
      FT_UINT16, BASE_HEX, NULL, 0x0, "", HFILL },
      },
      { &hf_ecat_mailbox_coe_sdoinfounittype,
      { "Info Data Type  ", "ecat_mailbox.coe.sdoinfounittype",
      FT_UINT16, BASE_HEX, NULL, 0x0, "", HFILL },
      },
      { &hf_ecat_mailbox_coe_sdoinfodefaultvalue,
      { "Info Default Val", "ecat_mailbox.coe.sdoinfodefaultvalue",
      FT_NONE, BASE_NONE, NULL, 0x0, "", HFILL },
      },
      { &hf_ecat_mailbox_coe_sdoinfominvalue,
      { "Info Min Val    ", "ecat_mailbox.coe.sdoinfominvalue",
      FT_NONE, BASE_NONE, NULL, 0x0, "", HFILL },
      },
      { &hf_ecat_mailbox_coe_sdoinfomaxvalue,
      { "Info Max Val    ", "ecat_mailbox.coe.sdoinfomaxvalue",
      FT_NONE, BASE_NONE, NULL, 0x0, "", HFILL },
      },
      { &hf_ecat_mailboxdata,
      { "MB Data", "ecat_mailbox.data",
      FT_NONE, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_foe,
      { "Foe", "ecat_mailbox.foe",
      FT_BYTES, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_foe_opmode,
      { "Foe OpMode    ", "ecat_mailbox.foe_opmode",
      FT_UINT8, BASE_HEX, VALS(FoEOpMode), 0x0, "Op modes", HFILL }
      },
      { &hf_ecat_mailbox_foe_filelength,
      { "Foe FileLength" , "ecat_mailbox.foe_filelength",
      FT_UINT32, BASE_DEC, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_foe_filename,
      { "Foe FileName   ", "ecat_mailbox.foe_filename",
      FT_STRING, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_foe_packetno,
      { "Foe PacketNo   ", "ecat_mailbox.foe_packetno",
      FT_UINT16, BASE_DEC, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_foe_errcode,
      { "Foe ErrorCode  ", "ecat_mailbox.foe_errcode",
      FT_UINT32, BASE_DEC, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_foe_errtext,
      { "Foe ErrorString", "ecat_mailbox.foe_errtext",
      FT_STRING, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_foe_busydone,
      { "Foe BusyDone   ", "ecat_mailbox.foe_busydone",
      FT_UINT16, BASE_HEX, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_foe_busyentire,
      { "Foe BusyEntire ", "ecat_mailbox.foe_busyentire",
      FT_UINT16, BASE_HEX, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_foe_data,
      { "Foe Data       ", "ecat_mailbox.foe_busydata",
      FT_BYTES, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_foe_efw,
      { "Firmware", "ecat_mailbox.foe.efw",
      FT_BYTES, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_foe_efw_cmd,
      { "Cmd     ", "ecat_mailbox.foe.efw.cmd",
      FT_UINT16, BASE_HEX, VALS(FoEEfwCmd), 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_foe_efw_size,
      { "Size    ", "ecat_mailbox.foe.efw.size",
      FT_UINT16, BASE_DEC, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_foe_efw_addresslw,
      { "AddressLW", "ecat_mailbox.foe.efw.addresslw",
      FT_UINT16, BASE_HEX, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_foe_efw_addresshw,
      { "AddressHW", "ecat_mailbox.foe.efw.addresshw",
      FT_UINT16, BASE_HEX, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_foe_efw_data,
      { "Data     ", "ecat_mailbox.foe.efw.data",
      FT_BYTES, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_soe,
      { "Soe", "ecat_mailbox.soe",
      FT_BYTES, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_soe_header,
      { "Soe Header     ", "ecat_mailbox.soe_header",
      FT_UINT16, BASE_HEX, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_soe_header_opcode,
      { "SoE OpCode", "ecat_mailbox.soe_opcode",
      FT_UINT16, BASE_DEC, VALS(SoeOpcode), 0x00000007, "", 0x00000007, 0, 0, 0, NULL, NULL }
      },
      { &hf_ecat_mailbox_soe_header_incomplete,
      { "More Follows...", "ecat_mailbox.soe_header_incomplete",
      FT_BOOLEAN, 16, TFS(&flags_set_truth), 0x00000008, "", HFILL }
      },
      { &hf_ecat_mailbox_soe_header_error,
      { "Error", "ecat_mailbox.soe_header_error",
      FT_BOOLEAN, 16, TFS(&flags_set_truth), 0x00000010,
      "", HFILL }
      },
      { &hf_ecat_mailbox_soe_header_driveno,
      { "Drive No", "ecat_mailbox.soe_header_driveno",
      FT_UINT16, BASE_DEC, NULL, 0x000000e0, "", 0x000000e0, 0, 0, 0, NULL, NULL}
      },
      { &hf_ecat_mailbox_soe_header_datastate,
      { "Datastate", "ecat_mailbox.soe_header_datastate",
      FT_BOOLEAN, 16, TFS(&flags_set_truth), 0x00000100,
      "", HFILL }
      },
      { &hf_ecat_mailbox_soe_header_name,
      { "Name", "ecat_mailbox.soe_header_name",
      FT_BOOLEAN, 16, TFS(&flags_set_truth), 0x00000200,
      "", HFILL }
      },
      { &hf_ecat_mailbox_soe_header_attribute,
      { "Attribute", "ecat_mailbox.soe_header_attribute",
      FT_BOOLEAN, 16, TFS(&flags_set_truth), 0x00000400,
      "", HFILL }
      },
      { &hf_ecat_mailbox_soe_header_unit,
      { "Unit", "ecat_mailbox.soe_header_unit",
      FT_BOOLEAN, 16, TFS(&flags_set_truth), 0x00000800,
      "", HFILL }
      },
      { &hf_ecat_mailbox_soe_header_min,
      { "Min", "ecat_mailbox.soe_header_min",
      FT_BOOLEAN, 16, TFS(&flags_set_truth), 0x00001000,
      "", HFILL }
      },
      { &hf_ecat_mailbox_soe_header_max,
      { "Max", "ecat_mailbox.soe_header_max",
      FT_BOOLEAN, 16, TFS(&flags_set_truth), 0x00002000,
      "", HFILL }
      },
      { &hf_ecat_mailbox_soe_header_value,
      { "Value", "ecat_mailbox.soe_header_value",
      FT_BOOLEAN, 16, TFS(&flags_set_truth), 0x00004000,
      "", HFILL }
      },
      { &hf_ecat_mailbox_soe_header_reserved,
      { "Reserved", "ecat_mailbox.soe_header_reserved",
      FT_BOOLEAN, 16, TFS(&flags_set_truth), 0x00008000,
      "", HFILL }
      },
      { &hf_ecat_mailbox_soe_idn,
      { "SoE IDN        ", "ecat_mailbox.soe_idn",
      FT_UINT16, BASE_HEX, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_soe_data,
      { "SoE Data       ", "ecat_mailbox.soe_data",
      FT_BYTES, BASE_NONE, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_soe_frag,
      { "SoE FragLeft   ", "ecat_mailbox.soe_frag",
      FT_UINT16, BASE_HEX, NULL, 0x0, "", HFILL }
      },
      { &hf_ecat_mailbox_soe_error,
      { "SoE Error      ", "ecat_mailbox.soe_error",
      FT_UINT16, BASE_HEX, NULL, 0x0, "", HFILL }
      }
   };

   static gint *ett[] =
   {
      &ett_ecat_mailbox,
      &ett_ecat_mailbox_eoe,
      &ett_ecat_mailbox_eoe_init,
      &ett_ecat_mailbox_eoe_macfilter,
      &ett_ecat_mailbox_eoe_macfilter_filter,
      &ett_ecat_mailbox_eoe_macfilter_filtermask,
      &ett_ecat_mailbox_coe,
      &ett_ecat_mailbox_sdo,
      &ett_ecat_mailbox_coe_sdoccs,
      &ett_ecat_mailbox_coe_sdoscs,
      &ett_ecat_mailbox_foe,
      &ett_ecat_mailbox_foe_efw,
      &ett_ecat_mailbox_soeopmode,
      &ett_ecat_mailbox_soeflag,
      &ett_ecat_mailbox_soe,
      &ett_ecat_mailbox_fraghead
   };

   proto_ecat_mailbox = proto_register_protocol("EtherCAT Mailbox Protocol",
      "ECAT_MAILBOX","ecat_mailbox");
   proto_register_field_array(proto_ecat_mailbox,hf,array_length(hf));
   proto_register_subtree_array(ett,array_length(ett));

   register_dissector("ecat_mailbox", dissect_ecat_mailbox, proto_ecat_mailbox);
}

void proto_reg_handoff_ecat_mailbox(void)
{
   static dissector_handle_t ecat_mailbox_handle;

   /* Register this dissector as a sub dissector to E88A4 based on ether type. */
   ecat_mailbox_handle = create_dissector_handle(dissect_ecat_mailbox, proto_ecat_mailbox);
   dissector_add("ecatf.type", 5, ecat_mailbox_handle);

   eth_handle = find_dissector("eth_withoutfcs");
   ams_handle = find_dissector("ams");
}
