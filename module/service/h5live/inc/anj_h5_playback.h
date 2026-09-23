#ifndef __ANJ_H5_PLAYBACK_H__
#define __ANJ_H5_PLAYBACK_H__

#ifdef __cplusplus
extern "C" {
#endif

int anj_h5_playback_cmd_ctrl(unsigned int session_id, unsigned int playmode,
                             char *filename, unsigned int timepos);
int anj_h5_playback_release_session(unsigned int session_id);

#ifdef __cplusplus
}
#endif

#endif
