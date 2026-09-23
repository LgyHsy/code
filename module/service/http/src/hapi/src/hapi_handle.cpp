#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>    
#include <sys/stat.h>
#include <sys/msg.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <string>
#include <map>
#include <list>
#include <set>
#include <algorithm>


#include "anj_mw_comm.h"
#include "anj_mw_time.h"
#include "anj_mw_mem.h"
#include "anj_mw_str.h"
#include "anj_mw_crypt.h"
#include "anj_mw_hwctrl.h"

#include "anj_config.h"
#include "anj_sysctl.h"
#include "anj_sysmng.h"
#include "anj_ispctl.h"
#include "anj_video.h"
#include "anj_systime.h"
#include "record_log.h"
#include "cJSON.h"
#include "function_list.h"
#include "alarm_link.h"
#include "user_auth.h"
#include "eventhub.h"

#include "http_handle.h"
#include "hapi_handle.h"
#include "cgi_handle.h"
#include "hapi_subs.h"
#include "hapi_handle.h"

using namespace std;

string cgi_session_id_get();
int cgi_session_id_refresh(string sid);

#define RESPONSE_ROOT       "Response"
#define RESPONSE_OK         "Succeed"
#define HEAD_CHANNELS       "/Channels/"

#define MAX_S32 0X7FFFFFFF
#define MAX_U32 0XFFFFFFFF


typedef enum
{
    AUTH_TYPE_NO = 0,//不需要认证
    AUTH_TYPE_ALL = 1,
    AUTH_TYPE_BY_USERNAME = 2,
    AUTH_TYPE_BY_UID = 3,
}AuthType;


typedef map<string, string> URLParamStruct;

typedef int (*callback_hapi_handle)(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid);
int hapi_sysinfo_function_list(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid);

typedef struct
{
    const char* szURLPrefix;
    callback_hapi_handle cb;
    AuthType eSupportAuthType;
}HAPIStaticTableStruct;

string &GetLowerString(string &szSrcStr)
{
    transform(szSrcStr.begin(),szSrcStr.end(),szSrcStr.begin(),::tolower);
    return szSrcStr;
}

string &GetUpperString(string &szSrcStr)
{
    transform(szSrcStr.begin(),szSrcStr.end(),szSrcStr.begin(),::toupper);
    return szSrcStr;
}

string &TrimLeft(string &str)
{
    string::iterator i;
    for (i = str.begin(); i != str.end(); i++) 
    {
        if (!isspace(*i))
        {
            break;
        }
    }
    if (i == str.end())
    {
        str.clear();
    } 
    else 
    {
        str.erase(str.begin(), i);
    }

    return str;
}

string &TrimRight(string &str)
{
    string::iterator i;
    for (i = str.end() - 1; ;i--)
    {
        if (!isspace(*i))
        {
            str.erase(i + 1, str.end());
            break;
        }
        if (i == str.begin())
        {
            str.clear();
            break;
        }
    }

    return str;
}

string &GetTrimString(string& szSrcStr)
{
    TrimLeft(szSrcStr);
    TrimRight(szSrcStr);
    return szSrcStr;
}

string GetJSON_DayTimeStr(const DayTime &dayTime)
{
    if(dayTime.hour > 24 ||
        dayTime.hour <0 ||
        dayTime.minute > 59 || 
        dayTime.minute < 0 ||
        dayTime.sec > 59 || 
        dayTime.sec <0 ||
        (dayTime.hour == 24 && dayTime.minute > 0) )
    {
        return string("00:00:00");
    }

    char buffer[64] = {0};
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", dayTime.hour, dayTime.minute, dayTime.sec);
    return string(buffer);
}

int GetDayTimeFromString(const string strTime, DayTime &data)
{
    if(strTime.length() != strlen("00:00:00"))
    {
        return -1;
    }

    int hour; int minute; int sec;
    if(3 != sscanf(strTime.c_str(), "%02d:%02d:%02d", &hour, &minute, &sec))
    {
        return -1;
    }
    
    if(hour > 24 || 
        hour <0 ||
        minute > 59 || 
        minute < 0 ||
        sec > 59 || 
        sec <0 ||
        (hour == 24 && minute > 0) )
    {
        return -1;
    }
    
    data.hour = hour;
    data.minute = minute;
    data.sec = sec;

    return 0;
}

void GetURLParamList(const char *path, const char *szHttpMethod, const char* szMsgBody, map<string, string> &paramMap)
{
    if( NULL == path)
        return;

    //if(strcmp("GET", szHttpMethod) == 0)
    {
        const char* p = strstr(path, "?");
        if(NULL == p )
            p = strstr(path, "&");

        if(NULL == p )
            goto PARSE_FROM_BODY;

        p = p+1;//跳过?或者&
        char buffer[2048] = {0};
        strncpy(buffer, p, sizeof(buffer)-1);
        
        const char s[4] = "&";
        char *token = NULL;
        char *pTmpStr = NULL;
        token = strtok_r(buffer, s, &pTmpStr); 
        while( token != NULL ) 
        {
            string szName;
            string szValue;
            
            const char* pToBeFind = "=";
            char* p = strstr(token, pToBeFind);
            if( NULL != p )
            {
                *p = 0;
                p++;

                szName = string(token);
                szValue = string(p);
            }
            else
            {
                szName = string(token);
                szValue = "";
            }
            
            GetLowerString(szName);//转换为小写

            paramMap[szName] = szValue;

            token = strtok_r(NULL, s, &pTmpStr);
        }
    }

PARSE_FROM_BODY:    
    if(strcmp("PUT", szHttpMethod) == 0 || strcmp("POST", szHttpMethod) == 0)
    {
        if(NULL == szHttpMethod)
            return;

        //__DBG("%s", szMsgBody);

        cJSON *pRequest = cJSON_Parse(szMsgBody);
        if(pRequest == NULL)
        {
            __ERR("cJSON_Parse failed\n");
            return;
        }

        cJSON *node = NULL;
        node = pRequest->child;
        
        while (node != NULL)
        {
            string szName = node->string;
            GetLowerString(szName);//转换为小写
            if(node->type == cJSON_String)
            {
                string szValue = node->valuestring;
                paramMap[szName] = szValue;
            }
            else if(node->type == cJSON_Number )
            {
                char szValue[64] = {0};
                sprintf(szValue, "%d", node->valueint);
                paramMap[szName] = szValue;
            }
                
            node = node->next;
        }
        
        cJSON_Delete(pRequest);
    }

    __DBG("url:%s\n", path);
    map<string, string>::iterator it = paramMap.begin();
    for(; it != paramMap.end(); ++it)
    {
        const string &szName = it->first;
        const string &szValue = it->second;
        
        __DBG("|%20s|%20s|\n", szName.c_str(), szValue.c_str());
    }
}

static map<string, unsigned long long> s_OnvifHostMap;
void http_hapi_onvif_host_notify(const char* clientip)
{
    if( NULL == clientip)
        return;

    s_OnvifHostMap[string(clientip)] = GetCurrentTimeStampU64();
    __DBG("onvif add clientip:%s\n", clientip);
}

int http_hapi_onvif_host_legal(const char* clientip)
{
    if( NULL == clientip)
        return 0;

    map<string, unsigned long long>::iterator it = s_OnvifHostMap.find(clientip);
    if( it == s_OnvifHostMap.end())
    {
        __ERR("%s not in onvif hosts\n", clientip);
        return 0;
    }

    unsigned int tOnvifTimeStamp = it->second;
    if( GetCurrentTimeStampU64() - tOnvifTimeStamp > 600000)    //10分钟以上都没有ONVIF认证过，则不合法
        return 0;

    return 1;
}

int GetParamValue_fromMap(map<string, string> &paramMap, string szParamName, string &szValue)
{
	int bHaveParam = 0;
	GetLowerString(szParamName);
	URLParamStruct::iterator itMap = paramMap.find(szParamName);
	if( itMap != paramMap.end())
	{
		bHaveParam = 1;
		szValue = itMap->second;
	}

	return bHaveParam;
}

//从参数map中得到INT VALUE，并判断范围，输出错误码等
int GetParamValueS32_fromMap(map<string, string> &paramMap, 
                                const string &szToBeFind, 
                                int min, int max, 
                                int &value_gotton, 
                                int &bHaveParam, 
                                int &nResponseCode, 
                                string &szResponseString)
{
    string szValue;
    int bFind = GetParamValue_fromMap(paramMap, szToBeFind, szValue);
    if(bFind)
    {
        bHaveParam = 1;
        int nValue = atoi(szValue.c_str());
        if(nValue >= min && nValue <= max)
        {
            value_gotton = (int)nValue;
        }
        else
        {
            nResponseCode = -1;
            szResponseString = "Param (" + szToBeFind + ") invalid";
            char szScope[128] = {0};
            snprintf(szScope, sizeof(szScope), ", should be in [%d-%d]", min, max);
            szResponseString += string(szScope);
        }
    }
    return bFind;
}
    
//从参数map中得到UINT VALUE，并判断范围，输出错误码等
int GetParamValueU32_fromMap(map<string, string> &paramMap, 
                                        const string &szToBeFind, unsigned int min, unsigned int max, 
                                        unsigned int &value_gotton, int &bHaveParam, int &nResponseCode, string &szResponseString)
{
    string szValue;
    int bFind = GetParamValue_fromMap(paramMap, szToBeFind, szValue);
    if(bFind)
    {
        bHaveParam = 1;
        unsigned int nValue = CheckAtoU(szValue.c_str());
        if(nValue >= min && nValue <= max)
        {
            value_gotton = nValue;
        }
        else
        {
            nResponseCode = -1;
            szResponseString = "Param (" + szToBeFind + ") invalid";
            char szScope[128] = {0};
            snprintf(szScope, sizeof(szScope), ", should be in [%d-%d]", min, max);
            szResponseString += string(szScope);
        }
    }
    return bFind;
}


int GetParamValueS16_fromMap(map<string, string> &paramMap, 
                                    const string &szToBeFind, short min, short max, 
                                    short &value_gotton, int &bHaveParam, int &nResponseCode, string &szResponseString)
{
    string szValue;
    int bFind = GetParamValue_fromMap(paramMap, szToBeFind, szValue);
    if(bFind)
    {
        bHaveParam    = 1;
        int nValue = atoi(szValue.c_str());
        if(nValue >= min && nValue <= max)
        {
            value_gotton = (short)nValue;
        }
        else
        {
            nResponseCode = -1;
            szResponseString = "Param (" + szToBeFind + ") invalid";
            char szScope[128] = {0};
            snprintf(szScope, sizeof(szScope), ", should be in [%d-%d]", min, max);
            szResponseString += string(szScope);
        }
    }
    return bFind;
}
int GetParamValueU8_fromMap(map<string, string> &paramMap, 
                                    const string &szToBeFind, unsigned char min, unsigned char max, 
                                    unsigned char &value_gotton, int &bHaveParam, int &nResponseCode, string &szResponseString)
{
    string szValue;
    int bFind = GetParamValue_fromMap(paramMap, szToBeFind, szValue);
    if(bFind)
    {
        bHaveParam    = 1;
        int nValue = atoi(szValue.c_str());
        if(nValue >= min && nValue <= max)
        {
            value_gotton = (unsigned char)nValue;
        }
        else
        {
            nResponseCode = -1;
            szResponseString = "Param (" + szToBeFind + ") invalid";
            char szScope[128] = {0};
            snprintf(szScope, sizeof(szScope), ", should be in [%d-%d]", min, max);
            szResponseString += string(szScope);
        }
    }
    return bFind;
}

    
//从参数map中得到string VALUE，并判断范围，输出错误码等
int GetParamValueString_fromMap(map<string, string> &paramMap, 
                                            const string &szToBeFind, 
                                            set<string> setSupportStrings, 
                                            string &value_gotton,
                                            int &bHaveParam, 
                                            int &nResponseCode, 
                                            string &szResponseString)
{
    string szValue;
    int bFind = GetParamValue_fromMap(paramMap, szToBeFind, szValue);
    if(bFind)
    {
        bHaveParam    = 1;
        if( setSupportStrings.size() > 0 )
        {
            set<string>::iterator itSet = setSupportStrings.find(szValue);
            if(itSet != setSupportStrings.end())
            {
                value_gotton = szValue;
            }
            else
            {
                nResponseCode = -1;
                szResponseString = "Param (" + szToBeFind + ") invalid";

                string szScope = ", should be in [ ";
                set<string>::iterator itSet = setSupportStrings.begin();
                for(;itSet != setSupportStrings.end(); ++itSet)
                {
                    szScope += *itSet;
                    szScope += " ";
                }
                szScope += "]";
                szResponseString += szScope;
            }
        }
        else
        {
            value_gotton = szValue;
        }
    }
    return bFind;
}

//从JSON中得到UINT VALUE，并判断范围，输出错误码等
int GetParamValueS32_fromJson(cJSON *pRootNode, 
                                        const string &szToBeFind, 
                                        int min, int max, 
                                        int &value_gotton,
                                        int &bHaveParam, 
                                        int &nResponseCode, 
                                        string &szResponseString)
{
    int bFind = 0;
    cJSON *pSubNode = cJSON_GetObjectItem(pRootNode, szToBeFind.c_str());
    if(NULL == pSubNode )
        return bFind;

    bFind = 1;
    if(pSubNode->type != cJSON_Number )
    {
        nResponseCode = -1;
        szResponseString = szToBeFind + " param invalid.";
        return bFind;
    }

    int nValue = pSubNode->valueint;
    if(nValue >= min && nValue <= max)
    {
        value_gotton = (int)nValue;
    }
    else
    {
        nResponseCode = -1;
        szResponseString = "Param (" + szToBeFind + ") invalid";
        char szScope[128] = {0};
        snprintf(szScope, sizeof(szScope), ", should be in [%d-%d]", min, max);
        szResponseString += string(szScope);
    }
    
    return bFind;
}
int GetParamValueU32_fromJson(cJSON *pRootNode, 
                                    const string &szToBeFind, 
                                    unsigned int min, unsigned int max, 
                                    unsigned int &value_gotton, 
                                    int &bHaveParam, 
                                    int &nResponseCode, 
                                    string &szResponseString)
{
    int bFind = 0;
    cJSON *pSubNode = cJSON_GetObjectItem(pRootNode, szToBeFind.c_str());
    if(NULL == pSubNode )
        return bFind;

    bFind =1;
    if(pSubNode->type != cJSON_Number )
    {
        nResponseCode = -1;
        szResponseString = szToBeFind + " param invalid.";
        return bFind;
    }

    unsigned int nValue = pSubNode->valueint;
    if(nValue >= min && nValue <= max)
    {
        value_gotton = (unsigned int)nValue;
    }
    else
    {
        nResponseCode = -1;
        szResponseString = "Param (" + szToBeFind + ") invalid";
        char szScope[128] = {0};
        snprintf(szScope, sizeof(szScope),", should be in [%d-%d]", min, max);
        szResponseString += string(szScope);
    }
    
    return bFind;
}

int GetParamValueS16_fromJson(cJSON *pRootNode,
                                        const string &szToBeFind, 
                                        short min, short max, 
                                        short &value_gotton,
                                        int &bHaveParam, 
                                        int &nResponseCode, 
                                        string &szResponseString)
{
    int bFind = 0;
    cJSON *pSubNode = cJSON_GetObjectItem(pRootNode, szToBeFind.c_str());
    if(NULL == pSubNode )
        return bFind;

    bFind =1;
    if(pSubNode->type != cJSON_Number )
    {
        nResponseCode = -1;
        szResponseString = szToBeFind + " param invalid.";
        return bFind;
    }

    int nValue = pSubNode->valueint;
    if(nValue >= min && nValue <= max)
    {
        value_gotton = (short)nValue;
    }
    else
    {
        nResponseCode = -1;
        szResponseString = "Param (" + szToBeFind + ") invalid";
        char szScope[128] = {0};
        snprintf(szScope, sizeof(szScope), ", should be in [%d-%d]", min, max);
        szResponseString += string(szScope);
    }
    
    return bFind;
}
    
int GetParamValueU8_fromJson(cJSON *pRootNode, 
                                        const string &szToBeFind, 
                                        unsigned char min, unsigned char max, 
                                        unsigned char &value_gotton,
                                        int &bHaveParam, 
                                        int &nResponseCode, 
                                        string &szResponseString)
{
    int bFind = 0;
    cJSON *pSubNode = cJSON_GetObjectItem(pRootNode, szToBeFind.c_str());
    if(NULL == pSubNode )
        return bFind;

    bFind =1;
    if(pSubNode->type != cJSON_Number )
    {
        nResponseCode = -1;
        szResponseString = szToBeFind + " param invalid.";
        return bFind;
    }

    int nValue = pSubNode->valueint;
    if(nValue >= min && nValue <= max)
    {
        value_gotton = (unsigned char)nValue;
    }
    else
    {
        nResponseCode = -1;
        szResponseString = "Param (" + szToBeFind + ") invalid";
        char szScope[128] = {0};
        snprintf(szScope, sizeof(szScope), ", should be in [%d-%d]", min, max);
        szResponseString += string(szScope);
    }
    
    return bFind;
}

//从参数map中得到string VALUE，并判断范围，输出错误码等
int GetParamValueString_fromJson(cJSON *pRootNode, 
                                            const string &szToBeFind, 
                                            set<string> setSupportStrings, 
                                            string &value_gotton,
                                            int &bHaveParam, 
                                            int &nResponseCode, 
                                            string &szResponseString)
{
    int bFind = 0;
    cJSON *pSubNode = cJSON_GetObjectItem(pRootNode, szToBeFind.c_str());
    if(NULL == pSubNode )
        return bFind;

    bHaveParam    = 1;
    bFind =1;

    if(pSubNode->type != cJSON_String )
    {
        nResponseCode = -1;
        szResponseString = szToBeFind + " param invalid.";
        return bFind;
    }

    string szValue = pSubNode->valuestring;

    if( setSupportStrings.size() > 0 )
    {
        set<string>::iterator itSet = setSupportStrings.find(szValue);
        if(itSet != setSupportStrings.end())
        {
            value_gotton = szValue;
        }
        else
        {
            nResponseCode = -1;
            szResponseString = "Param (" + szToBeFind + ") invalid";
            
            string szScope = ", should be in [ ";
            set<string>::iterator itSet = setSupportStrings.begin();
            for(;itSet != setSupportStrings.end(); ++itSet)
            {
                szScope += *itSet;
                szScope += " ";
            }
            szScope += "]";
            szResponseString += szScope;
        }
    }
    else
    {
        value_gotton = szValue;
    }

    return bFind;
}

string GetJsonValue(cJSON *p)
{
    string szValue = "";
    if( NULL == p )
        return szValue;

    if(p->type == cJSON_String)
    {
        szValue = p->valuestring;
    }
    else if(p->type == cJSON_Number )
    {
        char szBuffer[64] = {0};
        snprintf(szBuffer, sizeof(szBuffer), "%d", p->valueint);
        szValue = string(szBuffer);
    }
    return szValue;
}

void GetWeekPlanFromJson(cJSON *p, TimeSpanCfg &timeSpan, int &bHaveParam, int &nResponseCode, string &szResponseString)
{
    int iIndex = 0;
    for( iIndex = 0; iIndex < MAX_WORDDAYTIME_COUNT; iIndex++)
    {
        char szToBeFind[16] = {0};
        snprintf(szToBeFind, sizeof(szToBeFind), "day%d", iIndex);
        GetParamValueU32_fromJson(p, szToBeFind, 0, MAX_S32, timeSpan.workday[iIndex], bHaveParam, nResponseCode, szResponseString);
    }
}

int GetArmingFlagFromJson(cJSON *p, ArmingMode &arming_flag, TimeSpanCfg &timeSpan, int &bHaveParam, int &nResponseCode, string &szResponseString)
{
    string szToBeFind = "ArmingFlag";
    cJSON *pRootNode = cJSON_GetObjectItem(p, szToBeFind.c_str());
    if(NULL == pRootNode )
        return 0;

    bHaveParam = 1;

    if(nResponseCode == 0)
    {
        int nValue = 0;
        string szToBeFind = "ArmingMode";
        if( 1 == GetParamValueS32_fromJson(pRootNode, szToBeFind, ARMING_DISABLE, ARMING_CUSTOM, nValue, bHaveParam, nResponseCode, szResponseString) )
        {
            if(nResponseCode == 0)
                arming_flag = (ArmingMode)nValue;

            __DBG("Get arming_flag=%d\n", arming_flag);
        }
    }
    
    if( nResponseCode == 0 )
    {
        memset(&timeSpan, 0, sizeof(timeSpan));
        string szToBeFind = "WeekPlan";
        cJSON *pWeekPlanNode = cJSON_GetObjectItem(pRootNode, szToBeFind.c_str());
        if(NULL != pWeekPlanNode)
        {
            GetWeekPlanFromJson(pWeekPlanNode, timeSpan, bHaveParam, nResponseCode, szResponseString);
        }
    }

    return bHaveParam;
}

int GetArmingStructDayPlanFromJson(cJSON *p, ArmingStruct &config, int &bHaveParam, int &nResponseCode, string &szResponseString)
{
    string szToBeFind = "DayPlan";
    cJSON *pRootNode = cJSON_GetObjectItem(p, szToBeFind.c_str());
    if(NULL == pRootNode )
        return 0;

    bHaveParam = 1;

    if(nResponseCode == 0)
    {
        int iIndex = 0;
        int nSize = cJSON_GetArraySize(pRootNode);
        for( iIndex = 0; iIndex < nSize; iIndex++)
        {
            cJSON *pRow = cJSON_GetArrayItem(pRootNode, iIndex);
            if( pRow != NULL )
            {
                {
                    string szToBeFind = "startTime";        
                    cJSON * pData = cJSON_GetObjectItem(pRow, szToBeFind.c_str()) ;
                    if( pData != NULL )
                    {
                        string szDayTime = (string)pData->valuestring;

                        DayTime data;
                        if(GetDayTimeFromString(szDayTime, data) != 0 )
                        {
                            __DBG("arming day time:%s: %s invalid\n", szToBeFind.c_str(), szDayTime.c_str());
                            nResponseCode = -1;
                            szResponseString = szToBeFind + " param invalid.";
                            break;
                        }
                        else
                        {
                            config.timeSpans[iIndex].startTime = data;
                            __DBG("arming day time:%d: %s: %s\n", iIndex, szToBeFind.c_str(), szDayTime.c_str());
                        }
                    }
                }

                {
                    string szToBeFind = "endTime";        
                    cJSON * pData = cJSON_GetObjectItem(pRow, szToBeFind.c_str()) ;
                    if( pData != NULL )
                    {
                        string szDayTime = (string)pData->valuestring;

                        DayTime data;
                        if(GetDayTimeFromString(szDayTime, data) != 0 )
                        {
                            __DBG("arming day time:%s: %s invalid\n", szToBeFind.c_str(), szDayTime.c_str());
                            nResponseCode = -1;
                            szResponseString = szToBeFind + " param invalid.";
                            break;
                        }
                        else
                        {
                            config.timeSpans[iIndex].endTime = data;
                        }
                        __DBG("arming day time:%d: %s: %s\n", iIndex, szToBeFind.c_str(), szDayTime.c_str());
                    }
                }

            }
        }
    }

    return bHaveParam;
}

int GetMotionDetectAreaFromJson2(cJSON *p, MotionDetectAlarm *pstMotionAlarmCfg, int &bHaveParam, int &nResponseCode, string &szResponseString)
{
    string szToBeFind = "blockconfig";
    cJSON *pRootNode = cJSON_GetObjectItem(p, szToBeFind.c_str());
    if(NULL == pRootNode )
        return 0;

    bHaveParam = 1;

    string szBlockConfig = "";
    if( nResponseCode == 0 )
    {
        int iIndex = 0;
        int nSize = cJSON_GetArraySize(pRootNode);
        for( iIndex = 0; iIndex < nSize; iIndex++)
        {
            cJSON *pRow = cJSON_GetArrayItem(pRootNode, iIndex);
            if( pRow != NULL )
            {
                cJSON * pData = cJSON_GetObjectItem(pRow, "block_row") ;
                if( pData != NULL )
                {
                    string szTmp = (string)pData->valuestring;
                    szBlockConfig += szTmp;

                    __DBG("motion:%d: %s\n", iIndex, szTmp.c_str());
                }
            }
        }

        snprintf(pstMotionAlarmCfg->blockCfg, MAX_MOTIONDETECT_CONFIG_STRING-1, "%s", szBlockConfig.c_str());
    }

    return bHaveParam;
}

int GetMotionDetectAreaFromJson(cJSON *p, MotionDetectAlarm *pstMotionAlarmCfg, int &bHaveParam, int &nResponseCode, string &szResponseString)
{
    string szToBeFind = "AreaConfig";
    cJSON *pRootNode = cJSON_GetObjectItem(p, szToBeFind.c_str());
    if(NULL == pRootNode )
        return 0;

    bHaveParam = 1;

    int block_col, block_row;
    if( nResponseCode == 0 )
    {
        int bFindCol = 0;
        int bFindRow = 0;
        {
            string szToBeFind = "block_col";
            bFindCol = GetParamValueS32_fromJson(pRootNode, szToBeFind, 1, MD_MAX_GRID_COL, block_col, bHaveParam, nResponseCode, szResponseString);
        }
        {
            string szToBeFind = "block_row";
            bFindRow = GetParamValueS32_fromJson(pRootNode, szToBeFind, 1, MD_MAX_GRID_ROW, block_row, bHaveParam, nResponseCode, szResponseString);
        }
        if( nResponseCode == 0 && bFindCol > 0 && bFindRow >0 )
        {
            pstMotionAlarmCfg->blockCount = (block_col << 16) + block_row;

            //行列正确的情况下才去取区域配置
            GetMotionDetectAreaFromJson2(pRootNode, pstMotionAlarmCfg, bHaveParam, nResponseCode, szResponseString);
        }
    }

    return bHaveParam;
}


int GetObjectDetectTypeFromJson(cJSON *p, unsigned int &type, int &bHaveParam, int &nResponseCode, string &szResponseString)
{
    string szToBeFind = "type";
    cJSON *pRootNode = cJSON_GetObjectItem(p, szToBeFind.c_str());
    if(NULL == pRootNode)
        return 0;

    bHaveParam = 1;

    type = 0;    
    {
        int nValue = 0;
        string szToBeFind = "CAR";
        GetParamValueS32_fromJson(pRootNode, szToBeFind, 0, 1, nValue, bHaveParam, nResponseCode, szResponseString);
        if(nValue > 0 ) 
            BIT_SET_32(type, AI_TYPE_BIT_CAR);
    }
    {
        int nValue = 0;
        string szToBeFind = "MOTO";
        GetParamValueS32_fromJson(pRootNode, szToBeFind, 0, 1, nValue, bHaveParam, nResponseCode, szResponseString);
        if(nValue > 0 ) 
            BIT_SET_32(type, AI_TYPE_BIT_MOTO);
    }
    {
        int nValue = 0;
        string szToBeFind = "ELECTRICBICYCLE";
        GetParamValueS32_fromJson(pRootNode, szToBeFind, 0, 1, nValue, bHaveParam, nResponseCode, szResponseString);
        if(nValue > 0 )
            BIT_SET_32(type, AI_TYPE_BIT_ELECTRICBICYCLE);
    }
    {
        int nValue = 0;
        string szToBeFind = "BICYCLE";
        GetParamValueS32_fromJson(pRootNode, szToBeFind, 0, 1, nValue, bHaveParam, nResponseCode, szResponseString);
        if(nValue > 0 )
            BIT_SET_32(type, AI_TYPE_BIT_BICYCLE);
    }
    {
        int nValue = 0;
        string szToBeFind = "HUMAN";
        GetParamValueS32_fromJson(pRootNode, szToBeFind, 0, 1, nValue, bHaveParam, nResponseCode, szResponseString);
        if(nValue > 0 )
            BIT_SET_32(type, AI_TYPE_BIT_HUMAN);
    }
    {
        int nValue = 0;
        string szToBeFind = "FACE";
        GetParamValueS32_fromJson(pRootNode, szToBeFind, 0, 1, nValue, bHaveParam, nResponseCode, szResponseString);
        if(nValue > 0 )
            BIT_SET_32(type, AI_TYPE_BIT_FACE);
    }
    {
        int nValue = 0;
        string szToBeFind = "NONMOTO_VEHICLE";
        GetParamValueS32_fromJson(pRootNode, szToBeFind, 0, 1, nValue, bHaveParam, nResponseCode, szResponseString);
        if(nValue > 0 )
            BIT_SET_32(type, AI_TYPE_BIT_NONMOTO_VEHICLE);
    }
    {
        int nValue = 0;
        string szToBeFind = "FALLINGOBJECT";
        GetParamValueS32_fromJson(pRootNode, szToBeFind, 0, 1, nValue, bHaveParam, nResponseCode, szResponseString);
        if(nValue > 0 )
            BIT_SET_32(type, AI_TYPE_BIT_FALLINGOBJECT);
    }

    return bHaveParam;
}

int GetPolygonFromJson(cJSON *p, Polygon &config, int &bHaveParam, int &nResponseCode, string &szResponseString)
{
    string szToBeFind = "Polygon";
    cJSON *pRootNode = cJSON_GetObjectItem(p, szToBeFind.c_str());
    if(NULL == pRootNode )
    {
        __ERR("Not found %s", szToBeFind.c_str());
        return 0;
    }

    bHaveParam = 1;
    memset(&config, 0, sizeof(Polygon));
    if( nResponseCode == 0 )
    {
        string szToBeFind = "PointCnt";
        if(1 == GetParamValueS32_fromJson(pRootNode, szToBeFind, 1, MAX_POLYGON_POINT_CNT, config.count, bHaveParam, nResponseCode, szResponseString))
        {
            __DBG("found %s=%d\n", szToBeFind.c_str(), config.count);
        }
    }    
    
    if( nResponseCode == 0 )
    {
        string szToBeFind = "Points";
        cJSON *pSubNode = cJSON_GetObjectItem(pRootNode, szToBeFind.c_str());
        if(NULL != pSubNode )
        {
            int iIndex = 0;
            int nSize = cJSON_GetArraySize(pSubNode);
            for( iIndex = 0; iIndex < nSize; iIndex++)
            {
                cJSON *pRow = cJSON_GetArrayItem(pSubNode, iIndex);
                if( pRow != NULL )
                {
                    {
                        string szToBeFind = "x";
                        GetParamValueS32_fromJson(pRow, szToBeFind, 0, 100, config.points[iIndex].x, bHaveParam, nResponseCode, szResponseString);
                    }
                    {
                        string szToBeFind = "y";
                        GetParamValueS32_fromJson(pRow, szToBeFind, 0, 100, config.points[iIndex].y, bHaveParam, nResponseCode, szResponseString);
                    }
                    __DBG("point:%d: [%d %d]\n", iIndex, config.points[iIndex].x, config.points[iIndex].y);
                }
            }
        }
        else
        {
            __ERR("point not found %s\n", szToBeFind.c_str());
        }
    }

    return bHaveParam;
}

int GetAreaRectFromJson(cJSON *p, PD_AREA_ENTRY &config, int &bHaveParam, int &nResponseCode, string &szResponseString)
{
    string szToBeFind = "Area";
    cJSON *pRootNode = cJSON_GetObjectItem(p, szToBeFind.c_str());
    if(NULL == pRootNode )
    {
        __ERR("Area not found %s\n", szToBeFind.c_str());
        return 0;
    }

    bHaveParam = 1;
    if( nResponseCode == 0 )
    {
        {
            string szToBeFind = "x";
            GetParamValueS32_fromJson(pRootNode, szToBeFind, 0, 100, config.xPos, bHaveParam, nResponseCode, szResponseString);
        }
        {
            string szToBeFind = "y";
            GetParamValueS32_fromJson(pRootNode, szToBeFind, 0, 100, config.yPos, bHaveParam, nResponseCode, szResponseString);
        }
        {
            string szToBeFind = "w";
            GetParamValueS32_fromJson(pRootNode, szToBeFind, 0, 100, config.width, bHaveParam, nResponseCode, szResponseString);
        }
        {
            string szToBeFind = "h";
            GetParamValueS32_fromJson(pRootNode, szToBeFind, 0, 100, config.height, bHaveParam, nResponseCode, szResponseString);
        }
    }

    return bHaveParam;
}

int GetVideoGateRulesFromJson(cJSON *p, VideoLineStruct config[], int &bHaveParam, int &nResponseCode, string &szResponseString)
{
    string szToBeFind = "Rules";
    cJSON *pRootNode = cJSON_GetObjectItem(p, szToBeFind.c_str());
    if(NULL == pRootNode )
    {
        __ERR("Rules not found %s\n", szToBeFind.c_str());
        return 0;
    }

    if( nResponseCode == 0 )
    {
        memset(config, 0, sizeof(VideoLineStruct) * MAX_VIDEO_VG_LINE);
        int iIndex = 0;
        int nSize = cJSON_GetArraySize(pRootNode);
        for( iIndex = 0; iIndex < nSize && iIndex < MAX_VIDEO_VG_LINE; iIndex++)
        {
            VideoLineStruct &data = config[iIndex];
            cJSON *pRow = cJSON_GetArrayItem(pRootNode, iIndex);
            if( pRow != NULL )
            {
                {
                    string szToBeFind = "enable";
                    GetParamValueS32_fromJson(pRow, szToBeFind, 0, 1, data.enable, bHaveParam, nResponseCode, szResponseString);
                }
                {
                    string szToBeFind = "Sensitivity";
                    GetParamValueS32_fromJson(pRow, szToBeFind, 0, 100, data.sensitivity, bHaveParam, nResponseCode, szResponseString);
                }
                GetObjectDetectTypeFromJson(pRow,    data.type,    bHaveParam, nResponseCode, szResponseString);
                {
                    string szToBeFind = "direction";
                    GetParamValueS32_fromJson(pRow, szToBeFind, 0, 2, data.direction, bHaveParam, nResponseCode, szResponseString);
                }
                {
                    string szToBeFind = "x0";
                    GetParamValueS32_fromJson(pRow, szToBeFind, 0, 100, data.x0Pos, bHaveParam, nResponseCode, szResponseString);
                }
                {
                    string szToBeFind = "y0";
                    GetParamValueS32_fromJson(pRow, szToBeFind, 0, 100, data.y0Pos, bHaveParam, nResponseCode, szResponseString);
                }
                {
                    string szToBeFind = "x1";
                    GetParamValueS32_fromJson(pRow, szToBeFind, 0, 100, data.x1Pos, bHaveParam, nResponseCode, szResponseString);
                }
                {
                    string szToBeFind = "y1";
                    GetParamValueS32_fromJson(pRow, szToBeFind, 0, 100, data.y1Pos, bHaveParam, nResponseCode, szResponseString);
                }
                __DBG("Rules:%d: enable=%d, sensitivity=%d, type=%#x, direction=%d, pos0[%d %d] pos1[%d %d]\n", 
                    iIndex, data.enable, data.sensitivity, data.type, data.direction, data.x0Pos, data.y0Pos, data.x1Pos, data.y1Pos);
            }
        }
    }

    return bHaveParam;
}

int GetVideoRegionAIRulesFromJson(cJSON *p, RegionAiUnitStruct config[], int &bHaveParam, int &nResponseCode, string &szResponseString)
{
    string szToBeFind = "Rules";
    cJSON *pRootNode = cJSON_GetObjectItem(p, szToBeFind.c_str());
    if(NULL == pRootNode )
    {
        __ERR("Rules not found %s\n", szToBeFind.c_str());
        return 0;
    }

    if(nResponseCode == 0)
    {
        memset(config, 0, sizeof(RegionAiUnitStruct)*MAX_VIDEO_REGION_AI_NUM);
        int iIndex = 0;
        int nSize = cJSON_GetArraySize(pRootNode);
        for( iIndex = 0; iIndex < nSize && iIndex < MAX_VIDEO_REGION_AI_NUM; iIndex++)
        {
            RegionAiUnitStruct &data = config[iIndex];
            cJSON *pRow = cJSON_GetArrayItem(pRootNode, iIndex);
            if( pRow != NULL )
            {
                {
                    string szToBeFind = "enable";
                    GetParamValueS32_fromJson(pRow, szToBeFind, 0, 1, data.enable, bHaveParam, nResponseCode, szResponseString);
                }
                {
                    string szToBeFind = "Sensitivity";
                    GetParamValueS32_fromJson(pRow, szToBeFind, 0, 100, data.sensitivity, bHaveParam, nResponseCode, szResponseString);
                }
                {
                    string szToBeFind = "stayseconds";
                    GetParamValueS32_fromJson(pRow, szToBeFind, 0, MAX_S32, data.stayseconds, bHaveParam, nResponseCode, szResponseString);
                }
                GetObjectDetectTypeFromJson(pRow,    data.type_filter,    bHaveParam, nResponseCode, szResponseString);
                
                {
                    set<string> setSupportStrings = 
                    {
                        "STAY",
                        "ENTER",
                        "LEAVE"
                    };                    

                    string szToBeFind = "mode";
                    string value_gotton;
                    if(1 == GetParamValueString_fromJson(pRow, szToBeFind, setSupportStrings, 
                        value_gotton, bHaveParam, nResponseCode, szResponseString))
                    {
                        if( nResponseCode == 0 )
                        {
                            if( value_gotton.compare("STAY") == 0)
                                data.mode = AI_RETION_STAY;
                            else if( value_gotton.compare("ENTER") == 0)
                                data.mode = AI_RETION_ENTER;
                            else if( value_gotton.compare("LEAVE") == 0)
                                data.mode = AI_RETION_LEAVE;
                            else
                                data.mode = AI_RETION_STAY;
                        }
                    }
                }
                __DBG("Rules:%d: enable=%d, sensitivity=%d, type=%#x, mode=%d, stayseconds=%d\n", 
                    iIndex, data.enable, data.sensitivity, data.type_filter, data.mode, data.stayseconds);
            }
        }
    }

    return bHaveParam;
}

int GetArmingStructFromJson(cJSON *p, string szRootNodeName, ArmingStruct &config, int &bHaveParam, int &nResponseCode, string &szResponseString)
{
    string szToBeFind = szRootNodeName;
    cJSON *pRootNode = cJSON_GetObjectItem(p, szToBeFind.c_str());
    if(NULL == pRootNode)
        return 0;

    if(nResponseCode == 0)
    {
        int nValue = 0;
        string szToBeFind = "ArmingMode";
        if(1 == GetParamValueS32_fromJson(pRootNode, szToBeFind, ARMING_DISABLE, ARMING_CUSTOM, nValue, bHaveParam, nResponseCode, szResponseString))
        {
            config.enable_flag = (ArmingMode)nValue;
        }
    }

    if(nResponseCode == 0)
    {
        int nValue = 0;
        string szToBeFind = "TimeSpanNum";
        if(1 == GetParamValueS32_fromJson(pRootNode, szToBeFind, 0, DAY_TIMESPAN_MAX_NUM, nValue, bHaveParam, nResponseCode, szResponseString))
        {
            config.timespan_num = (unsigned int)nValue;

            __DBG("TimeSpanNum:%s: enable_flag=%d, timespan_num=%d\n", szRootNodeName.c_str(), config.enable_flag, config.timespan_num);
            GetArmingStructDayPlanFromJson(pRootNode, config, bHaveParam, nResponseCode, szResponseString);
        }
    }

    return bHaveParam;
}

int GetIOOutAlarmActionFromJson(cJSON *p, AlarmOutputAction &config, int &bHaveParam, int &nResponseCode, string &szResponseString)
{
    string szToBeFind = "IOOutputAction";
    cJSON *pRootNode = cJSON_GetObjectItem(p, szToBeFind.c_str());
    if(NULL == pRootNode )
    {
        __ERR("IOOutputAction not found %s\n", szToBeFind.c_str());
        return 0;
    }

    bHaveParam = 1;
    if(nResponseCode == 0)
    {
        string szToBeFind = "channelCnt";
        if(1 == GetParamValueS32_fromJson(pRootNode, szToBeFind, 0, MAX_OUTPUT_CHANENL_COUNT, config.channelCnt, bHaveParam, nResponseCode, szResponseString))
        {
            __DBG("found %s=%d", szToBeFind.c_str(), config.channelCnt);
        }
    }    
    
    if(nResponseCode == 0)
    {
        string szToBeFind = "channels";
        cJSON *pSubNode = cJSON_GetObjectItem(pRootNode, szToBeFind.c_str());
        if(NULL != pSubNode )
        {
            int iIndex = 0;
            int nSize = cJSON_GetArraySize(pSubNode);
            for( iIndex = 0; iIndex < nSize; iIndex++)
            {
                cJSON *pRow = cJSON_GetArrayItem(pSubNode, iIndex);
                if( pRow != NULL )
                {
                    {
                        string szToBeFind = "portIndex";
                        GetParamValueS32_fromJson(pRow, szToBeFind, 1, MAX_OUTPUT_CHANENL_COUNT, config.outputChnlActions[iIndex].portIndex, bHaveParam, nResponseCode, szResponseString);
                    }

                    {
                        string szToBeFind = "enable";
                        GetParamValueS32_fromJson(pRow, szToBeFind, 0, 1, config.outputChnlActions[iIndex].enable, bHaveParam, nResponseCode, szResponseString);
                    }
                    __DBG("channels:%d: portIndex=%d, enable=%d\n", iIndex,config.outputChnlActions[iIndex].portIndex, config.outputChnlActions[iIndex].enable );
                }
            }
        }
        else
        {
            __DBG("channels not found %s\n", szToBeFind.c_str());
        }
    }

    return bHaveParam;
}


int GetAudioActionFromJson(cJSON *p, AudioPlayAction &config, int &bHaveParam, int &nResponseCode, string &szResponseString)
{
    string szToBeFind = "AudioAction";
    cJSON *pRootNode = cJSON_GetObjectItem(p, szToBeFind.c_str());
    if(NULL == pRootNode )
    {
        __ERR("AudioAction not found %s\n", szToBeFind.c_str());
        return 0;
    }


    if(nResponseCode == 0)
    {
        string szToBeFind = "times";
        GetParamValueS32_fromJson(pRootNode, szToBeFind, 0, 1, config.times, bHaveParam, nResponseCode, szResponseString);
    }
    
    if(nResponseCode == 0)
    {
        string szToBeFind = "intervalsecnods";
        GetParamValueS32_fromJson(pRootNode, szToBeFind, 0, 10, config.intervalsecnods, bHaveParam, nResponseCode, szResponseString);
    }
    
    if(nResponseCode == 0)
    {
        set<string> setSupportStrings;
        string szToBeFind = "filename";
        string value_gotton;
        if(1 == GetParamValueString_fromJson(pRootNode, szToBeFind, setSupportStrings, 
            value_gotton, bHaveParam, nResponseCode, szResponseString))
        {
            if( nResponseCode == 0 )
            {
                strncpy(config.filename, value_gotton.c_str(), AUDIO_ACTION_LEN_FILENAME-1);
                __DBG("filename:%s\n", config.filename);
            }
        }
    }
    
    GetArmingStructFromJson(pRootNode, "ArmingSetting", config.enable, bHaveParam, nResponseCode, szResponseString);

    return bHaveParam;
}

cJSON *GetJSON_TimeConfig(TimeConfig *pTimeCfg)
{
    cJSON *pResponseData = cJSON_CreateObject();
    char timestr[128] = {0};
    struct tm *t, tbuf;
    time_t tsec = time(0);

    t = localtime_r(&tsec, &tbuf);
    sprintf(timestr, "%04d-%02d-%02d %02d:%02d:%02d", 
        2000 + t->tm_year - 100, t->tm_mon + 1, t->tm_mday, 
        t->tm_hour, t->tm_min, t->tm_sec);
    
    cJSON_AddStringToObject(pResponseData, "timeMode", pTimeCfg->timeMode.modeName);
    cJSON_AddNumberToObject(pResponseData, "timeZone", pTimeCfg->timeZone);
    cJSON_AddStringToObject(pResponseData, "nowtime", timestr);
    if( strcasecmp(pTimeCfg->timeMode.modeName, "NTP") == 0 )
    {        
        cJSON_AddStringToObject(pResponseData, "serverIP", pTimeCfg->ntpConfig.serverIP);
        cJSON_AddNumberToObject(pResponseData, "serverPort", pTimeCfg->ntpConfig.serverPort);
        cJSON_AddNumberToObject(pResponseData, "refreshInterval", pTimeCfg->ntpConfig.refreshInterval);
    }

    return pResponseData;
}

cJSON *GetJSON_VideoImage(VideoCaptureCfg *pstVideoCapCfg)
{
    cJSON *pResponseData = cJSON_CreateObject();

    cJSON_AddNumberToObject(pResponseData, "brightness", pstVideoCapCfg->brightness);
    cJSON_AddNumberToObject(pResponseData, "contrast", pstVideoCapCfg->contrast);
    cJSON_AddNumberToObject(pResponseData, "saturation", pstVideoCapCfg->saturation);
    cJSON_AddNumberToObject(pResponseData, "sharpness", pstVideoCapCfg->sharpness);

    cJSON_AddNumberToObject(pResponseData, "tvsystem", pstVideoCapCfg->tvsystem);
    cJSON_AddNumberToObject(pResponseData, "hflip", pstVideoCapCfg->hflip);
    cJSON_AddNumberToObject(pResponseData, "vflip", pstVideoCapCfg->vflip);
    cJSON_AddNumberToObject(pResponseData, "rotate", pstVideoCapCfg->rotate);

    return pResponseData;
}

cJSON *GetJSON_VideoLight(VideoCaptureCfg *pstVideoCapCfg)
{
    cJSON *pResponseData = cJSON_CreateObject();
    
    cJSON_AddNumberToObject(pResponseData, "led_ctrl_mode", pstVideoCapCfg->ircut_mode);
    cJSON_AddNumberToObject(pResponseData, "led_work_mode", pstVideoCapCfg->led_mode);
    cJSON_AddNumberToObject(pResponseData, "light_open_brightness", pstVideoCapCfg->ircut_openled_delay);
    cJSON_AddNumberToObject(pResponseData, "light_off_sensitivity", pstVideoCapCfg->light_off_sensitivity);
    cJSON_AddNumberToObject(pResponseData, "led_brightness_mode", pstVideoCapCfg->led_brightness_mode);
    cJSON_AddNumberToObject(pResponseData, "led_brightness_value", pstVideoCapCfg->led_brightness_value);
    
    cJSON_AddStringToObject(pResponseData, "night_starttime", GetJSON_DayTimeStr(pstVideoCapCfg->ircut_nighttime.startTime).c_str());
    cJSON_AddStringToObject(pResponseData, "night_endtime", GetJSON_DayTimeStr(pstVideoCapCfg->ircut_nighttime.endTime).c_str());
    
    return pResponseData;
}

cJSON *GetJSON_LightCtrlModeCapability()
{
    set<IRCutMode> eSupportModeSet;
    const char *config_str = anj_sysctl_get_capability_string();
    int bHideMore = 0;

    eSupportModeSet.insert(IRCUT_Mode_Active);
    eSupportModeSet.insert(IRCUT_Mode_DayNight);

    if(!bHideMore)
    {
        eSupportModeSet.insert(IRCUT_Mode_Passive);
        eSupportModeSet.insert(IRCUT_Mode_Manual);

        if(strstr(config_str, FUNCTION_IRCUT_BY_ADC_HARDWARE) != NULL)
        {
            eSupportModeSet.insert(IRCUT_Mode_AUTO_BY_HARDWARE);
        }
    }
    
    if(strstr(config_str, FUNCTION_IRCUT_LED_MANUAL_SWITCH) != NULL)
    {
        eSupportModeSet.insert(IRCUT_Mode_LIGHT_ALWAYS_ON);
        eSupportModeSet.insert(IRCUT_Mode_LIGHT_ALWAYS_OFF);
    }
    
    cJSON *pResponseData = cJSON_CreateArray();

    set<IRCutMode>::iterator it =  eSupportModeSet.begin();
    for(; it != eSupportModeSet.end(); ++it ) 
    {
        IRCutMode eMode = *it;
        
        cJSON *pNodeAdded = cJSON_CreateObject();    
        cJSON_AddNumberToObject(pNodeAdded, "mode", eMode);
        cJSON_AddItemToArray(pResponseData, pNodeAdded);
    }

    return pResponseData;
}

cJSON *GetJSON_LightWorkModeCapability()
{
    set<LedMode> eSupportModeSet;
    const char *config_str = anj_sysctl_get_capability_string();
    if( strstr(config_str, FUNCTION_LED_TYPE) != NULL)
    {
        if( strstr(config_str, FUNCTION_LEDPANEL_WHITE) != NULL)//纯白光
        {
            eSupportModeSet.insert(LED_PURE_WHITE);
        }
        else if( strstr(config_str, FUNCTION_LEDPANEL_IR) != NULL)//纯红外
        {
            eSupportModeSet.insert(LED_PURE_INFRAED);
        }
        else
        {
            eSupportModeSet.insert(LED_PURE_WHITE);
            eSupportModeSet.insert(LED_PURE_INFRAED);
            eSupportModeSet.insert(LED_INFRAED_THEN_WHITE);//智能双光
        }
    }

    cJSON *pResponseData = cJSON_CreateArray();
    set<LedMode>::iterator it =  eSupportModeSet.begin();
    for(; it != eSupportModeSet.end(); ++it ) 
    {
        LedMode eMode = *it;
        cJSON *pNodeAdded = cJSON_CreateObject();    
        cJSON_AddNumberToObject(pNodeAdded, "mode", eMode);
        cJSON_AddItemToArray(pResponseData, pNodeAdded);
    }
    
    return pResponseData;
}

cJSON *GetJSON_VideoOverlay(VideoOverlay *pstVideoOverlayCfg)
{
    cJSON *pResponseData = cJSON_CreateObject();

    cJSON_AddNumberToObject(pResponseData, "enable", pstVideoOverlayCfg->enable);
    cJSON_AddNumberToObject(pResponseData, "add_overlay", pstVideoOverlayCfg->transparency);
    cJSON_AddNumberToObject(pResponseData, "style", pstVideoOverlayCfg->style);
    cJSON_AddNumberToObject(pResponseData, "DsplayWeek", pstVideoOverlayCfg->bDsplayWeek);
    cJSON_AddNumberToObject(pResponseData, "OverlayFps", pstVideoOverlayCfg->bOverlayFps);
    cJSON_AddNumberToObject(pResponseData, "fontsize", pstVideoOverlayCfg->fontsize);
    cJSON_AddNumberToObject(pResponseData, "time24or12", pstVideoOverlayCfg->time24or12);

    cJSON *pOsdTime = cJSON_CreateObject();
    cJSON_AddNumberToObject(pOsdTime, "posType", pstVideoOverlayCfg->timeOverlay.posType);
    cJSON_AddNumberToObject(pOsdTime, "posX", pstVideoOverlayCfg->timeOverlay.posX);
    cJSON_AddNumberToObject(pOsdTime, "posY", pstVideoOverlayCfg->timeOverlay.posY);
    cJSON_AddStringToObject(pOsdTime, "timeFormat", pstVideoOverlayCfg->timeOverlay.timeFormat.format);
    cJSON_AddItemToObject(pResponseData, "timeOverlay", pOsdTime);

    cJSON *pOsdTitle = cJSON_CreateObject();
    cJSON_AddNumberToObject(pOsdTitle, "posType", pstVideoOverlayCfg->titleOverlay.posType);
    cJSON_AddNumberToObject(pOsdTitle, "posX", pstVideoOverlayCfg->titleOverlay.posX);
    cJSON_AddNumberToObject(pOsdTitle, "posY", pstVideoOverlayCfg->titleOverlay.posY);
    cJSON_AddNumberToObject(pOsdTitle, "titleType", pstVideoOverlayCfg->titleOverlay.titleType);
    if(pstVideoOverlayCfg->titleOverlay.titleType == TYPE_TYPE_BY_TEXT)
    {
        char title_hex[TITLE_MAX_LEN] = {0};
        hexdataTohexStr(pstVideoOverlayCfg->titleOverlay.title_utf8, strlen(pstVideoOverlayCfg->titleOverlay.title_utf8), title_hex, TITLE_MAX_LEN);
        cJSON_AddStringToObject(pOsdTitle, "title_utf8", title_hex);
    }
    else
    {
        cJSON_AddStringToObject(pOsdTitle, "title_utf8", pstVideoOverlayCfg->titleOverlay.title_utf8);
    }

    cJSON_AddItemToObject(pResponseData, "titleOverlay", pOsdTitle);
    return pResponseData;
}


cJSON *GetJSON_VideoUserOverlay_One(UserOSD *pstUserOsd)
{
    cJSON *pResponseData = cJSON_CreateObject();
    
    cJSON_AddNumberToObject(pResponseData, "enable", pstUserOsd->enable);
    cJSON_AddNumberToObject(pResponseData, "posType", pstUserOsd->posType);
    cJSON_AddNumberToObject(pResponseData, "posX", pstUserOsd->pos_xscale);
    cJSON_AddNumberToObject(pResponseData, "posY", pstUserOsd->pos_yscale);

//    cJSON_AddNumberToObject(pResponseData, "color_front", config.color_front);
//    cJSON_AddNumberToObject(pResponseData, "color_back", config.color_back);    
    
//    cJSON_AddNumberToObject(pResponseData, "transparency", config.transparency);
    cJSON_AddNumberToObject(pResponseData, "fontsize", pstUserOsd->fontsize);    
    cJSON_AddNumberToObject(pResponseData, "linegap", pstUserOsd->linegap);    
    cJSON_AddNumberToObject(pResponseData, "titleType", pstUserOsd->titleType);
    
    if(pstUserOsd->titleType == TYPE_TYPE_BY_TEXT)
    {
        char title_hex[TITLE_MAX_LEN];
        memset(title_hex, 0, TITLE_MAX_LEN);
        hexdataTohexStr(pstUserOsd->title_utf8, strlen(pstUserOsd->title_utf8), title_hex, TITLE_MAX_LEN);
        cJSON_AddStringToObject(pResponseData, "title_utf8", title_hex);
    }
    else
    {
        cJSON_AddStringToObject(pResponseData, "title_utf8", pstUserOsd->title_utf8);
    }

    return pResponseData;
}

cJSON *GetJSON_VideoUserOverlay(VideoUserOverlay *pstUserOverlayCfg)
{
    cJSON *pResponseData = cJSON_CreateArray();

    int iIndex = 0;
    for(iIndex = 0; iIndex <MAX_USER_OSD_NUM; iIndex++)
    {
        UserOSD *pstUserOsd = &pstUserOverlayCfg->data[iIndex];
        cJSON *pOneOsd = GetJSON_VideoUserOverlay_One(pstUserOsd);
        
        cJSON_AddItemToObject(pResponseData, "", pOneOsd);
    }

    return pResponseData;
}

cJSON *GetJSON_VideoEncode(VideoEncode *pstVideoEnc)
{
    cJSON *pResponseData = cJSON_CreateArray();

    int iIndex = 0;
    for(iIndex = 0; iIndex < MAX_VENC_CHN; iIndex++)
    {
        if(iIndex == 2)
        {
            const char *config_str = anj_sysctl_get_capability_string();
            if( strstr(config_str, FUNCTION_THREE_VIDEO) == NULL)
            {
                continue;
            }
        }

        const VideoEncodeCfg *pstEncodeCfg = &pstVideoEnc->encodeCfg[iIndex];
        cJSON *pVideoStream = cJSON_CreateObject();
        
        cJSON_AddNumberToObject(pVideoStream, "enable", pstEncodeCfg->enable);
        cJSON_AddNumberToObject(pVideoStream, "streamID", pstEncodeCfg->streamID);
        cJSON_AddStringToObject(pVideoStream, "encodeFormat", pstEncodeCfg->encodeFormat.name);
        cJSON_AddStringToObject(pVideoStream, "resolution", pstEncodeCfg->resolution.name);
        cJSON_AddStringToObject(pVideoStream, "bitRateControl", pstEncodeCfg->bitRateControl.name);
        cJSON_AddNumberToObject(pVideoStream, "gop", pstEncodeCfg->initQuant);
        cJSON_AddNumberToObject(pVideoStream, "frameRate", pstEncodeCfg->display_frameRate);
        cJSON_AddNumberToObject(pVideoStream, "bitRate", pstEncodeCfg->bitRate);
        cJSON_AddNumberToObject(pVideoStream, "bitRateQuality", pstEncodeCfg->bitRateQuality);
        cJSON_AddNumberToObject(pVideoStream, "qp_enable", pstEncodeCfg->qp.qp_enable);
        cJSON_AddNumberToObject(pVideoStream, "qp_min", pstEncodeCfg->qp.qp_min);
        cJSON_AddNumberToObject(pVideoStream, "qp_max", pstEncodeCfg->qp.qp_max);
    
        cJSON_AddItemToObject(pResponseData, "stream", pVideoStream);
    }

    return pResponseData;
}

cJSON *GetJSON_VideoCapability()
{
    int i;
    RESOLUTION_ENTRY *pEntryArray = NULL;
    int entrys_count = anj_sysmng_video_res_array_get(&pEntryArray);
    
    cJSON *pResponseData = cJSON_CreateArray();
    for( i = 0; i < entrys_count; ++i )
    {
        RESOLUTION_ENTRY *pEntry = &pEntryArray[i];
        cJSON *pNodeAdded = cJSON_CreateObject();    
    
        cJSON_AddStringToObject(pNodeAdded, "codec_name", pEntry->codec_name);
        cJSON_AddStringToObject(pNodeAdded, "res_name", pEntry->res_name);
        cJSON_AddNumberToObject(pNodeAdded, "stream_type", pEntry->stream_type);
        cJSON_AddNumberToObject(pNodeAdded, "def_bitrate", pEntry->def_bitrate);
        cJSON_AddNumberToObject(pNodeAdded, "min_bitrate", pEntry->min_bitrate);
        cJSON_AddNumberToObject(pNodeAdded, "max_bitrate", pEntry->max_bitrate);
        cJSON_AddNumberToObject(pNodeAdded, "def_framerate", pEntry->def_framerate);
        cJSON_AddNumberToObject(pNodeAdded, "min_framerate", pEntry->min_framerate);
        cJSON_AddNumberToObject(pNodeAdded, "max_framerate", pEntry->max_framerate);
        cJSON_AddNumberToObject(pNodeAdded, "def_config", pEntry->def_config);
        cJSON_AddItemToArray(pResponseData, pNodeAdded);
    }

    return pResponseData;
}

cJSON *GetJSON_AudioCapability()
{
    int i;
    AUDIO_CODEC_ENTRY *pEntryArray = NULL;
    int entrys_count = anj_sysmng_audio_res_array_get(&pEntryArray);
    
    cJSON *pResponseData = cJSON_CreateArray();
    for( i = 0; i < entrys_count; ++i )
    {
        AUDIO_CODEC_ENTRY *pEntry = &pEntryArray[i];
        cJSON *pNodeAdded = cJSON_CreateObject();    
    
        cJSON_AddStringToObject(pNodeAdded, "codec_name", pEntry->codec_name);
        cJSON_AddNumberToObject(pNodeAdded, "channels", pEntry->channels);
        cJSON_AddNumberToObject(pNodeAdded, "bitspersample", pEntry->bitspersample);
        cJSON_AddNumberToObject(pNodeAdded, "samplerate", pEntry->samplerate);
        cJSON_AddNumberToObject(pNodeAdded, "bitrate", pEntry->bitrate);
        cJSON_AddNumberToObject(pNodeAdded, "def_config", pEntry->def_config);
        cJSON_AddItemToArray(pResponseData, pNodeAdded);
    }
    

    return pResponseData;
}

cJSON *GetJSON_Audio(AudioConfig *ptsAudioCfg)
{
    cJSON *pResponseData = cJSON_CreateObject();

    cJSON_AddNumberToObject(pResponseData, "enable", ptsAudioCfg->audioEncode.enable);
    cJSON_AddNumberToObject(pResponseData, "sampleRate", ptsAudioCfg->audioEncode.sampleRate);
    cJSON_AddStringToObject(pResponseData, "encodeFormat", ptsAudioCfg->audioEncode.audioEncodeType.typeName);
    cJSON_AddNumberToObject(pResponseData, "bitRate", ptsAudioCfg->audioEncode.bitRate);
    
    cJSON_AddNumberToObject(pResponseData, "volume_capture", ptsAudioCfg->audioCapture.volume_capture);
    cJSON_AddNumberToObject(pResponseData, "volume_play", ptsAudioCfg->audioCapture.volume_play);
    cJSON_AddNumberToObject(pResponseData, "amplify", ptsAudioCfg->audioCapture.amplify);
    cJSON_AddNumberToObject(pResponseData, "aec_enable", ptsAudioCfg->audioCapture.aec_enable);
    cJSON_AddNumberToObject(pResponseData, "mute_ptz_turn", ptsAudioCfg->audioCapture.mute_ptz_turn);

    return pResponseData;
}

cJSON *GetJSON_SmartCapablity()
{
    set<string> eSupportCaps;
    const char *config_str = anj_sysctl_get_capability_string();

    const char* pos0 = strstr(config_str, FUNCTION_ALARM_VEHICLE_CAR);
    const char* pos1 = strstr(config_str, FUNCTION_ALARM_VEHICLE_MOTO);
    const char* pos2 = strstr(config_str, FUNCTION_ALARM_VEHICLE_ELECTRICBICYCLE);
    const char* pos3 = strstr(config_str, FUNCTION_ALARM_VEHICLE_BICYCLE);
    const char* pos4 = strstr(config_str, FUNCTION_ALARM_PD);

    int nSupportType = 0;
    if( pos0 != NULL ) BIT_SET_32(nSupportType, AI_TYPE_BIT_CAR);
    if( pos1 != NULL ) BIT_SET_32(nSupportType, AI_TYPE_BIT_MOTO);
    if( pos2 != NULL ) BIT_SET_32(nSupportType, AI_TYPE_BIT_ELECTRICBICYCLE);
    if( pos3 != NULL ) BIT_SET_32(nSupportType, AI_TYPE_BIT_BICYCLE);
    if( pos4 != NULL ) BIT_SET_32(nSupportType, AI_TYPE_BIT_HUMAN);

    if( strstr(config_str, FUNCTION_FACE_FD) != NULL)
        eSupportCaps.insert(FUNCTION_FACE_FD);//人脸检测
    if( strstr(config_str, FUNCTION_FACE_FR) != NULL)
        eSupportCaps.insert(FUNCTION_FACE_FR);//人脸识别
    if( strstr(config_str, FUNCTION_ALARM_LPR) != NULL)
        eSupportCaps.insert(FUNCTION_ALARM_LPR);//车牌识别
    if( strstr(config_str, FUNCTION_ALARM_VIDEOGATE) != NULL)
        eSupportCaps.insert(FUNCTION_ALARM_VIDEOGATE);//拌线检测
    if( strstr(config_str, FUNCTION_ALARM_REGION_AI) != NULL)
        eSupportCaps.insert(FUNCTION_ALARM_REGION_AI);//区域侦测
    if( strstr(config_str, FUNCTION_AUDIO_ALARM) != NULL)
        eSupportCaps.insert(FUNCTION_AUDIO_ALARM);//声音检测
    if( strstr(config_str, FUNCTION_ALARM_COVER) != NULL)
        eSupportCaps.insert(FUNCTION_ALARM_COVER);//视频遮挡
    if( strstr(config_str, FUNCTION_ALARM_FIRE) != NULL)
        eSupportCaps.insert(FUNCTION_ALARM_FIRE);//烟火检测

    if(nSupportType > 0)
        eSupportCaps.insert("TargetDetect");//人/车/非机动车目标检测

    cJSON *pResponseData = cJSON_CreateObject();

    cJSON_AddNumberToObject(pResponseData, "SupportMotionDetect", 1);
    cJSON_AddNumberToObject(pResponseData, "SupportTargetDetect", eSupportCaps.find("TargetDetect") != eSupportCaps.end() ? 1:0);
    cJSON_AddNumberToObject(pResponseData, "SupportFaceFd", eSupportCaps.find(FUNCTION_FACE_FD) != eSupportCaps.end() ? 1:0);
    cJSON_AddNumberToObject(pResponseData, "SupportFaceFr", eSupportCaps.find(FUNCTION_FACE_FR) != eSupportCaps.end() ? 1:0);
    cJSON_AddNumberToObject(pResponseData, "SupportLPR", eSupportCaps.find(FUNCTION_ALARM_LPR) != eSupportCaps.end() ? 1:0);
    cJSON_AddNumberToObject(pResponseData, "SupportVG", eSupportCaps.find(FUNCTION_ALARM_VIDEOGATE) != eSupportCaps.end() ? 1:0);
    cJSON_AddNumberToObject(pResponseData, "SupportRegionAI", eSupportCaps.find(FUNCTION_ALARM_REGION_AI) != eSupportCaps.end() ? 1:0);
    cJSON_AddNumberToObject(pResponseData, "SupportAudioDetect", eSupportCaps.find(FUNCTION_AUDIO_ALARM) != eSupportCaps.end() ? 1:0);
    cJSON_AddNumberToObject(pResponseData, "SupportCoverDetect", eSupportCaps.find(FUNCTION_ALARM_COVER) != eSupportCaps.end() ? 1:0);

    return pResponseData;
}

string GetAudioFileDescription(string szAudioFile)
{
    string szDesctiption;
    string szDescrptionFileName(szAudioFile);
    int pos = szDescrptionFileName.find_last_of(".");
    if( pos > 0 )
    {
        szDescrptionFileName = szDescrptionFileName.substr(0, pos);
        szDescrptionFileName += ".txt";
        if(anj_mw_file_exists(szDescrptionFileName.c_str()))
        {
            char buffer[512] = {0};
            int readlen = read_file_to_buffer(szDescrptionFileName.c_str(), buffer, sizeof(buffer)-1);
            __ERR("Open %s OK, readlen=%d\n", szDescrptionFileName.c_str(), readlen);
            if(readlen > 2 )
            {
//                                                debug_show_data_hex_ex((const unsigned char *)buffer, readlen, 1);
                __ERR("%#x %#x", buffer[0], buffer[1]);
                if(buffer[0] == 0xff && buffer[1] == 0xfe)
                {//UNICODE原始信息
                    char *outbuffer = new char[1024];
                    hexdataTohexStr((const char*)buffer, readlen, outbuffer, 1024);
                    szDesctiption = outbuffer;
                    delete []outbuffer;                                                 
                }
                if( readlen > 4 && 
                    tolower(buffer[0]) == 'f' && 
                    tolower(buffer[1]) == 'f' && 
                    tolower(buffer[2]) == 'f' && 
                    tolower(buffer[3]) == 'e')
                {//UNICODE已经转换成了16进制HEX字符串明文
                    szDesctiption = string(buffer);
                }
            }
        }
    }                            

    return szDesctiption;
}


cJSON *GetJSON_SmartAudioFiles()
{
    cJSON *pResponseData = cJSON_CreateArray();

    int iIndex = 0;
    AudioFileList *pstAudioFileList = http_audio_file_list_get();

    for (iIndex = 0; iIndex < pstAudioFileList->Num; iIndex++)
    {
        cJSON *pNodeAdded = cJSON_CreateObject();    
        cJSON_AddStringToObject(pNodeAdded, "filename", pstAudioFileList->Item[iIndex].file_pathname);

        string szDescrption = GetAudioFileDescription(pstAudioFileList->Item[iIndex].file_pathname);
        if(szDescrption.length() > 0 )
        {
            cJSON_AddStringToObject(pNodeAdded, "UnicodeDesc", szDescrption.c_str());
        }
        
        cJSON_AddItemToArray(pResponseData,  pNodeAdded);
    }

    return pResponseData;
}

cJSON *GetJSON_Smart_ObjectDetectCapablity()
{
    set<string> eSupportCaps;
    const char *config_str = anj_sysctl_get_capability_string();

    cJSON *pResponseData = cJSON_CreateArray();
    if(strstr(config_str, FUNCTION_ALARM_VEHICLE_CAR))
    {
        cJSON *pNodeAdded = cJSON_CreateObject();    
        cJSON_AddNumberToObject(pNodeAdded, "type", AI_TYPE_BIT_CAR);
        cJSON_AddItemToArray(pResponseData, pNodeAdded);
    }
    if(strstr(config_str, FUNCTION_ALARM_VEHICLE_MOTO))
    {
        cJSON *pNodeAdded = cJSON_CreateObject();    
        cJSON_AddNumberToObject(pNodeAdded, "type", AI_TYPE_BIT_MOTO);
        cJSON_AddItemToArray(pResponseData, pNodeAdded);
    }
    if(strstr(config_str, FUNCTION_ALARM_VEHICLE_ELECTRICBICYCLE))
    {
        cJSON *pNodeAdded = cJSON_CreateObject();    
        cJSON_AddNumberToObject(pNodeAdded, "type", AI_TYPE_BIT_ELECTRICBICYCLE);
        cJSON_AddItemToArray(pResponseData, pNodeAdded);
    }
    if(strstr(config_str, FUNCTION_ALARM_VEHICLE_BICYCLE))
    {
        cJSON *pNodeAdded = cJSON_CreateObject();    
        cJSON_AddNumberToObject(pNodeAdded, "type", AI_TYPE_BIT_BICYCLE);
        cJSON_AddItemToArray(pResponseData, pNodeAdded);
    }
    if(strstr(config_str, FUNCTION_ALARM_PD))
    {
        cJSON *pNodeAdded = cJSON_CreateObject();    
        cJSON_AddNumberToObject(pNodeAdded, "type", AI_TYPE_BIT_HUMAN);
        cJSON_AddItemToArray(pResponseData, pNodeAdded);
    }

    return pResponseData;
}

cJSON *GetJSON_Smart_LinkageActionCapablity()
{
    set<string> eSupportCaps;
    const char *config_str = anj_sysctl_get_capability_string();

    cJSON *pResponseData = cJSON_CreateArray();
    {
        cJSON *pNodeAdded = cJSON_CreateObject();    
        cJSON_AddNumberToObject(pNodeAdded, "SupportLightAction", strstr(config_str, FUNCTION_LIGHT_ACTION) != NULL ? 1:0);
        cJSON_AddItemToArray(pResponseData, pNodeAdded);
    }
    {
        cJSON *pNodeAdded = cJSON_CreateObject();    
        cJSON_AddNumberToObject(pNodeAdded, "SupportBRAlarmAction", strstr(config_str, FUNCTION_ALARM_LED) != NULL ? 1:0);
        cJSON_AddItemToArray(pResponseData, pNodeAdded);
    }
    {
        cJSON *pNodeAdded = cJSON_CreateObject();    
        cJSON_AddNumberToObject(pNodeAdded, "SupportAudioAction", strstr(config_str, FUNCTION_AUDIOPLAY_ACTION) != NULL ? 1:0);
        cJSON_AddItemToArray(pResponseData, pNodeAdded);
    }
    {
        cJSON *pNodeAdded = cJSON_CreateObject();    
        cJSON_AddNumberToObject(pNodeAdded, "SupportIOOutAction", strstr(config_str, FUNCTION_GPIO_OUTPUT) != NULL ? 1:0);
        cJSON_AddItemToArray(pResponseData, pNodeAdded);
    }

    return pResponseData;
}


cJSON *GetJSON_MotionDetectArea(MotionDetectAlarm *pstMotionAlarmCfg)
{
    cJSON *pNode = cJSON_CreateObject();

    int iIndex;
    int block_row = pstMotionAlarmCfg->blockCount & 0xffff;     //行
    int block_col = pstMotionAlarmCfg->blockCount >> 16;        //列
    char blockCfg[MAX_MOTIONDETECT_CONFIG_STRING]; 
    memcpy(blockCfg, pstMotionAlarmCfg->blockCfg, sizeof(blockCfg));

    if(block_col > MD_MAX_GRID_COL || block_col <= 0 || block_row > MD_MAX_GRID_ROW || block_row <= 0 )
    {
        block_col = MD_MAX_GRID_COL;
        block_row = MD_MAX_GRID_ROW;
        for(iIndex = 0; iIndex < block_col*block_row; iIndex++)
        {
            blockCfg[iIndex] = '1';            
        }
        blockCfg[iIndex] = 0;
    }
    
    cJSON_AddNumberToObject(pNode, "block_col", block_col);
    cJSON_AddNumberToObject(pNode, "block_row", block_row);

    cJSON *pBlockCfg = cJSON_CreateArray();
    for( iIndex = 0; iIndex < block_row; iIndex++)
    {
        char *p = blockCfg + block_col*iIndex;
        char szRowCfg[32];
        memcpy(szRowCfg, p, block_col);
        szRowCfg[block_col] = 0;

        cJSON *pNodeAdded = cJSON_CreateObject();    
        cJSON_AddStringToObject(pNodeAdded, "block_row", szRowCfg);
        cJSON_AddItemToArray(pBlockCfg, pNodeAdded);
    }
    cJSON_AddItemToObject(pNode, "blockconfig", pBlockCfg);

    return pNode;
}

cJSON *GetJSON_ObjectDetectType(unsigned int type)
{
    cJSON *pNode = cJSON_CreateObject();
    
    cJSON_AddNumberToObject(pNode, "CAR", BIT_GET_32(type, AI_TYPE_BIT_CAR)> 0 ?1:0);
    cJSON_AddNumberToObject(pNode, "MOTO", BIT_GET_32(type, AI_TYPE_BIT_MOTO)> 0 ?1:0);
    cJSON_AddNumberToObject(pNode, "ELECTRICBICYCLE", BIT_GET_32(type, AI_TYPE_BIT_ELECTRICBICYCLE)> 0 ?1:0);
    cJSON_AddNumberToObject(pNode, "BICYCLE", BIT_GET_32(type, AI_TYPE_BIT_BICYCLE)> 0 ?1:0);
    cJSON_AddNumberToObject(pNode, "HUMAN", BIT_GET_32(type, AI_TYPE_BIT_HUMAN)> 0 ?1:0);
    cJSON_AddNumberToObject(pNode, "NONMOTO_VEHICLE", BIT_GET_32(type, AI_TYPE_BIT_NONMOTO_VEHICLE)> 0 ?1:0);
    cJSON_AddNumberToObject(pNode, "FALLINGOBJECT", BIT_GET_32(type, AI_TYPE_BIT_FALLINGOBJECT)> 0 ?1:0);

    return pNode;
}

cJSON *GetJSON_ObjectDetectArea(const Polygon &polygonArea)
{
    cJSON *pNode = cJSON_CreateObject();

    cJSON_AddNumberToObject(pNode, "PointCnt", polygonArea.count);

    cJSON *pNodeArray = cJSON_CreateArray();
    int iIndex;
    for(iIndex = 0; iIndex < MAX_POLYGON_POINT_CNT && iIndex < polygonArea.count; iIndex++)
    {    
        cJSON *pNodeAdded = cJSON_CreateObject();    
        cJSON_AddNumberToObject(pNodeAdded, "x", polygonArea.points[iIndex].x);
        cJSON_AddNumberToObject(pNodeAdded, "y", polygonArea.points[iIndex].y);
        
        cJSON_AddItemToArray(pNodeArray, pNodeAdded);
    }
    cJSON_AddItemToObject(pNode, "Points", pNodeArray);

    return pNode;
}


cJSON *GetJSON_AreaRect(const PD_AREA_ENTRY &area)
{
    cJSON *pNode = cJSON_CreateObject();

    cJSON_AddNumberToObject(pNode, "x", area.xPos);
    cJSON_AddNumberToObject(pNode, "y", area.yPos);
    cJSON_AddNumberToObject(pNode, "w", area.width);
    cJSON_AddNumberToObject(pNode, "h", area.height);

    return pNode;
}

cJSON *GetJSON_VideoGateLine(const VideoLineStruct &data)
{
    cJSON *pNode = cJSON_CreateObject();

    cJSON_AddNumberToObject(pNode, "enable", data.enable);
    cJSON_AddNumberToObject(pNode, "Sensitivity", data.sensitivity);
    cJSON *pType = GetJSON_ObjectDetectType(data.type);
    cJSON_AddItemToObject(pNode, "type", pType);
    cJSON_AddNumberToObject(pNode, "direction", data.direction);
    
    cJSON_AddNumberToObject(pNode, "x0", data.x0Pos);
    cJSON_AddNumberToObject(pNode, "y0", data.y0Pos);
    cJSON_AddNumberToObject(pNode, "x1", data.x1Pos);
    cJSON_AddNumberToObject(pNode, "y1", data.y1Pos);

    return pNode;
}


cJSON *GetJSON_VideoGateLineArray(const VideoLineStruct pData[])
{
    cJSON *pNodeArray = cJSON_CreateArray();
    int iIndex;
    for(iIndex = 0; iIndex < MAX_VIDEO_VG_LINE ; iIndex++)
    {    
        cJSON *pNodeAdded = GetJSON_VideoGateLine(pData[iIndex]);    
        cJSON_AddItemToArray(pNodeArray, pNodeAdded);
    }

    return pNodeArray;
}

cJSON *GetJSON_VideoRegionAI(const RegionAiUnitStruct &data)
{
    cJSON *pNode = cJSON_CreateObject();

    cJSON_AddNumberToObject(pNode, "enable", data.enable);
    cJSON_AddNumberToObject(pNode, "Sensitivity", data.sensitivity);

    cJSON *pType = GetJSON_ObjectDetectType(data.type_filter);
    cJSON_AddItemToObject(pNode, "type", pType);

    switch(data.mode)
    {
        case AI_RETION_STAY:
            cJSON_AddStringToObject(pNode, "mode", "STAY");
            break;
        case AI_RETION_ENTER:
            cJSON_AddStringToObject(pNode, "mode", "ENTER");
            break;
        case AI_RETION_LEAVE:
            cJSON_AddStringToObject(pNode, "mode", "LEAVE");
            break;
        default:
            cJSON_AddStringToObject(pNode, "mode", "STAY");
            break;
    }
    cJSON_AddNumberToObject(pNode, "stayseconds", data.stayseconds);

    return pNode;
}

cJSON *GetJSON_VideoRegionAIArray(const RegionAiUnitStruct pData[])
{
    cJSON *pNodeArray = cJSON_CreateArray();
    int iIndex;
    for(iIndex = 0; iIndex < MAX_VIDEO_REGION_AI_NUM ; iIndex++)
    {    
        cJSON *pNodeAdded = GetJSON_VideoRegionAI(pData[iIndex]);    
        cJSON_AddItemToArray(pNodeArray, pNodeAdded);
    }

    return pNodeArray;
}


cJSON *GetJSON_ArmingWeekPlan(ArmingMode eMode, const TimeSpanCfg &timeSpan)
{
    cJSON *pNode = cJSON_CreateObject();
    cJSON_AddNumberToObject(pNode, "ArmingMode", eMode);
    
    cJSON *pTimeSpan = cJSON_CreateObject();
    int iIndex = 0;
    for( iIndex = 0; iIndex < MAX_WORDDAYTIME_COUNT; iIndex++)
    {
        char szDay[16];
        sprintf(szDay, "day%d", iIndex);
        cJSON_AddNumberToObject(pTimeSpan, szDay, timeSpan.workday[iIndex]);
    }
    cJSON_AddItemToObject(pNode, "WeekPlan", pTimeSpan);
    return pNode;
}

cJSON *GetJSON_ArmingDayPlan(const ArmingStruct &data)
{
    cJSON *pNode = cJSON_CreateObject();
    cJSON_AddNumberToObject(pNode, "ArmingMode", data.enable_flag);
    cJSON_AddNumberToObject(pNode, "TimeSpanNum", data.timespan_num);
    
    cJSON *pTimeSpan = cJSON_CreateArray();
    unsigned int iIndex = 0;
    for( iIndex = 0; iIndex < DAY_TIMESPAN_MAX_NUM && iIndex <data.timespan_num; iIndex++)
    {
        cJSON *pNodeAdded = cJSON_CreateObject();
        cJSON_AddStringToObject(pNodeAdded, "startTime", GetJSON_DayTimeStr(data.timeSpans[iIndex].startTime).c_str());
        cJSON_AddStringToObject(pNodeAdded, "endTime", GetJSON_DayTimeStr(data.timeSpans[iIndex].endTime).c_str());
        
        cJSON_AddItemToArray(pTimeSpan, pNodeAdded);
    }
    cJSON_AddItemToObject(pNode, "DayPlan", pTimeSpan);

    return pNode;
}

cJSON *GetJSON_AudioAction(const AudioPlayAction &data)
{
    cJSON *pNode = cJSON_CreateObject();
    cJSON_AddNumberToObject(pNode, "times", data.times);
    cJSON_AddNumberToObject(pNode, "intervalsecnods", data.intervalsecnods);
    cJSON_AddStringToObject(pNode, "filename", data.filename);

    cJSON *pAction = GetJSON_ArmingDayPlan(data.enable);
    cJSON_AddItemToObject(pNode, "ArmingSetting", pAction);

    return pNode;
}

cJSON *GetJSON_IOOutAction(const AlarmOutputAction &data)
{
    cJSON *pNode = cJSON_CreateObject();
    cJSON_AddNumberToObject(pNode, "channelCnt", data.channelCnt);
    
    cJSON *pChannelArray = cJSON_CreateArray();
    int iIndex = 0;
    for( iIndex = 0; iIndex < MAX_OUTPUT_CHANENL_COUNT && iIndex <data.channelCnt; iIndex++)
    {
        cJSON *pNodeAdded = cJSON_CreateObject();
        cJSON_AddNumberToObject(pNodeAdded, "portIndex", data.outputChnlActions[iIndex].portIndex);
        cJSON_AddNumberToObject(pNodeAdded, "enable", data.outputChnlActions[iIndex].enable);
        
        cJSON_AddItemToArray(pChannelArray, pNodeAdded);
    }
    cJSON_AddItemToObject(pNode, "channels", pChannelArray);
    return pNode;
}

cJSON *GetJSON_MotionDetectAlarm(MotionDetectAlarm *pstMotionAlarmCfg)
{
    cJSON *pResponseData = cJSON_CreateObject();
    cJSON_AddNumberToObject(pResponseData, "enable", pstMotionAlarmCfg->enable);

    cJSON *pArmingWeek = GetJSON_ArmingWeekPlan(pstMotionAlarmCfg->arming_flag, pstMotionAlarmCfg->timeSpan);
    cJSON_AddItemToObject(pResponseData, "ArmingFlag", pArmingWeek);
    
    cJSON *pAreaCfg = GetJSON_MotionDetectArea(pstMotionAlarmCfg);
    cJSON_AddItemToObject(pResponseData, "AreaConfig", pAreaCfg);

    cJSON_AddNumberToObject(pResponseData, "Sensitivity", pstMotionAlarmCfg->sensitivity);
    cJSON_AddNumberToObject(pResponseData, "AlarmThreshold", pstMotionAlarmCfg->alarmThreshold);
    cJSON_AddNumberToObject(pResponseData, "DayNightSwitch", pstMotionAlarmCfg->dayNightSwitch);    
    cJSON_AddNumberToObject(pResponseData, "NightSensitivity", pstMotionAlarmCfg->nightSensitivity);
    cJSON_AddNumberToObject(pResponseData, "NightAlarmThreshold", pstMotionAlarmCfg->nightAlarmThreshold);
    cJSON_AddStringToObject(pResponseData, "NightStartTime", GetJSON_DayTimeStr(pstMotionAlarmCfg->nightTime.startTime).c_str());
    cJSON_AddStringToObject(pResponseData, "NightEndTime", GetJSON_DayTimeStr(pstMotionAlarmCfg->nightTime.endTime).c_str());

    //报警联动
    cJSON *pActionNode = cJSON_CreateObject();
    {
        cJSON *pAction = GetJSON_ArmingDayPlan(pstMotionAlarmCfg->alarmAction.light_twinkle_enable);
        cJSON_AddItemToObject(pActionNode, "LightAction", pAction);
    }
    {
        cJSON *pAction = GetJSON_ArmingDayPlan(pstMotionAlarmCfg->alarmAction.alarm_led_enable);
        cJSON_AddItemToObject(pActionNode, "BRAlarmAction", pAction);
    }
    {
        cJSON *pAction = GetJSON_ArmingDayPlan(pstMotionAlarmCfg->alarmAction.notify_alarmserver_enable);
        cJSON_AddItemToObject(pActionNode, "AlarmServer", pAction);
    }
    {
        cJSON *pAction = GetJSON_ArmingDayPlan(pstMotionAlarmCfg->alarmAction.alarm_push);
        cJSON_AddItemToObject(pActionNode, "AlarmPush", pAction);
    }
    {
        cJSON *pAction = GetJSON_AudioAction(pstMotionAlarmCfg->alarmAction.audioAction);
        cJSON_AddItemToObject(pActionNode, "AudioAction", pAction);
    }
    {
        cJSON *pAction = GetJSON_IOOutAction(pstMotionAlarmCfg->alarmAction.outputAction);
        cJSON_AddItemToObject(pActionNode, "IOOutputAction", pAction);
    }

    cJSON_AddItemToObject(pResponseData, "AlarmAction", pActionNode);
    return pResponseData;
}

cJSON *GetJSON_VideoCoverAlarm(VideoCoverAlarm *pstVideoCoverCfg)
{  
    cJSON *pResponseData = cJSON_CreateObject();
    cJSON_AddNumberToObject(pResponseData, "enable", pstVideoCoverCfg->enable);
    cJSON_AddNumberToObject(pResponseData, "Sensitivity", pstVideoCoverCfg->sensitivity);
    cJSON_AddNumberToObject(pResponseData, "Threadhold_second", pstVideoCoverCfg->threadhold_second);
    cJSON_AddNumberToObject(pResponseData, "BackgroundUpdateSecond", pstVideoCoverCfg->backgroundUpdateSecond);
    
    cJSON *pArmingWeek = GetJSON_ArmingWeekPlan((ArmingMode)pstVideoCoverCfg->enable, pstVideoCoverCfg->timeSpan);
    cJSON_AddItemToObject(pResponseData, "ArmingFlag", pArmingWeek);


    //报警联动
    cJSON *pActionNode = cJSON_CreateObject();
    {
        cJSON *pAction = GetJSON_AudioAction(pstVideoCoverCfg->alarmAction.audioAction);
        cJSON_AddItemToObject(pActionNode, "AudioAction", pAction);
    }
    {
        cJSON *pAction = GetJSON_IOOutAction(pstVideoCoverCfg->alarmAction.outputAction);
        cJSON_AddItemToObject(pActionNode, "IOOutputAction", pAction);
    }
    
    cJSON_AddItemToObject(pResponseData, "AlarmAction", pActionNode);
    

    return pResponseData;
}

cJSON *GetJSON_ObjectDetectAlarm(PdAlarm *pstPdAlarm)
{
    cJSON *pResponseData = cJSON_CreateObject();
    cJSON_AddNumberToObject(pResponseData, "enable", pstPdAlarm->enable);

    cJSON *pType = GetJSON_ObjectDetectType(pstPdAlarm->type);
    cJSON_AddItemToObject(pResponseData, "type", pType);
    
    cJSON *pArmingWeek = GetJSON_ArmingWeekPlan(pstPdAlarm->arming_flag, pstPdAlarm->timeSpan);
    cJSON_AddItemToObject(pResponseData, "ArmingFlag", pArmingWeek);
    
    cJSON *pAreaCfg = GetJSON_ObjectDetectArea(pstPdAlarm->polygonArea);
    cJSON_AddItemToObject(pResponseData, "Polygon", pAreaCfg);

    cJSON_AddNumberToObject(pResponseData, "Sensitivity", pstPdAlarm->sensitivity);
    cJSON_AddNumberToObject(pResponseData, "AlarmThreshold", pstPdAlarm->threshold);
    cJSON_AddNumberToObject(pResponseData, "minTargetRate", pstPdAlarm->minTargetRate);
    cJSON_AddNumberToObject(pResponseData, "allowMd", pstPdAlarm->allowMd);
    cJSON_AddNumberToObject(pResponseData, "nonMotionFilter", pstPdAlarm->nonMotionFilter);
    
    //报警联动
    cJSON *pActionNode = cJSON_CreateObject();
    {
        cJSON *pAction = GetJSON_ArmingDayPlan(pstPdAlarm->alarmAction.light_twinkle_enable);
        cJSON_AddItemToObject(pActionNode, "LightAction", pAction);
    }
    {
        cJSON *pAction = GetJSON_ArmingDayPlan(pstPdAlarm->alarmAction.alarm_led_enable);
        cJSON_AddItemToObject(pActionNode, "BRAlarmAction", pAction);
    }
    {
        cJSON *pAction = GetJSON_ArmingDayPlan(pstPdAlarm->alarmAction.notify_alarmserver_enable);
        cJSON_AddItemToObject(pActionNode, "AlarmServer", pAction);
    }
    {
        cJSON *pAction = GetJSON_ArmingDayPlan(pstPdAlarm->alarmAction.alarm_push);
        cJSON_AddItemToObject(pActionNode, "AlarmPush", pAction);
    }
    {
        cJSON *pAction = GetJSON_AudioAction(pstPdAlarm->alarmAction.audioAction);
        cJSON_AddItemToObject(pActionNode, "AudioAction", pAction);
    }
    {
        cJSON *pAction = GetJSON_IOOutAction(pstPdAlarm->alarmAction.outputAction);
        cJSON_AddItemToObject(pActionNode, "IOOutputAction", pAction);
    }
    {
        cJSON_AddNumberToObject(pActionNode, "draw_rect_enable", pstPdAlarm->alarmAction.draw_rect_enable);
        cJSON_AddNumberToObject(pActionNode, "draw_human_enable", pstPdAlarm->alarmAction.draw_human_enable);
        cJSON_AddNumberToObject(pActionNode, "track_human_enable", pstPdAlarm->alarmAction.track_human_enable);
        cJSON_AddNumberToObject(pActionNode, "rect_twinkle_enable", pstPdAlarm->alarmAction.rect_twinkle_enable);
        cJSON_AddNumberToObject(pActionNode, "auto_zoom_enable", pstPdAlarm->alarmAction.auto_zoom_enable);
        cJSON_AddNumberToObject(pActionNode, "gunball_track_mode", pstPdAlarm->alarmAction.gunball_track_mode);
    }
    
    cJSON_AddItemToObject(pResponseData, "AlarmAction", pActionNode);
    

    return pResponseData;
}

cJSON *GetJSON_LprAlarm(LprAlarm *pstLprAlarmCfg)
{
    cJSON *pResponseData = cJSON_CreateObject();
    cJSON_AddNumberToObject(pResponseData, "enable", pstLprAlarmCfg->enable);
    cJSON_AddNumberToObject(pResponseData, "Sensitivity", pstLprAlarmCfg->sensitivity);
    cJSON_AddNumberToObject(pResponseData, "detectionmode", pstLprAlarmCfg->detectionmode);
    cJSON_AddNumberToObject(pResponseData, "actionInterval", pstLprAlarmCfg->actionInterval);
    cJSON_AddStringToObject(pResponseData, "snapQuality", pstLprAlarmCfg->snapQuality);

    cJSON *pArmingWeek = GetJSON_ArmingWeekPlan(pstLprAlarmCfg->arming_flag, pstLprAlarmCfg->timeSpan);
    cJSON_AddItemToObject(pResponseData, "ArmingFlag", pArmingWeek);

    cJSON *pAreaCfg = GetJSON_ObjectDetectArea(pstLprAlarmCfg->polygonArea);
    cJSON_AddItemToObject(pResponseData, "Polygon", pAreaCfg);

    //报警联动
    cJSON *pActionNode = cJSON_CreateObject();
    {
        cJSON *pAction = GetJSON_ArmingDayPlan(pstLprAlarmCfg->alarmAction.notify_alarmserver_enable);
        cJSON_AddItemToObject(pActionNode, "AlarmServer", pAction);
    }
    {
        cJSON *pAction = GetJSON_ArmingDayPlan(pstLprAlarmCfg->alarmAction.alarm_push);
        cJSON_AddItemToObject(pActionNode, "AlarmPush", pAction);
    }
    {
        cJSON_AddNumberToObject(pActionNode, "draw_rect_enable", pstLprAlarmCfg->alarmAction.draw_rect_enable);
        cJSON_AddNumberToObject(pActionNode, "draw_target_enable", pstLprAlarmCfg->alarmAction.draw_target_enable);
        cJSON_AddNumberToObject(pActionNode, "draw_osd_enable", pstLprAlarmCfg->alarmAction.draw_osd_enable);
        cJSON_AddNumberToObject(pActionNode, "play_voice_enable", pstLprAlarmCfg->alarmAction.play_voice_enable);
    }

    cJSON_AddItemToObject(pResponseData, "AlarmAction", pActionNode);
    return pResponseData;
}

cJSON *GetJSON_VideoGateAlarm(VideoGateAlarm *pstVideoGateAlarm)
{
    cJSON *pResponseData = cJSON_CreateObject();
    cJSON_AddNumberToObject(pResponseData, "enable", pstVideoGateAlarm->enable);

    cJSON *pArmingWeek = GetJSON_ArmingWeekPlan(pstVideoGateAlarm->arming_flag, pstVideoGateAlarm->timeSpan);
    cJSON_AddItemToObject(pResponseData, "ArmingFlag", pArmingWeek);
    
    cJSON *pRules = GetJSON_VideoGateLineArray(pstVideoGateAlarm->data);
    cJSON_AddItemToObject(pResponseData, "Rules", pRules);

    //报警联动
    cJSON *pActionNode = cJSON_CreateObject();
    {
        cJSON *pAction = GetJSON_ArmingDayPlan(pstVideoGateAlarm->alarmAction.light_twinkle_enable);
        cJSON_AddItemToObject(pActionNode, "LightAction", pAction);
    }
    {
        cJSON *pAction = GetJSON_ArmingDayPlan(pstVideoGateAlarm->alarmAction.alarm_led_enable);
        cJSON_AddItemToObject(pActionNode, "BRAlarmAction", pAction);
    }
    {
        cJSON *pAction = GetJSON_ArmingDayPlan(pstVideoGateAlarm->alarmAction.notify_alarmserver_enable);
        cJSON_AddItemToObject(pActionNode, "AlarmServer", pAction);
    }
    {
        cJSON *pAction = GetJSON_ArmingDayPlan(pstVideoGateAlarm->alarmAction.alarm_push);
        cJSON_AddItemToObject(pActionNode, "AlarmPush", pAction);
    }
    {
        cJSON *pAction = GetJSON_AudioAction(pstVideoGateAlarm->alarmAction.audioAction);
        cJSON_AddItemToObject(pActionNode, "AudioAction", pAction);
    }
    {
        cJSON *pAction = GetJSON_IOOutAction(pstVideoGateAlarm->alarmAction.outputAction);
        cJSON_AddItemToObject(pActionNode, "IOOutputAction", pAction);
    }
    {
        cJSON_AddNumberToObject(pActionNode, "draw_rect_enable", pstVideoGateAlarm->alarmAction.draw_rect_enable);
        cJSON_AddNumberToObject(pActionNode, "draw_target_enable", pstVideoGateAlarm->alarmAction.draw_target_enable);
    }
    
    cJSON_AddItemToObject(pResponseData, "AlarmAction", pActionNode);
    return pResponseData;
}

cJSON *GetJSON_VideoRegionAIAlarm(VideoRegionAiAlarm *pstVideoRegionAlarm)
{
    cJSON *pResponseData = cJSON_CreateObject();
    cJSON_AddNumberToObject(pResponseData, "enable", pstVideoRegionAlarm->enable);

    cJSON *pArmingWeek = GetJSON_ArmingWeekPlan(pstVideoRegionAlarm->arming_flag, pstVideoRegionAlarm->timeSpan);
    cJSON_AddItemToObject(pResponseData, "ArmingFlag", pArmingWeek);
    
    cJSON *pAreaCfg = GetJSON_ObjectDetectArea(pstVideoRegionAlarm->polygonArea);
    cJSON_AddItemToObject(pResponseData, "Polygon", pAreaCfg);
    
    cJSON *pRules = GetJSON_VideoRegionAIArray(pstVideoRegionAlarm->data);
    cJSON_AddItemToObject(pResponseData, "Rules", pRules);

    //报警联动
    cJSON *pActionNode = cJSON_CreateObject();
    {
        cJSON *pAction = GetJSON_ArmingDayPlan(pstVideoRegionAlarm->alarmAction.light_twinkle_enable);
        cJSON_AddItemToObject(pActionNode, "LightAction", pAction);
    }
    {
        cJSON *pAction = GetJSON_ArmingDayPlan(pstVideoRegionAlarm->alarmAction.alarm_led_enable);
        cJSON_AddItemToObject(pActionNode, "BRAlarmAction", pAction);
    }
    {
        cJSON *pAction = GetJSON_ArmingDayPlan(pstVideoRegionAlarm->alarmAction.notify_alarmserver_enable);
        cJSON_AddItemToObject(pActionNode, "AlarmServer", pAction);
    }
    {
        cJSON *pAction = GetJSON_ArmingDayPlan(pstVideoRegionAlarm->alarmAction.alarm_push);
        cJSON_AddItemToObject(pActionNode, "AlarmPush", pAction);
    }
    {
        cJSON *pAction = GetJSON_AudioAction(pstVideoRegionAlarm->alarmAction.audioAction);
        cJSON_AddItemToObject(pActionNode, "AudioAction", pAction);
    }
    {
        cJSON *pAction = GetJSON_IOOutAction(pstVideoRegionAlarm->alarmAction.outputAction);
        cJSON_AddItemToObject(pActionNode, "IOOutputAction", pAction);
    }
    {
        cJSON_AddNumberToObject(pActionNode, "draw_rect_enable", pstVideoRegionAlarm->alarmAction.draw_rect_enable);
        cJSON_AddNumberToObject(pActionNode, "draw_target_enable", pstVideoRegionAlarm->alarmAction.draw_target_enable);
    }
    
    cJSON_AddItemToObject(pResponseData, "AlarmAction", pActionNode);
    return pResponseData;
}

cJSON *GetJSON_FaceDetectAlarm(FaceDetectAlarm *pstFaceAlarmCfg)
{
    cJSON *pResponseData = cJSON_CreateObject();
    cJSON_AddNumberToObject(pResponseData, "enable", pstFaceAlarmCfg->enable);


    cJSON *pArmingWeek = GetJSON_ArmingWeekPlan(pstFaceAlarmCfg->arming_flag, pstFaceAlarmCfg->timeSpan);
    cJSON_AddItemToObject(pResponseData, "ArmingFlag", pArmingWeek);
    
    cJSON *pAreaCfg = GetJSON_AreaRect(pstFaceAlarmCfg->area);
    cJSON_AddItemToObject(pResponseData, "Area", pAreaCfg);

    cJSON_AddNumberToObject(pResponseData, "Sensitivity", pstFaceAlarmCfg->sensitivity);
    cJSON_AddNumberToObject(pResponseData, "AlarmThreshold", pstFaceAlarmCfg->threshold);
    
    //报警联动
    cJSON *pActionNode = cJSON_CreateObject();
    {
        cJSON_AddNumberToObject(pActionNode, "draw_rect_enable", pstFaceAlarmCfg->alarmAction.draw_rect_enable);
    }    
    {
        cJSON *pAction = GetJSON_AudioAction(pstFaceAlarmCfg->alarmAction.audioAction);
        cJSON_AddItemToObject(pActionNode, "AudioAction", pAction);
    }
    {
        cJSON *pAction = GetJSON_IOOutAction(pstFaceAlarmCfg->alarmAction.outputAction);
        cJSON_AddItemToObject(pActionNode, "IOOutputAction", pAction);
    }

    cJSON_AddItemToObject(pResponseData, "AlarmAction", pActionNode);
    return pResponseData;
}

cJSON *GetJSON_FlameAndFlumesAlarm(FlameAndFlumesAlarm *pstFlameCfg)
{
    cJSON *pResponseData = cJSON_CreateObject();
    cJSON_AddNumberToObject(pResponseData, "enable", pstFlameCfg->enable);
    cJSON_AddNumberToObject(pResponseData, "Sensitivity", pstFlameCfg->sensitivity);
    cJSON_AddNumberToObject(pResponseData, "Sensitivity_smog", pstFlameCfg->sensitivity_smog);
    cJSON_AddNumberToObject(pResponseData, "actionInterval", pstFlameCfg->actionInterval);
    cJSON_AddStringToObject(pResponseData, "snapQuality", pstFlameCfg->snapQuality);

    cJSON *pArmingWeek = GetJSON_ArmingWeekPlan(pstFlameCfg->arming_flag, pstFlameCfg->timeSpan);
    cJSON_AddItemToObject(pResponseData, "ArmingFlag", pArmingWeek);

    cJSON *pAreaCfg = GetJSON_ObjectDetectArea(pstFlameCfg->polygonArea);
    cJSON_AddItemToObject(pResponseData, "Polygon", pAreaCfg);

    //报警联动
    cJSON *pActionNode = cJSON_CreateObject();
    {
        cJSON *pAction = GetJSON_ArmingDayPlan(pstFlameCfg->alarmAction.light_twinkle_enable);
        cJSON_AddItemToObject(pActionNode, "LightAction", pAction);
    }
    {
        cJSON *pAction = GetJSON_ArmingDayPlan(pstFlameCfg->alarmAction.alarm_led_enable);
        cJSON_AddItemToObject(pActionNode, "BRAlarmAction", pAction);
    }
    {
        cJSON *pAction = GetJSON_ArmingDayPlan(pstFlameCfg->alarmAction.notify_alarmserver_enable);
        cJSON_AddItemToObject(pActionNode, "AlarmServer", pAction);
    }
    {
        cJSON *pAction = GetJSON_ArmingDayPlan(pstFlameCfg->alarmAction.alarm_push);
        cJSON_AddItemToObject(pActionNode, "AlarmPush", pAction);
    }
    {
        cJSON *pAction = GetJSON_AudioAction(pstFlameCfg->alarmAction.audioAction);
        cJSON_AddItemToObject(pActionNode, "AudioAction", pAction);
    }
    {
        cJSON *pAction = GetJSON_IOOutAction(pstFlameCfg->alarmAction.outputAction);
        cJSON_AddItemToObject(pActionNode, "IOOutputAction", pAction);
    }
    {
        cJSON_AddNumberToObject(pActionNode, "draw_rect_enable", pstFlameCfg->alarmAction.draw_rect_enable);
        cJSON_AddNumberToObject(pActionNode, "draw_target_enable", pstFlameCfg->alarmAction.draw_target_enable);
    }

    cJSON_AddItemToObject(pResponseData, "AlarmAction", pActionNode);
    return pResponseData;
}

cJSON *GetJSON_SubscriptionAdd(unsigned int ID, const HapiSubNodeStruct &data)
{
    cJSON *pResponseData = cJSON_CreateObject();
    cJSON_AddNumberToObject(pResponseData, "ID", ID);
    cJSON_AddNumberToObject(pResponseData, "ServerType", data.nServerType);
    cJSON_AddStringToObject(pResponseData, "ServerName", data.szServerName);
    cJSON_AddNumberToObject(pResponseData, "ServerPort", data.nServerPort);
    if(strlen(data.szPostURLPrefix) >0 )
        cJSON_AddStringToObject(pResponseData, "PostURLPrefix", data.szPostURLPrefix);
    if(strlen(data.szEventType) >0 )
        cJSON_AddStringToObject(pResponseData, "EventType", data.szEventType);
    
    cJSON_AddNumberToObject(pResponseData, "CurrentTime", time(0));
    cJSON_AddNumberToObject(pResponseData, "TerminationTime", data.localtime_timeout);

    return pResponseData;
}


//找到分辨率能力集中相应的条目，设置默认比特率帧率，用于更改编码、分辨率时先设置为默认参数
static void SetVideoDefBpsFps(VideoEncodeCfg *pstEncodeCfg)
{
    RESOLUTION_ENTRY *pEntryFound = NULL;
    RESOLUTION_ENTRY *pEntryArray = NULL;
    int nrescount = anj_sysmng_video_res_array_get(&pEntryArray);
    if( nrescount == 0 || pEntryArray == NULL )
    {
        return;
    }
    
    int iIndex = 0;
    for( iIndex = 0; iIndex < nrescount; iIndex++)
    {
        if( 0 == strcmp(pEntryArray[iIndex].codec_name, pstEncodeCfg->encodeFormat.name) && 
            0 == strcmp(pEntryArray[iIndex].res_name, pstEncodeCfg->resolution.name) && 
            pEntryArray[iIndex].stream_type == pstEncodeCfg->streamID - 1)
        {
            pEntryFound = &pEntryArray[iIndex];
            pstEncodeCfg->bitRate = pEntryFound->def_bitrate;
            pstEncodeCfg->frameRate = pEntryFound->def_framerate;
            pstEncodeCfg->display_frameRate = pEntryFound->def_framerate;
            break;
        }
    }
}

static int ValidateVideoParams(VideoEncodeCfg *pstEncodeCfg, string &failreason)
{
    RESOLUTION_ENTRY *pEntryFound = NULL;
    RESOLUTION_ENTRY *pEntryArray = NULL;
    int nrescount = anj_sysmng_video_res_array_get(&pEntryArray);
    if( nrescount == 0 || pEntryArray == NULL )
    {
        failreason = string("Get video capability failed.");
        return 0;
    }

    int nMaxStreamID = 2;
    const char *config_str = anj_sysctl_get_capability_string();
    if( strstr(config_str, FUNCTION_THREE_VIDEO) != NULL)
    {
        nMaxStreamID = 3;
    }

    if( pstEncodeCfg->streamID < 1 && pstEncodeCfg->streamID > nMaxStreamID)
    {
        char szBuffer[256] = {0};
        sprintf(szBuffer, "Video streamID %d error.", pstEncodeCfg->streamID);
        failreason = szBuffer;
        return 0;
    }
    
    int iIndex = 0;
    for( iIndex = 0; iIndex < nrescount; iIndex++)
    {
        if( 0 == strcmp(pEntryArray[iIndex].codec_name, pstEncodeCfg->encodeFormat.name) && 
            0 == strcmp(pEntryArray[iIndex].res_name, pstEncodeCfg->resolution.name) && 
            pEntryArray[iIndex].stream_type == pstEncodeCfg->streamID - 1)
        {
            pEntryFound = &pEntryArray[iIndex];
            break;
        }
    }

    if( NULL == pEntryFound)
    {
        char szBuffer[256];
        sprintf(szBuffer, "invalid encode: %s, res: %s", pstEncodeCfg->encodeFormat.name, pstEncodeCfg->resolution.name);    
        failreason = szBuffer;
        return 0;
    }

    if(pstEncodeCfg->frameRate < pEntryFound->min_framerate|| pstEncodeCfg->frameRate > pEntryFound->max_framerate)
    {
#if 1
        pstEncodeCfg->frameRate= pEntryArray->def_framerate;
#else    
        char szBuffer[256];
        sprintf(szBuffer, "invalid fps: %d, should be in [%d-%d]", 
            pEncodeCfg->frameRate, pEntryFound->min_framerate, pEntryFound->max_framerate);    
        failreason = szBuffer;
        return 0;
#endif        
    }

    if(pstEncodeCfg->bitRate < pEntryFound->min_bitrate|| pstEncodeCfg->bitRate > pEntryFound->max_bitrate)
    {
#if 1
        pstEncodeCfg->bitRate= pEntryArray->def_bitrate;
#else    
        char szBuffer[256];
        sprintf(szBuffer, "invalid bitRate: %d, should be in [%d-%d]", 
            pEncodeCfg->bitRate, pEntryFound->min_bitrate, pEntryFound->max_bitrate);    
        failreason = szBuffer;
        return 0;
#endif        
    }

    if(pstEncodeCfg->qp.qp_enable> 0)
    {
        if(pstEncodeCfg->qp.qp_min < 0  ||
            pstEncodeCfg->qp.qp_min > 51 ||
            pstEncodeCfg->qp.qp_max < 0 ||  
            pstEncodeCfg->qp.qp_max > 51 ||
            pstEncodeCfg->qp.qp_max < pstEncodeCfg->qp.qp_min)
        {
            char szBuffer[256] = {0};
            snprintf(szBuffer, sizeof(szBuffer), "invalid custom qp [%d-%d]", pstEncodeCfg->qp.qp_min, pstEncodeCfg->qp.qp_max);    
            failreason = szBuffer;
            return 0;
        }
    }

    return 1;
}

static int ValidateAudioParams(const AudioEncode &config, string &failreason)
{
    AUDIO_CODEC_ENTRY *pEntryFound = NULL;
    AUDIO_CODEC_ENTRY *pEntryArray = NULL;
    int nrescount = anj_sysmng_audio_res_array_get(&pEntryArray);
    if( nrescount == 0 || pEntryArray == NULL )
    {
        failreason = string("Get audio capability failed.");
        return 0;
    }

    const AudioEncode *pEncodeCfg = &config;
    int iIndex = 0;
    for( iIndex = 0; iIndex < nrescount; iIndex++)
    {
        if( strcmp(pEntryArray[iIndex].codec_name, pEncodeCfg->audioEncodeType.typeName) == 0 && 
            pEntryArray[iIndex].samplerate * 1000 == pEncodeCfg->sampleRate && 
            pEntryArray[iIndex].bitrate * 1000 == pEncodeCfg->bitRate)
        {
            pEntryFound = &pEntryArray[iIndex];
            break;
        }
    }

    if( NULL == pEntryFound)
    {
        char szBuffer[256];
        snprintf(szBuffer, sizeof(szBuffer), "Invalid audio: %s %d %d",pEncodeCfg->audioEncodeType.typeName,  pEncodeCfg->sampleRate, pEncodeCfg->bitRate);
        failreason = szBuffer;
        return 0;
    }

    return 1;
}


void hapi_get_authentication_info(const char *szHttpMethod, 
                                        const char *szURLFullname, 
                                        const char* szMsgBody, 
                                        char *szUsername, int username_len,
                                        char *szPassword, int passwd_len,
                                        char *szUid, int uid_len)
{
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);
    if (szUsername)
        szUsername[0] = '\0';
    if (szPassword)
        szPassword[0] = '\0';
    if (szUid)
        szUid[0] = '\0';

    {
        string szValue;
        string szToBeFind = "username";	

        int bFind = GetParamValue_fromMap(paramMap, szToBeFind, szValue);
        if(bFind)
        {
            strncpy(szUsername, szValue.c_str(), username_len - 1);
            szUsername[username_len - 1] = '\0';
        }
    }

    {
        string szToBeFind = "password";        
        string szValue;
        int bFind = GetParamValue_fromMap(paramMap, szToBeFind, szValue);
        if(bFind)
        {
            strncpy(szPassword, szValue.c_str(), passwd_len - 1);
            szPassword[passwd_len - 1] = '\0';
        }
    }
    {
        string szToBeFind = "uid";        
        string szValue;
        int bFind = GetParamValue_fromMap(paramMap, szToBeFind, szValue);
        if(bFind)
        {
            strncpy(szUid, szValue.c_str(), uid_len - 1);
            szUid[uid_len - 1] = '\0';
        }
    }
}

//HAPI合法性认证
int hapi_authentication(void *pInst, const char* clientip, const char *szURLPrefix, AuthType type, string szUsername, string szPassword, string szUid)
{
    __DBG("host ip:%s\n", clientip);

    string szFailReason;
    if(AUTH_TYPE_BY_UID == type  || AUTH_TYPE_ALL == type)
    {
        if( szUid.length() > 0 )
        {
            if(cgi_session_id_refresh(string(szUid)) != 0 )
            {
                szFailReason = "invalid uid";
                goto __AUTHFAILED;
            }

            return 0;
        }
    }

    if(AUTH_TYPE_BY_USERNAME == type  || AUTH_TYPE_ALL == type)
    {
        if(szUsername.length() > 0)
        {
            char md5Buf[64] = {0};        
            char ipcpasswd[50] = {0};
            if (UserAuthGetPassword((char *)szUsername.c_str(),ipcpasswd) != 0)
            {
                szFailReason = "username error";
                goto __AUTHFAILED;
            }

            MediaStreamConfig *pstMediaStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
            if( pstMediaStreamCfg->webConfig.onvif_auth || !http_hapi_onvif_host_legal(clientip))
            {            
                our_md5_encode(md5Buf, (const unsigned char *)ipcpasswd, strlen(ipcpasswd));

                string ipcpasswd_lower = string(ipcpasswd);
                string md5password_lower = string(md5Buf);
                string szPassword_lower = string(szPassword);

                GetLowerString(ipcpasswd_lower);        //转换为小写
                GetLowerString(md5password_lower);      //转换为小写
                GetLowerString(szPassword_lower);       //转换为小写

                if( szPassword_lower.compare(ipcpasswd_lower) != 0 &&
                    szPassword_lower.compare(md5password_lower) != 0 )
                {
                    szFailReason = "password error";
                    goto __AUTHFAILED;
                }
            }

            return 0;
        }
    }

    if(AUTH_TYPE_BY_UID == type )
        szFailReason = "uid missing";
    else if(AUTH_TYPE_BY_USERNAME == type )
        szFailReason = "username/password missing";
    else
        szFailReason = "username/password and uid are missing";
    __INFO("failed reason:%s\n", szFailReason.c_str());

    goto __AUTHFAILED;

__AUTHFAILED:
    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", "invalid");
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", -1);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szFailReason.c_str());
    cJSON_AddStringToObject(pResponseNode, "Data", "null");
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, RESPONSE_ROOT, pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);
    
    if( NULL != pJsonText )
    {
        http_response_cb(pInst, pJsonText, 401);
        anj_mw_free(pJsonText);
    }

    return -1;
}

//    根据username和password分配uid
int hapi_uid_getuid(void *pInst, 
                        const char *szHttpMethod, 
                        const char *szURLFullname, 
                        const char* szMsgBody, 
                        const char *szURLPrefix, 
                        const char *clientip, 
                        const char* host, 
                        string szUid)
{
    __DBG("url:%s\n", szURLFullname);
    szUid = cgi_session_id_get();
    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", 0);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", RESPONSE_OK);
    cJSON_AddStringToObject(pResponseNode, "Data", "null");
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);
    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }
    return HTTP_RES_STATUS_OK;    
}

int hapi_uid_keep_alive(void *pInst, 
                            const char *szHttpMethod, 
                            const char *szURLFullname, 
                            const char* szMsgBody, 
                            const char *szURLPrefix, 
                            const char *clientip, 
                            const char* host, 
                            string szUid)
{
    __DBG("url:%s\n", szURLFullname);
    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", 0);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", RESPONSE_OK);
    cJSON_AddStringToObject(pResponseNode, "Data", "null");
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);
    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);
        anj_mw_free(pJsonText);
    }
    
    return HTTP_RES_STATUS_OK;    
}

int hapi_sysinfo_deviceinfo(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);
    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", 0);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", RESPONSE_OK);

    cJSON *pResponseData = cJSON_CreateObject();    

    {
        char mySN[20] = {0};
        anj_sysmng_load_sn(mySN, sizeof(mySN));
        cJSON_AddStringToObject(pResponseData, "SN", mySN);
    }

    {
        char szDeviceType[64] = {0};
        GetDeviceTypeStr(szDeviceType);
        
        AjOemStruct stOemInfo = {0};
        anj_config_oem_get(&stOemInfo);
        
        if(strlen(stOemInfo.szDeviceType) >0 )
        {
            cJSON_AddStringToObject(pResponseData, "device_type", stOemInfo.szDeviceType);
            cJSON_AddStringToObject(pResponseData, "model", szDeviceType);
        }
        else
        {
            cJSON_AddStringToObject(pResponseData, "device_type", szDeviceType);
        }
    }
    
    {
        NetworkConfigNew *pstNetworkCfg = (NetworkConfigNew *)getNetWorkConfig();
        cJSON_AddStringToObject(pResponseData, "ether", (char*)pstNetworkCfg->lanCfg.MACAddress);
    }
    
    {
        SYSTEM_VERSION_DATA stSystemVersion = {0};
        anj_sysmng_version_info_get(&stSystemVersion, 0);
        cJSON_AddStringToObject(pResponseData, "kernelversion", stSystemVersion.kernelVersion);
        cJSON_AddStringToObject(pResponseData, "fsversion", stSystemVersion.fsVersion);
    }
    
    cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);
    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }
    
    return HTTP_RES_STATUS_OK;    
}

int hapi_sysinfo_capability(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);
    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", 0);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", RESPONSE_OK);

    cJSON *pFunctionList = cJSON_CreateArray(); //这里是数组，才会有方括号
    char *config_str = anj_sysctl_get_capability_string();
    if( NULL != config_str)
    {
        const char s[4] = "+";
        char *token = NULL;
        char *pTmpStr = NULL;
        token = strtok_r(config_str, s, &pTmpStr); 
        while( token != NULL ) 
        {
            string szCapability = string(token);
            GetTrimString(szCapability);
            if( szCapability.length() > 0)
            {
                cJSON *pNodeAdded = cJSON_CreateObject();    
                cJSON_AddStringToObject(pNodeAdded, "caps", szCapability.c_str());
                cJSON_AddItemToArray(pFunctionList, pNodeAdded);
            }

            token = strtok_r(NULL, s, &pTmpStr);
        }
    }

    cJSON_AddItemToObject(pResponseNode, "Data", pFunctionList);

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }
    
    return HTTP_RES_STATUS_OK;      
}

int hapi_sysinfo_rtspurl(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);
    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", 0);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", RESPONSE_OK);

    cJSON *pResponseData = cJSON_CreateObject();    

    MediaStreamConfig *pstMediaStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();

    char szRtspUrl[128] = {0};
    sprintf(szRtspUrl, "rtsp://%s:%d/stream0", host && (*host != 0) ? host : "*.*.*.*", pstMediaStreamCfg->rtspConfig.videoPort);
    cJSON_AddStringToObject(pResponseData, "ch0_main", szRtspUrl);
    sprintf(szRtspUrl, "rtsp://%s:%d/stream1", host && (*host != 0) ? host : "*.*.*.*", pstMediaStreamCfg->rtspConfig.videoPort);
    cJSON_AddStringToObject(pResponseData, "ch0_sub", szRtspUrl);

    cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;      
}

int hapi_sysman_reboot(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);
    __WARN("hapi reboot service request from:%s\n", clientip);
    __RECORD_LOG_INFO("hapi reboot service request from:%s\n", clientip);
    anj_sysmng_delay_reboot(2);

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", 0);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", RESPONSE_OK);
    cJSON_AddStringToObject(pResponseNode, "Data", "null");
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;   
}

int hapi_sysman_factory(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    unsigned int reserved_bits = 0;         
    __RECORD_LOG_INFO("restore config reserved_bits=%u\n", reserved_bits); 
    anj_sysmng_config_restore(reserved_bits);

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", 0);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", RESPONSE_OK);
    cJSON_AddStringToObject(pResponseNode, "Data", "null");
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);
    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;    
}

int hapi_io_input_get(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);

    int chn = -1;

    URLParamStruct::iterator itMap = paramMap.find("chn");
    if( itMap != paramMap.end())
    {
        chn = atoi(itMap->second.c_str());
    }

    int status = -1;
    if(chn <= 0)
    {
        __ERR("error: param chn not found\n");
        szResponseString = "params error";
        nResponseCode = -1;
    }
    else
    {
        status = anj_mw_hwctrl_alarmin_chn_status_get(chn);
        if(status != 0 && status != 1)
        {
            __ERR("error: io input chn %d not exist\n", chn);
            szResponseString = "params error";
            nResponseCode = -1;
        }
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0 )
    {
        cJSON_AddNumberToObject(pResponseNode, "Data", status);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;     
}

int hapi_io_output_get(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);

    int chn = -1;
    URLParamStruct::iterator itMap = paramMap.find("chn");
    if( itMap != paramMap.end())
    {
        chn = atoi(itMap->second.c_str());
    }

    int status = -1;
    if(chn <= 0)
    {
        __ERR("error: param chn not found\n");
        szResponseString = "params error";
        nResponseCode = -1;
    }
    else
    {
        status = anj_mw_hwctrl_alarmout_chn_status_get(chn);
        if(status != 0 && status != 1)
        {
            __ERR("error: io output chn %d not exist\n", chn);
            szResponseString = "params error";
            nResponseCode = -1;
        }
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0 )
    {
        cJSON_AddNumberToObject(pResponseNode, "Data", status);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;  
}

int hapi_io_output_set(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);

    int chn = -1;
    int status = -1;

    URLParamStruct::iterator itMap = paramMap.find("chn");
    if( itMap != paramMap.end())
    {
        chn = atoi(itMap->second.c_str());
    }
    itMap = paramMap.find("status");
    if( itMap != paramMap.end())
    {
        status = atoi(itMap->second.c_str());
    }    

    if( chn <= 0 || (status != 0 && status != 1))
    {
        __ERR("error: param chn not found\n");
        szResponseString = "params error";
        nResponseCode = -1;
    }
    else
    {
        int oldstatus = anj_mw_hwctrl_alarmout_chn_status_get(chn);
        if((oldstatus != 0 && oldstatus != 1))
        {
            __ERR("error: io output chn %d not exist\n", chn);
            szResponseString = "params error";
            nResponseCode = -1;
        }
        else
        {
            int ret = anj_mw_hwctrl_alarmout_chn_status_set(chn, status);
            if(ret < 0)
            {
                __ERR("error: io output chn %d not exist\n", chn);
                szResponseString = "params error";
                nResponseCode = -1;
            }
        }
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0 )
    {
        cJSON_AddNumberToObject(pResponseNode, "Data", status);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;  
}

int hapi_systime_gettime(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    TimeConfig *pstTimeCfg = &pstSystemCfg->timeCfg;

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_TimeConfig(pstTimeCfg);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;  
}

int hapi_systime_settime(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    TimeConfig *pstTimeCfg = &pstSystemCfg->timeCfg;

    int nTimeZone = -1;//时区
    string szNowTime = "";
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);

    if( nResponseCode == 0 )
    {
        string szToBeFind = "localtime";        
        string szValue;
        int bFind = GetParamValue_fromMap(paramMap, szToBeFind, szValue);
        if(bFind)
        {
            szNowTime = szValue;
        }
    }

    if( nResponseCode == 0 )
    {
        string szToBeFind = "timezone";        
        string szValue;
        int bFind = GetParamValue_fromMap(paramMap, szToBeFind, szValue);
        if(bFind)
        {
            nTimeZone = atoi(szValue.c_str());
            if(nTimeZone != pstTimeCfg->timeZone)
            {
                if(nTimeZone < 0 || nTimeZone > 1440)
                {
                    __ERR("timezone invalid, set to current config %d\n", pstTimeCfg->timeZone);
                    szResponseString = "param timezone invalid";
                    nResponseCode = -1;
                }
                else
                {
                    __INFO("set timezone from %d to %d\n", pstTimeCfg->timeZone, nTimeZone);
                    pstTimeCfg->timeZone = nTimeZone;
                    anj_config_system_time_set(pstTimeCfg);
                }
            }
        }
    }

    if( szNowTime.length() == 0 && nTimeZone < 0)
    {
        szResponseString = "param time and timezone are not found";
        nResponseCode = -1;
    }
    else if( szNowTime.length() > 0)
    {
        __DBG("set time to %s\n", szNowTime.c_str());
        int year, mon, day, hour, min, sec;    
        if(6 == sscanf(szNowTime.c_str(), "%04d%02d%02d%02d%02d%02d", &year, &mon, &day, &hour, &min, &sec))
        {
            struct tm time;
            memset(&time, 0, sizeof(time));
            time.tm_sec = sec;
            time.tm_min = min;
            time.tm_hour = hour;
            time.tm_mday = day;
            time.tm_mon = mon-1; 
            time.tm_year = year-1900; 

            anj_systime_set_time_and_zone(time, nTimeZone, 1);
        }
        else
        {
            __ERR("localtime %s error\n", szNowTime.c_str());
            szResponseString = "param localtime error";
            nResponseCode = -1;
        }
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0)
    {
        cJSON *pResponseData = GetJSON_TimeConfig(pstTimeCfg);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;   
}

int hapi_systime_setntp(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    TimeConfig *pstTimeCfg = &pstSystemCfg->timeCfg;

    string szServer = "";
    unsigned short nPort = 123;
    unsigned int refreshInterval = 60;
    
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);

    URLParamStruct::iterator itMap = paramMap.find("serverip");
    if( itMap != paramMap.end())
    {
        szServer = itMap->second;
    }        
    itMap = paramMap.find("serverport");
    if( itMap != paramMap.end())
    {
        nPort = atoi(itMap->second.c_str());
    }        
    itMap = paramMap.find("refreshinterval");
    if( itMap != paramMap.end())
    {
        refreshInterval = atoi(itMap->second.c_str());
    } 

    if( szServer.length() == 0 || nPort == 0 || refreshInterval == 0 )
    {
        __ERR("error: %s %d %d\n", szServer.c_str(), nPort, refreshInterval);
        szResponseString = "params error";
        nResponseCode = -1;
    }
    else
    {
        strcpy(pstTimeCfg->timeMode.modeName, "NTP");
        strcpy(pstTimeCfg->ntpConfig.serverIP, szServer.c_str());
        pstTimeCfg->ntpConfig.serverPort= nPort;
        pstTimeCfg->ntpConfig.refreshInterval= refreshInterval;

        anj_config_system_time_set(pstTimeCfg);
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_TimeConfig(pstTimeCfg);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;  
}

int hapi_pztctrl_stop(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", 0);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", RESPONSE_OK);
    cJSON_AddStringToObject(pResponseNode, "Data", "null");

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    PtzCmdParse stPtzCmdParse = {0};
    EventResult event_result = {0};

    StrCpy(stPtzCmdParse.ptzCmd, sizeof(stPtzCmdParse.ptzCmd), "stop");
    eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);

    return HTTP_RES_STATUS_OK;
}

int hapi_pztctrl_move_direction(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    set<string> setSupportDirection = {"left", "right", "up", "down", "left_up", "right_up", "left_down", "right_down"};
    int speed = 5;
    string szDirection = "";

    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);
    if( nResponseCode == 0 )
    {
        string szToBeFind = "direction";        
        if(GetParamValueString_fromMap(paramMap, szToBeFind, setSupportDirection, szDirection, bHaveParam, nResponseCode, szResponseString) != 1)
        {
            szResponseString = szToBeFind + " param not found";
            nResponseCode = -1;
        }
    }

    if(nResponseCode == 0)
    {
        string szToBeFind = "speed";        
        GetParamValueS32_fromMap(paramMap, szToBeFind, 1, 10, speed, bHaveParam, nResponseCode, szResponseString);
    }

    PtzCmdParse stPtzCmdParse = {0};
    EventResult event_result = {0};

    if(nResponseCode == 0)
    {
        if(speed < 1 || speed >10)
        {
            szResponseString = "param speed error";
            nResponseCode = -1;
        }
        else
        {
            StrCpy(stPtzCmdParse.ptzCmd, sizeof(stPtzCmdParse.ptzCmd), szDirection.c_str());
            eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        }
    }

    int nAutoStopTime = 0;
    if( nResponseCode == 0 )
    {
        string szToBeFind = "autostop";        
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, MAX_S32, nAutoStopTime, bHaveParam, nResponseCode, szResponseString);
        if(nAutoStopTime > 1000)
            nAutoStopTime = 1000;

        if(nAutoStopTime > 0)
        {
            usleep(nAutoStopTime * 1000);

            memset(&stPtzCmdParse, 0, sizeof(stPtzCmdParse));
            StrCpy(stPtzCmdParse.ptzCmd, sizeof(stPtzCmdParse.ptzCmd), "stop");
            eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);

        }
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = cJSON_CreateObject();
        cJSON_AddStringToObject(pResponseData, "direction", szDirection.c_str());
        cJSON_AddNumberToObject(pResponseData, "speed", speed);
        cJSON_AddNumberToObject(pResponseData, "autostop", nAutoStopTime);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;   
}

int hapi_pztctrl_preset(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    set<string> setSupportMethod = {"set", "call", "delete"};
    int presetno = -1;
    string szMethod = "";

    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);
    if( nResponseCode == 0 )
    {
        string szToBeFind = "method";        
        if(GetParamValueString_fromMap(paramMap, szToBeFind, setSupportMethod, szMethod, bHaveParam, nResponseCode, szResponseString) != 1)
        {
            szResponseString = szToBeFind + " param not found";
            nResponseCode = -1;
        }
    }

    if(nResponseCode == 0)
    {
        string szToBeFind = "presetno";        
        if(GetParamValueS32_fromMap(paramMap, szToBeFind, 1, 255, presetno, bHaveParam, nResponseCode, szResponseString) != 1)
        {
            szResponseString = szToBeFind + " param not found";
            nResponseCode = -1;
        }
    }

    PtzCmdParse stPtzCmdParse = {0};
    EventResult event_result = {0};

    if( nResponseCode == 0)
    {
        if(szMethod.compare("set") == 0)
        {
            StrCpy(stPtzCmdParse.ptzCmd, sizeof(stPtzCmdParse.ptzCmd), "setpreset");
            stPtzCmdParse.presetID = presetno;
        }
        else if(szMethod.compare("call") == 0)
        {
            StrCpy(stPtzCmdParse.ptzCmd, sizeof(stPtzCmdParse.ptzCmd), "callpreset");
            stPtzCmdParse.presetID = presetno;
        }
        else
        {
            StrCpy(stPtzCmdParse.ptzCmd, sizeof(stPtzCmdParse.ptzCmd), "clearpreset");
            stPtzCmdParse.presetID = presetno;
        }

        eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = cJSON_CreateObject();
        cJSON_AddStringToObject(pResponseData, "method", szMethod.c_str());
        cJSON_AddNumberToObject(pResponseData, "presetno", presetno);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;  
}

int hapi_pztctrl_zoom(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    set<string> setSupportDirection = {"in", "out"};
    string szDirection = "";

    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);
    if(nResponseCode == 0)
    {
        string szToBeFind = "direction";        
        if(GetParamValueString_fromMap(paramMap, szToBeFind, setSupportDirection, szDirection, bHaveParam, nResponseCode, szResponseString) != 1)
        {
            szResponseString = szToBeFind + " param not found";
            nResponseCode = -1;
        }
    }

    PtzCmdParse stPtzCmdParse = {0};
    EventResult event_result = {0};

    if(nResponseCode == 0)
    {
        if(szDirection.compare("in") == 0)
        {
            StrCpy(stPtzCmdParse.ptzCmd, sizeof(stPtzCmdParse.ptzCmd), "zoomtele");
        }
        else
        {
            StrCpy(stPtzCmdParse.ptzCmd, sizeof(stPtzCmdParse.ptzCmd), "zoomwide");
        }
        eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
    }

    int nAutoStopTime = 0;
    if( nResponseCode == 0 )
    {
        string szToBeFind = "autostop";        
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, MAX_S32, nAutoStopTime, bHaveParam, nResponseCode, szResponseString);
        if(nAutoStopTime > 1000)
            nAutoStopTime = 1000;

        if(nAutoStopTime > 0)
        {
            usleep(nAutoStopTime * 1000);

            memset(&stPtzCmdParse, 0, sizeof(stPtzCmdParse));
            StrCpy(stPtzCmdParse.ptzCmd, sizeof(stPtzCmdParse.ptzCmd), "stop");
            eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        }
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = cJSON_CreateObject();
        cJSON_AddStringToObject(pResponseData, "direction", szDirection.c_str());
        cJSON_AddNumberToObject(pResponseData, "autostop", nAutoStopTime);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;
}

int hapi_pztctrl_focus(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    set<string> setSupportDirection = {"near", "far"};
    string szDirection = "";

    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);
    if(nResponseCode == 0)
    {
        string szToBeFind = "direction";        
        if(GetParamValueString_fromMap(paramMap, szToBeFind, setSupportDirection, szDirection, bHaveParam, nResponseCode, szResponseString) != 1)
        {
            szResponseString = szToBeFind + " param not found";
            nResponseCode = -1;
        }
    }

    PtzCmdParse stPtzCmdParse = {0};
    EventResult event_result = {0};

    if(nResponseCode == 0)
    {
        if(szDirection.compare("near") == 0)
        {
            StrCpy(stPtzCmdParse.ptzCmd, sizeof(stPtzCmdParse.ptzCmd), "FocusNearAutoOff");
        }
        else
        {
            StrCpy(stPtzCmdParse.ptzCmd, sizeof(stPtzCmdParse.ptzCmd), "FocusFarAutoOff");
        }
        eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
    }
    
    int nAutoStopTime = 0;
    if( nResponseCode == 0 )
    {
        string szToBeFind = "autostop";        
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, MAX_S32, nAutoStopTime, bHaveParam, nResponseCode, szResponseString);
        if(nAutoStopTime > 1000)
            nAutoStopTime = 1000;

        if(nAutoStopTime > 0)
        {
            usleep(nAutoStopTime * 1000);

            memset(&stPtzCmdParse, 0, sizeof(stPtzCmdParse));
            StrCpy(stPtzCmdParse.ptzCmd, sizeof(stPtzCmdParse.ptzCmd), "stop");
            eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        }
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = cJSON_CreateObject();
        cJSON_AddStringToObject(pResponseData, "direction", szDirection.c_str());
        cJSON_AddNumberToObject(pResponseData, "autostop", nAutoStopTime);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;
}

int hapi_pztctrl_iris(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    set<string> setSupportDirection = {"open", "close"};
    string szDirection = "";

    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);
    if( nResponseCode == 0 )
    {
        string szToBeFind = "direction";        
        if(GetParamValueString_fromMap(paramMap, szToBeFind, setSupportDirection, szDirection, bHaveParam, nResponseCode, szResponseString) != 1)
        {
            szResponseString = szToBeFind + " param not found";
            nResponseCode = -1;
        }
    }

    PtzCmdParse stPtzCmdParse = {0};
    EventResult event_result = {0};

    if(nResponseCode == 0)
    {
        if(szDirection.compare("open") == 0)
        {
            StrCpy(stPtzCmdParse.ptzCmd, sizeof(stPtzCmdParse.ptzCmd), "IrisOpenAutoOff");
        }
        else
        {
            StrCpy(stPtzCmdParse.ptzCmd, sizeof(stPtzCmdParse.ptzCmd), "IrisCloseAutoOff");
        }

        eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        
    }
    int nAutoStopTime = 0;
    if( nResponseCode == 0 )
    {
        string szToBeFind = "autostop";        
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, MAX_S32, nAutoStopTime, bHaveParam, nResponseCode, szResponseString);
        if(nAutoStopTime > 1000)
            nAutoStopTime = 1000;

        if(nAutoStopTime > 0)
        {
            usleep(nAutoStopTime * 1000);

            memset(&stPtzCmdParse, 0, sizeof(stPtzCmdParse));
            StrCpy(stPtzCmdParse.ptzCmd, sizeof(stPtzCmdParse.ptzCmd), "stop");
            eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        }
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = cJSON_CreateObject();
        cJSON_AddStringToObject(pResponseData, "direction", szDirection.c_str());
        cJSON_AddNumberToObject(pResponseData, "autostop", nAutoStopTime);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;
}

int hapi_pztctrl_advfunction_exec(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    set<string> setSupportFunctions;
    string szFunction = "";

    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    PTZConfig *pstPtzConfig = &pstSystemCfg->ptzCfg;

    PtzCmdParse stPtzCmdParse = {0};
    EventResult event_result = {0};

    int iIndex = 0;
    for(iIndex = 0; iIndex < MAX_PTZFUCTION_COUNT && iIndex < pstPtzConfig->advanceCfg.functionCnt; iIndex++)
    {
        string szFunction = pstPtzConfig->advanceCfg.functions[iIndex].functionName;
        
        if(szFunction.length() >0 )
        {
            setSupportFunctions.insert(szFunction);
        }
    }

    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);
    if( nResponseCode == 0 )
    {
        string szToBeFind = "functionname";
        if(GetParamValueString_fromMap(paramMap, szToBeFind, setSupportFunctions, szFunction, bHaveParam, nResponseCode, szResponseString) != 1)
        {
            szResponseString = szToBeFind + " param not found";
            nResponseCode = -1;
        }
    }

    if( nResponseCode == 0 )
    {
        StrCpy(stPtzCmdParse.ptzCmd, sizeof(stPtzCmdParse.ptzCmd), szFunction.c_str());
        eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
    }
    
    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0)
    {
        cJSON *pResponseData = cJSON_CreateObject();
        cJSON_AddStringToObject(pResponseData, "functionname", szFunction.c_str());
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;
}

int hapi_pztctrl_advfunction_list(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    set<string> setSupportFunctions;

    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    PTZConfig *pstPtzConfig = &pstSystemCfg->ptzCfg;

    int iIndex = 0;
    for(iIndex = 0; iIndex < MAX_PTZFUCTION_COUNT && iIndex < pstPtzConfig->advanceCfg.functionCnt; iIndex++)
    {
        string szFunction = pstPtzConfig->advanceCfg.functions[iIndex].functionName;
        
        if(szFunction.length() > 0)
        {
            setSupportFunctions.insert(szFunction);
        }
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0)
    {
        cJSON *pResponseData = cJSON_CreateArray();

        set<string>::iterator it =  setSupportFunctions.begin();
        for(; it != setSupportFunctions.end(); ++it ) 
        {
            string szFunction = *it;
            
            cJSON *pNodeAdded = cJSON_CreateObject();    
            cJSON_AddStringToObject(pNodeAdded, "functionname", szFunction.c_str());
            cJSON_AddItemToArray(pResponseData, pNodeAdded);
        }

        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;
}


/*获取设备支持的灯光开关控制模式:
IRCUT_Mode_Active = 0, //主动模式/软光敏自动控制模式, ISP自动判断SENSOR增益，控制IRCUT和灯板
IRCUT_Mode_DayNight =1, //日夜模式，根据时间段来控制IRCUT和图像彩转灰
IRCUT_Mode_Passive = 2, //被动模式/硬光敏外部控制模式，根据灯板的光敏电阻给的硬件信号，来控制IRCUT
IRCUT_Mode_Manual = 3,    //手动模式，不根据灯板和SENSOR增益，由调用者来手动切换
IRCUT_Mode_ReversePassive = 4, //反向被动模式
IRCUT_Mode_AUTO_BY_HARDWARE = 5, //硬光敏自动控制模式，根据硬光敏的adc数据来切换日夜
IRCUT_Mode_LIGHT_ALWAYS_ON = 6, //手动灯光常开
IRCUT_Mode_LIGHT_ALWAYS_OFF = 7, //手动灯光常关
*/
int hapi_system_light_ctrlmode_capability(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0)
    {
        cJSON *pResponseData = GetJSON_LightCtrlModeCapability();
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;  
}

int hapi_system_light_workmode_capability(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_LightWorkModeCapability();
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;  
}


int hapi_system_light_get(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int chn = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapCfg = &pstMediaCfg->videoConfig[chn].videoCapture;

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_VideoLight(pstVideoCapCfg);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;  
}


int hapi_system_light_set(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int chn = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapCfg = &pstMediaCfg->videoConfig[chn].videoCapture;

    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);

    if(nResponseCode == 0)
    {
        int nValue;
        string szToBeFind = "led_ctrl_mode";        
        if(GetParamValueS32_fromMap(paramMap, szToBeFind, IRCUT_Mode_Active, IRCUT_Mode_MAX-1, nValue, bHaveParam, nResponseCode, szResponseString) == 1)
        {
            if(nResponseCode == 0)
                pstVideoCapCfg->ircut_mode = (IRCutMode)nValue;
        }
    }

    if(nResponseCode == 0)
    {
        int nValue;
        string szToBeFind = "led_work_mode";        
        if(GetParamValueS32_fromMap(paramMap, szToBeFind, LED_PURE_INFRAED, LED_INFRAED_THEN_WHITE, nValue, bHaveParam, nResponseCode, szResponseString) == 1)
        {
            if(nResponseCode == 0)
                pstVideoCapCfg->led_mode = (LedMode)nValue;
        }
    }
    
    if(nResponseCode == 0)
    {
        string szToBeFind = "light_open_brightness";        
        GetParamValueU8_fromMap(paramMap, szToBeFind, IRCUT_OPENLED_ON_ILLUMINATION_0_01, IRCUT_OPENLED_ON_ILLUMINATION_1_50, pstVideoCapCfg->ircut_openled_delay, bHaveParam, nResponseCode, szResponseString);
    }

    if(nResponseCode == 0)
    {
        string szToBeFind = "light_off_sensitivity";        
        GetParamValueU8_fromMap(paramMap, szToBeFind, 0, 100, pstVideoCapCfg->light_off_sensitivity, bHaveParam, nResponseCode, szResponseString);
    }
    
    if(nResponseCode == 0)
    {
        string szToBeFind = "led_brightness_mode";        
        GetParamValueU8_fromMap(paramMap, szToBeFind, 0, 2, pstVideoCapCfg->led_brightness_mode, bHaveParam, nResponseCode, szResponseString);
    }

    if(nResponseCode == 0)
    {
        string szToBeFind = "led_brightness_value";        
        GetParamValueU8_fromMap(paramMap, szToBeFind, 0, 100, pstVideoCapCfg->led_brightness_value, bHaveParam, nResponseCode, szResponseString);
    }

    if(nResponseCode == 0)
    {
        string szToBeFind = "night_starttime";        
        set<string> setSupportStrings;
        string szDayTime;
        if(GetParamValueString_fromMap(paramMap, szToBeFind, setSupportStrings, szDayTime, bHaveParam, nResponseCode, szResponseString) > 0)
        {
            DayTime data;
            if(GetDayTimeFromString(szDayTime, data) != 0 )
            {
                __ERR("%s: %s invalid\n", szToBeFind.c_str(), szDayTime.c_str());
                nResponseCode = -1;
                szResponseString = szToBeFind + " param invalid.";
            }
            else
            {
                pstVideoCapCfg->ircut_nighttime.startTime = data;
            }
        }
    }

    if(nResponseCode == 0)
    {
        string szToBeFind = "night_endtime";        
        set<string> setSupportStrings;
        string szDayTime;
        if(GetParamValueString_fromMap(paramMap, szToBeFind, setSupportStrings, szDayTime, bHaveParam, nResponseCode, szResponseString) > 0 )
        {
            DayTime data;
            if(GetDayTimeFromString(szDayTime, data) != 0 )
            {
                __ERR("%s: %s invalid\n", szToBeFind.c_str(), szDayTime.c_str());
                nResponseCode = -1;
                szResponseString = szToBeFind + " param invalid.";
            }
            else
            {
                pstVideoCapCfg->ircut_nighttime.endTime = data;
            }
        }
    }

    int cameraIndex = 0;
    if(bHaveParam > 0)
    {
        if(nResponseCode == 0)
        {
            for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
            {
                anj_config_video_capture_set(pstVideoCapCfg, cameraIndex);
                anj_ispctl_config_set();
            }
        }
    }
    else
    {
        nResponseCode = -1;
        szResponseString = "params not found";
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0)
    {
        cJSON *pResponseData = GetJSON_VideoLight(pstVideoCapCfg);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;   
}


int hapi_system_image_get(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int chn = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapCfg = &pstMediaCfg->videoConfig[chn].videoCapture;

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_VideoImage(pstVideoCapCfg);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;   
}

int hapi_system_image_set(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int chn = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapCfg = &pstMediaCfg->videoConfig[chn].videoCapture;

    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);
    
    if( nResponseCode == 0 )
    {
        string szToBeFind = "brightness";        
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, 255, pstVideoCapCfg->brightness, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        string szToBeFind = "contrast";        
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, 255, pstVideoCapCfg->contrast, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        string szToBeFind = "saturation";        
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, 255, pstVideoCapCfg->saturation, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        string szToBeFind = "sharpness";        
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, 255, pstVideoCapCfg->sharpness, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        string szToBeFind = "tvsystem";        
        GetParamValueU8_fromMap(paramMap, szToBeFind, 0, 1, pstVideoCapCfg->tvsystem, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        string szToBeFind = "hflip";        
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, 1, pstVideoCapCfg->hflip, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        string szToBeFind = "vflip";        
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, 1, pstVideoCapCfg->vflip, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        string szToBeFind = "rotate";        
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, 1, pstVideoCapCfg->rotate, bHaveParam, nResponseCode, szResponseString);
    }

    int cameraIndex = 0;
    if(bHaveParam > 0)
    {
        if(nResponseCode == 0)
        {
            for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
            {
                anj_config_video_capture_set(pstVideoCapCfg, cameraIndex);
                anj_ispctl_config_set();
            }
        }
    }
    else
    {
        nResponseCode = -1;
        szResponseString = "params not found";
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_VideoImage(pstVideoCapCfg);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;  
}

int hapi_system_video_capability(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";
    set<IRCutMode> eSupportModeSet;

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_VideoCapability();
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;  
}

int hapi_system_video_get(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int chn = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoEncode *pstVideoEnc = &pstMediaCfg->videoConfig[chn].videoEncode;

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_VideoEncode(pstVideoEnc);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;  
}

int hapi_system_video_set(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int chn = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoEncode *pstVideoEnc = &pstMediaCfg->videoConfig[chn].videoEncode;

    VideoEncodeCfg *pstEncodeCfg = NULL;

    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);

    if(nResponseCode == 0)
    {
        int nValue;
        int nMaxStreamID = 2;
        const char *config_str = anj_sysctl_get_capability_string();
        if( strstr(config_str, FUNCTION_THREE_VIDEO) != NULL)
        {
            nMaxStreamID = 3;
        }

        string szToBeFind = "streamID";        
        if(GetParamValueS32_fromMap(paramMap, szToBeFind, 1, nMaxStreamID, nValue, bHaveParam, nResponseCode, szResponseString) == 1)
        {
            if( nResponseCode == 0)
            {
                pstEncodeCfg = &pstVideoEnc->encodeCfg[nValue - 1];
                __DBG("found video config for stream %d\n", nValue-1);
            }
        }
        else
        {
            __ERR("Not found %s\n", szToBeFind.c_str());
            nResponseCode = -1;
            szResponseString = szToBeFind + " missing.";
        }
    }

    if( nResponseCode == 0 && NULL != pstEncodeCfg)
    {
        string szToBeFind = "enable";        
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, 1, pstEncodeCfg->enable, bHaveParam, nResponseCode, szResponseString);
    }

    if( nResponseCode == 0 && NULL != pstEncodeCfg)
    {
        string szToBeFind = "encodeFormat";        
        string szValue;
        int bFind = GetParamValue_fromMap(paramMap, szToBeFind, szValue);
        if(bFind)
        {
            bHaveParam  = 1;
            strncpy(pstEncodeCfg->encodeFormat.name, szValue.c_str(), VIDEO_ENCODE_FORAMT_MAX_LEN-1);
            SetVideoDefBpsFps(pstEncodeCfg);//先设置默认码率帧率，避免单独改变编码格式/分辨率的情况下码率帧率没有相应改变
        }
    }        

    if( nResponseCode == 0 && NULL != pstEncodeCfg)
    {
        string szToBeFind = "resolution";        
        string szValue;
        int bFind = GetParamValue_fromMap(paramMap, szToBeFind, szValue);
        if(bFind)
        {
            bHaveParam  = 1;
            strncpy(pstEncodeCfg->resolution.name, szValue.c_str(), RESOLUTION_NAME_MAX_LEN-1);
            SetVideoDefBpsFps(pstEncodeCfg);//先设置默认码率帧率，避免单独改变编码格式/分辨率的情况下码率帧率没有相应改变
        }
    }
    if( nResponseCode == 0 && NULL != pstEncodeCfg)
    {
        string szToBeFind = "bitRateControl";        
        string szValue;
        int bFind = GetParamValue_fromMap(paramMap, szToBeFind, szValue);
        if(bFind)
        {
            bHaveParam  = 1;
            strncpy(pstEncodeCfg->bitRateControl.name, szValue.c_str(), BITRATE_CONTROL_MAX_LEN-1);
        }
    }        
    
    if( nResponseCode == 0 && NULL != pstEncodeCfg)
    {
        string szToBeFind = "gop";        
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, MAX_S32, pstEncodeCfg->initQuant, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 && NULL != pstEncodeCfg)
    {
        string szToBeFind = "bitRate";
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, MAX_S32, pstEncodeCfg->bitRate, bHaveParam, nResponseCode, szResponseString);
    }

    if( nResponseCode == 0 && NULL != pstEncodeCfg)
    {
        string szToBeFind = "frameRate";
        if( GetParamValueS32_fromMap(paramMap, szToBeFind, 0, MAX_S32, pstEncodeCfg->frameRate, bHaveParam, nResponseCode, szResponseString) == 1)
        {
            if( nResponseCode == 0 )
            {
                pstEncodeCfg->display_frameRate = pstEncodeCfg->frameRate;
            }
        }
    }

    if( nResponseCode == 0 && NULL != pstEncodeCfg)
    {
        string szToBeFind = "bitRateQuality";
        int bitRateQuality;
        if(1 == GetParamValueS32_fromMap(paramMap, szToBeFind, VIDEO_QUALITY_CUSTOM, VIDEO_QUALITY_BEST, bitRateQuality, bHaveParam, nResponseCode, szResponseString))
        {
            if( nResponseCode == 0)
            {
                pstEncodeCfg->bitRateQuality = (VideoQualityEnum)bitRateQuality;
            }
        }
    }

    if( nResponseCode == 0 && NULL != pstEncodeCfg)
    {
        string szToBeFind = "qp_enable";
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, 1, pstEncodeCfg->qp.qp_enable, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 && NULL != pstEncodeCfg)
    {
        string szToBeFind = "qp_min";
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, 1, pstEncodeCfg->qp.qp_min, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 && NULL != pstEncodeCfg)
    {
        string szToBeFind = "qp_max";
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, 1, pstEncodeCfg->qp.qp_max, bHaveParam, nResponseCode, szResponseString);
    }

    if( nResponseCode == 0 && NULL != pstEncodeCfg)
    {
        if(bHaveParam > 0 )
        {
            int cameraIndex = 0;
            if(nResponseCode == 0 )
            {
                if(ValidateVideoParams(pstEncodeCfg, szResponseString) > 0 )        
                {
                    int iNeedSwitch = 0;
                    for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
                    {
                        iNeedSwitch |= anj_config_video_encode_set(pstVideoEnc, cameraIndex);
                        anj_ispctl_config_set();
                    }
                    if (iNeedSwitch)
                    {
                        anj_video_encode_switch();
                    }
                }
                else
                {
                    nResponseCode = -1;
                    __ERR("ValidateVideoParams failed: %s\n", szResponseString.c_str());
                }
            }
        }
        else
        {
            nResponseCode = -1;
            szResponseString = "params not found";
        }
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0 )
    { 
        cJSON *pResponseData = GetJSON_VideoEncode(pstVideoEnc);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }    
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;    
}

int hapi_system_audio_capability(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";
    set<IRCutMode> eSupportModeSet;

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_AudioCapability();
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;  
}

int hapi_system_audio_get(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    AudioConfig *ptsAudioCfg = &pstMediaCfg->audioConfig;

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_Audio(ptsAudioCfg);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);

    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;  
}

int hapi_system_audio_set(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    AudioConfig *ptsAudioCfg = &pstMediaCfg->audioConfig;

    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);
    if( nResponseCode == 0)
    {
        string szToBeFind = "enable";        
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, 1, ptsAudioCfg->audioEncode.enable, bHaveParam, nResponseCode, szResponseString);
    }

    if( nResponseCode == 0)
    {
        string szToBeFind = "encodeFormat";        
        string szValue;
        int bFind = GetParamValue_fromMap(paramMap, szToBeFind, szValue);
        if(bFind)
        {
            bHaveParam  = 1;
            strncpy(ptsAudioCfg->audioEncode.audioEncodeType.typeName, szValue.c_str(), AUDIO_ENCODE_TYPE_MAX_LEN-1);
        }
    }        

    if( nResponseCode == 0)
    {
        string szToBeFind = "sampleRate";        
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, MAX_S32, ptsAudioCfg->audioEncode.sampleRate, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0)
    {
        string szToBeFind = "bitRate";        
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, MAX_S32, ptsAudioCfg->audioEncode.bitRate, bHaveParam, nResponseCode, szResponseString);
    }
    
    if( nResponseCode == 0)
    {
        string szToBeFind = "volume_capture";    
        GetParamValueS16_fromMap(paramMap, szToBeFind, 0, 100, ptsAudioCfg->audioCapture.volume_capture, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0)
    {
        string szToBeFind = "volume_play";    
        GetParamValueS16_fromMap(paramMap, szToBeFind, 0, 100, ptsAudioCfg->audioCapture.volume_play, bHaveParam, nResponseCode, szResponseString);
    }

    if( nResponseCode == 0)
    {
        string szToBeFind = "amplify";        
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, 1, ptsAudioCfg->audioCapture.amplify, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0)
    {
        string szToBeFind = "aec_enable";    
        GetParamValueS16_fromMap(paramMap, szToBeFind, 0, 100, ptsAudioCfg->audioCapture.aec_enable, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0)
    {
        string szToBeFind = "mute_ptz_turn";    
        GetParamValueS16_fromMap(paramMap, szToBeFind, 0, 100, ptsAudioCfg->audioCapture.mute_ptz_turn, bHaveParam, nResponseCode, szResponseString);
    }

    if(bHaveParam > 0)
    {
        if(nResponseCode == 0)
        {
            if(ValidateAudioParams(ptsAudioCfg->audioEncode, szResponseString) > 0 )        
            {
                anj_config_audio_set(ptsAudioCfg);
            }
            else
            {
                nResponseCode = -1;
                __DBG("ValidateAudioParams failed: %s\n", szResponseString.c_str());
            }
        }
    }
    else
    {
        nResponseCode = -1;
        szResponseString = "params not found";
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0)
    {
        cJSON *pResponseData = GetJSON_Audio(ptsAudioCfg);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }    
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;  
}

int hapi_system_osd_get(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int chn = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoOverlay *pstVideoOverlayCfg = &pstMediaCfg->videoConfig[chn].overlay;

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_VideoOverlay(pstVideoOverlayCfg);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;   
}

int hapi_system_osd_set(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int chn = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoOverlay *pstVideoOverlayCfg = &pstMediaCfg->videoConfig[chn].overlay;

    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);
    if( nResponseCode == 0)
    {
        string szToBeFind = "enable";        
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, 1, pstVideoOverlayCfg->enable, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0)
    {
        int transparency = pstVideoOverlayCfg->transparency;
        string szToBeFind = "add_overlay";        
        GetParamValueS32_fromMap(paramMap, szToBeFind, TITLE_ADD_NOTHING, TITLE_ADD_RESOLUTION_AND_BITRATE, transparency, bHaveParam, nResponseCode, szResponseString);
        pstVideoOverlayCfg->transparency = (titleFormatEn)transparency;
    }
    if( nResponseCode == 0)
    {
        string szToBeFind = "style";    
        GetParamValueS16_fromMap(paramMap, szToBeFind, AJ_OVERLAY_STYLE_BLACK_WHITE, AJ_OVERLAY_STYLE_INVERSE_COLOR, pstVideoOverlayCfg->style, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0)
    {
        string szToBeFind = "DsplayWeek";    
        GetParamValueS16_fromMap(paramMap, szToBeFind, 0, 2, pstVideoOverlayCfg->bDsplayWeek, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0)
    {
        string szToBeFind = "OverlayFps";    
        GetParamValueS16_fromMap(paramMap, szToBeFind, 0, 1, pstVideoOverlayCfg->bOverlayFps, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0)
    {
        string szToBeFind = "fontsize";    
        GetParamValueS16_fromMap(paramMap, szToBeFind, 0, 2, pstVideoOverlayCfg->fontsize, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0)
    {
        string szToBeFind = "time24or12";    
        GetParamValueS16_fromMap(paramMap, szToBeFind, 0, 1, pstVideoOverlayCfg->time24or12, bHaveParam, nResponseCode, szResponseString);
    }

    if(nResponseCode == 0 )
    {
        cJSON *pRequest = cJSON_Parse(szMsgBody);
        if(pRequest == NULL)
        {
            __ERR("cJSON_Parse failed\n");
            nResponseCode = -1;
            szResponseData = "message json parse failed";
        }

        if( nResponseCode == 0 && pRequest != NULL)
        {
            string szToBeFind = "timeOverlay";        
            cJSON *pTimeOverlay = cJSON_GetObjectItem(pRequest, szToBeFind.c_str());
            if(NULL != pTimeOverlay )
            {
                if( nResponseCode == 0 )
                {
                    int nValue = 0;
                    string szToBeFind = "posType";    
                    if( 1 == GetParamValueS32_fromJson(pTimeOverlay, szToBeFind, POSITION_TYPE_BY_FOUR_CORNER, POSITION_TYPE_BY_SCALE, nValue, bHaveParam, nResponseCode, szResponseString))
                    {
                        pstVideoOverlayCfg->timeOverlay.posType = (Positiontype)nValue;
                    }
                }

                if( nResponseCode == 0 )
                {
                    string szToBeFind = "posX";
                    GetParamValueS32_fromJson(pTimeOverlay, szToBeFind, 0, 100, pstVideoOverlayCfg->timeOverlay.posX, bHaveParam, nResponseCode, szResponseString);
                }
                
                if( nResponseCode == 0 )
                {
                    string szToBeFind = "posY";        
                    GetParamValueS32_fromJson(pTimeOverlay, szToBeFind, 0, 100, pstVideoOverlayCfg->timeOverlay.posY, bHaveParam, nResponseCode, szResponseString);
                }

                if( nResponseCode == 0 )
                {
                    set<string> setSupportStrings = 
                    {
                        "yyyy-mm-dd hh:mm:ss",
                        "yyyy/mm/dd hh:mm:ss",
                        "yy-mm-dd hh:mm:ss",
                        "yy/mm/dd hh:mm:ss",
                        "hh:mm:ss dd/mm/yyyy",
                        "hh:mm:ss dd-mm-yyyy",
                        "hh:mm:ss mm/dd/yyyy",
                        "hh:mm:ss mm-dd-yyyy",    
                        "mm/dd/yyyy hh:mm:ss",
                        "mm-dd-yyyy hh:mm:ss"
                    };

                    string szToBeFind = "timeFormat";
                    string value_gotton;
                    if(1 == GetParamValueString_fromJson(pTimeOverlay, szToBeFind, setSupportStrings, 
                        value_gotton, bHaveParam, nResponseCode, szResponseString))
                    {
                        if( nResponseCode == 0 )
                        {
                            strcpy(pstVideoOverlayCfg->timeOverlay.timeFormat.format, value_gotton.c_str());                            
                            __DBG("gotton:%s\n", value_gotton.c_str());
                        }
                    }
                }
            }

            if( nResponseCode == 0 && pRequest != NULL)
            {
                szToBeFind = "titleOverlay";        
                cJSON *pTitleOverlay = cJSON_GetObjectItem(pRequest, szToBeFind.c_str());
                if(NULL != pTitleOverlay )
                {
                    if( nResponseCode == 0 )
                    {
                        int nValue = 0;
                        string szToBeFind = "posType";    
                        if( 1 == GetParamValueS32_fromJson(pTitleOverlay, szToBeFind, POSITION_TYPE_BY_FOUR_CORNER, POSITION_TYPE_BY_SCALE, nValue, bHaveParam, nResponseCode, szResponseString))
                        {
                            pstVideoOverlayCfg->titleOverlay.posType = (Positiontype)nValue;
                        }
                    }

                    if( nResponseCode == 0)
                    {
                        string szToBeFind = "posX";
                        GetParamValueS32_fromJson(pTitleOverlay, szToBeFind, 0, 100, pstVideoOverlayCfg->titleOverlay.posX, bHaveParam, nResponseCode, szResponseString);
                    }

                    if( nResponseCode == 0)
                    {
                        string szToBeFind = "posY";     
                        GetParamValueS32_fromJson(pTitleOverlay, szToBeFind, 0, 100, pstVideoOverlayCfg->titleOverlay.posY, bHaveParam, nResponseCode, szResponseString);
                    }

                    if( nResponseCode == 0)
                    {
                        int nValue = 0;
                        string szToBeFind = "titleType";    
                        if( 1 == GetParamValueS32_fromJson(pTitleOverlay, szToBeFind, TYPE_TYPE_BY_TEXT, TYPE_TYPE_BY_BMP, nValue, bHaveParam, nResponseCode, szResponseString))
                        {
                            pstVideoOverlayCfg->titleOverlay.titleType = (Titletype)nValue;
                        }
                    }

                    if( nResponseCode == 0 )
                    {
                        set<string> setSupportStrings;                        
                        string szToBeFind = "title_utf8";
                        string value_gotton;
                        if(1 == GetParamValueString_fromJson(pTitleOverlay, szToBeFind, setSupportStrings, 
                            value_gotton, bHaveParam, nResponseCode, szResponseString))
                        {
                            if( nResponseCode == 0 )
                            {
                                if( TYPE_TYPE_BY_BMP == pstVideoOverlayCfg->titleOverlay.titleType)
                                {
                                    strcpy(pstVideoOverlayCfg->titleOverlay.title_utf8, value_gotton.c_str());
                                }
                                else
                                {
                                    hex2ascii(value_gotton.c_str(), value_gotton.length(), pstVideoOverlayCfg->titleOverlay.title_utf8, TITLE_MAX_LEN);
                                }
                                __DBG("gotton:%s\n", value_gotton.c_str());
                            }
                        }
                    }
                }            
            }

            if(NULL != pRequest)
            {            
                cJSON_Delete(pRequest);
            }
        }
    }

    if(bHaveParam > 0 )
    {
        int cameraIndex = 0;
        if(nResponseCode == 0 )
        {
            for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
            {
                anj_config_overlay_set(pstVideoOverlayCfg, cameraIndex);
            }
        }
    }
    else
    {
        nResponseCode = -1;
        szResponseString = "params not found";
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_VideoOverlay(pstVideoOverlayCfg);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);

    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;   
}

int hapi_system_userosd_get(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int chn = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoUserOverlay *pstUserOverlayCfg = &pstMediaCfg->videoConfig[chn].useroverlay;

    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);

    int osdnumber = -1;
    URLParamStruct::iterator itMap = paramMap.find("osdnumber");
    if( itMap != paramMap.end())
    {
        osdnumber = atoi(itMap->second.c_str());
        if(osdnumber < 0 || osdnumber >= MAX_USER_OSD_NUM)
        {
            nResponseCode = -1;
            szResponseString = "osd number error";
        }
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0)
    {
        if(osdnumber <= 0)
        {
            __DBG("Get all user osd\n");
            cJSON *pResponseData = GetJSON_VideoUserOverlay(pstUserOverlayCfg);
            cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
        }
        else
        {
            __DBG("Get one user osd\n");
            cJSON *pResponseData = GetJSON_VideoUserOverlay_One(&pstUserOverlayCfg->data[osdnumber]);
            cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
        }
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;   
}

int hapi_system_userosd_set(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int iCameraIdx = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoUserOverlay *pstUserOverlayCfg = &pstMediaCfg->videoConfig[iCameraIdx].useroverlay;

    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);

    int osdnumber = -1;
    URLParamStruct::iterator itMap = paramMap.find("osdnumber");
    if( itMap != paramMap.end())
    {
        osdnumber = atoi(itMap->second.c_str());
        if(osdnumber <0 || osdnumber >= MAX_USER_OSD_NUM)
        {
            nResponseCode = -1;
            szResponseString = "osd number error";
        }
    }
    else
    {
        __ERR("error: param osdnumber not found\n");
        szResponseString = "params error";
        nResponseCode = -1;
    }

    UserOSD *pstUserOsd = &pstUserOverlayCfg->data[osdnumber];
    if( nResponseCode == 0 )
    {
        string szToBeFind = "enable";        
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, 1, pstUserOsd->enable, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        string szToBeFind = "fontsize";    
        GetParamValueU8_fromMap(paramMap, szToBeFind, 0, 2, pstUserOsd->fontsize, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        string szToBeFind = "linegap";    
        GetParamValueU8_fromMap(paramMap, szToBeFind, 0, 8, pstUserOsd->linegap, bHaveParam, nResponseCode, szResponseString);
    }
    
    if( nResponseCode == 0 )
    {
        int nValue = 0;
        string szToBeFind = "posType";    
        if( 1 == GetParamValueS32_fromMap(paramMap, szToBeFind, POSITION_TYPE_BY_FOUR_CORNER, POSITION_TYPE_BY_SCALE, nValue, bHaveParam, nResponseCode, szResponseString))
        {
            pstUserOsd->posType = (Positiontype)nValue;
        }
    }
    
    if( nResponseCode == 0 )
    {
        string szToBeFind = "posX";
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, 100, pstUserOsd->pos_xscale, bHaveParam, nResponseCode, szResponseString);
    }
    
    if( nResponseCode == 0 )
    {
        string szToBeFind = "posY";     
        GetParamValueS32_fromMap(paramMap, szToBeFind, 0, 100, pstUserOsd->pos_yscale, bHaveParam, nResponseCode, szResponseString);
    }

    if( nResponseCode == 0 )
    {
        int nValue = 0;
        string szToBeFind = "titleType";    
        if( 1 == GetParamValueS32_fromMap(paramMap, szToBeFind, TYPE_TYPE_BY_TEXT, TYPE_TYPE_BY_BMP, nValue, bHaveParam, nResponseCode, szResponseString))
        {
            pstUserOsd->titleType = (Titletype)nValue;
        }
    }

    if( nResponseCode == 0)
    {
        set<string> setSupportStrings;                        
        string szToBeFind = "title_utf8";
        string value_gotton;
        if(1 == GetParamValueString_fromMap(paramMap, szToBeFind, setSupportStrings, 
            value_gotton, bHaveParam, nResponseCode, szResponseString))
        {
            if( nResponseCode == 0 )
            {
                if( TYPE_TYPE_BY_BMP == pstUserOsd->titleType)                            
                    strcpy(pstUserOsd->title_utf8, value_gotton.c_str());
                else
                {
                    hex2ascii(value_gotton.c_str(), value_gotton.length(), pstUserOsd->title_utf8, TITLE_MAX_LEN);
                }
                __DBG("gotton:%s\n", value_gotton.c_str());
            }
        }
    }

    if(bHaveParam > 0)
    {
        if(nResponseCode == 0)
        {
            for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
            {
                anj_config_user_overlay_set(pstUserOverlayCfg, iCameraIdx);
            }
        }
    }
    else
    {
        nResponseCode = -1;
        szResponseString = "params not found";
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_VideoUserOverlay_One(pstUserOsd);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK; 
}

int hapi_smart_capability(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_SmartCapablity();
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;  
}

int hapi_smart_audiofiles_get(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0)
    {
        cJSON *pResponseData = GetJSON_SmartAudioFiles();
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;   
}

int hapi_smart_ObjectDetect_capability(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0)
    {
        cJSON *pResponseData = GetJSON_Smart_ObjectDetectCapablity();
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;   
}

int hapi_smart_linkageaction_capability(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0)
    {
        cJSON *pResponseData = GetJSON_Smart_LinkageActionCapablity();
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;    
}

int hapi_smart_motiondetect_get(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    MotionDetectAlarm *pstMotionAlarmCfg = &pstAlarmCfg->normalAlarm.motionDetectAlarm[chn];

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0)
    {
        cJSON *pResponseData = GetJSON_MotionDetectAlarm(pstMotionAlarmCfg);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;   
}

int hapi_smart_motiondetect_set(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int iCameraIdx = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    MotionDetectAlarm *pstTmpMotionAlarmConfig = (MotionDetectAlarm *) anj_mw_malloc (sizeof(MotionDetectAlarm));
    memcpy(pstTmpMotionAlarmConfig, &pstAlarmConfig->normalAlarm.motionDetectAlarm[iCameraIdx], sizeof(MotionDetectAlarm));

    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "enable", 0, 1, pstTmpMotionAlarmConfig->enable, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "Sensitivity", 0, 100, pstTmpMotionAlarmConfig->sensitivity, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "AlarmThreshold", 0, 100, pstTmpMotionAlarmConfig->alarmThreshold, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "DayNightSwitch", 0, 1, pstTmpMotionAlarmConfig->dayNightSwitch, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "NightSensitivity", 0, 100, pstTmpMotionAlarmConfig->nightSensitivity, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "NightAlarmThreshold", 0, 100, pstTmpMotionAlarmConfig->nightAlarmThreshold, bHaveParam, nResponseCode, szResponseString);
    }

    if(nResponseCode == 0)
    {
        string szToBeFind = "NightStartTime";        
        set<string> setSupportStrings;
        string szDayTime;
        if( GetParamValueString_fromMap(paramMap, szToBeFind, setSupportStrings, szDayTime, bHaveParam, nResponseCode, szResponseString) > 0 )
        {
            DayTime data;
            if(GetDayTimeFromString(szDayTime, data) != 0 )
            {
                __ERR("daytime:%s: %s invalid\n", szToBeFind.c_str(), szDayTime.c_str());
                nResponseCode = -1;
                szResponseString = szToBeFind + " param invalid.";
            }
            else
            {
                pstTmpMotionAlarmConfig->nightTime.startTime = data;
            }
        }
    }

    if( nResponseCode == 0)
    {
        string szToBeFind = "NightEndTime";        
        set<string> setSupportStrings;
        string szDayTime;
        if( GetParamValueString_fromMap(paramMap, szToBeFind, setSupportStrings, szDayTime, bHaveParam, nResponseCode, szResponseString) > 0 )
        {
            DayTime data;
            if(GetDayTimeFromString(szDayTime, data) != 0 )
            {
                __ERR("daytime:%s: %s invalid\n", szToBeFind.c_str(), szDayTime.c_str());
                nResponseCode = -1;
                szResponseString = szToBeFind + " param invalid.";
            }
            else
            {
                pstTmpMotionAlarmConfig->nightTime.endTime = data;
            }
        }
    }

    cJSON *pRequest = cJSON_Parse(szMsgBody);
    if(pRequest == NULL)
    {
        __ERR("cJSON_Parse failed\n");
        nResponseCode = -1;
        szResponseData = "message json parse failed";
    }
    else
    {
        GetArmingFlagFromJson(pRequest, pstTmpMotionAlarmConfig->arming_flag, pstTmpMotionAlarmConfig->timeSpan, bHaveParam, nResponseCode, szResponseString);
        GetMotionDetectAreaFromJson(pRequest, pstTmpMotionAlarmConfig, bHaveParam, nResponseCode, szResponseString);

        string szToBeFind = "AlarmAction";
        cJSON *pAlarmActionNode = cJSON_GetObjectItem(pRequest, szToBeFind.c_str());
        if(NULL != pAlarmActionNode )
        {
            GetArmingStructFromJson(pAlarmActionNode, "LightAction", pstTmpMotionAlarmConfig->alarmAction.light_twinkle_enable, bHaveParam, nResponseCode, szResponseString);
            GetArmingStructFromJson(pAlarmActionNode, "BRAlarmAction", pstTmpMotionAlarmConfig->alarmAction.alarm_led_enable, bHaveParam, nResponseCode, szResponseString);
            GetArmingStructFromJson(pAlarmActionNode, "AlarmServer", pstTmpMotionAlarmConfig->alarmAction.notify_alarmserver_enable, bHaveParam, nResponseCode, szResponseString);
            GetArmingStructFromJson(pAlarmActionNode, "AlarmPush", pstTmpMotionAlarmConfig->alarmAction.alarm_push, bHaveParam, nResponseCode, szResponseString);

            GetAudioActionFromJson(pAlarmActionNode, pstTmpMotionAlarmConfig->alarmAction.audioAction, bHaveParam, nResponseCode, szResponseString);
            GetIOOutAlarmActionFromJson(pAlarmActionNode, pstTmpMotionAlarmConfig->alarmAction.outputAction, bHaveParam, nResponseCode, szResponseString);
        }
        
        cJSON_Delete(pRequest);
    }

    if(bHaveParam > 0 )
    {
        if(nResponseCode == 0 )
        {
            MotionDetectAlarm stMotionAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
            for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
            {
                memcpy(&stMotionAlarmArray[iCameraIdx], pstTmpMotionAlarmConfig, sizeof(MotionDetectAlarm));
            }
            anj_config_alarm_motion_set(stMotionAlarmArray);
        }
    }
    else
    {
        nResponseCode = -1;
        szResponseString = "params not found";
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_MotionDetectAlarm(pstTmpMotionAlarmConfig);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);

    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if (pstTmpMotionAlarmConfig)
    {
        anj_mw_free(pstTmpMotionAlarmConfig);
        pstTmpMotionAlarmConfig = NULL;
    }

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;   
}

int hapi_smart_videocover_get(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int cameraIndex = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    VideoCoverAlarm *pstVideoCoverCfg = &pstAlarmCfg->normalAlarm.videoCoverAlarm[cameraIndex];

    if (!anj_sysctl_capability_check((char *)FUNCTION_ALARM_COVER))
    {
        nResponseCode = -1;
        szResponseString = "no function " + string(FUNCTION_ALARM_COVER);
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0)
    {
        cJSON *pResponseData = GetJSON_VideoCoverAlarm(pstVideoCoverCfg);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;   
}

int hapi_smart_videocover_set(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int cameraIndex = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    VideoCoverAlarm *pstVideoCoverAlarm = (VideoCoverAlarm *)anj_mw_malloc(sizeof(VideoCoverAlarm));
    memcpy(pstVideoCoverAlarm, &pstAlarmConfig->normalAlarm.motionDetectAlarm[cameraIndex], sizeof(VideoCoverAlarm));

    if (!anj_sysctl_capability_check(FUNCTION_ALARM_COVER))
    {
        nResponseCode = -1;
        szResponseString = "no function " + string(FUNCTION_ALARM_COVER);
    }

    int delayenable = 0;//延时启用

    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "enable", 0, 1, pstVideoCoverAlarm->enable, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "Sensitivity", 0, 100, pstVideoCoverAlarm->sensitivity, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "Threadhold_second", 0, MAX_S32, pstVideoCoverAlarm->threadhold_second, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "BackgroundUpdateSecond", 0, MAX_S32, pstVideoCoverAlarm->backgroundUpdateSecond, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "delayenable", 0, MAX_S32, delayenable, bHaveParam, nResponseCode, szResponseString);
    }

    cJSON *pRequest = cJSON_Parse(szMsgBody);
    if(pRequest == NULL)
    {
        __ERR("cJSON_Parse failed\n");
        nResponseCode = -1;
        szResponseData = "message json parse failed";
    }
    else
    {
        ArmingMode arming_flag = (ArmingMode)pstVideoCoverAlarm->enable;
        GetArmingFlagFromJson(pRequest,  arming_flag, pstVideoCoverAlarm->timeSpan, bHaveParam, nResponseCode, szResponseString);
        pstVideoCoverAlarm->enable = arming_flag > 0 ? 1:0;

        string szToBeFind = "AlarmAction";
        cJSON *pAlarmActionNode = cJSON_GetObjectItem(pRequest, szToBeFind.c_str());
        if(NULL != pAlarmActionNode )
        {
            GetAudioActionFromJson(pAlarmActionNode, pstVideoCoverAlarm->alarmAction.audioAction, bHaveParam, nResponseCode, szResponseString);
            GetIOOutAlarmActionFromJson(pAlarmActionNode, pstVideoCoverAlarm->alarmAction.outputAction, bHaveParam, nResponseCode, szResponseString);
        }
        
        cJSON_Delete(pRequest);
    }

    if(bHaveParam > 0)
    {
        if(nResponseCode == 0 )
        {
            VideoCoverAlarm stVideoCoverAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
            for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
            {
                memcpy(&stVideoCoverAlarmArray[cameraIndex], pstVideoCoverAlarm, sizeof(VideoCoverAlarm));
            }
            anj_config_alarm_video_cover_set(stVideoCoverAlarmArray);

            if(delayenable > 0 && pstVideoCoverAlarm->enable == 0)
            {
                //HAPI_SetDelayEnable_VideoCover(delayenable);
            }
        }
    }
    else
    {
        nResponseCode = -1;
        szResponseString = "params not found";
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0)
    {
        cJSON *pResponseData = GetJSON_VideoCoverAlarm(pstVideoCoverAlarm);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);

    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if (pstVideoCoverAlarm)
    {
        anj_mw_free(pstVideoCoverAlarm);
        pstVideoCoverAlarm = NULL;
    }

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK; 
}

int hapi_smart_objectdetect_get(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int cameraIndex = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    PdAlarm *pstPdAlarm = &pstAlarmCfg->aiAlarm.pdAlarm[cameraIndex];

    http_alarm_ability_t *pstAbility = http_alarm_ability_get();
    if(0 == pstAbility->human_enable)
    {
        nResponseCode = -1;
        szResponseString = "no function " + string("TargetDetect");
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_ObjectDetectAlarm(pstPdAlarm);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;   
}

int hapi_smart_objectdetect_set(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int cameraIndex = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    PdAlarm *pstPdAlarm = (PdAlarm *) anj_mw_malloc(sizeof(PdAlarm));
    memcpy(pstPdAlarm, &pstAlarmConfig->normalAlarm.motionDetectAlarm[cameraIndex], sizeof(PdAlarm));

    http_alarm_ability_t *pstAbility = http_alarm_ability_get();
    if( 0 == pstAbility->human_enable)
    {
        nResponseCode = -1;
        szResponseString = "no function " + string("TargetDetect");
    }

    int delayenable = 0;//延时启用
    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "enable", 0, 1, pstPdAlarm->enable, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "Sensitivity", 0, 100, pstPdAlarm->sensitivity, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "AlarmThreshold", 0, 100, pstPdAlarm->threshold, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        GetParamValueU8_fromMap(paramMap, "minTargetRate", 0, 100, pstPdAlarm->minTargetRate, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        GetParamValueU8_fromMap(paramMap, "nonMotionFilter", 0, 1, pstPdAlarm->nonMotionFilter, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        GetParamValueU8_fromMap(paramMap, "allowMd", 0, 1, pstPdAlarm->allowMd, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "delayenable", 0, MAX_S32, delayenable, bHaveParam, nResponseCode, szResponseString);
    }

    cJSON *pRequest = cJSON_Parse(szMsgBody);
    if(pRequest == NULL)
    {
        __ERR("cJSON_Parse failed\n");
        nResponseCode = -1;
        szResponseData = "message json parse failed";
    }
    else
    {
        GetObjectDetectTypeFromJson(pRequest, pstPdAlarm->type, bHaveParam, nResponseCode, szResponseString);
        GetArmingFlagFromJson(pRequest, pstPdAlarm->arming_flag, pstPdAlarm->timeSpan, bHaveParam, nResponseCode, szResponseString);
        GetPolygonFromJson(pRequest, pstPdAlarm->polygonArea, bHaveParam, nResponseCode, szResponseString);

        string szToBeFind = "AlarmAction";
        cJSON *pAlarmActionNode = cJSON_GetObjectItem(pRequest, szToBeFind.c_str());
        if(NULL != pAlarmActionNode )
        {
            {
                string szToBeFind = "draw_rect_enable";
                GetParamValueU8_fromJson(pAlarmActionNode, szToBeFind, 0, 1, pstPdAlarm->alarmAction.draw_rect_enable, bHaveParam, nResponseCode, szResponseString);
            }
            {
                string szToBeFind = "draw_human_enable";
                GetParamValueU8_fromJson(pAlarmActionNode, szToBeFind, 0, 1, pstPdAlarm->alarmAction.draw_human_enable, bHaveParam, nResponseCode, szResponseString);
            }
            {
                string szToBeFind = "track_human_enable";
                GetParamValueU8_fromJson(pAlarmActionNode, szToBeFind, 0, 1, pstPdAlarm->alarmAction.track_human_enable, bHaveParam, nResponseCode, szResponseString);
            }
            {
                string szToBeFind = "rect_twinkle_enable";
                GetParamValueU8_fromJson(pAlarmActionNode, szToBeFind, 0, 2, pstPdAlarm->alarmAction.rect_twinkle_enable, bHaveParam, nResponseCode, szResponseString);
            }
            {
                string szToBeFind = "auto_zoom_enable";
                GetParamValueU8_fromJson(pAlarmActionNode, szToBeFind, 0, 1, pstPdAlarm->alarmAction.auto_zoom_enable, bHaveParam, nResponseCode, szResponseString);
            }
            {
                string szToBeFind = "gunball_track_mode";
                GetParamValueU8_fromJson(pAlarmActionNode, szToBeFind, 0, 1, pstPdAlarm->alarmAction.gunball_track_mode, bHaveParam, nResponseCode, szResponseString);
            }
        
            GetArmingStructFromJson(pAlarmActionNode, "LightAction", pstPdAlarm->alarmAction.light_twinkle_enable, bHaveParam, nResponseCode, szResponseString);
            GetArmingStructFromJson(pAlarmActionNode, "BRAlarmAction", pstPdAlarm->alarmAction.alarm_led_enable, bHaveParam, nResponseCode, szResponseString);
            GetArmingStructFromJson(pAlarmActionNode, "AlarmServer", pstPdAlarm->alarmAction.notify_alarmserver_enable, bHaveParam, nResponseCode, szResponseString);
            GetArmingStructFromJson(pAlarmActionNode, "AlarmPush", pstPdAlarm->alarmAction.alarm_push, bHaveParam, nResponseCode, szResponseString);

            GetAudioActionFromJson(pAlarmActionNode, pstPdAlarm->alarmAction.audioAction, bHaveParam, nResponseCode, szResponseString);
            GetIOOutAlarmActionFromJson(pAlarmActionNode, pstPdAlarm->alarmAction.outputAction, bHaveParam, nResponseCode, szResponseString);
        }
        
        cJSON_Delete(pRequest);
    }

    if(bHaveParam > 0 )
    {
        if(nResponseCode == 0 )
        {
            PdAlarm stPdAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
            for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
            {
                memcpy(&stPdAlarmArray[cameraIndex], pstPdAlarm, sizeof(PdAlarm));
            }
            anj_config_alarm_pd_set(stPdAlarmArray);

           if(delayenable > 0 && (pstPdAlarm->enable== 0 || pstPdAlarm->arming_flag != ARMING_ALLDAY))
            {
                //HAPI_SetDelayEnable_ObjectDetect(60);
            }
        }
    }
    else
    {
        nResponseCode = -1;
        szResponseString = "params not found";
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_ObjectDetectAlarm(pstPdAlarm);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if (pstPdAlarm)
    {
        anj_mw_free(pstPdAlarm);
        pstPdAlarm = NULL;
    }

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;  
}

int hapi_smart_fd_get(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int cameraIndex = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    FaceDetectAlarm *pstFaceAlarmCfg = &pstAlarmCfg->aiAlarm.fdAlarm[cameraIndex];

    http_alarm_ability_t *pstAbility = http_alarm_ability_get();
    if (0 == pstAbility->face_detect)
    {
        nResponseCode = -1;
        szResponseString = "no function " + string(FUNCTION_FACE_FD);
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_FaceDetectAlarm(pstFaceAlarmCfg);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;  
}

int hapi_smart_fd_set(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int cameraIndex = 0;
    FaceDetectAlarm stNewFaceDetectAlarm = {0};
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    memcpy(&stNewFaceDetectAlarm, &pstAlarmCfg->aiAlarm.fdAlarm[cameraIndex], sizeof(FaceDetectAlarm));

    http_alarm_ability_t *pstAbility = http_alarm_ability_get();
    if (0 == pstAbility->face_detect)
    {
        nResponseCode = -1;
        szResponseString = "no function " + string(FUNCTION_FACE_FD);
    }

    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "enable", 0, 1, stNewFaceDetectAlarm.enable, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "Sensitivity", 0, 100, stNewFaceDetectAlarm.sensitivity, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "AlarmThreshold", 0, 100, stNewFaceDetectAlarm.threshold, bHaveParam, nResponseCode, szResponseString);
    }

    cJSON *pRequest = cJSON_Parse(szMsgBody);
    if(pRequest == NULL)
    {
        __ERR("cJSON_Parse failed\n");
        nResponseCode = -1;
        szResponseData = "message json parse failed";
    }
    else
    {
        GetArmingFlagFromJson(pRequest, stNewFaceDetectAlarm.arming_flag, stNewFaceDetectAlarm.timeSpan, bHaveParam, nResponseCode, szResponseString);
        GetAreaRectFromJson(pRequest, stNewFaceDetectAlarm.area, bHaveParam, nResponseCode, szResponseString);

        string szToBeFind = "AlarmAction";
        cJSON *pAlarmActionNode = cJSON_GetObjectItem(pRequest, szToBeFind.c_str());
        if(NULL != pAlarmActionNode )
        {
            {
                string szToBeFind = "draw_rect_enable";
                GetParamValueS32_fromJson(pAlarmActionNode, szToBeFind, 0, 1, stNewFaceDetectAlarm.alarmAction.draw_rect_enable, bHaveParam, nResponseCode, szResponseString);
            }
            GetAudioActionFromJson(pAlarmActionNode, stNewFaceDetectAlarm.alarmAction.audioAction, bHaveParam, nResponseCode, szResponseString);
            GetIOOutAlarmActionFromJson(pAlarmActionNode, stNewFaceDetectAlarm.alarmAction.outputAction, bHaveParam, nResponseCode, szResponseString);
        }
        
        cJSON_Delete(pRequest);
    }

    if(bHaveParam > 0 )
    {
        if(nResponseCode == 0 )
        {
            FaceDetectAlarm stFaceAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
            for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
            {
                memcpy(&stFaceAlarmArray[cameraIndex], &stNewFaceDetectAlarm, sizeof(FaceDetectAlarm));
            }
            anj_config_alarm_fd_set(stFaceAlarmArray);
        }
    }
    else
    {
        nResponseCode = -1;
        szResponseString = "params not found";
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_FaceDetectAlarm(&stNewFaceDetectAlarm);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;    
}

int hapi_smart_vg_get(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int cameraIndex = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    VideoGateAlarm *pstVideoGateAlarm = &pstAlarmCfg->aiAlarm.vgAlarm[cameraIndex];

    http_alarm_ability_t *pstAbility = http_alarm_ability_get();
    if (0 == pstAbility->video_gate)
    {
        nResponseCode = -1;
        szResponseString = "no function " + string(FUNCTION_ALARM_VIDEOGATE);
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_VideoGateAlarm(pstVideoGateAlarm);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK; 
}

int hapi_smart_vg_set(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int cameraIndex = 0;
    VideoGateAlarm stVideoGateAlarmCfg = {0};
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    memcpy(&stVideoGateAlarmCfg, &pstAlarmCfg->aiAlarm.vgAlarm[cameraIndex], sizeof(VideoGateAlarm));


    http_alarm_ability_t *pstAbility = http_alarm_ability_get();
    if (0 == pstAbility->video_gate)
    {
        nResponseCode = -1;
        szResponseString = "no function " + string(FUNCTION_ALARM_VIDEOGATE);
    }

    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "enable", 0, 1, stVideoGateAlarmCfg.enable, bHaveParam, nResponseCode, szResponseString);
    }

    cJSON *pRequest = cJSON_Parse(szMsgBody);
    if(pRequest == NULL)
    {
        __ERR("cJSON_Parse failed\n");
        nResponseCode = -1;
        szResponseData = "message json parse failed";
    }
    else
    {
        GetArmingFlagFromJson(pRequest, stVideoGateAlarmCfg.arming_flag, stVideoGateAlarmCfg.timeSpan, bHaveParam, nResponseCode, szResponseString);
        GetVideoGateRulesFromJson(pRequest, stVideoGateAlarmCfg.data, bHaveParam, nResponseCode, szResponseString);

        string szToBeFind = "AlarmAction";
        cJSON *pAlarmActionNode = cJSON_GetObjectItem(pRequest, szToBeFind.c_str());
        if(NULL != pAlarmActionNode )
        {
            {
                int nValue = 0;
                string szToBeFind = "draw_rect_enable";
                if( 1 == GetParamValueS32_fromJson(pAlarmActionNode, szToBeFind, 0, 1, nValue, bHaveParam, nResponseCode, szResponseString))
                    stVideoGateAlarmCfg.alarmAction.draw_rect_enable = nValue;
            }
            {
                int nValue = 0;
                string szToBeFind = "draw_target_enable";
                if( 1 == GetParamValueS32_fromJson(pAlarmActionNode, szToBeFind, 0, 1, nValue, bHaveParam, nResponseCode, szResponseString))
                    stVideoGateAlarmCfg.alarmAction.draw_target_enable = nValue;
            }

            GetArmingStructFromJson(pAlarmActionNode, "LightAction", stVideoGateAlarmCfg.alarmAction.light_twinkle_enable, bHaveParam, nResponseCode, szResponseString);
            GetArmingStructFromJson(pAlarmActionNode, "BRAlarmAction", stVideoGateAlarmCfg.alarmAction.alarm_led_enable, bHaveParam, nResponseCode, szResponseString);
            GetArmingStructFromJson(pAlarmActionNode, "AlarmServer", stVideoGateAlarmCfg.alarmAction.notify_alarmserver_enable, bHaveParam, nResponseCode, szResponseString);
            GetArmingStructFromJson(pAlarmActionNode, "AlarmPush", stVideoGateAlarmCfg.alarmAction.alarm_push, bHaveParam, nResponseCode, szResponseString);

            GetAudioActionFromJson(pAlarmActionNode, stVideoGateAlarmCfg.alarmAction.audioAction, bHaveParam, nResponseCode, szResponseString);
            GetIOOutAlarmActionFromJson(pAlarmActionNode, stVideoGateAlarmCfg.alarmAction.outputAction, bHaveParam, nResponseCode, szResponseString);
        }
        
        cJSON_Delete(pRequest);
    }

    if(bHaveParam > 0 )
    {
        if(nResponseCode == 0 )
        {
            VideoGateAlarm stVideoGateAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
            for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
            {
                memcpy(&stVideoGateAlarmArray[cameraIndex], &stVideoGateAlarmCfg, sizeof(VideoGateAlarm));
            }
            anj_config_alarm_video_gate_set(stVideoGateAlarmArray);
        }
    }
    else
    {
        nResponseCode = -1;
        szResponseString = "params not found";
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_VideoGateAlarm(&stVideoGateAlarmCfg);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;    
}

int hapi_smart_regionai_get(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int cameraIndex = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    VideoRegionAiAlarm *pstVideoRegionAlarm = &pstAlarmCfg->aiAlarm.regionAiAlarm[cameraIndex];

    http_alarm_ability_t *pstAbility = http_alarm_ability_get();
    if (0 == pstAbility->region_enable)
    {
        nResponseCode = -1;
        szResponseString = "no function " + string(FUNCTION_ALARM_VIDEOGATE);
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_VideoRegionAIAlarm(pstVideoRegionAlarm);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;  
}

int hapi_smart_regionai_set(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int cameraIndex = 0;
    VideoRegionAiAlarm stRegionAlarm = {0};
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    memcpy(&stRegionAlarm, &pstAlarmCfg->aiAlarm.regionAiAlarm[cameraIndex], sizeof(VideoRegionAiAlarm));

    http_alarm_ability_t *pstAbility = http_alarm_ability_get();
    if (0 == pstAbility->region_enable)
    {
        nResponseCode = -1;
        szResponseString = "no function " + string(FUNCTION_ALARM_VIDEOGATE);
    }

    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "enable", 0, 1, stRegionAlarm.enable, bHaveParam, nResponseCode, szResponseString);
    }

    cJSON *pRequest = cJSON_Parse(szMsgBody);
    if(pRequest == NULL)
    {
        __ERR("cJSON_Parse failed\n");
        nResponseCode = -1;
        szResponseData = "message json parse failed";
    }
    else
    {
        GetArmingFlagFromJson(pRequest, stRegionAlarm.arming_flag, stRegionAlarm.timeSpan, bHaveParam, nResponseCode, szResponseString);
        GetVideoRegionAIRulesFromJson(pRequest, stRegionAlarm.data, bHaveParam, nResponseCode, szResponseString);
        GetPolygonFromJson(pRequest, stRegionAlarm.polygonArea, bHaveParam, nResponseCode, szResponseString);

        string szToBeFind = "AlarmAction";
        cJSON *pAlarmActionNode = cJSON_GetObjectItem(pRequest, szToBeFind.c_str());
        if(NULL != pAlarmActionNode )
        {
            {
                int nValue = 0;
                string szToBeFind = "draw_rect_enable";
                if( 1 == GetParamValueS32_fromJson(pAlarmActionNode, szToBeFind, 0, 1, nValue, bHaveParam, nResponseCode, szResponseString))
                    stRegionAlarm.alarmAction.draw_rect_enable = nValue;
            }

            {
                int nValue = 0;
                string szToBeFind = "draw_target_enable";
                if( 1 == GetParamValueS32_fromJson(pAlarmActionNode, szToBeFind, 0, 1, nValue, bHaveParam, nResponseCode, szResponseString))
                    stRegionAlarm.alarmAction.draw_target_enable = nValue;
            }

            GetArmingStructFromJson(pAlarmActionNode, "LightAction", stRegionAlarm.alarmAction.light_twinkle_enable, bHaveParam, nResponseCode, szResponseString);
            GetArmingStructFromJson(pAlarmActionNode, "BRAlarmAction", stRegionAlarm.alarmAction.alarm_led_enable, bHaveParam, nResponseCode, szResponseString);
            GetArmingStructFromJson(pAlarmActionNode, "AlarmServer", stRegionAlarm.alarmAction.notify_alarmserver_enable, bHaveParam, nResponseCode, szResponseString);
            GetArmingStructFromJson(pAlarmActionNode, "AlarmPush", stRegionAlarm.alarmAction.alarm_push, bHaveParam, nResponseCode, szResponseString);
            
            GetAudioActionFromJson(pAlarmActionNode, stRegionAlarm.alarmAction.audioAction, bHaveParam, nResponseCode, szResponseString);
            GetIOOutAlarmActionFromJson(pAlarmActionNode, stRegionAlarm.alarmAction.outputAction, bHaveParam, nResponseCode, szResponseString);
        }

        cJSON_Delete(pRequest);
    }

    if(bHaveParam > 0 )
    {
        if(nResponseCode == 0 )
        {
            VideoRegionAiAlarm stRegionAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
            for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
            {
                memcpy(&stRegionAlarmArray[cameraIndex], &stRegionAlarm, sizeof(VideoRegionAiAlarm));
            }
            anj_config_alarm_region_set(stRegionAlarmArray);
        }
    }
    else
    {
        nResponseCode = -1;
        szResponseString = "params not found";
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_VideoRegionAIAlarm(&stRegionAlarm);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
        __DBG("res:%s\n", szResponseString.c_str());
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;  
}

int hapi_smart_lpr_get(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int cameraIndex = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    LprAlarm *pstLprAlarmCfg = &pstAlarmCfg->aiAlarm.lprAlarm[cameraIndex];

    if (0 == anj_sysctl_capability_check(FUNCTION_ALARM_LPR))
    {
        nResponseCode = -1;
        szResponseString = "no function " + string("LPR");
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_LprAlarm(pstLprAlarmCfg);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;   
}

int hapi_smart_lpr_set(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int cameraIndex = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    LprAlarm stNewLprAlarmCfg = {0};
    memcpy(&stNewLprAlarmCfg, &pstAlarmCfg->aiAlarm.lprAlarm[cameraIndex], sizeof(LprAlarm));

    if (0 == anj_sysctl_capability_check(FUNCTION_ALARM_LPR))
    {
        nResponseCode = -1;
        szResponseString = "no function " + string("LPR");
    }

    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);
    
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "enable", 0, 1, stNewLprAlarmCfg.enable, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "Sensitivity", 0, 100, stNewLprAlarmCfg.sensitivity, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "detectionmode", 0, 1, stNewLprAlarmCfg.detectionmode, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "actionInterval", 0, MAX_S32, stNewLprAlarmCfg.actionInterval, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        set<string> setSupportStrings = 
        {
            "Default",
            "1280X720"
        };                    
        string szToBeFind = "snapQuality";
        string value_gotton;
        if(1 == GetParamValueString_fromMap(paramMap, szToBeFind, setSupportStrings, 
            value_gotton, bHaveParam, nResponseCode, szResponseString))
        {
            if( nResponseCode == 0 )
            {
                strncpy(stNewLprAlarmCfg.snapQuality, value_gotton.c_str(), RESOLUTION_NAME_MAX_LEN-1);
            }
        }
    }

    cJSON *pRequest = cJSON_Parse(szMsgBody);
    if(pRequest == NULL)
    {
        __ERR("cJSON_Parse failed\n");
        nResponseCode = -1;
        szResponseData = "message json parse failed";
    }
    else
    {
        GetArmingFlagFromJson(pRequest, stNewLprAlarmCfg.arming_flag, stNewLprAlarmCfg.timeSpan, bHaveParam, nResponseCode, szResponseString);
        GetPolygonFromJson(pRequest, stNewLprAlarmCfg.polygonArea, bHaveParam, nResponseCode, szResponseString);

        string szToBeFind = "AlarmAction";
        cJSON *pAlarmActionNode = cJSON_GetObjectItem(pRequest, szToBeFind.c_str());
        if(NULL != pAlarmActionNode )
        {
            {
                int nValue = 0;
                string szToBeFind = "draw_rect_enable";
                if( 1 == GetParamValueS32_fromJson(pAlarmActionNode, szToBeFind, 0, 1, nValue, bHaveParam, nResponseCode, szResponseString))
                    stNewLprAlarmCfg.alarmAction.draw_rect_enable = nValue;
            }
            {
                int nValue = 0;
                string szToBeFind = "draw_target_enable";
                if( 1 == GetParamValueS32_fromJson(pAlarmActionNode, szToBeFind, 0, 1, nValue, bHaveParam, nResponseCode, szResponseString))
                    stNewLprAlarmCfg.alarmAction.draw_target_enable = nValue;
            }
            {
                int nValue = 0;
                string szToBeFind = "draw_osd_enable";
                if( 1 == GetParamValueS32_fromJson(pAlarmActionNode, szToBeFind, 0, 1, nValue, bHaveParam, nResponseCode, szResponseString))
                    stNewLprAlarmCfg.alarmAction.draw_osd_enable = nValue;
            }
            {
                int nValue = 0;
                string szToBeFind = "play_voice_enable";
                if( 1 == GetParamValueS32_fromJson(pAlarmActionNode, szToBeFind, 0, 1, nValue, bHaveParam, nResponseCode, szResponseString))
                    stNewLprAlarmCfg.alarmAction.play_voice_enable = nValue;
            }

            GetArmingStructFromJson(pAlarmActionNode, "AlarmServer", stNewLprAlarmCfg.alarmAction.notify_alarmserver_enable, bHaveParam, nResponseCode, szResponseString);
            GetArmingStructFromJson(pAlarmActionNode, "AlarmPush", stNewLprAlarmCfg.alarmAction.alarm_push, bHaveParam, nResponseCode, szResponseString);
        }

        cJSON_Delete(pRequest);
    }

    if(bHaveParam > 0)
    {
        if(nResponseCode == 0 )
        {
            LprAlarm stLprAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
            for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
            {
                memcpy(&stLprAlarmArray[cameraIndex], &stNewLprAlarmCfg, sizeof(LprAlarm));
            }
            anj_config_alarm_lpr_set(stLprAlarmArray);
        }
    }
    else
    {
        nResponseCode = -1;
        szResponseString = "params not found";
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_LprAlarm(&stNewLprAlarmCfg);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;   
}

int hapi_smart_FlameFlumes_get(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    FlameAndFlumesAlarm *pstFlameCfg = &pstAlarmCfg->aiAlarm.fireAlarm;

    if (0 == anj_sysctl_capability_check(FUNCTION_ALARM_FIRE))
    {
        nResponseCode = -1;
        szResponseString = "no function " + string("flame and flumes");
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());
    
    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_FlameAndFlumesAlarm(pstFlameCfg);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;
}

int hapi_smart_FlameFlumes_set(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    FlameAndFlumesAlarm *pstFlameCfg = &pstAlarmCfg->aiAlarm.fireAlarm;

    if (0 == anj_sysctl_capability_check(FUNCTION_ALARM_FIRE))
    {
        nResponseCode = -1;
        szResponseString = "no function " + string("flame and flumes");
    }

    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);
    
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "enable", 0, 1, pstFlameCfg->enable, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        int value = 0;
        if(1 == GetParamValueS32_fromMap(paramMap, "Sensitivity", 0, 100, value, bHaveParam, nResponseCode, szResponseString))
            pstFlameCfg->sensitivity = (short)value;
    }
    if( nResponseCode == 0 )
    {
        int value = 0;
        if(1 == GetParamValueS32_fromMap(paramMap, "Sensitivity_smog", 0, 100, value, bHaveParam, nResponseCode, szResponseString))
            pstFlameCfg->sensitivity_smog = (short)value;
    }
    if( nResponseCode == 0 )
    {
        GetParamValueS32_fromMap(paramMap, "actionInterval", 0, MAX_S32, pstFlameCfg->actionInterval, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        set<string> setSupportStrings = 
        {
            "Default",
            "1280X720"
        };                    
        string szToBeFind = "snapQuality";
        string value_gotton;
        if(1 == GetParamValueString_fromMap(paramMap, szToBeFind, setSupportStrings, 
            value_gotton, bHaveParam, nResponseCode, szResponseString))
        {
            if( nResponseCode == 0 )
            {
                strncpy(pstFlameCfg->snapQuality, value_gotton.c_str(), RESOLUTION_NAME_MAX_LEN-1);
            }
        }
    }

    cJSON *pRequest = cJSON_Parse(szMsgBody);
    if(pRequest == NULL)
    {
        __ERR("cJSON_Parse failed\n");
        nResponseCode = -1;
        szResponseData = "message json parse failed";
    }
    else
    {
        GetArmingFlagFromJson(pRequest, pstFlameCfg->arming_flag, pstFlameCfg->timeSpan, bHaveParam, nResponseCode, szResponseString);
        GetPolygonFromJson(pRequest, pstFlameCfg->polygonArea, bHaveParam, nResponseCode, szResponseString);

        string szToBeFind = "AlarmAction";
        cJSON *pAlarmActionNode = cJSON_GetObjectItem(pRequest, szToBeFind.c_str());
        if(NULL != pAlarmActionNode )
        {
            {
                int nValue = 0;
                string szToBeFind = "draw_rect_enable";
                if( 1 == GetParamValueS32_fromJson(pAlarmActionNode, szToBeFind, 0, 1, nValue, bHaveParam, nResponseCode, szResponseString))
                    pstFlameCfg->alarmAction.draw_rect_enable = nValue;
            }
            {
                int nValue = 0;
                string szToBeFind = "draw_target_enable";
                if( 1 == GetParamValueS32_fromJson(pAlarmActionNode, szToBeFind, 0, 1, nValue, bHaveParam, nResponseCode, szResponseString))
                    pstFlameCfg->alarmAction.draw_target_enable = nValue;
            }
            
            GetArmingStructFromJson(pAlarmActionNode, "LightAction", pstFlameCfg->alarmAction.light_twinkle_enable, bHaveParam, nResponseCode, szResponseString);
            GetArmingStructFromJson(pAlarmActionNode, "BRAlarmAction", pstFlameCfg->alarmAction.alarm_led_enable, bHaveParam, nResponseCode, szResponseString);
            GetArmingStructFromJson(pAlarmActionNode, "AlarmServer", pstFlameCfg->alarmAction.notify_alarmserver_enable, bHaveParam, nResponseCode, szResponseString);
            GetArmingStructFromJson(pAlarmActionNode, "AlarmPush", pstFlameCfg->alarmAction.alarm_push, bHaveParam, nResponseCode, szResponseString);

            GetAudioActionFromJson(pAlarmActionNode, pstFlameCfg->alarmAction.audioAction, bHaveParam, nResponseCode, szResponseString);
            GetIOOutAlarmActionFromJson(pAlarmActionNode, pstFlameCfg->alarmAction.outputAction, bHaveParam, nResponseCode, szResponseString);
        }

        cJSON_Delete(pRequest);
    }

    if(bHaveParam > 0 )
    {
        if(nResponseCode == 0 )
        {
            anj_config_alarm_fire_set(pstFlameCfg);
        }
    }
    else
    {
        nResponseCode = -1;
        szResponseString = "params not found";
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_FlameAndFlumesAlarm(pstFlameCfg);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;    
}


int hapi_event_subscription_regist(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);
/*
{
"ServerType":0,
"ServerName": "192.168.1.253",
"Port": 9998,
"Duration": 3600,
"EventType: "all"
}
*/
    ServerType nServerType = SERVER_TYPE_IPV4;
    string szServerName;
    int nServerPort = 0;
    string szPostURLPrefix;
    string szEventType;
    unsigned int nDuration = 3600;
    
    if( nResponseCode == 0 )
    {
        string szToBeFind = "ServerType";        
        int nValue = 0;
        if(1 != GetParamValueS32_fromMap(paramMap, szToBeFind, SERVER_TYPE_IPV4, SERVER_TYPE_DOMAINNAME, nValue, bHaveParam, nResponseCode, szResponseString))
        {
            nResponseCode = -1;
            szResponseString = "Param (" + szToBeFind + ") not found";
        }
        else
        {
            nServerType = (ServerType)nValue;
        }
    }
    
    if( nResponseCode == 0 )
    {
        set<string> setSupportStrings;
        string szToBeFind = "ServerName";        
        if(1 != GetParamValueString_fromMap(paramMap, szToBeFind, setSupportStrings, szServerName, bHaveParam, nResponseCode, szResponseString))
        {
            nResponseCode = -1;
            szResponseString = "Param (" + szToBeFind + ") not found";
        }
    }
    if( nResponseCode == 0 )
    {
        string szToBeFind = "Port";        
        if(1 != GetParamValueS32_fromMap(paramMap, szToBeFind, 1, 65535, nServerPort, bHaveParam, nResponseCode, szResponseString))
        {
            nResponseCode = -1;
            szResponseString = "Param (" + szToBeFind + ") not found";
        }
    }
    if( nResponseCode == 0 )
    {
        string szToBeFind = "Duration";        
        if(1 != GetParamValueU32_fromMap(paramMap, szToBeFind, 30, 3600, nDuration, bHaveParam, nResponseCode, szResponseString))
        {
            nResponseCode = -1;
            szResponseString = "Param (" + szToBeFind + ") not found";
        }
    }
    if( nResponseCode == 0 )
    {
        set<string> setSupportStrings;
        string szToBeFind = "EventType";        
        GetParamValueString_fromMap(paramMap, szToBeFind, setSupportStrings, szEventType, bHaveParam, nResponseCode, szResponseString);
    }
    if( nResponseCode == 0 )
    {
        set<string> setSupportStrings;
        string szToBeFind = "PostURLPrefix";        
        GetParamValueString_fromMap(paramMap, szToBeFind, setSupportStrings, szPostURLPrefix, bHaveParam, nResponseCode, szResponseString);
    }

    unsigned int ID = 0;
    HapiSubNodeStruct node;
    if(nResponseCode == 0 && bHaveParam > 0 )
    {
        memset(&node, 0, sizeof(node));
        node.nServerType = nServerType;
        strncpy(node.szServerName, szServerName.c_str(), sizeof(node.szServerName)-1);
        node.nServerPort = nServerPort;
        strncpy(node.szPostURLPrefix, szPostURLPrefix.c_str(), sizeof(node.szPostURLPrefix)-1);
        strncpy(node.szEventType, szEventType.c_str(), sizeof(node.szEventType)-1);
        node.uptime_timeout = GetCurrentTimeStampU64() + nDuration * 1000;
        node.localtime_timeout = time(0) + nDuration;

        ID = hapi_submap_add(&node);    
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);

    char mySN[32] = {0};
    anj_sysmng_load_sn(mySN, sizeof(mySN));
    cJSON_AddStringToObject(pResponseNode, "DeviceID", mySN);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_SubscriptionAdd(ID, node);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;  
}

int hapi_event_subscription_refresh(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);
/*
{
"ID":0,
"Duration": 3600,
}
*/
    unsigned int ID = 0;
    unsigned int nDuration = 3600;

    if( nResponseCode == 0 )
    {
        string szToBeFind = "ID";        
        if(1 != GetParamValueU32_fromMap(paramMap, szToBeFind, 0, 0xffffffff, ID, bHaveParam, nResponseCode, szResponseString))
        {
            nResponseCode = -1;
            szResponseString = "Param (" + szToBeFind + ") not found";
        }
    }

    if( nResponseCode == 0 )
    {
        string szToBeFind = "Duration";        
        if(1 != GetParamValueU32_fromMap(paramMap, szToBeFind, 30, 3600, nDuration, bHaveParam, nResponseCode, szResponseString))
        {
            nResponseCode = -1;
            szResponseString = "Param (" + szToBeFind + ") not found";
        }
    }

    HapiSubNodeStruct *pFoundNode = NULL;
    if(nResponseCode == 0 && bHaveParam > 0 )
    {
        if(0 == hapi_submap_refresh(ID, nDuration, &pFoundNode))
        {
        }
        else
        {
            nResponseCode = -1;
            szResponseString = "Subscription refresh failed";
        }
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);

    char mySN[32] = {0};
    anj_sysmng_load_sn(mySN, sizeof(mySN));
    cJSON_AddStringToObject(pResponseNode, "DeviceID", mySN);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = GetJSON_SubscriptionAdd(ID, *pFoundNode);
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;    
}


int hapi_event_subscription_delete(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);

    int nResponseCode = 0;
    string szResponseString = RESPONSE_OK;
    string szResponseData = "null";

    int bHaveParam = 0;
    URLParamStruct paramMap;
    GetURLParamList(szURLFullname, szHttpMethod, szMsgBody, paramMap);
/*
{
"ID":0,
}
*/
    unsigned int ID = 0;
    if( nResponseCode == 0 )
    {
        string szToBeFind = "ID";        
        if(1 != GetParamValueU32_fromMap(paramMap, szToBeFind, 0, 0xffffffff, ID, bHaveParam, nResponseCode, szResponseString))
        {
            nResponseCode = -1;
            szResponseString = "Param (" + szToBeFind + ") not found";
        }
    }

    if(nResponseCode == 0 && bHaveParam > 0 )
    {
        if(0 == hapi_submap_delete(ID))
        {
        }
        else
        {
            nResponseCode = -1;
            szResponseString = "Subscription delete failed";
        }
    }

    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", nResponseCode);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", szResponseString.c_str());

    if(nResponseCode == 0 )
    {
        cJSON *pResponseData = cJSON_CreateObject();
        cJSON_AddNumberToObject(pResponseData, "ID", ID);
        
        cJSON_AddItemToObject(pResponseNode, "Data", pResponseData);
    }
    else
    {
        cJSON_AddStringToObject(pResponseNode, "Data", "null");
    }

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);

    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_OK;   
}

/* 没有匹配的cgi url       */
int hapi_no_interface(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);
    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", "invalid");
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", -1);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", "api not found");
    cJSON_AddStringToObject(pResponseNode, "Data", "null");
    
    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }

    return HTTP_RES_STATUS_NOT_FOUND;
}

static HAPIStaticTableStruct g_HAPI_Table[] = 
{
    { "/uid/getuid", hapi_uid_getuid, AUTH_TYPE_BY_USERNAME},
    { "/uid/keep_alive", hapi_uid_keep_alive, AUTH_TYPE_BY_UID},
    
    { "/sysinfo/device_info", hapi_sysinfo_deviceinfo, AUTH_TYPE_ALL },
    { "/sysinfo/functionlist", hapi_sysinfo_function_list, AUTH_TYPE_ALL},
    { "/sysinfo/capability", hapi_sysinfo_capability, AUTH_TYPE_ALL},
    { "/sysinfo/rtspurl", hapi_sysinfo_rtspurl, AUTH_TYPE_ALL },

    { "/sysman/reboot", hapi_sysman_reboot, AUTH_TYPE_ALL },
    { "/sysman/factory", hapi_sysman_factory, AUTH_TYPE_ALL },
    { "/io/input/get", hapi_io_input_get, AUTH_TYPE_ALL },
    { "/io/output/get", hapi_io_output_get, AUTH_TYPE_ALL },
    { "/io/output/set", hapi_io_output_set, AUTH_TYPE_ALL },

    { "/systime/gettime", hapi_systime_gettime, AUTH_TYPE_ALL },
    { "/systime/settime", hapi_systime_settime, AUTH_TYPE_ALL },
    { "/systime/setntp", hapi_systime_setntp, AUTH_TYPE_ALL },

    { "/ptz_ctrl/stop", hapi_pztctrl_stop, AUTH_TYPE_ALL },
    { "/ptz_ctrl/move", hapi_pztctrl_move_direction, AUTH_TYPE_ALL },
    { "/ptz_ctrl/preset", hapi_pztctrl_preset, AUTH_TYPE_ALL },
    { "/ptz_ctrl/zoom", hapi_pztctrl_zoom, AUTH_TYPE_ALL },
    { "/ptz_ctrl/focus", hapi_pztctrl_focus, AUTH_TYPE_ALL },
    { "/ptz_ctrl/iris", hapi_pztctrl_iris, AUTH_TYPE_ALL },
    { "/ptz_ctrl/advfunction/exec", hapi_pztctrl_advfunction_exec, AUTH_TYPE_ALL },
    { "/ptz_ctrl/advfunction/get", hapi_pztctrl_advfunction_list, AUTH_TYPE_ALL },

    { "/system/light/ctrlmode/capability", hapi_system_light_ctrlmode_capability, AUTH_TYPE_ALL },
    { "/system/light/workmode/capability", hapi_system_light_workmode_capability, AUTH_TYPE_ALL },
    { "/system/light/get", hapi_system_light_get, AUTH_TYPE_ALL },
    { "/system/light/set", hapi_system_light_set, AUTH_TYPE_ALL },
    
    { "/system/image/get", hapi_system_image_get, AUTH_TYPE_ALL },
    { "/system/image/set", hapi_system_image_set, AUTH_TYPE_ALL },

    { "/system/video/capability", hapi_system_video_capability, AUTH_TYPE_ALL },
    { "/system/video/get", hapi_system_video_get, AUTH_TYPE_ALL },
    { "/system/video/set", hapi_system_video_set, AUTH_TYPE_ALL },
    
    { "/system/audio/capability", hapi_system_audio_capability, AUTH_TYPE_ALL },
    { "/system/audio/get", hapi_system_audio_get, AUTH_TYPE_ALL },
    { "/system/audio/set", hapi_system_audio_set, AUTH_TYPE_ALL },
    
    { "/system/osd/get", hapi_system_osd_get, AUTH_TYPE_ALL },
    { "/system/osd/set", hapi_system_osd_set, AUTH_TYPE_ALL },
    
    { "/system/userosd/get", hapi_system_userosd_get, AUTH_TYPE_ALL },
    { "/system/userosd/set", hapi_system_userosd_set, AUTH_TYPE_ALL },
    
    { "/Smart/capability", hapi_smart_capability, AUTH_TYPE_ALL },
    { "/Smart/audiofiles/get", hapi_smart_audiofiles_get, AUTH_TYPE_ALL },
    { "/Smart/objectdetect/capability", hapi_smart_ObjectDetect_capability, AUTH_TYPE_ALL },
    { "/Smart/linkage/capability", hapi_smart_linkageaction_capability, AUTH_TYPE_ALL },
    { "/Smart/motiondetect/get", hapi_smart_motiondetect_get, AUTH_TYPE_ALL },
    { "/Smart/motiondetect/set", hapi_smart_motiondetect_set, AUTH_TYPE_ALL },
    { "/Smart/videocover/get", hapi_smart_videocover_get, AUTH_TYPE_ALL },
    { "/Smart/videocover/set", hapi_smart_videocover_set, AUTH_TYPE_ALL },
    { "/Smart/objectdetect/get", hapi_smart_objectdetect_get, AUTH_TYPE_ALL },
    { "/Smart/objectdetect/set", hapi_smart_objectdetect_set, AUTH_TYPE_ALL },
    { "/Smart/facedetect/get", hapi_smart_fd_get, AUTH_TYPE_ALL },
    { "/Smart/facedetect/set", hapi_smart_fd_set, AUTH_TYPE_ALL },
    { "/Smart/videogate/get", hapi_smart_vg_get, AUTH_TYPE_ALL },
    { "/Smart/videogate/set", hapi_smart_vg_set, AUTH_TYPE_ALL },
    { "/Smart/regionai/get", hapi_smart_regionai_get, AUTH_TYPE_ALL },
    { "/Smart/regionai/set", hapi_smart_regionai_set, AUTH_TYPE_ALL },
    { "/Smart/lpr/get", hapi_smart_lpr_get, AUTH_TYPE_ALL },
    { "/Smart/lpr/set", hapi_smart_lpr_set, AUTH_TYPE_ALL },
    { "/Smart/flameflumes/get", hapi_smart_FlameFlumes_get, AUTH_TYPE_ALL },
    { "/Smart/flameflumes/set", hapi_smart_FlameFlumes_set, AUTH_TYPE_ALL },

    { "/Event/subscription/regist", hapi_event_subscription_regist, AUTH_TYPE_ALL },
    { "/Event/subscription/refresh", hapi_event_subscription_refresh, AUTH_TYPE_ALL },
    { "/Event/subscription/delete", hapi_event_subscription_delete, AUTH_TYPE_ALL },
};

int hapi_sysinfo_function_list(void *pInst, const char *szHttpMethod, const char *szURLFullname, const char* szMsgBody, const char *szURLPrefix, const char *clientip, const char* host, string szUid)
{
    __DBG("url:%s\n", szURLFullname);
    cJSON *pResponseNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pResponseNode, "ResponseURL", szURLPrefix);
    cJSON_AddStringToObject(pResponseNode, "SessionID", szUid.c_str());
    cJSON_AddNumberToObject(pResponseNode, "ResponseCode", 0);
    cJSON_AddStringToObject(pResponseNode, "ResponseString", RESPONSE_OK);

    cJSON *pFunctionList = cJSON_CreateArray(); //这里是数组，才会有方括号
    unsigned int iIndex = 0;
    for(iIndex = 0; iIndex < sizeof(g_HAPI_Table)/sizeof(g_HAPI_Table[0]); iIndex++)
    {
        HAPIStaticTableStruct *p = &g_HAPI_Table[iIndex];
        if(strlen(p->szURLPrefix) == 0)
            continue;

        cJSON *pNodeAdded = cJSON_CreateObject();
        if( NULL != pNodeAdded)
        {
            string szHead = "/HAPI/V1.0[/Channels/ID]";
            string szURLPrefix = szHead + string(p->szURLPrefix);
            cJSON_AddStringToObject(pNodeAdded, "api", szURLPrefix.c_str());
            cJSON_AddItemToArray(pFunctionList, pNodeAdded);
        }
    }
        
    cJSON_AddItemToObject(pResponseNode, "Data", pFunctionList);

    cJSON *pRoot = cJSON_CreateObject();
    cJSON_AddItemToObject(pRoot, "Response", pResponseNode);
    
    char *pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if(NULL != pJsonText)
    {
        http_response_cb(pInst, pJsonText, HTTP_RES_STATUS_OK);

        anj_mw_free(pJsonText);
        pJsonText = NULL;
    }
    
    return HTTP_RES_STATUS_OK;       
}

/************************** 
*    ipc http cgi接口 统一处理url为/HAPI/的api命令
*    name:        http_hapi_handle
*    parameters:    
                pInst 
                szURLFullname    api的url
*                clientip    client ip
*    return:        http status code    
*    added on 2023-11-03
**************************/

int http_hapi_handle(void *pInst, const char *szMethod, const char *szURLFullname, const char* szMsgBody, 
                        const char *clientip, const char* host)
{
    if(NULL == szURLFullname)
        return -1;

    int iRet = 0;
    char szHttpURL[512] = {0};
    strncpy(szHttpURL, szURLFullname, sizeof(szHttpURL) - 1);

    //将//替换成/
    char *pos = strstr(szHttpURL, "//");
    while (pos != NULL)
    {
        // 将第一个'/'向后移动一位，覆盖第二个'/'
        memmove(pos, pos + 1, strlen(pos));
        pos = strstr(szHttpURL, "//");
    }

    // 查找 "/HAPI/V1.0"
    const char *pHead = strstr(szHttpURL, HAPI_V1_HEAD);
    if( NULL == pHead)
    {
        return -1;
    }

    // 完整头部信息
    char szFullHead[256] = {0};
    strncpy(szFullHead, HAPI_V1_HEAD, sizeof(szFullHead) - 1);

    unsigned int nChannels = 0;
    const char *pChannels = strstr(szHttpURL, HEAD_CHANNELS);
    if(NULL != pChannels)
    {
        pChannels += strlen(HEAD_CHANNELS);
        nChannels = atoi(pChannels);

        // 带chn的header信息
        strcat(szFullHead, HEAD_CHANNELS);
        char szTmp[16] = {0};
        sprintf(szTmp, "%u", nChannels);
        strcat(szFullHead, szTmp);
    }

    const char *pEnd = NULL;
    pHead += strlen(szFullHead);
    pEnd = strstr(pHead, "?");
    if(NULL == pEnd)
    {
        pEnd = strstr(pHead, "&");
    }
    if(NULL == pEnd)
    {
        pEnd = pHead + strlen(pHead);
    }

    char szPrefixHeader[256] = {0};
    int prefix_len = pEnd - pHead;
    if (prefix_len > 0 && prefix_len < (int)sizeof(szPrefixHeader))
    {
        strncpy(szPrefixHeader, pHead, prefix_len);
        szPrefixHeader[prefix_len] = '\0';
    }
    __DBG("prefix=%s, URL=%s, head=%s\n", szPrefixHeader, szHttpURL, szFullHead);

    int bFound = 0;
    unsigned int iIndex = 0;
    for(iIndex = 0; iIndex < sizeof(g_HAPI_Table)/sizeof(g_HAPI_Table[0]); iIndex++)
    {
        HAPIStaticTableStruct *p = &g_HAPI_Table[iIndex];
        if(strlen(p->szURLPrefix) == 0)
        {
            continue;
        }

        if(strcmp(p->szURLPrefix, szPrefixHeader) == 0)
        {
            __INFO("found prefix:%s\n",p->szURLPrefix);
            if(p->cb != NULL)
            {
                bFound = 1;

                char szURLPrefix[512] = {0};
                snprintf(szURLPrefix, sizeof(szURLPrefix), "%s%s", szFullHead, p->szURLPrefix);

                char szUsername[64] = {0};
                char szPassword[64] = {0};
                char szUid[64] = {0};


                if(p->eSupportAuthType != AUTH_TYPE_NO)     //统一在这里进行认证处理
                {
                    hapi_get_authentication_info(szMethod, szHttpURL, szMsgBody, 
                                                 szUsername, sizeof(szUsername),
                                                 szPassword, sizeof(szPassword),
                                                 szUid, sizeof(szUid));

                    iRet = hapi_authentication(pInst, clientip, szURLPrefix, p->eSupportAuthType, szUsername, szPassword, szUid);
                    if(iRet != 0)
                    {
                        break;
                    }              
                }

                p->cb(pInst, szMethod, szHttpURL, szMsgBody, szURLPrefix, clientip, host, szUid);
            }
            break;
        }
    }

    if(!bFound)
    {
        hapi_no_interface(pInst, szMethod, szHttpURL, szMsgBody, szHttpURL, clientip, host, "");
    }

    return 0;
}

