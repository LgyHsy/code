#ifndef __MS_PU_ERROR_H__
#define __MS_PU_ERROR_H__

#define PU_SUCCESS					    0
#define PU_PASSWORD_ERROR			    1
#define PU_NOENOUGHPRI				    2
#define PU_NOINIT				        3
#define PU_CHANNEL_ERROR			    4
#define PU_OVER_MAXLINK				    5
#define PU_VERSIONNOMATCH			    6
#define PU_NETWORK_FAIL_CONNECT			7
#define PU_NETWORK_SEND_ERROR			8
#define PU_NETWORK_RECV_ERROR			9
#define PU_NETWORK_RECV_TIMEOUT			10
#define PU_NETWORK_ERRORDATA			11
#define PU_ORDER_ERROR				    12
#define PU_OPERNOPERMIT				    13
#define PU_COMMANDTIMEOUT			    14
#define PU_ERRORSERIALPORT			    15
#define PU_ERRORALARMPORT			    16
#define PU_PARAMETER_ERROR			    17
#define PU_CHAN_EXCEPTION			    18
#define PU_NODISK				        19
#define PU_ERRORDISKNUM				    20
#define PU_DISK_FULL				    21
#define PU_DISK_ERROR				    22
#define PU_NOSUPPORT				    23
#define PU_BUSY					        24
#define PU_MODIFY_FAIL				    25
#define PU_PASSWORD_FORMAT_ERROR		26
#define PU_DISK_FORMATING			    27
#define PU_NORESOURCE				    28
#define PU_OPRATEFAILED				    29
#define PU_OPENHOSTSOUND_FAIL			30
#define PU_VOICEOPENED				    31
#define PU_TIMEINPUTERROR			    32
#define PU_NOSPECFILE				    33
#define PU_CREATEFILE_ERROR			    34
#define PU_FILEOPENFAIL				    35
#define PU_OPERNOTFINISH			    36
#define PU_GETPLAYTIMEFAIL			    37
#define PU_PLAYFAIL				        38
#define PU_FILEFORMAT_ERROR			    39
#define PU_DIR_ERROR				    40
#define PU_ALLOC_RESOURCE_ERROR			41
#define PU_AUDIO_MODE_ERROR			    42
#define PU_NOENOUGH_BUF				    43
#define PU_CREATESOCKET_ERROR			44
#define PU_SETSOCKET_ERROR			    45
#define PU_MAX_NUM				        46
#define PU_USERNOTEXIST				    47
#define PU_WRITEFLASHERROR			    48
#define PU_UPGRADEFAIL				    49
#define PU_CARDHAVEINIT				    50
#define PU_PLAYERFAILED				    51
#define PU_MAX_USERNUM				    52
#define PU_GETLOCALIPANDMACFAIL			53
#define PU_NOENCODEING				    54
#define PU_IPMISMATCH				    55
#define PU_MACMISMATCH				    56
#define PU_UPGRADELANGMISMATCH			57
#define PU_MAX_PLAYERPORT			    58
#define PU_NOSPACEBACKUP			    59
#define PU_NODEVICEBACKUP			    60
#define PU_MAX_ERRNO				    61

const char* msGetPuErrorInfo(int msPuErrno);
#endif
