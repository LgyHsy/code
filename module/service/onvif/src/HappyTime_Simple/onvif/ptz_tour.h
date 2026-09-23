// #include "cgi_def.h"  // ipc1 header, not needed in ipc2

#ifndef _PTZ_HOUR_H_
#define _PTZ_HOUR_H_

#if defined (__cplusplus)
extern "C" {
#endif


#define MAX_TOUR_CNT 8

/**
* @brief PTZ configuration data.
*/
typedef struct
{
  char      ptzzoomin[6];	///< zoom-in
  char      ptzzoomout[7];	///< zoom-out
  char      ptzpanup[2];	///< pan-up
  char      ptzpandown[4];	///< pan-down
  char		ptzpanleft[4];	///< pan-left
  char		ptzpanright[5];	///< pan-right
}Ptz_Config_Data;

/*
typedef struct preset
{
	int sizePreset;
	char presetID[16];
	struct preset *next;
}ptz_preset_list;
*/

typedef struct preset_entry
{
	int isHome;
	int presetNumber;
	char profileToken[64];
	char presetName[64];
	char presetToken[64];
	struct preset_entry *next;	
}ptz_preset_list;



typedef struct tour_spot_s
{
	int stay_time;
	char preset_token[32];
	int speed;
}tour_spot_t;


typedef struct  tour_spot_entry_s
{
	tour_spot_t tour_spot;

	struct tour_spot_entry_s *next;

}tour_spot_entry_t;


typedef struct ptz_tour_s
{
	tour_spot_entry_t *p_spot_head;

	tour_spot_entry_t *p_spot_last;

	int spot_cnt;
	
	int free;
	char token[32];
	int index;
	int active;
}ptz_tour_t;



typedef struct ptz_tour_ctx_s
{
	ptz_tour_t ptz_tours[MAX_TOUR_CNT];

	int b_stop_tour;
	int b_tour_running;
	pthread_t tour_thrd_id;	
	
}ptz_tour_ctx_t;



char *create_ptz_tour();


int remove_ptz_tour(char *tour_token);


ptz_tour_t *find_ptz_tour(char *tour_token);


int start_ptz_tour(char *tour_token);


int stop_ptz_tour(char *tour_token);


int save_ptz_tours();

int load_ptz_tours();


int ptz_tour_init();

int get_tour_cnt();

int ptz_tour_release();

int add_ptz_spot(int tourIdx, int speed, int stayTime, char *presetToken);
int clear_tour_spots(ptz_tour_t *ptour);

#if defined (__cplusplus)
}
#endif


#endif


