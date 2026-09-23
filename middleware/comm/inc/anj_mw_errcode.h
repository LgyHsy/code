#ifndef _ANJ_MW_ERRCODE_H_
#define _ANJ_MW_ERRCODE_H_

#ifdef __cplusplus
extern "C"
{
#endif

#define ANJ_SUCCESS (0)
#define ANJ_FAILURE (-1)

#define ANJ_ERR_NOT_INIT (-99)
#define ANJ_ERR_NOT_START (-98)
#define ANJ_ERR_INVALID_STATUS (-97)
#define ANJ_ERR_INVALID_INPUT (-96)
#define ANJ_ERR_INVALID_SIZE (-95)

#define ANJ_ERR_BUF_MALLOC (-89)

#define ANJ_ERR_FILE_OPEN (-79)
#define ANJ_ERR_FILE_WRITE (-78)
#define ANJ_ERR_FILE_READ (-77)
#define ANJ_ERR_FILE_DATA (-76)
#define ANJ_ERR_FILE_NOT_EXIST (-75)


#define ANJ_ERR_REC_NOT_INIT (-69)

#ifdef __cplusplus
}
#endif
#endif
