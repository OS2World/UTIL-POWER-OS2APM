/*
 * os2apm.c
 *
 * Access program to APM subsystem
 *
 * (C) 2002 Stephan Dahl
 */


#define INCL_DOSFILEMGR
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_DOSERRORS
#include <os2.h>

#include <stdio.h>
#include <string.h>

char POWERINFOSTR[]   = "GETPOWERINFO: BIOSFlags: %0xH  BIOSVersion: %d.%d  Subsysversion: %d.%d\n";
char RAWPOWERINFOSTR[]= "%0x V%d.%d S%d.%d\n";
char STATUSSTR[]      = "GETPOWERSTATUS: Flags: %d %s Battery %s  Life: %d%%\n";
char RAWSTATUSSTR[]   = "%d A%d B%d %d%%\n";
char MSGSTR[]         = "GETPOWEREVENT: %d msgs; %04XH %04XH %04XH %04XH\n";
char RAWMSGSTR[]      = "%d %04XH %04XH %04XH %04XH\n";

APIRET rc;
HFILE  apmfile;
ULONG  action;
ULONG  attr;
ULONG  openflags;
ULONG  openmode;
	
ULONG  category;
ULONG  function;
ULONG  parmLength;
ULONG  dataLength;
	
struct powerInfoPacket {
	USHORT length;
	USHORT BIOSFlags;
	USHORT BIOSVersion;
	USHORT Subsysversion;
};

struct powerStatusPacket {
	USHORT length;
	USHORT PowerFlags;
	UCHAR  ACStatus;
	UCHAR  BatteryStatus;
	UCHAR  BatteryLife;
};

struct powerMsgPacket {
	USHORT length;
	USHORT MsgCount;
	ULONG  parm1;
	ULONG  parm2;
};

struct powerCmdPacket {
	ULONG  parm1;
	ULONG  parm2;
};

struct dataPacket {
	USHORT retCde;
};

struct powerInfoPacket powerInfo;
struct powerStatusPacket powerStatus;
struct powerMsgPacket powerMsg;
struct powerCmdPacket powerCmd;
struct dataPacket data;

/* options */
UCHAR info  =0;
UCHAR status=0;
UCHAR quiet =0;
UCHAR raw   =0;

/* commands */
UCHAR getmessage =0;
UCHAR enable     =0;
UCHAR disable    =0;
		
char thisPgm[] = "OS2APM";

/*
 * Internal procedures prototypes
 */

void getInfo(void);
void getStatus(void);
void getEvtMessage(void);
void sendPowerCmd(int);

/*
 * Main
 */

int main(int argc, char **argv) {
	int arg;	
	int options = 0;
	int commands= 0;
	
	for (arg=1; arg<argc; arg++) {
		if (argv[arg][0]=='/' || argv[arg][0]=='-')
		{
			/* parameter */
			if (strcmpi(argv[arg]+1,"?")==0) {
				printf("Usage: %s [options] [command]\n"
				       "Options (prefix with '/' or '-'):\n"
				       "  ?: This help\n"
				       "  i: Info: APM version info\n"
				       "  s: Status: power status\n"
				       "  q: Quiet: No output\n"
				       "  r: Raw: Just the data, please\n"
				       "Command (specify only one - no default)\n"
				       "  getmsg : Get a power event message\n"
				       "  enable : Enable APM\n"
				       "  disable: Disable APM\n"
				       ,
				       thisPgm);
				return 0;
			}
			else if (strcmpi(argv[arg]+1,"i")==0) {
				info=1;
				options++;
			}
			else if (strcmpi(argv[arg]+1,"s")==0) {
				status=1;
				options++;
			}
			else if (strcmpi(argv[arg]+1,"q")==0) {
				quiet=1;
				options++;
			}
			else if (strcmpi(argv[arg]+1,"r")==0) {
				raw=1;
				options++;
			}
			else {
				printf("%s: Unknown option '%s'. Use /? for help\n",
				       thisPgm,argv[arg]);
				return 1;
		  }
		}
		else if (commands>0) {
			printf("%s: Too many commands. Use /? for help\n",
			       thisPgm);
			return 2;
		}
		else if (strcmpi(argv[arg],"getmsg")==0) {
			getmessage=1;			
			commands++;
		}
		else if (strcmpi(argv[arg],"enable")==0) {
			enable=1;			
			commands++;
		}
		else if (strcmpi(argv[arg],"disable")==0) {
			disable=1;			
			commands++;
		}
		else {
			printf("%s: Unknown command '%s'. Use /? for help\n",
			       thisPgm,argv[arg]);
			return 1;
		}
	}
	
	if (options==0) {
		/* default */
		info=status=1;
	}
	
	if (!quiet && !raw) {
		printf("%s: APM Command Line interface\n"
		       "(C) 2002  Stephan Dahl\n",thisPgm);
  }

	attr      = FILE_NORMAL;
	openflags = FILE_OPEN;
	openmode  = OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYNONE;
	
	rc = DosOpen("APM$",&apmfile,&action,0,attr,openflags,openmode,NULL);
	
	if (rc!=0) {
		printf("Open error: %ld\n",rc);
	}

	/* Get Power info */
	getInfo();
	/* Get Power status */
	getStatus();
	
	/* handle commands */
	if (getmessage)
		getEvtMessage();

	if (enable)
		sendPowerCmd(1);

	if (disable)
		sendPowerCmd(0);
	

	if (rc==0) {
		rc = DosClose(apmfile);
		if (rc!=0 && !quiet) {
			printf("Close error: %ld\n",rc);
		}
	}
	
	return (int)rc;
}

/*
 * Internal procedures
 */

void getInfo() {
	/* Get and show APM system info */
	if (rc==0 && info) {
		category   = IOCTL_POWER;
		function   = POWER_GETPOWERINFO;
		parmLength = sizeof(struct powerInfoPacket);
		dataLength = sizeof(struct dataPacket);
		powerInfo.length= 8;
		
		rc = DosDevIOCtl(apmfile,category,function,
		                 &powerInfo,parmLength,&parmLength,
		                 &data,dataLength,&dataLength);
		if (rc!=0 && !quiet) {
			printf("IOCtl error: %ld, retCde %d\n",rc,data.retCde);
		}
	}
	
	if (rc==0 && info && !quiet) {
		printf(raw?RAWPOWERINFOSTR:POWERINFOSTR,
			     powerInfo.BIOSFlags,
		  	   powerInfo.BIOSVersion>>8,
		    	 powerInfo.BIOSVersion&255,
			     powerInfo.Subsysversion>>8,
			     powerInfo.Subsysversion&255);
	}
	
}

void getStatus() {
	/* Get and show power status */
	if (rc==0 && status) {
		category   = IOCTL_POWER;
		function   = POWER_GETPOWERSTATUS;
		parmLength = sizeof(struct powerStatusPacket);
		dataLength = sizeof(struct dataPacket);
		powerStatus.length= 7;
		
		rc = DosDevIOCtl(apmfile,category,function,
		                 &powerStatus,parmLength,&parmLength,
		                 &data,dataLength,&dataLength);
		if (rc!=0 && !quiet) {
			printf("IOCtl error: %ld, retCde %d\n",rc,data.retCde);
		}
	}
	
	if (rc==0 && status && !quiet) {
		if (raw)
			printf(RAWSTATUSSTR,
			     powerStatus.PowerFlags,
			     powerStatus.ACStatus,
			     powerStatus.BatteryStatus,
			     powerStatus.BatteryLife);
		else
			printf(STATUSSTR,
				     powerStatus.PowerFlags,
			  	   powerStatus.ACStatus==1?"AC Power":"",
			    	 powerStatus.BatteryStatus==0?"High":
				       powerStatus.BatteryStatus==1?"Low":
				       powerStatus.BatteryStatus==2?"Critical":
				       powerStatus.BatteryStatus==3?"Charging":"?",
				     powerStatus.BatteryLife);
	}
}

void getEvtMessage() {
	/* Get and show power status */
	if (rc==0) {
		category   = IOCTL_POWER;
		function   = POWER_GETPOWEREVENT;
		parmLength = sizeof(struct powerMsgPacket);
		dataLength = sizeof(struct dataPacket);
		powerMsg.length= 12;
		
		rc = DosDevIOCtl(apmfile,category,function,
		                 &powerMsg,parmLength,&parmLength,
		                 &data,dataLength,&dataLength);
		if (rc==87 && !quiet) {
			if (data.retCde==10) {
				printf("GETPOWEREVENT: ERROR: No event queue!\n",rc,data.retCde);
				return;
			}
			else if (data.retCde==14) {
				printf("GETPOWEREVENT: ERROR: Queue overflow!\n",rc,data.retCde);
				return;
			}
		}
		if (rc!=0 && !quiet) {
			printf("IOCtl error: %ld, retCde %d\n",rc,data.retCde);
		}
	}
	
	if (rc==0 && !quiet) {
		if (raw)
			printf(RAWMSGSTR,
			  powerMsg.MsgCount,
			  powerMsg.parm1>>(sizeof(long)/8),
			  powerMsg.parm1&0xFFFF,
			  powerMsg.parm2>>(sizeof(long)/8),
			  powerMsg.parm2&0xFFFF);
		else
			printf(MSGSTR,
			  powerMsg.MsgCount,
			  powerMsg.parm1>>(sizeof(long)/8),
			  powerMsg.parm1&0xFFFF,
			  powerMsg.parm2>>(sizeof(long)/8),
			  powerMsg.parm2&0xFFFF);
	}
}

void sendPowerCmd(int enable) {
	/* Get and show power status */
	if (rc==0) {
		category   = IOCTL_POWER;
		function   = POWER_SENDPOWEREVENT;
		parmLength = sizeof(struct powerCmdPacket);
		dataLength = sizeof(struct dataPacket);
		
		if (enable)
			powerCmd.parm1 = 0x3 << (sizeof(long)/8);
		else
			powerCmd.parm1 = 0x4 << (sizeof(long)/8);
		powerCmd.parm2 = 0;
		rc = DosDevIOCtl(apmfile,category,function,
		                 &powerCmd,parmLength,&parmLength,
		                 &data,dataLength,&dataLength);
		if (rc==87 && !quiet) {
			if (data.retCde==1) {
				printf("SENDPOWEREVENT: ERROR: Bad Sub Id: %04X\n",powerCmd.parm1>>(sizeof(long)/8));
				return;
			}
			if (data.retCde==2) {
				printf("SENDPOWEREVENT: ERROR: Bad Reserved: %04X\n",powerCmd.parm1&0xFFFF);
				return;
			}
			if (data.retCde==3) {
				printf("SENDPOWEREVENT: ERROR: Bad Dev Id: %04X\n",powerCmd.parm2>>(sizeof(long)/8));
				return;
			}
			if (data.retCde==4) {
				printf("SENDPOWEREVENT: ERROR: Bad PwrState: %04X\n",powerCmd.parm2&0xFFFF);
				return;
			}
			if (data.retCde==9) {
				printf("SENDPOWEREVENT: ERROR: Disabled.\n");
				return;
			}
		}
		if (!quiet) {
	 		if (rc==0)
	 			printf("OK");
	 		else
 				printf("IOCtl error: %ld, retCde %d\n",rc,data.retCde);
 			
 		}
	}
}


