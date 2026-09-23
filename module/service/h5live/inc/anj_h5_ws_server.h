#ifndef __ANJ_H5_WS_SERVER_H__
#define __ANJ_H5_WS_SERVER_H__

#include <pthread.h>
#include <stdint.h>

typedef enum
  { AMF_NUMBER = 0,
    AMF_BOOLEAN, 
    AMF_STRING,
    AMF_OBJECT,
    AMF_MOVIECLIP,		/* reserved, not used */
    AMF_NULL, AMF_UNDEFINED, AMF_REFERENCE, AMF_ECMA_ARRAY, AMF_OBJECT_END,
    AMF_STRICT_ARRAY, AMF_DATE, AMF_LONG_STRING, AMF_UNSUPPORTED,
    AMF_RECORDSET,		/* reserved, not used */
    AMF_XML_DOC, AMF_TYPED_OBJECT,
    AMF_AVMPLUS,		/* switch to AMF3 */
    AMF_INVALID = 0xff
  } AMFDataType;

enum
{
    FLV_CODECID_H264 = 7,
};

typedef enum
  { SPSPPS_Type = 1,
    IFatme_Type, 
    PFatme_Type,
    cmd_Type,
    AudioConfig_Type,
    AudioData_Type
  } FarmeDataType;


enum
{
    STREAM_ID_MAIN = 1,
    STREAM_ID_MAIN_GUN,
    STREAM_ID_SUB,
    STREAM_ID_SUB_GUN,
    STREAM_ID_THIRD,
    STREAM_ID_AUDIO_LIVE,
    STREAM_ID_AUDIO_PLAYBACK,
    STREAM_ID_PLAYBACK,
};

enum
{
    ACTION_PLAY=0,
    ACTION_PAUSE,		// 1
    ACTION_RESUME,		// 2
    ACTION_FAST,		// 3
    ACTION_SLOW,		// 4
    ACTION_SEEK,		// 5
    ACTION_FRAMESKIP,	// 6
    ACTION_STOP,		// 7
    ACTION_TIMEPLAY		// 8
};

typedef struct
{
    int stream_type;
    uint32_t session_id;
    uint32_t sequence_id;
    int frame_len;
    unsigned char *frame_data;
    uint32_t nBufferSize;
}frameDataBuffer;

#define sBUFSIZE 3

typedef struct {
    unsigned int buf_pos;
    frameDataBuffer buffer[sBUFSIZE];
    pthread_rwlock_t rwlock;
    int align;
    int is_audio;
    uint32_t seq_counter;
    int flag_send;
} StreamBuffer;

#define STREAM_BUF_MAX 11

extern StreamBuffer g_stream_buffers[STREAM_BUF_MAX];
StreamBuffer* getStreamBuffer(int stream_id);
void initStreamBuffers(void);
void freeStreamBuffers(void);
void clearStreamBuffer(int stream_id);

int anj_h5_ws_server_start(void);
void anj_h5_ws_server_stop(void);

extern int g_h5_client_count;
extern int g_bNeedAuth;
extern char g_audio_codec_type[32];
extern int g_audio_channels;
extern int g_audio_sample_rate;

int websocketserver_start(void);
unsigned char * put_byte( unsigned char *output, uint8_t nVal );  
unsigned char * put_be16(unsigned char *output, uint16_t nVal );  
unsigned char * put_be24(unsigned char *output,uint32_t nVal );  
unsigned char * put_be32(unsigned char *output, uint32_t nVal );  
unsigned char *  put_be64( unsigned char *output, uint64_t nVal );  
unsigned char * put_amf_string( unsigned char *c, const char *str );  
unsigned char * put_amf_double( unsigned char *c, double d ); 


#endif
