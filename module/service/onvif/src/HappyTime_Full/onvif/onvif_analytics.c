/***************************************************************************************
 *
 *  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 *
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install, 
 *  copy or use the software.
 *
 *  Copyright (C) 2014-2024, Happytimesoft Corporation, all rights reserved.
 *
 *  Redistribution and use in binary forms, with or without modification, are permitted.
 *
 *  Unless required by applicable law or agreed to in writing, software distributed 
 *  under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 *  CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
 *  language governing permissions and limitations under the License.
 *
****************************************************************************************/

#include "sys_inc.h"
#include "onvif.h"
#include "onvif_cm.h"
#include "onvif_analytics.h"
#include "xml_node.h"
#include "onvif_extool.h"
#include "../bm/base64.h"
#ifdef VIDEO_ANALYTICS

/***************************************************************************************/
extern ONVIF_CFG g_onvif_cfg;
extern ONVIF_CLS g_onvif_cls;

/***************************************************************************************/

/**
 * @brief
 *  A device signaling support for rules via the RuleSupport capability shall 
 *  support this operation. It returns a list of rule descriptions according 
 *  to the Rule Description Language.Additionally, it contains a list of URLs 
 *  that provide the location of the schema files. These schema files describe 
 *  the types and elements used in the rule descriptions. Rule descriptions 
 *  that reference types or elements imported from any ONVIF defined schema 
 *  files need not explicitly list those schema files. The device shall indicate
 *  its limit for maximum number of rules through the maxInstances attribute.
 *
 * @return
 *  The possible return value
 *	ONVIF_ERR_NoConfig
 **/
ONVIF_RET onvif_tan_GetSupportedRules(tan_GetSupportedRules_REQ * p_req, tan_GetSupportedRules_RES * p_res)
{
	VideoAnalyticsConfigurationList * p_va_cfg;

	p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_req->ConfigurationToken);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	memcpy(&p_res->SupportedRules, &p_va_cfg->SupportedRules, sizeof(onvif_SupportedRules));
	
	return ONVIF_OK;
}

/**
 * @brief
 *  A device signaling support for rules via the RuleSupport capability shall 
 *  support this operation to add rules to an AnalyticsConfiguration.
 *
 *  If all rules can not be created as requested, the device responds with
 *  a fault message.
 *
 *  The device shall accept adding of analytics rules with an empty Parameter
 *  definition. Note that the resulted configuration may include a set of 
 *  default parameter values.
 *
 * @return
 *  The possible return value
 *  ONVIF_ERR_NoConfig
 *  ONVIF_ERR_InvalidRule,
 *  ONVIF_ERR_RuleAlreadyExistent
 *  ONVIF_ERR_TooManyRules
 *  ONVIF_ERR_ConfigurationConflict
 **/
ONVIF_RET onvif_tan_CreateRules(tan_CreateRules_REQ * p_req)
{
	VideoAnalyticsConfigurationList * p_va_cfg;
	ConfigList * p_config;

	if (NULL == p_req->Rule)
	{
		return ONVIF_ERR_InvalidRule;
	}
	
	p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_req->ConfigurationToken);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	p_config = onvif_find_Config(p_va_cfg->Configuration.RuleEngineConfiguration.Rule, p_req->Rule->Config.Name);
	if (NULL != p_config)
	{
		return ONVIF_ERR_RuleAlreadyExistent;
	}
	
	// check rule configuration pararmeters ...

	p_config = p_va_cfg->Configuration.RuleEngineConfiguration.Rule;
	if (NULL == p_config)
	{
		p_va_cfg->Configuration.RuleEngineConfiguration.Rule = p_req->Rule;
	}
	else
	{
		while (p_config && p_config->next) p_config = p_config->next;

		p_config->next = p_req->Rule;
	}
	
	return ONVIF_OK;
}

/**
 * @brief
 *  A device signaling support for rules via the RuleSupport capability shall
 *  support this operation to delete one or more rules. If all rules can not
 *  be deleted as requested, the device responds with a fault message.
 *
 * @return
 *  The possible return value
 *  ONVIF_ERR_NoConfig
 *  ONVIF_ERR_RuleNotExistent
 *  ONVIF_ERR_ConfigurationConflict
 **/
ONVIF_RET onvif_tan_DeleteRules(tan_DeleteRules_REQ * p_req)
{
	int i;
	VideoAnalyticsConfigurationList * p_va_cfg;
	ConfigList * p_config;
	
	p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_req->ConfigurationToken);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	for (i = 0; i < p_req->sizeRuleName; i++)
	{
		p_config = onvif_find_Config(p_va_cfg->Configuration.RuleEngineConfiguration.Rule, p_req->RuleName[i]);
		if (NULL == p_config)
		{
			return ONVIF_ERR_RuleNotExistent;
		}

		onvif_remove_Config(&p_va_cfg->Configuration.RuleEngineConfiguration.Rule, p_config);
	}
	
	return ONVIF_OK;
}

/**
 * @brief
 *  A device signaling support for rules via the RuleSupport capability shall
 *  support this operation to retrieve the currently associated rules with a 
 *  video analytics configuration.
 *
 * @return
 *  The possible return value
 *  ONVIF_ERR_NoConfig
 **/
ONVIF_RET onvif_tan_GetRules(tan_GetRules_REQ * p_req, tan_GetRules_RES * p_res)
{
	VideoAnalyticsConfigurationList * p_va_cfg;
	
	p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_req->ConfigurationToken);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	p_res->Rule = p_va_cfg->Configuration.RuleEngineConfiguration.Rule;
	
	return ONVIF_OK;
}

/**
 * @brief
 *  A device signaling support for modifying rules via the RuleOptionsSupported
 *  capability shall support this operation. If all rules can not be modified 
 *  as requested, the device responds with a fault message. A device may reject
 *  a request to change the rule type.
 *
 *  A device should interpret parameters not present in the payload as unchanged. 
 *  Note that this does not always result in unchanged values of such parameters, 
 *  since some parameters may change due to dependency on others.
 *
 * @return
 *  The possible return value
 *  ONVIF_ERR_NoConfig
 *  ONVIF_ERR_InvalidRule,
 *  ONVIF_ERR_RuleNotExistent
 *  ONVIF_ERR_TooManyRules
 *  ONVIF_ERR_ConfigurationConflict
 **/
ONVIF_RET onvif_tan_ModifyRules(tan_ModifyRules_REQ * p_req)
{
	VideoAnalyticsConfigurationList * p_va_cfg;
	ConfigList * p_config;
	ConfigList * p_tmp;
	ConfigList * p_prev;
	ConfigList * p_next;
	
	SimpleItemList *p_simpleitem;
	ElementItemList *p_elementitem;
	
	if (NULL == p_req->Rule)
	{
		return ONVIF_ERR_InvalidRule;
	}
	
	p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_req->ConfigurationToken);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	p_tmp = p_req->Rule;
	while (p_tmp)
	{
		// check rule configuration parameters ...

		p_config = onvif_find_Config(p_va_cfg->Configuration.RuleEngineConfiguration.Rule, p_tmp->Config.Name);
		if (NULL == p_config)
		{
			onvif_free_Configs(&p_tmp);	// free resource
			
			return ONVIF_ERR_RuleNotExistent;
		}

		if (!strcmp(p_tmp->Config.Name, "MyMotionDetectorRule") && !strcmp(p_tmp->Config.Type, "tt:CellMotionDetector"))
		{
			MotionDetectAlarm md_alarm;
			memset(&md_alarm, 0, sizeof(MotionDetectAlarm));
			memcpy(&md_alarm, &((AlarmConfig *)getAlarmConfig())->normalAlarm.motionDetectAlarm[0], sizeof(md_alarm));
			
			int nBlockX = (md_alarm.blockCount >> 16) & 0xffff;
			int nBlockY = (md_alarm.blockCount >> 0) & 0xffff;
			
			p_simpleitem = p_tmp->Config.Parameters.SimpleItem;
			while (p_simpleitem)
			{
				if (strcmp(p_simpleitem->SimpleItem.Name, "ActiveCells") == 0)
				{
					char str_activeCells[512] = {0};
					char bits_activeCells[400/8];  //the md divide to 22*18=396
					int str_len, bits_len;
					//log_print(HT_LOG_INFO, "p_simpleitem->SimpleItem.Value:%s\n\n", p_simpleitem->SimpleItem.Value);
					base64_decode_bm(p_simpleitem->SimpleItem.Value, strlen(p_simpleitem->SimpleItem.Value), (unsigned char *)str_activeCells, sizeof(str_activeCells));
					int i;
					for (i = 512; i >= 0; i--)
					{
						if(str_activeCells[i - 1] != 0x00)
						{
							str_len = i;
							break;
						}
					}
					bits_len = tiff6_unPackBits(str_activeCells, str_len, (unsigned char *)bits_activeCells);
					if(nBlockX != 22 || nBlockY != 18)
					{
						nBlockX = 22;
						nBlockY = 18;
						md_alarm.blockCount = (nBlockX << 16)|(nBlockY & 0xffff);
					}
					memset(md_alarm.blockCfg, 0, sizeof(md_alarm.blockCfg));
					memset(md_alarm.blockCfg, '0', nBlockX * nBlockY);
					int k;
					for( k = 0; k < bits_len; k++)
					{
						char sztmp[12];
						memset(sztmp, 0, 12);
						int n = 0;
						for( n = 0; n < 8; n ++)
						{
							int r = 7 - n;
							int index = k * 8 + n;
							if( index > nBlockX * nBlockY )
								break;
							int d = (bits_activeCells[k] >> r) & 1;
							sztmp[n] = '0' + d;
							md_alarm.blockCfg[index] = '0' + d; 
						}
						//log_print(HT_LOG_INFO, "bytes pos %d: %#x, bits: %s\n", k, bits_activeCells[k], sztmp);
					}
					md_alarm.enable = 1;
					md_alarm.arming_flag = 4;
					md_alarm.dayNightSwitch = 1;
					memset(&md_alarm.timeSpan,0,sizeof(md_alarm.timeSpan));
					int kkk;
					for( kkk = 0 ; kkk < MAX_WORDDAYTIME_COUNT; kkk++)
					{
						md_alarm.timeSpan.workday[kkk] = 0xffffff;
					}
				}
				p_simpleitem = p_simpleitem->next;
			}
			
			p_elementitem = p_tmp->Config.Parameters.ElementItem;
			while (p_elementitem)
			{
				if (strcmp(p_elementitem->ElementItem.Name, "hb_ext") == 0)
				{
					char *pAnyBuf = (char *)malloc(strlen(p_elementitem->ElementItem.Any) + 40);
					sprintf(pAnyBuf, "<MDAlarm>%s</MDAlarm>", p_elementitem->ElementItem.Any);
					char *pIndex = pAnyBuf;
					while(strlen(pIndex) > 0)
					{
						char *pTT = strstr(pIndex, "tt:");
						if(pTT)
							*(pTT+2) = '_';
						pIndex ++;
					}
					get_HB_MDCfgByXml(&md_alarm, pAnyBuf);
				}
				p_elementitem = p_elementitem->next;
			}
			anj_config_alarm_motion_set(&md_alarm);
		}
		
		if (!strcmp(p_tmp->Config.Name, "MySmartMotionDetector") && !strcmp(p_tmp->Config.Type, "tt:SmartMotionDetector"))
		{
			PdAlarm pAlarmt;
			memset(&pAlarmt, 0, sizeof(PdAlarm));
			memcpy(&pAlarmt, &((AlarmConfig *)getAlarmConfig())->aiAlarm.pdAlarm[0], sizeof(pAlarmt));

			p_simpleitem = p_tmp->Config.Parameters.SimpleItem;
			while (p_simpleitem)
			{
				if (strcmp(p_simpleitem->SimpleItem.Name, "Sensitivity") == 0)
				{
					int Sensitivity = atoi(p_simpleitem->SimpleItem.Value);
					log_print(HT_LOG_INFO, "Sensitivity:%d\n", Sensitivity);
					pAlarmt.sensitivity = Sensitivity / 10;
					pAlarmt.threshold = Sensitivity % 10;
				}
				if (strcmp(p_simpleitem->SimpleItem.Name, "Type") == 0)
				{
					int Type = atoi(p_simpleitem->SimpleItem.Value);
					log_print(HT_LOG_INFO, "Type:%d\n", Type);
					
					int type = pAlarmt.type;
					char type_buf[33] = {0};
					type_buf[32] = '\0';
					ToBin(type, type_buf);
					if (Type == 0){
						type_buf[31-0] = 0x30;
						type_buf[31-4] = 0x30;
					}
					else if(Type == 1){
						type_buf[31-0] = 0x30;
						type_buf[31-4] = 0x31;
					}
					else if(Type == 2){
						type_buf[31-0] = 0x31;
						type_buf[31-4] = 0x30;
					}
					else if(Type == 3){
						type_buf[31-0] = 0x31;
						type_buf[31-4] = 0x31;
					}
					type = ToInt(type_buf);
					pAlarmt.type = type;
				}
				p_simpleitem = p_simpleitem->next;
			}
			
			p_elementitem = p_tmp->Config.Parameters.ElementItem;
			while (p_elementitem)
			{
				if (strcmp(p_elementitem->ElementItem.Name, "Date") == 0)
				{
					char *date = malloc(1024);
					log_print(HT_LOG_INFO, "p_elementitem->ElementItem.Any:\n%s\n", p_elementitem->ElementItem.Any);
					get_xml_value_Date(p_elementitem->ElementItem.Any, "tt:ElementItem Name=\"Week\" Value=\"Sunday\"", "tt:ElementItem", date);
					Set_date(date, 0, &pAlarmt);
					memset(date, 0, 1024);
					get_xml_value_Date(p_elementitem->ElementItem.Any, "tt:ElementItem Name=\"Week\" Value=\"Monday\"", "tt:ElementItem", date);
					Set_date(date, 1, &pAlarmt);
					memset(date, 0, 1024);
					get_xml_value_Date(p_elementitem->ElementItem.Any, "tt:ElementItem Name=\"Week\" Value=\"Tuesday\"", "tt:ElementItem", date);
					Set_date(date, 2, &pAlarmt);
					memset(date, 0, 1024);
					get_xml_value_Date(p_elementitem->ElementItem.Any, "tt:ElementItem Name=\"Week\" Value=\"Wednesday\"", "tt:ElementItem", date);
					Set_date(date, 3, &pAlarmt);
					memset(date, 0, 1024);
					get_xml_value_Date(p_elementitem->ElementItem.Any, "tt:ElementItem Name=\"Week\" Value=\"Thursday\"", "tt:ElementItem", date);
					Set_date(date, 4, &pAlarmt);
					memset(date, 0, 1024);
					get_xml_value_Date(p_elementitem->ElementItem.Any, "tt:ElementItem Name=\"Week\" Value=\"Friday\"", "tt:ElementItem", date);
					Set_date(date, 5, &pAlarmt);
					memset(date, 0, 1024);
					get_xml_value_Date(p_elementitem->ElementItem.Any, "tt:ElementItem Name=\"Week\" Value=\"Saturday\"", "tt:ElementItem", date);
					Set_date(date, 6, &pAlarmt);
					memset(date, 0, 1024);
					
					free(date);
				}
				if (strcmp(p_elementitem->ElementItem.Name, "Field") == 0)
				{
					//log_print(HT_LOG_INFO, "Field:%s\n", p_elementitem->ElementItem.Name);
					int i;
					int Num = 0;
					Get_Elementltem_Coordinate_Num(p_elementitem->ElementItem.Any, &Num);
					
					int xy[Num][2];
					Get_ElementItem_Coordinate_Value(p_elementitem->ElementItem.Any, (int**)xy, Num, 2);
					pAlarmt.polygonArea.count = Num;
					
					for (i = 0; i < Num; i++)
					{
						pAlarmt.polygonArea.points[i].x = xy[i][0]/100;
						pAlarmt.polygonArea.points[i].y = xy[i][1]/100;
						//log_print(HT_LOG_INFO, "x%d = %d, y%d = %d\n", i, xy[i][0], i, xy[i][1]);
					}
				}
				p_elementitem = p_elementitem->next;
			}
			anj_config_alarm_pd_set(&pAlarmt);
		}
		p_prev = onvif_get_prev_Config(p_va_cfg->Configuration.RuleEngineConfiguration.Rule, p_config);
		if (NULL == p_prev)
		{
			p_va_cfg->Configuration.RuleEngineConfiguration.Rule = p_tmp;
			p_next = p_tmp->next;
			p_tmp->next = p_config->next;
		}
		else
		{
			p_prev->next = p_tmp;
			p_next = p_tmp->next;
			p_tmp->next = p_config->next;
		}

		onvif_free_Config(p_config);
		free(p_config);

		p_tmp = p_next;
	}
	
	return ONVIF_OK;
}

/**
 * @brief
 *  A device signaling support for analytics modules via the 
 *  AnalyticsModuleSupport capability shall support this method to add 
 *  analytics modules to an analytics configuration.
 *
 *  The device shall accept adding of analytics modules with an empty 
 *  Parameter definition. Note that the resulted configuration may include
 *  a set of default parameter values.
 *
 * @return
 *  The possible return value
 *  ONVIF_ERR_NoConfig
 *  ONVIF_ERR_NameAlreadyExistent
 *  ONVIF_ERR_TooManyModules
 *  ONVIF_ERR_InvalidModule
 *  ONVIF_ERR_ConfigurationConflict
 **/
ONVIF_RET onvif_tan_CreateAnalyticsModules(tan_CreateAnalyticsModules_REQ * p_req)
{
	VideoAnalyticsConfigurationList * p_va_cfg;
	ConfigList * p_config;

	if (NULL == p_req->AnalyticsModule)
	{
		return ONVIF_ERR_InvalidModule;
	}
	
	p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_req->ConfigurationToken);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	p_config = onvif_find_Config(p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule, p_req->AnalyticsModule->Config.Name);
	if (NULL != p_config)
	{
		return ONVIF_ERR_NameAlreadyExistent;
	}
	
	// check analytics module configuration pararmeters ...

	p_config = p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule;
	if (NULL == p_config)
	{
		p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule = p_req->AnalyticsModule;
	}
	else
	{
		while (p_config && p_config->next) p_config = p_config->next;

		p_config->next = p_req->AnalyticsModule;
	}
	
	return ONVIF_OK;
}

/**
 * @breif
 *  A device signaling support for analytics modules via the 
 *  AnalyticsModuleSupport capability shall support this method for removing
 *  analytics module from an analytics configuration.
 *
 * @return
 *  The possible return value
 *  ONVIF_ERR_NoConfig
 *  ONVIF_ERR_InvalidModule
 *  ONVIF_ERR_ConfigurationConflict
 **/
ONVIF_RET onvif_tan_DeleteAnalyticsModules(tan_DeleteAnalyticsModules_REQ * p_req)
{
	int i;
	VideoAnalyticsConfigurationList * p_va_cfg;
	ConfigList * p_config;
	
	p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_req->ConfigurationToken);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	for (i = 0; i < p_req->sizeAnalyticsModuleName; i++)
	{
		p_config = onvif_find_Config(p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule, p_req->AnalyticsModuleName[i]);
		if (NULL == p_config)
		{
			return ONVIF_ERR_InvalidModule;
		}

		onvif_remove_Config(&p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule, p_config);
	}
	
	return ONVIF_OK;
}

/**
 * @brief
 *  A device signaling support for analytics modules via the 
 *  AnalyticsModuleSupport capability shall support this method to retrieve
 *  currently associated analytics modules for an analytics configuration.
 *
 * @return
 *  The possible return value
 *  ONVIF_ERR_NoConfig
 **/
ONVIF_RET onvif_tan_GetAnalyticsModules(tan_GetAnalyticsModules_REQ * p_req, tan_GetAnalyticsModules_RES * p_res)
{
	VideoAnalyticsConfigurationList * p_va_cfg;
	
	p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_req->ConfigurationToken);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	p_res->AnalyticsModule = p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule;
	
	return ONVIF_OK;
}

/**
 * @brief
 *  A device signaling support for analytics modules via the 
 *  AnalyticsModuleOptionsSupported capability shall support this method
 *  to modify analytics module configurations. A device may reject a 
 *  request to change the module type.
 *
 * @return
 *  The possible return value
 *  ONVIF_ERR_NoConfig
 *  ONVIF_ERR_InvalidModule
 *  ONVIF_ERR_TooManyModules
 *  ONVIF_ERR_InvalidModule
 *  ONVIF_ERR_ConfigurationConflict
 **/
ONVIF_RET onvif_tan_ModifyAnalyticsModules(tan_ModifyAnalyticsModules_REQ * p_req)
{
	VideoAnalyticsConfigurationList * p_va_cfg;
	ConfigList * p_config;
	ConfigList * p_tmp;
	ConfigList * p_prev;
	ConfigList * p_next;

	SimpleItemList *p_simpleitem;
	ElementItemList *p_elementitem;

	if (NULL == p_req->AnalyticsModule)
	{
		return ONVIF_ERR_InvalidRule;
	}
	
	p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_req->ConfigurationToken);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	p_tmp = p_req->AnalyticsModule;
	while (p_tmp)
	{
		// check rule configuration parameters ...

		p_config = onvif_find_Config(p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule, p_tmp->Config.Name);
		if (NULL == p_config)
		{
			// free resource 
			onvif_free_Configs(&p_tmp);

			return ONVIF_ERR_InvalidModule;
		}

		
		if (!strcmp(p_tmp->Config.Name, "MySmartMotionDetector") && !strcmp(p_tmp->Config.Type, "tt:SmartMotionDetectorEngine"))
		{
			PdAlarm pAlarmt;
			memset(&pAlarmt, 0, sizeof(PdAlarm));
			memcpy(&pAlarmt, &((AlarmConfig *)getAlarmConfig())->aiAlarm.pdAlarm[0], sizeof(pAlarmt));
			
			p_simpleitem = p_tmp->Config.Parameters.SimpleItem;
			while (p_simpleitem)
			{
				if (!strcmp(p_simpleitem->SimpleItem.Name, "Enable"))
				{
					if (!strcmp(p_simpleitem->SimpleItem.Value, "true"))
					{
						pAlarmt.enable = 1;
						pAlarmt.arming_flag = ARMING_ALLDAY;
					}
					else
					{
						pAlarmt.enable = 0;
						pAlarmt.arming_flag = ARMING_DISABLE;
					}
				}
				p_simpleitem = p_simpleitem->next;
			}
			
			anj_config_alarm_pd_set(&pAlarmt);
			
		}
		
		if (!strcmp(p_tmp->Config.Name, "MyCellMotionModule") && !strcmp(p_tmp->Config.Type, "tt:CellMotionEngine"))
		{
			MotionDetectAlarm md_alarm;
			memset(&md_alarm, 0, sizeof(MotionDetectAlarm));
			memcpy(&md_alarm, &((AlarmConfig *)getAlarmConfig())->normalAlarm.motionDetectAlarm[0], sizeof(md_alarm));
			
			p_simpleitem = p_tmp->Config.Parameters.SimpleItem;
			while (p_simpleitem)
			{
				if (!strcmp(p_simpleitem->SimpleItem.Name, "Sensitivity"))
				{
					md_alarm.sensitivity = atoi(p_simpleitem->SimpleItem.Value);
					if(!md_alarm.sensitivity)
					{
						//md_alarm.enable = 0;
					}
					else
					{
						md_alarm.enable = 1;
						md_alarm.dayNightSwitch = 1;
					}
				}
				p_simpleitem = p_simpleitem->next;
			}

			p_elementitem = p_tmp->Config.Parameters.ElementItem;
			while (p_elementitem)
			{
				if (strcmp(p_elementitem->ElementItem.Name, "Layout") == 0)
				{
					int i;
					int col = 0, row = 0;
					col = atoi(strlen("Columns=\"") + strstr(p_elementitem->ElementItem.Any, "Columns=\""));
					row = atoi(strlen("Rows=\"") + strstr(p_elementitem->ElementItem.Any, "Rows=\""));

					if(col != 22 || row != 18)
					{
						col = 22;
						row = 18;
					}
					log_print(HT_LOG_INFO, "%s, parse Columns=%d, Rows=%d\n", p_elementitem->ElementItem.Any, col, row);
					if(col && row)
					{
						md_alarm.blockCount = (col<<16) + row;
						log_print(HT_LOG_INFO, "md_alarm.blockCount=%d (%#x)\n", md_alarm.blockCount, md_alarm.blockCount);
					}
					for( i = 0; i < col*row; i++)
					{
						if( md_alarm.blockCfg[i] != '0' && md_alarm.blockCfg[i] != '1' ) md_alarm.blockCfg[i] = '0' ;
					}
				}
				p_elementitem = p_elementitem->next;
			}
			
			anj_config_alarm_motion_set(&md_alarm);
		}
		p_prev = onvif_get_prev_Config(p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule, p_config);
		if (NULL == p_prev)
		{
			p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule = p_tmp;
			p_next = p_tmp->next;
			p_tmp->next = p_config->next;
		}
		else
		{
			p_prev->next = p_tmp;
			p_next = p_tmp->next;
			p_tmp->next = p_config->next;
		}

		onvif_free_Config(p_config);
		free(p_config);

		p_tmp = p_next;
	}
	
	return ONVIF_OK;
}

/**
 * @brief
 *  This operation allows to query what metadata a device can generate. 
 *  It shall be supported if the SupportedMetadata capability is set to true.
 *
 *  The response contains the following information:
 *  SampleFrame [tt:Frame]
 *  An example Frame instance. The instance shall include all elements that 
 *  the analytics module outputs to the frame of a metadata stream. A device 
 *  implementation should strive for including all supported enumeration values.
 *
 *  If the sample frame includes object bounding boxes or shapes, these shall
 *  be located in the top left quarter of the image.
 *
 * @return
 *  The possible return value
 *  TypeNotExistent
 **/
ONVIF_RET onvif_tan_GetSupportedMetadata(tan_GetSupportedMetadata_REQ * p_req, tan_GetSupportedMetadata_RES * p_res)
{
	int found = 0;
	int buflen = 4096;
	ConfigList * p_cfg_list = NULL;
	VideoAnalyticsConfigurationList * p_va_cfg = NULL;

	// check the request parameter

	if (p_req->Type[0] != '\0')
	{
		p_va_cfg = g_onvif_cfg.va_cfg;
		while (p_va_cfg)
		{
			p_cfg_list = p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule;
			while (p_cfg_list)
			{
				if (soap_strcmp(p_cfg_list->Config.Type, p_req->Type) == 0)
				{
					found = 1;
					break;
				}

				p_cfg_list = p_cfg_list->next;
			}

			if (found)
			{
				break;
			}

			p_va_cfg = p_va_cfg->next;
		}

		if (!found)
		{
			return ONVIF_ERR_TypeNotExistent;
		}
	}

	if (NULL == p_cfg_list)
	{
		if (NULL == p_va_cfg)
		{
			p_va_cfg = g_onvif_cfg.va_cfg;
		}

		if (p_va_cfg)
		{
			p_cfg_list = p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule;
		}
	}

	while (p_cfg_list)
	{
		int idx = p_res->sizeAnalyticsModule;

		p_res->AnalyticsModule[idx].attrFlag = p_cfg_list->Config.attrFlag;
		strcpy(p_res->AnalyticsModule[idx].Type, p_cfg_list->Config.Type);
		strcpy(p_res->AnalyticsModule[idx].attr, p_cfg_list->Config.attr);

		p_res->AnalyticsModule[idx].Frame = (char *)malloc(buflen);
		if (p_res->AnalyticsModule[idx].Frame)
		{
			int offset = 0;
			char tstr[64];
			char * buff = p_res->AnalyticsModule[idx].Frame;

			get_tstring_by_time(time(NULL), tstr, sizeof(tstr));

			offset += snprintf(buff+offset, buflen-offset, 
				"<tan:SampleFrame UtcTime=\"%s\">", tstr);

			// todo : add the SampleFrame content
			offset += snprintf(buff+offset, buflen-offset, 
				"<tt:Object ObjectId=\"1\">"
					"<tt:Appearance>"
						"<tt:VehicleInfo>"
							"<tt:Type Likelihood=\"0.9\">Car</tt:Type>"
							"<tt:Brand Likelihood=\"0.7\">Volvo</tt:Brand>"
							"<tt:Model Likelihood=\"0.6\">XC60</tt:Model>"
						"</tt:VehicleInfo>"
					"</tt:Appearance>"
				"</tt:Object>");

			offset += snprintf(buff+offset, buflen-offset, 
				"<tt:Object ObjectId=\"2\">"
					"<tt:Appearance>"
						"<tt:LicensePlateInfo>"
							"<tt:PlateNumber Likelihood=\"0.9\">6DZG261</tt:PlateNumber>"
							"<tt:PlateType Likelihood=\"0.8\">Normal</tt:PlateType>"
							"<tt:CountryCode Likelihood=\"0.7\">US</tt:CountryCode>"
							"<tt:IssuingEntity Likelihood=\"0.6\">California</tt:IssuingEntity>"
						"</tt:LicensePlateInfo>"
					"</tt:Appearance>"
				"</tt:Object>");

			offset += snprintf(buff+offset, buflen-offset, 
				"<tt:Object ObjectId=\"3\">"
					"<tt:Appearance>"
						"<tt:HumanFace>"
							"<tt:Gender Likelihood=\"0.8\">Male</tt:Gender>"
							"<tt:Temperature>36.8</tt:Temperature>"
							"<tt:Hair Likelihood=\"0.6\">Straight</tt:Hair>"
						"</tt:HumanFace>"
					"</tt:Appearance>"
				"</tt:Object>");

			offset += snprintf(buff+offset, buflen-offset, 
				"<tt:Object ObjectId=\"4\">"
					"<tt:Appearance>"
						"<tt:HumanBody>"
							"<tt:BodyMetric>"
								"<tt:Height>172</tt:Height>"
								"<tt:BodyShape>Fat</tt:BodyShape>"
							"</tt:BodyMetric>"
						"</tt:HumanBody>"
					"</tt:Appearance>"
				"</tt:Object>");

			offset += snprintf(buff+offset, buflen-offset, 
				"<tt:Object ObjectId=\"5\">"
					"<tt:Appearance>"
						"<tt:GeoLocation>"
							"<tt:lon>33.423</tt:lon>"
							"<tt:lat>146.443</tt:lat>"
						"</tt:GeoLocation>"
					"</tt:Appearance>"
				"</tt:Object>");

			offset += snprintf(buff+offset, buflen-offset, "</tan:SampleFrame>");
		}

		p_res->sizeAnalyticsModule++;
		if (p_res->sizeAnalyticsModule >= ARRAY_SIZE(p_res->AnalyticsModule))
		{
			break;
		}

		if (found)
		{
			break;
		}

		p_cfg_list = p_cfg_list->next;
	}

	return ONVIF_OK;
}

#endif	// end of VIDEO_ANALYTICS


