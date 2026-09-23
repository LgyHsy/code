#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ixml.h>
#include <errno.h>
#include <sys/prctl.h>
#include <unistd.h>

#include "driver_interface.h"
#include "ptz_tour.h"
#include "ptz.h"
// #include "debug_util.h"
#include "onvif_compat_shim.h"


extern ptz_preset_list *g_ptz_list;

extern "C" ptz_tour_ctx_t g_tour_ctx;


char *create_ptz_tour()
{
#if 0
	int free_idx = -1;
	int i;
	for (i = 0; i < MAX_TOUR_CNT; i++)
	{
		if (g_tour_ctx.ptz_tours[i].free)
		{
			free_idx = i; 
			break;
		}

	}

	if (free_idx == -1)
		return NULL;

	g_tour_ctx.ptz_tours[free_idx].free = 0;

	return g_tour_ctx.ptz_tours[free_idx].token;
#endif
	int free_idx = -1;
	int i;
	//int not_active_idx = -1;
	
	for (i = 0; i < MAX_TOUR_CNT; i++)
	{
		if (g_tour_ctx.ptz_tours[i].free)
		{
			free_idx = i; 
			break;
		}
	}

	if (free_idx == -1)
	{
		int j = 0;
		for (j = 0; j < MAX_TOUR_CNT; j++)
		{
			if (g_tour_ctx.ptz_tours[j].active == 0)
			{
				HTTP_LOG("no free tour , recycle tour (%d)\n", j);
				g_tour_ctx.ptz_tours[j].free = 1;
				clear_tour_spots(&g_tour_ctx.ptz_tours[j]);
				if (free_idx == -1)
				{
					free_idx = j;
				}
			
			}
		}
	}

	if (free_idx == -1)
		return NULL;

	g_tour_ctx.ptz_tours[free_idx].free = 0;

	return g_tour_ctx.ptz_tours[free_idx].token;
}


ptz_tour_t *find_ptz_tour(char *tour_token)
{
	int i;
	for (i = 0; i < MAX_TOUR_CNT; i++)
	{
		if (strcmp(tour_token, g_tour_ctx.ptz_tours[i].token) == 0)
		{
			return &g_tour_ctx.ptz_tours[i];
		}

	}

	return NULL;

}

int clear_tour_spots(ptz_tour_t *ptour)
{
	if (ptour->p_spot_head)
	{
		tour_spot_entry_t *pentry = ptour->p_spot_head;
		tour_spot_entry_t *pnext;
		while (pentry != ptour->p_spot_last)
		{
			pnext = pentry->next;
			free(pentry);
		
			pentry = pnext;
		}
		
		free(pentry);
		
		ptour->p_spot_head = 0;
		ptour->p_spot_last = 0;
		ptour->spot_cnt = 0;
	}

	return 0;
}


int remove_ptz_tour(char *tour_token)
{
	int i;
	for (i = 0; i < MAX_TOUR_CNT; i++)
	{
		if (g_tour_ctx.ptz_tours[i].free == 0
			&& strcmp(tour_token, g_tour_ctx.ptz_tours[i].token) == 0)
		{
			g_tour_ctx.ptz_tours[i].free = 0;
			if (g_tour_ctx.ptz_tours[i].p_spot_head)
			{
				tour_spot_entry_t *pentry = g_tour_ctx.ptz_tours[i].p_spot_head;
				tour_spot_entry_t *pnext;
				while (pentry != g_tour_ctx.ptz_tours[i].p_spot_last)
				{
					pnext = pentry->next;
					free(pentry);

					pentry = pnext;
				}

				free(pentry);
				
				g_tour_ctx.ptz_tours[i].p_spot_head = 0;
				g_tour_ctx.ptz_tours[i].p_spot_last = 0;
				g_tour_ctx.ptz_tours[i].spot_cnt = 0;
			}
			
		}

	}

	return 0;
}



int goto_preset_with_speed(char *profileToken, char *presetToken, int panSpeed,int tiltSpeed )
{
	//ptz_preset_list *pTail=0;
	
	if(!profileToken)
	{
		HTTP_LOG("profileToken is NULL!!!\n");
		return -1;
	}
	
	if(!presetToken)
	{
		HTTP_LOG("presetToken is NULL!!!\n");
		return -1;
	}
	
	ptz_mutex_lock();
	ptz_preset_list *pEntry=g_ptz_list;

	while(pEntry)
	{
		if(!strcmp(pEntry->profileToken, profileToken))
		{
			if(presetToken)
			{
				if(!strcmp(pEntry->presetToken, presetToken))
				{
					HTTP_LOG("found existed one, presetNumber = %d!!!", pEntry->presetNumber);
					
					PtzCmdHandle(LENS_PRESET_GOTO, panSpeed, tiltSpeed, pEntry->presetNumber);
					
					ptz_mutex_unlock();
					return 0;
				}
			}
		}
		
		pEntry=pEntry->next;
	}
	ptz_mutex_unlock();
	
	HTTP_LOG("token: %s not found!!!\n", presetToken);
	return -1;
}



void *tour_proc(void *arg)
{
	prctl(PR_SET_NAME, __func__); 
	pthread_detach(pthread_self());

	g_tour_ctx.b_tour_running = 1;
	int cnt = 0;

	int active_idx = -1;
	int i;
	for (i = 0; i < MAX_TOUR_CNT; i++)
	{
		if (g_tour_ctx.ptz_tours[i].active == 1)
		{
			active_idx = i;
			break;
		}

	}
	
	ptz_tour_t *ptour = &g_tour_ctx.ptz_tours[active_idx];

	tour_spot_entry_t *pentry = ptour->p_spot_head;
	
	if(pentry != NULL)
	{
		while (!g_tour_ctx.b_stop_tour)
		{
			if (cnt <= 0)
			{

				HTTP_LOG("@@@@@@@@@@@@@@goto preset(token:%s, speed:%d, staytime:%d)\n", pentry->tour_spot.preset_token,
							pentry->tour_spot.speed, pentry->tour_spot.stay_time);
		
				goto_preset_with_speed(const_cast<char*>("MainStream"), pentry->tour_spot.preset_token,
					pentry->tour_spot.speed, pentry->tour_spot.speed);

				cnt = (pentry->tour_spot.stay_time / 1000)  * 2;
				pentry = pentry->next;
			}
			
			usleep(500 * 1000);
			cnt--;

		}
		
		g_tour_ctx.b_tour_running = 0;
	}
	else
	{
		HTTP_LOG("@@@@@@@@@@@ pentry is NULL @@@@@@@@@@\n");
	}

	return NULL;
}



int start_ptz_tour(char *tour_token)
{
	char *active_tour = 0;
	int active_idx = -1;
	int tour_idx = -1;
	

	int i;
	for (i = 0; i < MAX_TOUR_CNT; i++)
	{

		ptz_tour_t *ptour = &g_tour_ctx.ptz_tours[i];

	//	HTTP_LOG("tour(idx:%d, free:%d, active:%d, token:%s\n",
		//	ptour->index, ptour->free, ptour->active, ptour->token);

		if (ptour->active == 1)
		{
			active_tour = ptour->token;
			active_idx = i;
		}

		if (strcmp(ptour->token, tour_token) == 0)
		{
			HTTP_LOG("find ptz tour(token:%s, indx:%d)\n", tour_token, i);
			tour_idx = i;
		}

	}

	if (tour_idx == -1)
	{
		HTTP_LOG("invalid tour token(%s)\n", tour_token);
		return -1;
	}


	if (active_tour  && strcmp(active_tour, tour_token) == 0)
	{
		__WARN("tour (%s) already active\n");
	}


	if (active_tour)
	{
		g_tour_ctx.b_stop_tour = 1;
		
		while(g_tour_ctx.b_tour_running)
		{
			usleep(100 * 1000);
		}

		g_tour_ctx.b_stop_tour = 0;
		
		g_tour_ctx.tour_thrd_id = -1;	
		g_tour_ctx.ptz_tours[active_idx].active = 0;

		

	}

	g_tour_ctx.ptz_tours[tour_idx].active  = 1;
	
	if (pthread_create(&g_tour_ctx.tour_thrd_id, 0, tour_proc, 0) != 0)
	{
		HTTP_LOG("create tour thread fail\n");

		g_tour_ctx.ptz_tours[tour_idx].active  = 0;
		return -1;
	}


	return 0;

}


int stop_ptz_tour(char *tour_token)
{
	int tour_idx = -1;
	
	int i;
	for (i = 0; i < MAX_TOUR_CNT; i++)
	{
		if (strcmp(g_tour_ctx.ptz_tours[i].token, tour_token) == 0)
		{
			tour_idx = i;
		}

	}

	if (tour_idx == -1)
	{
		HTTP_LOG("can not find tour token(%s)\n", tour_token);
		return -1;
	}

	if (g_tour_ctx.ptz_tours[tour_idx].active == 0)
	{
		__WARN("tour (%s) not active\n", tour_token);
		
		return 0;
	}


	HTTP_LOG("stopping tour (%s)\n", tour_token);

	g_tour_ctx.b_stop_tour = 1;

	while(g_tour_ctx.b_tour_running)
	{
		usleep(100 * 1000);
	}
	
	
	g_tour_ctx.b_stop_tour = 0;

	g_tour_ctx.tour_thrd_id = -1;	
	g_tour_ctx.ptz_tours[tour_idx].active = 0;

	return 0;

}




int save_ptz_tours()
{

#if 0

	char buf[4096];
	char *pb = buf;
	char *pe = buf + 4095;
	ptz_tour_t *ptour = 0;


	pb += snprintf(pb, pe-pb, "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\r\n");
	pb += snprintf(pb, pe-pb, "<PtzTourConfig>\r\n");

	int i; 
	for (i = 0; i < MAX_TOUR_CNT; i++)
	{
		ptour = &(g_tour_ctx.ptz_tours[i]);
		pb += snprintf(pb, pe - pb, "<tour index=\"%d\" free=\"%d\"  active=\"%d\"  token=\"%s\">\r\n", 
			   ptour->index, ptour->free, ptour->active, ptour->token);

		HTTP_LOG("tour(idx:%d, free:%d, active:%d, token:%s, spot_cnt:%d\n",
			ptour->index, ptour->free, ptour->active, ptour->token, ptour->spot_cnt);


		if (ptour->p_spot_head)
		{
			tour_spot_entry_t *pentry = ptour->p_spot_head;
			while (pentry != ptour->p_spot_last)
			{
				pb += snprintf(pb, pe - pb, "<spot presettoken=\"%s\" staytime=\"%d\" speed=\"%d\"/>\r\n",
					  pentry->tour_spot.preset_token, pentry->tour_spot.stay_time, pentry->tour_spot.speed);
				pentry = pentry->next;
			}

			pb += snprintf(pb, pe - pb, "<spot presettoken=\"%s\" staytime=\"%d\" speed=\"%d\"/>\r\n",
				  pentry->tour_spot.preset_token, pentry->tour_spot.stay_time, pentry->tour_spot.speed);			

		}

		pb += snprintf(pb, pe - pb, "</tour>\r\n");

	}


	pb += snprintf(pb, pe-pb, "</PtzTourConfig>\r\n");


	HTTP_LOG("save contents(%s)\n", buf);


	chdir(DATA_BLOCK_MOUNT_PATH);


	int fd = open("ptz_tour_tmp.txt", O_RDWR | O_CREAT);
	if (fd == -1)
	{
		HTTP_LOG("open file (/tmp/ptz_tour.txt) fail(reason:%s)\n", strerror(errno));
		return -1;
	}


	int len = strlen(buf);
	int write_cnt = write(fd, buf, len);
	if (write_cnt != len)
	{
		close(fd);
		return -1;

	}

	fdatasync(fd);

	close(fd);

	if(rename("ptz_tour_tmp.txt" , "ptz_tour.txt") == -1)
	{
		HTTP_LOG("rename error(%s)\n", strerror(errno));
		remove("ptz_tour_tmp.txt");
		return -1;
	}

	remove("ptz_tour_tmp.txt");

#endif
	return 0;



}


int add_ptz_spot(int tourIdx, int speed, int stayTime, char *presetToken)
{
	ptz_tour_t *ptour = &g_tour_ctx.ptz_tours[tourIdx];	
	tour_spot_entry_t *pentry = 0;

	if (ptour->p_spot_head == 0)
	{
		pentry = (tour_spot_entry_t *)malloc(sizeof(tour_spot_entry_t));
		pentry->next = pentry;

		pentry->tour_spot.speed = speed;
		pentry->tour_spot.stay_time = stayTime;
		strcpy(pentry->tour_spot.preset_token, presetToken);

		ptour->p_spot_head = pentry;
		ptour->p_spot_last = pentry;

		ptour->spot_cnt++;

		return 0;

	}

	pentry = (tour_spot_entry_t *)malloc(sizeof(tour_spot_entry_t));
	pentry->tour_spot.speed = speed;
	pentry->tour_spot.stay_time = stayTime;
	strcpy(pentry->tour_spot.preset_token, presetToken);	

	pentry->next = ptour->p_spot_head;
	ptour->p_spot_last->next = pentry;

	ptour->p_spot_last = pentry;

	ptour->spot_cnt++;

	return 0;
}


int parse_spot_xml_node(int tourIdx, IXML_Node *pSpotNode)
{
	Nodeptr attrNode = pSpotNode->firstAttr;
	int speed = 0;
	int stayTime = 0;
	char presetToken[32];

	while (attrNode)
	{
		if (strcmp(attrNode->nodeName, "speed") == 0)
		{
			speed = atoi(attrNode->nodeValue);
		}
		else if (strcmp(attrNode->nodeName, "staytime") == 0)
		{
			stayTime = atoi(attrNode->nodeValue);
		}
		else if (strcmp(attrNode->nodeName, "presettoken") == 0)
		{
			strcpy(presetToken, attrNode->nodeValue);
		}

		attrNode = attrNode->nextSibling;
	}

	HTTP_LOG("add ptz sport (tourIdx:%d, speed:%d, statyTime:%d, presetToken:%s\n",
		   tourIdx, speed, stayTime, presetToken);


	add_ptz_spot(tourIdx, speed, stayTime, presetToken);

	return 0;
	
}


int parse_tour_xml_node(IXML_Node *pTourNode)
{

	Nodeptr attrNode = pTourNode->firstAttr;
	int t_idx = 0;
	int t_free = 0;
	int t_active = 0;
	char t_token[32];

	while (attrNode)
	{
		if (strcmp(attrNode->nodeName, "index") == 0)
		{
			t_idx = atoi(attrNode->nodeValue);
		}
		else if (strcmp(attrNode->nodeName, "free") == 0)
		{
			t_free = atoi(attrNode->nodeValue);
		}
		else if (strcmp(attrNode->nodeName, "active") == 0)
		{

			t_active = atoi(attrNode->nodeValue);
		}

		else if (strcmp(attrNode->nodeName, "token") == 0)
		{
			strcpy(t_token, attrNode->nodeValue);
		}

		attrNode = attrNode->nextSibling;
	}

	HTTP_LOG("tour(idx:%d, free:%d, active:%d, token:%s\n", t_idx, t_free, t_active, t_token);

	if (t_idx >= MAX_TOUR_CNT || t_idx < 0)
	{
		HTTP_LOG("invalid idx \n");
		return -1;
	}

	g_tour_ctx.ptz_tours[t_idx].free = t_free;
	
	g_tour_ctx.ptz_tours[t_idx].active = t_active;
	strcpy(g_tour_ctx.ptz_tours[t_idx].token, t_token);
	g_tour_ctx.ptz_tours[t_idx].index = t_idx;


	Nodeptr childNode = pTourNode->firstChild;
	while(childNode)
	{
		if (strcmp(childNode->nodeName, "spot") == 0)
		{
			parse_spot_xml_node(t_idx, childNode);
		}

		childNode = childNode->nextSibling;
	}
	
	return 0;

}

int load_ptz_tours()
{

#if 0
	int fd = open("/mnt/nand/ptz_tour.txt", O_RDWR);
	if (fd == -1)
	{
		HTTP_LOG("open file(/mnt/nand/ptz_tour.txt) fail(reason:%s)\n", strerror(errno));
		return -1;
	}

	char buf[4096];
	memset(buf, 0, 4096);
	int read_cnt = read(fd, buf, 4096);
	close(fd);
	if (read_cnt > 0)
	{
		buf[read_cnt] = '\0';
			
	}
	else
	{
		HTTP_LOG("read file fail(/mnt/nand/ptz_tour.txt) fail(reasion:%s)\n", strerror(errno));
		return -1;
	}

	IXML_Document *pDoc = ixmlParseBuffer(buf);
	if(pDoc == 0)
	{
		HTTP_LOG("ixmlParseBuffer faild, xml=\n%s\n", buf);
		return -1;
	}

	IXML_NodeList * pNodelist = ixmlDocument_getElementsByTagName(pDoc, "tour");
	if(pNodelist == 0)
	{
		HTTP_LOG(" Tag(tour) not found!!!\n");
		
		ixmlDocument_free(pDoc);
		return -1;
	}
	else		
	{
		IXML_NodeList *ptmpNodeList = pNodelist;
		
		while(ptmpNodeList)
		{
			
			parse_tour_xml_node(ptmpNodeList->nodeItem);
			ptmpNodeList = ptmpNodeList->next;

		}

		ixmlNodeList_free(pNodelist);

		ixmlDocument_free(pDoc);

		return 0;

	}			
#endif

	return 0;

}



int get_tour_cnt()
{
	int i = 0;
	int cnt = 0;
	for (i = 0; i < MAX_TOUR_CNT; i++)
	{
		if (g_tour_ctx.ptz_tours[i].free == 0)
		{
			cnt++;
		}
	}

	return cnt;

}



int ptz_tour_init()
{
	memset(&g_tour_ctx, 0,  sizeof(ptz_tour_ctx_t));

	g_tour_ctx.tour_thrd_id = -1;
	g_tour_ctx.b_stop_tour = 0;
	g_tour_ctx.b_tour_running = 0;

	int i = 0;
	for (i = 0; i < MAX_TOUR_CNT; i++)
	{
		g_tour_ctx.ptz_tours[i].active = 0;
		g_tour_ctx.ptz_tours[i].index = i;
		g_tour_ctx.ptz_tours[i].free = 1;
		sprintf(g_tour_ctx.ptz_tours[i].token, "tour_%d", i);
		g_tour_ctx.ptz_tours[i].p_spot_head = 0;
		g_tour_ctx.ptz_tours[i].p_spot_last = 0;
		g_tour_ctx.ptz_tours[i].spot_cnt = 0;
	}
	
	load_ptz_tours();
	
	return 0;
}


int ptz_tour_release()
{
	g_tour_ctx.b_stop_tour = 1;

	while(g_tour_ctx.b_tour_running)
	{
		usleep(100 * 1000);
	}

	g_tour_ctx.b_stop_tour = 0;

	g_tour_ctx.tour_thrd_id = -1;

	save_ptz_tours();

	return 0;

}


		


