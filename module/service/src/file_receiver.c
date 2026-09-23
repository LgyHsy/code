#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/prctl.h>

#include "anj_mw_comm.h"
#include "anj_sys.h"
#include "anj_sysmng.h"
#include "anj_service.h"
#include "anj_config.h"
#include "file_receiver.h"
#include "file_sender.h"
#include "record_log.h"

static file_recver_t *pFileReceiver = NULL;

static int file_recver_check_finish()
{
	int iRet = 0;
	ANJ_CHK(pFileReceiver != NULL, 0, "input Invalid");
	__ERR("write_len=%d, file_len=%d\n", pFileReceiver->writelen, pFileReceiver->filelen);
	iRet = (pFileReceiver->writelen == pFileReceiver->filelen) ? 1 : 0;
endFunc:
	return iRet;
}

static int file_recver_thread(void *ctx, int *bStart)
{
	int iRet = 0;
	ANJ_CHK(pFileReceiver != NULL, -1, "input Invalid");

	unsigned int waitsec = pFileReceiver->timeoutsec;
	unsigned long long tStart = anj_mw_get_cputime_ms(NULL);

	while (bStart && *bStart)
	{
		sleep(1);
		unsigned long long tNow = anj_mw_get_cputime_ms(NULL);

		if (tNow > tStart + waitsec * 1000)
		{
			if (!file_recver_check_finish(pFileReceiver))
			{
				__ERR("file recv timeout. calling callback\n");
				__RECORD_LOG_INFO("file recv timeout. calling callback\n");
				anj_sysmng_reboot();
			}
			break;
		}
	}

endFunc:
	__ERR("exited main loop\n");
	return iRet;
}

static int file_recver_set_timeout(unsigned int timeoutsec)
{
	int iRet = 0;
	ANJ_CHK(pFileReceiver != NULL, -1, "input Invalid");

	pFileReceiver->timeoutsec = timeoutsec;
	anj_thread_task_destroy(&pFileReceiver->stRecverThread, 0);

	pFileReceiver->stRecverThread.bAutoDestroy = 0;
	strncpy(pFileReceiver->stRecverThread.iThreadName, "file_recver_thread", sizeof(pFileReceiver->stRecverThread.iThreadName) - 1);
	pFileReceiver->stRecverThread.iThreadjob.ctx = pFileReceiver;
	pFileReceiver->stRecverThread.iThreadjob.func = file_recver_thread;
	ANJ_CHK_FUNC(anj_thread_task_create(&pFileReceiver->stRecverThread), 0, "file_recver_thread create failed\n");
endFunc:
	return iRet;
}

static int file_recver_start()
{
	int iRet = 0;
	ANJ_CHK(pFileReceiver != NULL, -1, "input Invalid");

	if (strlen(pFileReceiver->filename) > 0)
	{
		pFileReceiver->fp = anj_mw_fopen(pFileReceiver->filename, "wb");
		if (!pFileReceiver->fp)
		{
			__ERR("fopen %s failed: %s\n", pFileReceiver->filename, strerror(errno));
			iRet = -1;
			goto endFunc;
		}
		else
		{
			__INFO("fopen %s ok\n", pFileReceiver->filename);
		}
	}
	pFileReceiver->writelen = 0;
endFunc:
	return iRet;
}

static void file_recver_create(char *filename, int filelen, int filetype, int lognum)
{
	pFileReceiver = (file_recver_t *)anj_mw_malloc(sizeof(file_recver_t));
	if (!pFileReceiver)
		return;

	memset(pFileReceiver, 0, sizeof(file_recver_t));
	if (filename)
	{
		strncpy(pFileReceiver->filename, filename, sizeof(pFileReceiver->filename) - 1);
	}
	pFileReceiver->filelen = filelen;
	pFileReceiver->filetype = filetype;
	pFileReceiver->lognum = lognum;
	pFileReceiver->timeoutsec = 300;
}

static void file_recver_create_mmap(unsigned int u32PhyAddr, char *pMapedAddr,
									int filelen, int filetype, int lognum)
{
	pFileReceiver = (file_recver_t *)anj_mw_malloc(sizeof(file_recver_t));
	if (!pFileReceiver)
		return;

	memset(pFileReceiver, 0, sizeof(file_recver_t));
	pFileReceiver->u32PhyAddr = u32PhyAddr;
	pFileReceiver->pMappedAddr = pMapedAddr;
	pFileReceiver->filelen = filelen;
	pFileReceiver->filetype = filetype;
	pFileReceiver->lognum = lognum;
	pFileReceiver->timeoutsec = 300;
}

static int file_recver_destroy(int rmfile)
{
	int iRet = 0;
	ANJ_CHK(pFileReceiver != NULL, -1, "input Invalid");
	file_recver_stop(rmfile);
	if (pFileReceiver->u32PhyAddr != 0)
	{
		anj_sys_munmap(pFileReceiver->pMappedAddr, pFileReceiver->filelen);
	}
	anj_mw_free(pFileReceiver);
endFunc:
	return iRet;
}

int file_recver_feed_data(const char *pData, int nLength)
{
	int iRet = 0;
	ANJ_CHK(pFileReceiver != NULL, -1, "input Invalid");

	if (pFileReceiver->pMappedAddr)
	{
		memcpy(pFileReceiver->pMappedAddr + pFileReceiver->writelen, pData, nLength);
	}

	pFileReceiver->writelen += nLength;
	__INFO("recv frame: len=%d, got=%d, total=%d\n",
		  nLength, pFileReceiver->writelen, pFileReceiver->filelen);

	if (pFileReceiver->fp)
	{
		int ret = anj_mw_fwrite(pFileReceiver->fp, pData, nLength);
		if (ret != nLength)
		{
			iRet = -1;
		}
	}
endFunc:
	return iRet;
}

int file_recver_init(char *filename, int filelen, int filetype, int lognum, int MsgSrc)
{
	int iRet = 0;
	if (getFileSender() || pFileReceiver)
	{
		__ERR("ont trans task is running, can not start upload!!!\n");
		iRet = -3;
	}
	else
	{
		if (filetype == UPLOAD_FIRMWARE_FILE_TYPE)
		{
			unsigned long long u32PhyAddr = 0;
			iRet = anj_sys_alloc(filelen, &u32PhyAddr);
			if (iRet != 0)
			{
				__ERR("sys alloc failed!\n");
				return iRet;
			}
			char *pMappedAddr = anj_sys_mmap(u32PhyAddr, filelen);
			if (pMappedAddr == NULL)
			{
				__ERR("sys mmap failed!\n");
				return -1;
			}
			file_recver_create_mmap((unsigned int)u32PhyAddr, pMappedAddr, filelen, UPLOAD_FIRMWARE_FILE_TYPE, 0);
			if (pFileReceiver == NULL)
			{
				__ERR("file_recver_create_mmap failed!\n");
				anj_sys_munmap(pMappedAddr, filelen);
				return -1;
			}
		}
		else
		{
			if (filename)
			{
				file_recver_create(filename, filelen, filetype, lognum);
			}
		}
		if (MsgSrc == MSG_SRC_PRI)
		{
			if (file_recver_start() < 0)
				iRet = -4;
			else
			{
				if (filetype == UPLOAD_FIRMWARE_FILE_TYPE)
				{
					file_recver_set_timeout(300);
					/* MMA 路径 filename 仅存客户端原名，须在 start 之后写入，避免 fopen 落盘 */
					if (filename && filename[0] && pFileReceiver)
					{
						strncpy(pFileReceiver->filename, filename, sizeof(pFileReceiver->filename) - 1);
					}
				}
			}
		}
		else if (MsgSrc == MSG_SRC_SER)
		{
			if (filetype != UPLOAD_FIRMWARE_FILE_TYPE)
			{
				if (file_recver_start() < 0)
					iRet = -4;
			}
			else if (filename && filename[0] && pFileReceiver)
			{
				strncpy(pFileReceiver->filename, filename, sizeof(pFileReceiver->filename) - 1);
			}
		}
	}
	return iRet;
}

int file_recver_uninit(int rmfile)
{
	int iRet = 0;
	if (pFileReceiver)
	{
		iRet = file_recver_destroy(rmfile);
		pFileReceiver = NULL;
	}
	return iRet;
}

int file_recver_big_init(char *filename, int filelen)
{
	int iRet = 0;
	ANJ_CHK(((filename != NULL) && filelen <= 0), -1, "input Invalid");

	if (getFileSender() || pFileReceiver)
	{
		__ERR("ont trans task is running, can not start upload!!!\n");
		iRet = -3;
	}
	else
	{
		file_recver_create(filename, filelen, UPLOAD_FIRMWARE_FILE_TYPE, 0);
		ANJ_CHK((pFileReceiver != NULL), -1, "file_recver_create failed!");
		if (file_recver_start() < 0)
		{
			__ERR("StartRecv %s failed\n", filename);
		}
	}

endFunc:
	return iRet;
}

int file_recver_stop(int rmfile)
{
	int iRet = 0;
	ANJ_CHK(pFileReceiver != NULL, -1, "input Invalid");

	if (pFileReceiver->fp)
	{
		anj_mw_fclose(pFileReceiver->fp);
		pFileReceiver->fp = NULL;
	}

	if (rmfile && strlen(pFileReceiver->filename))
	{
		remove(pFileReceiver->filename);
	}

	if (pFileReceiver->stRecverThread.pid)
	{
		anj_thread_task_destroy(&pFileReceiver->stRecverThread, 0);

		if (!file_recver_check_finish(pFileReceiver))
		{
			__ERR("file recv not finished. calling callback\n");
			__RECORD_LOG_INFO("file recv not finished. calling callback\n");
			anj_sysmng_reboot();
		}
	}
endFunc:
	return iRet;
}

int file_recver_proc(int dataerror, char *payload, int payloadlen, int MsgSrc)
{
	int iRet = 0;
	if (pFileReceiver)
	{
		if (dataerror)
		{
			file_recver_uninit(1);
		}
		else
		{
			if (payloadlen == 0) // finished
			{
				__ERR("got length = 0 packet, uploading finished...\n");

				if (!file_recver_check_finish(pFileReceiver))
				{
					__ERR("file length check failed while uploading file!!!\n");
					file_recver_uninit(1);
					iRet = -3;
				}
				else
				{
					file_recver_stop(0);

					if (pFileReceiver->filetype == UPLOAD_CONFIG_FILE_TYPE)
					{
						__INFO("upload file: %s ok, update config.\n", pFileReceiver->filename);
						// 执行篡改配置
						iRet = anj_sysmng_config_update(pFileReceiver->filename);
					}
					else if (pFileReceiver->filetype == UPLOAD_CONFIG_XML_FILE_TYPE)
					{
						__ERR("upload second_config file: %s ok\n", pFileReceiver->filename);
						anj_sysmng_second_config_copy(pFileReceiver->filename);
					}
					else if (pFileReceiver->filetype == UPLOAD_OEM_MP3_FILE_TYPE)
					{
						__ERR("upload file: %s ok\n", pFileReceiver->filename);
					}
					else if (pFileReceiver->filetype == UPLOAD_OEM_LOGO_FILE_TYPE)
					{
						__ERR("upload oem logo file: %s ok\n", pFileReceiver->filename);
					}
					else if (pFileReceiver->filetype == UPLOAD_CERTIFICATION_FILE_TYPE)
					{
						__ERR("upload certification: %s ok\n", pFileReceiver->filename);
					}
					else if (pFileReceiver->filetype == UPLOAD_KEY_FILE_TYPE)
					{
						__ERR("upload key: %s ok\n", pFileReceiver->filename);
					}
					else if (pFileReceiver->filetype == UPLOAD_OEM_APP_FILE_TYPE)
					{
						__ERR("upload oem: %s ok\n", pFileReceiver->filename);
					}
					else if (pFileReceiver->filetype == UPLOAD_FIRMWARE_FILE_TYPE)
					{
						__ERR("upload file: %s ok, update firmware.\n", pFileReceiver->filename);

						if (MsgSrc == MSG_SRC_PRI)
						{
							if (pFileReceiver->u32PhyAddr != 0)
							{
								anj_sys_munmap(pFileReceiver->pMappedAddr, pFileReceiver->filelen);
							}

							APPBIN_UPDATE_DATA updateData = {0};
							snprintf(updateData.filePath, sizeof(updateData.filePath), "%s", pFileReceiver->filename);
							strncpy(updateData.origName, pFileReceiver->filename, sizeof(updateData.origName) - 1);
							updateData.nFileLen = pFileReceiver->filelen;
							updateData.nPhyAddr = pFileReceiver->u32PhyAddr;
							iRet = anj_sysmng_app_update(&updateData);
						}
					}
				}

				__ERR("update finished, iRet=%d\n", iRet);
			}
			else
			{
				//__ERR("upload file recv data, len = %d\n", payloadlen);
				if (file_recver_feed_data(payload, payloadlen) < 0)
				{
					__ERR("write upload data failed!!!\n");
				}
			}
		}
	}
	return iRet;
}

file_recver_t *getFileRecver()
{
	return pFileReceiver;
}
