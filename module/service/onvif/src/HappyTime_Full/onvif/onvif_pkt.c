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

/***************************************************************************************/
#include "sys_inc.h"
#include "onvif.h"
#include "xml_node.h"
#include "onvif_device.h"
#include "onvif_pkt.h"
#include "onvif_event.h"
#include "onvif_ptz.h"
#include "onvif_utils.h"
#include "onvif_err.h"
#include "onvif_media.h"
#include "onvif_image.h"
// #include "cgi_def.h"

#ifdef VIDEO_ANALYTICS
#include "onvif_analytics.h"
#endif
#ifdef PROFILE_G_SUPPORT
#include "onvif_recording.h"
#endif
#ifdef PROFILE_C_SUPPORT
#include "onvif_doorcontrol.h"
#endif
#ifdef DEVICEIO_SUPPORT
#include "onvif_deviceio.h"
#endif
#ifdef MEDIA2_SUPPORT
#include "onvif_media2.h"
#endif
#ifdef THERMAL_SUPPORT
#include "onvif_thermal.h"
#endif
#ifdef CREDENTIAL_SUPPORT
#include "onvif_credential.h"
#endif
#ifdef ACCESS_RULES
#include "onvif_accessrules.h"
#endif
#ifdef SCHEDULE_SUPPORT
#include "onvif_schedule.h"
#endif
#ifdef RECEIVER_SUPPORT
#include "onvif_receiver.h"
#endif
#ifdef PROVISIONING_SUPPORT
#include "onvif_provisioning.h"
#endif

#if __WINDOWS_OS__
#pragma warning(disable:4996)
#endif

/***************************************************************************************/
extern ONVIF_CFG g_onvif_cfg;
extern ONVIF_CLS g_onvif_cls;

extern char xml_hdr[];
extern char onvif_xmlns[];
extern char soap_head[];
extern char soap_body[];
extern char soap_tailer[];
extern MediaConfig gMediaCfg;

extern ptz_tour_ctx_t g_tour_ctx;
extern int g_mask_flag[4];
extern int g_openeye_smw;
extern int g_stereo;
extern int g_onvif_expand;
extern int g_hasIO;
extern int g_isHxVersion;
extern int g_bHxVersion;
/***************************************************************************************/


int build_err_rly_xml
(
char * p_buf, 
int mlen, 
const char * code, 
const char * subcode, 
const char * subcode_ex, 
const char * reason,
const char * action
)
{
	int offset = snprintf(p_buf, mlen, "%s", xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, "%s", onvif_xmlns);

	if (action)
	{
		offset += snprintf(p_buf+offset, mlen-offset, soap_head, action);
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "%s", soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<s:Fault>");
	offset += snprintf(p_buf+offset, mlen-offset, "<s:Code>");
	offset += snprintf(p_buf+offset, mlen-offset, "<s:Value>%s</s:Value>", code);

	if (subcode)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<s:Subcode>");
		offset += snprintf(p_buf+offset, mlen-offset, "<s:Value>%s</s:Value>", subcode);

		if (subcode_ex)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<s:Subcode><s:Value>%s</s:Value></s:Subcode>", subcode_ex);
		}

		offset += snprintf(p_buf+offset, mlen-offset, "</s:Subcode>");
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</s:Code>");

	if (reason)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<s:Reason><s:Text xml:lang=\"en\">%s</s:Text></s:Reason>", reason);
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</s:Fault>");

	offset += snprintf(p_buf+offset, mlen-offset, "%s", soap_tailer);
	
	return offset;
}

/***************************************************************************************/

int build_DeviceCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen)
{
	uint32 i;
	int offset = 0;
	char saddr[256] = {'\0'};
	onvif_DevicesCapabilities * p_cap = &g_onvif_cfg.Capabilities.device;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Device>"
			"<tt:XAddr>%s</tt:XAddr>",
		onvif_get_service_addr_by_user(CapabilityCategory_Device, p_user, saddr, sizeof(saddr)-1));

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Network>"
			"<tt:IPFilter>%s</tt:IPFilter>"
			"<tt:ZeroConfiguration>%s</tt:ZeroConfiguration>"
			"<tt:IPVersion6>%s</tt:IPVersion6>"
			"<tt:DynDNS>%s</tt:DynDNS>"
			"<tt:Extension>"
				"<tt:Dot11Configuration>%s</tt:Dot11Configuration>"
			"</tt:Extension>"
		"</tt:Network>",
		p_cap->IPFilter ? "true" : "false",
		p_cap->ZeroConfiguration ? "true" : "false",
		p_cap->IPVersion6 ? "true" : "false",
		p_cap->DynDNS ? "true" : "false",
		p_cap->Dot11Configuration ? "true" : "false");

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:System>"
			"<tt:DiscoveryResolve>%s</tt:DiscoveryResolve>"
			"<tt:DiscoveryBye>%s</tt:DiscoveryBye>"
			"<tt:RemoteDiscovery>%s</tt:RemoteDiscovery>"
			"<tt:SystemBackup>%s</tt:SystemBackup>"
			"<tt:SystemLogging>%s</tt:SystemLogging>"
			"<tt:FirmwareUpgrade>%s</tt:FirmwareUpgrade>",
		p_cap->DiscoveryResolve ? "true" : "false",
		p_cap->DiscoveryBye ? "true" : "false",
		p_cap->RemoteDiscovery ? "true" : "false",
		p_cap->SystemBackup ? "true" : "false",
		p_cap->SystemLogging ? "true" : "false",
		p_cap->FirmwareUpgrade ? "true" : "false");

	for (i = 0; i < p_cap->sizeSupportedVersions; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:SupportedVersions>"
				"<tt:Major>%d</tt:Major>"
				"<tt:Minor>%d</tt:Minor>"
			"</tt:SupportedVersions>",
			p_cap->SupportedVersions[i].Major,
			p_cap->SupportedVersions[i].Minor);
	}

	offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:Extension>"
				"<tt:HttpFirmwareUpgrade>%s</tt:HttpFirmwareUpgrade>"
				"<tt:HttpSystemBackup>%s</tt:HttpSystemBackup>"
				"<tt:HttpSystemLogging>%s</tt:HttpSystemLogging>"
				"<tt:HttpSupportInformation>%s</tt:HttpSupportInformation>"
			"</tt:Extension>"
		"</tt:System>",
		p_cap->HttpFirmwareUpgrade ? "true" : "false",
		p_cap->HttpSystemBackup ? "true" : "false",
		p_cap->HttpSystemLogging ? "true" : "false",
		p_cap->HttpSupportInformation ? "true" : "false");

#ifdef DEVICEIO_SUPPORT
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:IO>");
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:InputConnectors>%d</tt:InputConnectors>"
		"<tt:RelayOutputs>%d</tt:RelayOutputs>",
		p_cap->InputConnectors,
		p_cap->RelayOutputs);
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Extension>");
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Auxiliary>%s</tt:Auxiliary>", 
		p_cap->Auxiliary ? "true" : "false");

	for  (i = 0; i < p_cap->sizeAuxiliaryCommands; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:AuxiliaryCommands>%s</tt:AuxiliaryCommands>", p_cap->AuxiliaryCommands[i]);
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tt:Extension>");
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:IO>");
#endif

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Security>"
			"<tt:TLS1.1>%s</tt:TLS1.1>"
			"<tt:TLS1.2>%s</tt:TLS1.2>"
			"<tt:OnboardKeyGeneration>%s</tt:OnboardKeyGeneration>"
			"<tt:AccessPolicyConfig>%s</tt:AccessPolicyConfig>"
			"<tt:X.509Token>%s</tt:X.509Token>"
			"<tt:SAMLToken>%s</tt:SAMLToken>"
			"<tt:KerberosToken>%s</tt:KerberosToken>"
			"<tt:RELToken>%s</tt:RELToken>"
			"<tt:Extension>"
				"<tt:TLS1.0>%s</tt:TLS1.0>"
				"<tt:Extension>"
					"<tt:Dot1X>%s</tt:Dot1X>"
					"<tt:SupportedEAPMethod>%d</tt:SupportedEAPMethod>"
					"<tt:RemoteUserHandling>%s</tt:RemoteUserHandling>"
				"</tt:Extension>"
			"</tt:Extension>"	
		"</tt:Security>",
		p_cap->TLS11 ? "true" : "false",
		p_cap->TLS12 ? "true" : "false",
		p_cap->OnboardKeyGeneration ? "true" : "false",
		p_cap->AccessPolicyConfig ? "true" : "false",
		p_cap->X509Token ? "true" : "false",
		p_cap->SAMLToken ? "true" : "false",
		p_cap->KerberosToken ? "true" : "false",
		p_cap->RELToken ? "true" : "false",
		p_cap->TLS10 ? "true" : "false",
		p_cap->Dot1X ? "true" : "false",
		p_cap->SupportedEAPMethods,
		p_cap->RemoteUserHandling ? "true" : "false");
		
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:Device>");

	return offset;
}

int build_DeviceServicesCapabilities_xml(char * p_buf, int mlen)
{
	//uint32 i;
	int offset = 0;
	onvif_DevicesCapabilities * p_cap = &g_onvif_cfg.Capabilities.device;

	offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:Network IPFilter=\"%s\" "
			"ZeroConfiguration=\"%s\" "
			"IPVersion6=\"%s\" "
			"DynDNS=\"%s\" "
			"Dot11Configuration=\"%s\" "
			"Dot1XConfigurations=\"%d\" "
			"HostnameFromDHCP=\"%s\" "
			"NTP=\"%d\" "
			"DHCPv6=\"%s\">"
		"</tds:Network>",
		p_cap->IPFilter ? "true" : "false",
		p_cap->ZeroConfiguration ? "true" : "false",
		p_cap->IPVersion6 ? "true" : "false",
		p_cap->DynDNS ? "true" : "false",
		p_cap->Dot11Configuration ? "true" : "false",
		p_cap->Dot1XConfigurations,
		p_cap->HostnameFromDHCP ? "true" : "false",
		p_cap->NTP,
		p_cap->DHCPv6 ? "true" : "false");
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:Security TLS1.0=\"%s\" "
			"TLS1.1=\"%s\" "
			"TLS1.2=\"%s\" "
			"OnboardKeyGeneration=\"%s\" "
			"AccessPolicyConfig=\"%s\" "
			"DefaultAccessPolicy=\"%s\" "
			"Dot1X=\"%s\" "
			"RemoteUserHandling=\"%s\" "
			"X.509Token=\"%s\" "
			"SAMLToken=\"%s\" "
			"KerberosToken=\"%s\" "
			"UsernameToken=\"%s\" "
			"HttpDigest=\"%s\" "
			"RELToken=\"%s\" "
			//"JsonWebToken=\"%s\" "
			//"SupportedEAPMethods=\"%d\" "
			//"MaxUsers=\"%d\" "
			//"MaxUserNameLength=\"%d\" "
			//"MaxPasswordLength=\"%d\" "
			//"HashingAlgorithms=\"%s\" "
			,
		p_cap->TLS10 ? "true" : "false",
		p_cap->TLS11 ? "true" : "false",
		p_cap->TLS12 ? "true" : "false",
		p_cap->OnboardKeyGeneration ? "true" : "false",
		p_cap->AccessPolicyConfig ? "true" : "false",
		p_cap->DefaultAccessPolicy ? "true" : "false",
		p_cap->Dot1X ? "true" : "false",
		p_cap->RemoteUserHandling ? "true" : "false",
		p_cap->X509Token ? "true" : "false",
		p_cap->SAMLToken ? "true" : "false",
		p_cap->KerberosToken ? "true" : "false",
		p_cap->UsernameToken ? "true" : "false",
		p_cap->HttpDigest ? "true" : "false",
		p_cap->RELToken ? "true" : "false"
		//,
		//p_cap->JsonWebToken ? "true" : "false",
		//p_cap->SupportedEAPMethods,
		//p_cap->MaxUsers,
		//p_cap->MaxUserNameLength,
		//p_cap->MaxPasswordLength,
		//p_cap->HashingAlgorithms
		);

	if (p_cap->SecurityPolicies[0] != '\0')
	{
		offset += snprintf(p_buf+offset, mlen-offset, "SecurityPolicies=\"%s\" ", p_cap->SecurityPolicies);
	}

	offset += snprintf(p_buf+offset, mlen-offset, "></tds:Security>");
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:System DiscoveryResolve=\"%s\" "
			"DiscoveryBye=\"%s\" "
			"RemoteDiscovery=\"%s\" "
			"SystemBackup=\"%s\" "
			"SystemLogging=\"%s\" "
			"FirmwareUpgrade=\"%s\" "
			"HttpFirmwareUpgrade=\"%s\" "
			"HttpSystemBackup=\"%s\" "
			"HttpSystemLogging=\"%s\" "
			"HttpSupportInformation=\"%s\">"// "
			//"StorageConfiguration=\"%s\" "
			//"MaxStorageConfigurations=\"%d\" "
			//"GeoLocationEntries=\"%d\" "
			//"AutoGeo=\"%s\" "
			//"StorageTypesSupported=\"%s\" "
			//"DiscoveryNotSupported=\"%s\" "
			//"NetworkConfigNotSupported=\"%s\" "
			//"UserConfigNotSupported=\"%s\" "
			//"Addons=\"%s\">"
		"</tds:System>",
		p_cap->DiscoveryResolve ? "true" : "false",
		p_cap->DiscoveryBye ? "true" : "false",
		p_cap->RemoteDiscovery ? "true" : "false",
		p_cap->SystemBackup ? "true" : "false",
		p_cap->SystemLogging ? "true" : "false",
		p_cap->FirmwareUpgrade ? "true" : "false",
		p_cap->HttpFirmwareUpgrade ? "true" : "false",
		p_cap->HttpSystemBackup ? "true" : "false",
		p_cap->HttpSystemLogging ? "true" : "false",
		p_cap->HttpSupportInformation ? "true" : "false"
		//,
		//p_cap->StorageConfiguration ? "true" : "false",
		//p_cap->MaxStorageConfigurations,
		//p_cap->GeoLocationEntries,
		//p_cap->AutoGeo,
		//p_cap->StorageTypesSupported,
		//p_cap->DiscoveryNotSupported ? "true" : "false",
		//p_cap->NetworkConfigNotSupported ? "true" : "false",
		//p_cap->UserConfigNotSupported ? "true" : "false",
		//p_cap->Addons
		);

	//offset += snprintf(p_buf+offset, mlen-offset, "<tds:Misc AuxiliaryCommands=\"");
	
	//for (i = 0; i < p_cap->sizeAuxiliaryCommands; i++)
	//{
	//	offset += snprintf(p_buf+offset, mlen-offset, "%s ", p_cap->AuxiliaryCommands[i]);
	//}
	
	//offset += snprintf(p_buf+offset, mlen-offset, "\"></tds:Misc>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");

	return offset;
}

int build_EventsCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen)
{
	int offset = 0;
	char saddr[256] = {'\0'};
	onvif_EventCapabilities * p_cap = &g_onvif_cfg.Capabilities.events;
	 
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Events>"
			"<tt:XAddr>%s</tt:XAddr>"
			"<tt:WSSubscriptionPolicySupport>%s</tt:WSSubscriptionPolicySupport>"
			"<tt:WSPullPointSupport>%s</tt:WSPullPointSupport>"
			"<tt:WSPausableSubscriptionManagerInterfaceSupport>%s</tt:WSPausableSubscriptionManagerInterfaceSupport>"
		"</tt:Events>",
		onvif_get_service_addr_by_user(CapabilityCategory_Events, p_user, saddr, sizeof(saddr)-1),
		p_cap->WSSubscriptionPolicySupport ? "true" : "false",
		p_cap->WSPullPointSupport ? "true" : "false",
		p_cap->WSPausableSubscriptionManagerInterfaceSupport ? "true" : "false");

	return offset;
}

int build_EventsServicesCapabilities_xml(char * p_buf, int mlen)
{
	int offset = 0;
	onvif_EventCapabilities * p_cap = &g_onvif_cfg.Capabilities.events;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tev:Capabilities "
			"WSSubscriptionPolicySupport=\"%s\" "
			"WSPullPointSupport=\"%s\" "
			"WSPausableSubscriptionManagerInterfaceSupport=\"%s\" "
			"MaxNotificationProducers=\"%d\" "
			"MaxPullPoints=\"%d\" "
			"PersistentNotificationStorage=\"%s\" ",
		p_cap->WSSubscriptionPolicySupport ? "true" : "false",
		p_cap->WSPullPointSupport ? "true" : "false",
		p_cap->WSPausableSubscriptionManagerInterfaceSupport ? "true" : "false",
		p_cap->MaxNotificationProducers,
		p_cap->MaxPullPoints,
		p_cap->PersistentNotificationStorage ? "true" : "false");

	if (p_cap->EventBrokerProtocols[0] != '\0')
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"EventBrokerProtocols=\"%s\" "
			"MaxEventBrokers=\"%d\" ",
			p_cap->EventBrokerProtocols,
			p_cap->MaxEventBrokers);
	}

	offset += snprintf(p_buf+offset, mlen-offset, "MetadataOverMQTT=\"%s\">", p_cap->MetadataOverMQTT ? "true" : "false");

	offset += snprintf(p_buf+offset, mlen-offset, "</tev:Capabilities>");

	return offset;
}


#ifdef MEDIA_SUPPORT

int build_MediaCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen)
{
	int offset = 0;
	char saddr[256] = {'\0'};
	onvif_MediaCapabilities * p_cap = &g_onvif_cfg.Capabilities.media;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Media>"
			"<tt:XAddr>%s</tt:XAddr>"
			"<tt:StreamingCapabilities>"
				"<tt:RTPMulticast>%s</tt:RTPMulticast>"
				"<tt:RTP_TCP>%s</tt:RTP_TCP>"
				"<tt:RTP_RTSP_TCP>%s</tt:RTP_RTSP_TCP>"
			"</tt:StreamingCapabilities>"
		"</tt:Media>",
		onvif_get_service_addr_by_user(CapabilityCategory_Media, p_user, saddr, sizeof(saddr)-1),
		p_cap->RTPMulticast ? "true" : "false",
		p_cap->RTP_TCP ? "true" : "false",
		p_cap->RTP_RTSP_TCP ? "true" : "false");

	return offset;
}

int build_MediaServicesCapabilities_xml(char * p_buf, int mlen)
{
	int offset = 0;
	onvif_MediaCapabilities * p_cap = &g_onvif_cfg.Capabilities.media;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<trt:Capabilities "
			"SnapshotUri=\"%s\" "
			"Rotation=\"%s\" "
			"VideoSourceMode=\"%s\" "
			"OSD=\"%s\" "
			"TemporaryOSDText=\"%s\" "
			"EXICompression=\"%s\">",
		p_cap->SnapshotUri ? "true" : "false",
		p_cap->Rotation ? "true" : "false",
		p_cap->VideoSourceMode ? "true" : "false",
		p_cap->OSD ? "true" : "false",
		p_cap->TemporaryOSDText ? "true" : "false",
		p_cap->EXICompression ? "true" : "false");
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<trt:ProfileCapabilities MaximumNumberOfProfiles=\"%d\" />"
		"<trt:StreamingCapabilities "
			"RTPMulticast=\"%s\" "
			"RTP_TCP=\"%s\" "
			"RTP_RTSP_TCP=\"%s\" "
			"NonAggregateControl=\"%s\" "
			"NoRTSPStreaming=\"%s\" />",
		p_cap->MaximumNumberOfProfiles,
		p_cap->RTPMulticast ? "true" : "false",
		p_cap->RTP_TCP ? "true" : "false",
		p_cap->RTP_RTSP_TCP ? "true" : "false",
		p_cap->NonAggregateControl ? "true" : "false",
		p_cap->NoRTSPStreaming ? "true" : "false");
			
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:Capabilities>");

	return offset;
}

#endif // MEDIA_SUPPORT

#ifdef IMAGE_SUPPORT

int build_ImagingCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen)
{
	int offset = 0;
	char saddr[256] = {'\0'};
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Imaging>"
			"<tt:XAddr>%s</tt:XAddr>"
		"</tt:Imaging>",		    
		onvif_get_service_addr_by_user(CapabilityCategory_Imaging, p_user, saddr, sizeof(saddr)-1));

	return offset;
}

int build_ImagingServicesCapabilities_xml(char * p_buf, int mlen)
{
	int offset = 0;
	onvif_ImagingCapabilities * p_cap = &g_onvif_cfg.Capabilities.image;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<timg:Capabilities "
			"ImageStabilization=\"%s\" "
			"Presets=\"%s\" "
			"AdaptablePreset=\"%s\" />",
		p_cap->ImageStabilization ? "true" : "false", 
		p_cap->Presets ? "true" : "false", 
		p_cap->AdaptablePreset ? "true" : "false");

	return offset;
}

#endif // IMAGE_SUPPORT

#ifdef PTZ_SUPPORT

int build_PTZCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen)
{
	int offset = 0;
	char saddr[256] = {'\0'};
	 
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:PTZ>"
			"<tt:XAddr>%s</tt:XAddr>"
		"</tt:PTZ>",
		onvif_get_service_addr_by_user(CapabilityCategory_PTZ, p_user, saddr, sizeof(saddr)-1));

	return offset;
}

int build_PTZServicesCapabilities_xml(char * p_buf, int mlen)
{
	int offset = 0;
	onvif_PTZCapabilities * p_cap = &g_onvif_cfg.Capabilities.ptz;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tptz:Capabilities "
			"EFlip=\"%s\" "
			"Reverse=\"%s\" "
			"GetCompatibleConfigurations=\"%s\" "
			"MoveStatus=\"%s\" "
			"StatusPosition=\"%s\" ",
		p_cap->EFlip ? "true" : "false",
		p_cap->Reverse ? "true" : "false",
		p_cap->GetCompatibleConfigurations ? "true" : "false",
		p_cap->MoveStatus ? "true" : "false",
		p_cap->StatusPosition ? "true" : "false");

	if (p_cap->MoveAndTrack[0] != '\0')
	{
		offset += snprintf(p_buf+offset, mlen-offset, "MoveAndTrack=\"%s\" ", p_cap->MoveAndTrack);
	}

	offset += snprintf(p_buf+offset, mlen-offset, "/>");

	return offset;
}

#endif

#ifdef VIDEO_ANALYTICS

int build_AnalyticsCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen)
{
	int offset = 0;
	char saddr[256] = {'\0'};
	onvif_AnalyticsCapabilities * p_cap = &g_onvif_cfg.Capabilities.analytics;
	 
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Analytics>"
			"<tt:XAddr>%s</tt:XAddr>"
			"<tt:RuleSupport>%s</tt:RuleSupport>"
			"<tt:AnalyticsModuleSupport>%s</tt:AnalyticsModuleSupport>"
		"</tt:Analytics>",
		onvif_get_service_addr_by_user(CapabilityCategory_Analytics, p_user, saddr, sizeof(saddr)-1),
		p_cap->RuleSupport ? "true" : "false",
		p_cap->AnalyticsModuleSupport ? "true" : "false");

	return offset;
}

int build_AnalyticsServicesCapabilities_xml(char * p_buf, int mlen)
{
	int offset = 0;
	onvif_AnalyticsCapabilities * p_cap = &g_onvif_cfg.Capabilities.analytics;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tan:Capabilities "
			"RuleSupport=\"%s\" "
			"AnalyticsModuleSupport=\"%s\" "
			"CellBasedSceneDescriptionSupported=\"%s\" "
			"RuleOptionsSupported=\"%s\" "
			"AnalyticsModuleOptionsSupported=\"%s\" "
			"SupportedMetadata=\"%s\" "
			"ImageSendingType=\"%s\" />",
		p_cap->RuleSupport ? "true" : "false",
		p_cap->AnalyticsModuleSupport ? "true" : "false",
		p_cap->CellBasedSceneDescriptionSupported ? "true" : "false",
		p_cap->RuleOptionsSupported ? "true" : "false",
		p_cap->AnalyticsModuleOptionsSupported ? "true" : "false",
		p_cap->SupportedMetadata ? "true" : "false",
		p_cap->ImageSendingType);

	return offset;
}

#endif

#ifdef PROFILE_G_SUPPORT

int build_RecordingCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen)
{
	int offset = 0;
	char saddr[256] = {'\0'};
	onvif_RecordingCapabilities * p_cap = &g_onvif_cfg.Capabilities.recording;
	 
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Recording>"
			"<tt:XAddr>%s</tt:XAddr>"
			"<tt:ReceiverSource>%s</tt:ReceiverSource>"
			"<tt:MediaProfileSource>%s</tt:MediaProfileSource>"
			"<tt:DynamicRecordings>%s</tt:DynamicRecordings>"
			"<tt:DynamicTracks>%s</tt:DynamicTracks>"
			"<tt:MaxStringLength>%d</tt:MaxStringLength>"
		"</tt:Recording>",
		onvif_get_service_addr_by_user(CapabilityCategory_Recording, p_user, saddr, sizeof(saddr)-1),
		p_cap->ReceiverSource ? "true" : "false",
		p_cap->MediaProfileSource ? "true" : "false",
		p_cap->DynamicRecordings ? "true" : "false", 
		p_cap->DynamicTracks ? "true" : "false",
		p_cap->MaxStringLength);

	return offset;    	
}

int build_RecordingServicesCapabilities_xml(char * p_buf, int mlen)
{
	int offset = 0;
	char Encoding[100];
	onvif_RecordingCapabilities * p_cap = &g_onvif_cfg.Capabilities.recording;

	memset(Encoding, 0, sizeof(Encoding));
	
	if (p_cap->JPEG)
	{
		strcat(Encoding, "JPEG ");
	}
	if (p_cap->MPEG4)
	{
		strcat(Encoding, "MPEG4 ");
	}
	if (p_cap->H264)
	{
		strcat(Encoding, "H264 ");
	}
	if (p_cap->H265)
	{
		strcat(Encoding, "H265 ");
	}
	if (p_cap->G711)
	{
		strcat(Encoding, "G711 ");
	}
	if (p_cap->G726)
	{
		strcat(Encoding, "G726 ");
	}
	if (p_cap->AAC)
	{
		strcat(Encoding, "AAC ");
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<trc:Capabilities "
			"DynamicRecordings=\"%s\" "
			"DynamicTracks=\"%s\" "
			"Encoding=\"%s\" "
			"MaxRate=\"%0.1f\" "
			"MaxTotalRate=\"%0.1f\" "
			"MaxRecordings=\"%d\" "
			"MaxRecordingJobs=\"%d\" "
			"Options=\"%s\" "
			"MetadataRecording=\"%s\" "
			"SupportedExportFileFormats=\"%s\" "
			"EventRecording=\"%s\" "
			"BeforeEventLimit=\"%d\" "
			"AfterEventLimit=\"%d\" ",
		p_cap->DynamicRecordings ? "true" : "false",
		p_cap->DynamicTracks ? "true" : "false",
		Encoding,
		p_cap->MaxRate,
		p_cap->MaxTotalRate,
		p_cap->MaxRecordings,
		p_cap->MaxRecordingJobs,
		p_cap->Options ? "true" : "false",
		p_cap->MetadataRecording ? "true" : "false",
		p_cap->SupportedExportFileFormats,
		p_cap->EventRecording ? "true" : "false",
		p_cap->BeforeEventLimit,
		p_cap->AfterEventLimit);

	if (p_cap->SupportedTargetFormatsFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "SupportedTargetFormats=\"%s\" ", p_cap->SupportedTargetFormats);
	}

	if (p_cap->EncryptionEntryLimitFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "EncryptionEntryLimit=\"%d\" ", p_cap->EncryptionEntryLimit);
	}

	if (p_cap->SupportedEncryptionModesFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "SupportedEncryptionModes=\"%s\" ", p_cap->SupportedEncryptionModes);
	}

	offset += snprintf(p_buf+offset, mlen-offset, "/>");

	return offset;
}

int build_SearchCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen)
{
	int offset = 0;
	char saddr[256] = {'\0'};
	onvif_SearchCapabilities * p_cap = &g_onvif_cfg.Capabilities.search;
     
    offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Search>"
			"<tt:XAddr>%s</tt:XAddr>"
			"<tt:MetadataSearch>%s</tt:MetadataSearch>"
		"</tt:Search>",		    
    	onvif_get_service_addr_by_user(CapabilityCategory_Search, p_user, saddr, sizeof(saddr)-1),
    	p_cap->MetadataSearch ? "true" : "false");

	return offset;    	
}

int build_SearchServicesCapabilities_xml(char * p_buf, int mlen)
{
	int offset = 0;
	onvif_SearchCapabilities * p_cap = &g_onvif_cfg.Capabilities.search;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tse:Capabilities "
			"MetadataSearch=\"%s\" "
			"GeneralStartEvents=\"%s\" />",
		p_cap->MetadataSearch ? "true" : "false",
		p_cap->GeneralStartEvents ? "true" : "false");

	return offset;
}

int build_ReplayCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen)
{
	int offset = 0;
	char saddr[256] = {'\0'};
     
    offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Replay>"
			"<tt:XAddr>%s</tt:XAddr>"
		"</tt:Replay>",		    
    	onvif_get_service_addr_by_user(CapabilityCategory_Replay, p_user, saddr, sizeof(saddr)-1));

	return offset;
}

int build_ReplayServicesCapabilities_xml(char * p_buf, int mlen)
{
	int offset = 0;
	onvif_ReplayCapabilities * p_cap = &g_onvif_cfg.Capabilities.replay;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<trp:Capabilities "
			"ReversePlayback=\"%s\" "
			"SessionTimeoutRange=\"%0.1f %0.1f\" "
			"RTP_RTSP_TCP=\"%s\" "
			"RTSPWebSocketUri=\"%s\" />",
		p_cap->ReversePlayback ? "true" : "false",
		p_cap->SessionTimeoutRange.Min,
		p_cap->SessionTimeoutRange.Max,
		p_cap->RTP_RTSP_TCP ? "true" : "false",
		p_cap->RTSPWebSocketUri);

	return offset;
}

#endif // end of PROFILE_G_SUPPORT

#ifdef DEVICEIO_SUPPORT

int build_DeviceIOCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen)
{
	int offset = 0;
	char saddr[256] = {'\0'};
	onvif_DeviceIOCapabilities * p_cap = &g_onvif_cfg.Capabilities.deviceIO;
	 
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:DeviceIO>"
			"<tt:XAddr>%s</tt:XAddr>"
			"<tt:VideoSources>%d</tt:VideoSources>"
			"<tt:VideoOutputs>%d</tt:VideoOutputs>"
			"<tt:AudioSources>%d</tt:AudioSources>"
			"<tt:AudioOutputs>%d</tt:AudioOutputs>"
			"<tt:RelayOutputs>%d</tt:RelayOutputs>"
		"</tt:DeviceIO>", 
		onvif_get_service_addr_by_user(CapabilityCategory_DeviceIO, p_user, saddr, sizeof(saddr)-1),
		p_cap->VideoSources,
		p_cap->VideoOutputs,
		p_cap->AudioSources,
		p_cap->AudioOutputs,
		p_cap->RelayOutputs);

	return offset;
}

int build_DeviceIOServicesCapabilities_xml(char * p_buf, int mlen)
{
	int offset = 0;
	onvif_DeviceIOCapabilities * p_cap = &g_onvif_cfg.Capabilities.deviceIO;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tmd:Capabilities "
			"VideoSources=\"%d\" "
			"VideoOutputs=\"%d\" "
			"AudioSources=\"%d\" "
			"AudioOutputs=\"%d\" "
			"RelayOutputs=\"%d\" "
			"SerialPorts=\"%d\" "
			"DigitalInputs=\"%d\" "
			"DigitalInputOptions=\"%s\" />",
		p_cap->VideoSources,
		p_cap->VideoOutputs,
		p_cap->AudioSources,
		p_cap->AudioOutputs,
		p_cap->RelayOutputs,
		p_cap->SerialPorts,
		p_cap->DigitalInputs,
		p_cap->DigitalInputOptions ? "true" : "false");

	return offset;
}

#endif // end of DEVICEIO_SUPPORT

int build_ExtensionCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen)
{
	int i;
	int offset = 0;
	char saddr[256] = {'\0'};
	if(g_hasIO > 0)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<hikxsd:hikCapabilities>"
				"<hikxsd:XAddr>%s</hikxsd:XAddr>"
				"<hikxsd:IOInputSupport>true</hikxsd:IOInputSupport>"
			"</hikxsd:hikCapabilities>",
			onvif_get_service_addr_by_user(CapabilityCategory_Hikxsd, p_user, saddr, sizeof(saddr)-1));
	}
	
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Extensions>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:TelexCapabilities>"
						"<tt:XAddr>%s</tt:XAddr>"
						"<tt:TimeOSDSupport>true</tt:TimeOSDSupport>"
						"<tt:TitleOSDSupport>true</tt:TitleOSDSupport>"
						"<tt:PTZ3DZoomSupport>true</tt:PTZ3DZoomSupport>"
						"<tt:PTZAuxSwitchSupport>true</tt:PTZAuxSwitchSupport>"
						"<tt:MotionDetectorSupport>true</tt:MotionDetectorSupport>"
						"<tt:TamperDetectorSupport>true</tt:TamperDetectorSupport>"
						"</tt:TelexCapabilities>",
		onvif_get_service_addr_by_user(CapabilityCategory_Telecom, p_user, saddr, sizeof(saddr)-1));
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:Extensions>");

	RESOLUTION_ENTRY *pEntry = NULL;
	int nrescount = anj_sysmng_video_res_array_get(&pEntry);
	char H265_profile = NOT_EXIST;
	for( i = 0; i < nrescount; i++)
	{
		if( strncmp(pEntry[i].codec_name, "H265", strlen("H265")) == 0 )
			H265_profile = EXIST;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<ewsd:hbCapabilities>"
			"<exsd:XAddr>%s</exsd:XAddr>",
		onvif_get_service_addr_by_user(CapabilityCategory_Hbgk, p_user, saddr, sizeof(saddr)-1));
	if (H265_profile)
		offset += snprintf(p_buf+offset, mlen-offset, "<exsd:H265Support>true</exsd:H265Support>");
	else
		offset += snprintf(p_buf+offset, mlen-offset, "<exsd:H265Support>false</exsd:H265Support>");
	
	offset += snprintf(p_buf+offset, mlen-offset, 	"<exsd:PrivacyMaskSupport>true</exsd:PrivacyMaskSupport>"
													"<exsd:CameraNum>1</exsd:CameraNum>"
													"<exsd:MaxMaskAreaNum>4</exsd:MaxMaskAreaNum>"
													"</ewsd:hbCapabilities>");

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tplt:Plus>"
			"<tplt:XAddr>%s</tplt:XAddr>",
		onvif_get_service_addr_by_user(CapabilityCategory_Hbgk, p_user, saddr, sizeof(saddr)-1));
	if (H265_profile)
		offset += snprintf(p_buf+offset, mlen-offset, "<tplt:H265>true</tplt:H265>");
	else
		offset += snprintf(p_buf+offset, mlen-offset, "<tplt:H265>false</tplt:H265>");
	
	offset += snprintf(p_buf+offset, mlen-offset,	"<tplt:PrivacyMask>true</tplt:PrivacyMask>"
													"<tplt:CameraNum>1</tplt:CameraNum>"
													"<tplt:MaxMaskAreaNum>4</tplt:MaxMaskAreaNum>"
													"</tplt:Plus>");
	
	return offset;
}

#ifdef RECEIVER_SUPPORT

int build_ReceiverCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen)
{
	int offset = 0;
	char saddr[256] = {'\0'};
	onvif_ReceiverCapabilities * p_cap = &g_onvif_cfg.Capabilities.receiver;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Receiver>"
			"<tt:XAddr>%s</tt:XAddr>"
			"<tt:RTP_Multicast>%s</tt:RTP_Multicast>"
			"<tt:RTP_TCP>%s</tt:RTP_TCP>"
			"<tt:RTP_RTSP_TCP>%s</tt:RTP_RTSP_TCP>"
			"<tt:SupportedReceivers>%d</tt:SupportedReceivers>"
			"<tt:MaximumRTSPURILength>%d</tt:MaximumRTSPURILength>"
		"</tt:Receiver>", 
		onvif_get_service_addr_by_user(CapabilityCategory_Receiver, p_user, saddr, sizeof(saddr)-1),
		p_cap->RTP_USCOREMulticast ? "true" : "false",
		p_cap->RTP_USCORETCP ? "true" : "false",
		p_cap->RTP_USCORERTSP_USCORETCP ? "true" : "false",
		p_cap->SupportedReceivers,
		p_cap->MaximumRTSPURILength);

	return offset;
}

int build_ReceiverServicesCapabilities_xml(char * p_buf, int mlen)
{
	int offset = 0;
	onvif_ReceiverCapabilities * p_cap = &g_onvif_cfg.Capabilities.receiver;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<trv:Capabilities "
			"RTP_Multicast=\"%s\" "
			"RTP_TCP=\"%s\" "
			"RTP_RTSP_TCP=\"%s\" "
			"SupportedReceivers=\"%d\" "
			"MaximumRTSPURILength=\"%d\" />",
		p_cap->RTP_USCOREMulticast ? "true" : "false",
		p_cap->RTP_USCORETCP ? "true" : "false",
		p_cap->RTP_USCORERTSP_USCORETCP ? "true" : "false",
		p_cap->SupportedReceivers,
		p_cap->MaximumRTSPURILength);

	return offset;
}

#endif // end of RECEIVER_SUPPORT

#ifdef MEDIA2_SUPPORT

int build_Media2ServicesCapabilities_xml(char * p_buf, int mlen)
{
	int offset = 0;
	onvif_MediaCapabilities2 * p_cap = &g_onvif_cfg.Capabilities.media2;
	log_print(HT_LOG_INFO, "build_Media2ServicesCapabilities_xml\n");
	log_print(HT_LOG_INFO, "p_cap->OSD:%d\n", p_cap->OSD);
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tr2:Capabilities "
			"SnapshotUri=\"%s\" "
			"Rotation=\"%s\" "
			"VideoSourceMode=\"%s\" "
			"OSD=\"%s\" "
			"TemporaryOSDText=\"%s\" "
			"Mask=\"%s\" "
			"SourceMask=\"%s\">",
		p_cap->SnapshotUri ? "true" : "false",
		p_cap->Rotation ? "true" : "false",
		p_cap->VideoSourceMode ? "true" : "false",
		p_cap->OSD ? "true" : "false",
		p_cap->TemporaryOSDText ? "true" : "false",
		p_cap->Mask ? "true" : "false",
		p_cap->SourceMask ? "true" : "false");
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tr2:ProfileCapabilities "
			"MaximumNumberOfProfiles=\"%d\" "
			"ConfigurationsSupported=\"%s\" />"
		"<tr2:StreamingCapabilities "
			"RTSPStreaming=\"%s\" "
			"RTPMulticast=\"%s\" "
			"RTP_RTSP_TCP=\"%s\" "
			"NonAggregateControl=\"%s\" ", 
		p_cap->ProfileCapabilities.MaximumNumberOfProfiles,
		p_cap->ProfileCapabilities.ConfigurationsSupported,
		p_cap->StreamingCapabilities.RTSPStreaming ? "true" : "false",
		p_cap->StreamingCapabilities.RTPMulticast ? "true" : "false",
		p_cap->StreamingCapabilities.RTP_RTSP_TCP ? "true" : "false",
		p_cap->StreamingCapabilities.NonAggregateControl ? "true" : "false");

	if (p_cap->StreamingCapabilities.RTSPWebSocketUri[0] != '\0')
	{
		offset += snprintf(p_buf+offset, mlen-offset, "RTSPWebSocketUri=\"%s\" ", p_cap->StreamingCapabilities.RTSPWebSocketUri);
	}

	offset += snprintf(p_buf+offset, mlen-offset, 
		"AutoStartMulticast=\"%s\" "
		"SecureRTSPStreaming=\"%s\" />", 
		p_cap->StreamingCapabilities.AutoStartMulticast ? "true" : "false",
		p_cap->StreamingCapabilities.SecureRTSPStreaming ? "true" : "false");

	offset += snprintf(p_buf+offset, mlen-offset, "</tr2:Capabilities>");

	return offset;
}

#endif

#ifdef PROFILE_C_SUPPORT

int build_AccessControlServicesCapabilities_xml(char * p_buf, int mlen)
{
	int offset = 0;
	onvif_AccessControlCapabilities * p_cap = &g_onvif_cfg.Capabilities.accesscontrol;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tac:Capabilities "
			"MaxLimit=\"%d\" "
			"MaxAccessPoints=\"%d\" "
			"MaxAreas=\"%d\" "
			"ClientSuppliedTokenSupported=\"%s\" "
			"AccessPointManagementSupported=\"%s\" "
			"AreaManagementSupported=\"%s\" />",
		p_cap->MaxLimit,
		p_cap->MaxAccessPoints,
		p_cap->MaxAreas,
		p_cap->ClientSuppliedTokenSupported ? "true" : "false",
		p_cap->AccessPointManagementSupported ? "true" : "false",
		p_cap->AreaManagementSupported ? "true" : "false");

	return offset;
}

int build_DoorControlServicesCapabilities_xml(char * p_buf, int mlen)
{
	int offset = 0;
	onvif_DoorControlCapabilities * p_cap = &g_onvif_cfg.Capabilities.doorcontrol;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tdc:Capabilities "
			"MaxLimit=\"%d\" "
			"MaxDoors=\"%d\" "
			"ClientSuppliedTokenSupported=\"%s\" "
			"DoorManagementSupported=\"%s\" />",
		p_cap->MaxLimit,
		p_cap->MaxDoors,
		p_cap->ClientSuppliedTokenSupported ? "true" : "false",
		p_cap->DoorManagementSupported ? "true" : "false");

	return offset;
}

#endif // PROFILE_C_SUPPORT

#ifdef THERMAL_SUPPORT

int build_ThermalServicesCapabilities_xml(char * p_buf, int mlen)
{
	int offset = 0;
	onvif_ThermalCapabilities * p_cap = &g_onvif_cfg.Capabilities.thermal;
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tth:Capabilities Radiometry=\"%s\" />", p_cap->Radiometry ? "true" : "false");

	return offset;
}

#endif // THERMAL_SUPPORT

#ifdef CREDENTIAL_SUPPORT

int build_CredentialServicesCapabilities_xml(char * p_buf, int mlen)
{
	uint32 i;
	int offset = 0;
	onvif_CredentialCapabilities * p_cap = &g_onvif_cfg.Capabilities.credential;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tcr:Capabilities "
			"MaxLimit=\"%d\" "
			"CredentialValiditySupported=\"%s\" "
			"CredentialAccessProfileValiditySupported=\"%s\" "
			"ValiditySupportsTimeValue=\"%s\" "
			"MaxCredentials=\"%d\" "
			"MaxAccessProfilesPerCredential=\"%d\" "
			"ResetAntipassbackSupported=\"%s\" "
			"ClientSuppliedTokenSupported=\"%s\" "
			"DefaultCredentialSuspensionDuration=\"%s\" "
			"MaxWhitelistedItems=\"%d\" "
			"MaxBlacklistedItems=\"%d\">",
		p_cap->MaxLimit,
		p_cap->CredentialValiditySupported ? "true" : "false",
		p_cap->CredentialAccessProfileValiditySupported ? "true" : "false",
		p_cap->ValiditySupportsTimeValue ? "true" : "false",
		p_cap->MaxCredentials,
		p_cap->MaxAccessProfilesPerCredential,
		p_cap->ResetAntipassbackSupported ? "true" : "false",
		p_cap->ClientSuppliedTokenSupported ? "true" : "false",
		p_cap->DefaultCredentialSuspensionDuration,
		p_cap->MaxWhitelistedItems,
		p_cap->MaxBlacklistedItems);

	for (i = 0; i < p_cap->sizeSupportedIdentifierType; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tcr:SupportedIdentifierType>%s</tcr:SupportedIdentifierType>", p_cap->SupportedIdentifierType[i]);
	}

	if (p_cap->ExtensionFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tcr:Extension>");

		for (i = 0; i < p_cap->Extension.sizeSupportedExemptionType; i++)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tcr:SupportedExemptionType>%s</tcr:SupportedExemptionType>", p_cap->Extension.SupportedExemptionType[i]);
		}

		offset += snprintf(p_buf+offset, mlen-offset, "</tcr:Extension>");
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tcr:Capabilities>");

	return offset;
}

#endif // end of CREDENTIAL_SUPPORT

#ifdef ACCESS_RULES

int build_AccessRulesServicesCapabilities_xml(char * p_buf, int mlen)
{
	int offset = 0;
	onvif_AccessRulesCapabilities * p_cap = &g_onvif_cfg.Capabilities.accessrules;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tar:Capabilities "
			"MaxLimit=\"%d\" "
			"MaxAccessProfiles=\"%d\" "
			"MaxAccessPoliciesPerAccessProfile=\"%d\" "
			"MultipleSchedulesPerAccessPointSupported=\"%s\" "
			"ClientSuppliedTokenSupported=\"%s\" />",
		p_cap->MaxLimit,
		p_cap->MaxAccessProfiles,
		p_cap->MaxAccessPoliciesPerAccessProfile,
		p_cap->MultipleSchedulesPerAccessPointSupported ? "true" : "false",
		p_cap->ClientSuppliedTokenSupported ? "true" : "false");

	return offset;
}

#endif // end of ACCESS_RULES

#ifdef SCHEDULE_SUPPORT

int build_ScheduleServicesCapabilities_xml(char * p_buf, int mlen)
{
	int offset = 0;
	onvif_ScheduleCapabilities * p_cap = &g_onvif_cfg.Capabilities.schedule;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tsc:Capabilities "
			"MaxLimit=\"%d\" "
			"MaxSchedules=\"%d\" "
			"MaxTimePeriodsPerDay=\"%d\" "
			"MaxSpecialDayGroups=\"%d\" "
			"MaxDaysInSpecialDayGroup=\"%d\" "
			"MaxSpecialDaysSchedules=\"%d\" "
			"ExtendedRecurrenceSupported=\"%s\" "
			"SpecialDaysSupported=\"%s\" "
			"StateReportingSupported=\"%s\" "
			"ClientSuppliedTokenSupported=\"%s\" />",
		p_cap->MaxLimit,
		p_cap->MaxSchedules,
		p_cap->MaxTimePeriodsPerDay,
		p_cap->MaxSpecialDayGroups,
		p_cap->MaxDaysInSpecialDayGroup,
		p_cap->MaxSpecialDaysSchedules,
		p_cap->ExtendedRecurrenceSupported ? "true" : "false",
		p_cap->SpecialDaysSupported ? "true" : "false",
		p_cap->StateReportingSupported ? "true" : "false",
		p_cap->ClientSuppliedTokenSupported ? "true" : "false");

	return offset;
}

#endif // end of SCHEDULE_SUPPORT

#ifdef PROVISIONING_SUPPORT

int build_SourceCapabilities_xml(char * p_buf, int mlen, onvif_SourceCapabilities * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tpv:Source VideoSourceToken=\"%s\" ", p_res->VideoSourceToken);

	if (p_res->MaximumPanMovesFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, " MaximumPanMoves=\"%d\"", p_res->MaximumPanMoves);
	}

	if (p_res->MaximumTiltMovesFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, " MaximumTiltMoves=\"%d\"", p_res->MaximumTiltMoves);
	}

	if (p_res->MaximumZoomMovesFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, " MaximumZoomMoves=\"%d\"", p_res->MaximumZoomMoves);
	}

	if (p_res->MaximumRollMovesFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, " MaximumRollMoves=\"%d\"", p_res->MaximumRollMoves);
	}

	if (p_res->AutoLevelFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, " AutoLevel=\"%s\"", p_res->AutoLevel ? "true" : "false");
	}

	if (p_res->MaximumFocusMovesFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, " MaximumFocusMoves=\"%d\"", p_res->MaximumFocusMoves);
	}

	if (p_res->AutoFocusFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, " AutoFocus=\"%s\"", p_res->AutoFocus ? "true" : "false");
	}

	offset += snprintf(p_buf+offset, mlen-offset, "></tpv:Source>");

	return offset;
}

int build_ProvisioningServicesCapabilities_xml(char * p_buf, int mlen)
{
	uint32 i;
	int offset = 0;
	onvif_ProvisioningCapabilities * p_cap = &g_onvif_cfg.Capabilities.provisioning;

	offset += snprintf(p_buf+offset, mlen-offset, "<tpv:Capabilities>");

	offset += snprintf(p_buf+offset, mlen-offset, "<tpv:DefaultTimeout>PT%dS</tpv:DefaultTimeout>", p_cap->DefaultTimeout);

	for (i = 0; i < p_cap->sizeSource; i++)
	{
		offset += build_SourceCapabilities_xml(p_buf+offset, mlen-offset, &p_cap->Source[i]);
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tpv:Capabilities>");

	return offset;
}

int build_PlusServicesCapabilities_xml(char * p_buf, int mlen)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:Capabilities  VideoSources=\"1\" VideoOutputs=\"0\" AudioSources=\"1\" AudioOutputs=\"1\" RelayOutputs=\"1\" DigitalInputs=\"1\" SerialPorts=\"0\" DigitalInputOptions=\"true\" />");

	return offset;
}

#endif // PROVISIONING_SUPPORT

int build_Version_xml(char * p_buf, int mlen, onvif_Version * p_res)
{
	int offset = 0;
	if (g_onvif_expand)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:Version>"
			"<tt:Major>%d</tt:Major>"
			"<tt:Minor>%d</tt:Minor>"
		"</tds:Version>",
		1, 
		11);
	}
	else
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:Version>"
			"<tt:Major>%d</tt:Major>"
			"<tt:Minor>%d</tt:Minor>"
		"</tds:Version>",
		p_res->Major, 
		p_res->Minor);
	}
	

	return offset;
}

int build_FloatRange_xml(char * p_buf, int mlen, onvif_FloatRange * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Min>%0.2f</tt:Min>"
		"<tt:Max>%0.2f</tt:Max>",
		p_res->Min, p_res->Max);

	return offset;
}

int build_IntList_xml(char * p_buf, int mlen, onvif_IntList * p_res)
{
	uint32 i;
	int offset = 0;

	for (i = 0; i < p_res->sizeItems; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Items>%d</tt:Items>\r\n", p_res->Items[i]);
	}

	return offset;
}

int build_FloatList_xml(char * p_buf, int mlen, onvif_FloatList * p_res)
{
    int i, offset = 0;

    for (i = 0; i < p_res->sizeItems; i++)
    {
        offset += snprintf(p_buf+offset, mlen-offset,
            "<tt:Items>%0.2f</tt:Items>\r\n", 
            p_res->Items[i]);
    }       

    return offset;
}

int build_VideoResolution_xml(char * p_buf, int mlen, onvif_VideoResolution * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:ResolutionsAvailable>\r\n"
			"<tt:Width>%d</tt:Width>\r\n"
			"<tt:Height>%d</tt:Height>\r\n"
		"</tt:ResolutionsAvailable>\r\n",
		p_res->Width, 
		p_res->Height);

	return offset;
}

int build_GetServiceCapabilities_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	onvif_CapabilityCategory category = *(onvif_CapabilityCategory *)argv;

	if (CapabilityCategory_Device == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetServiceCapabilitiesResponse>");
		offset += build_DeviceServicesCapabilities_xml(p_buf+offset, mlen-offset);
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetServiceCapabilitiesResponse>");
	}
#ifdef MEDIA2_SUPPORT
	else if (CapabilityCategory_Media2 == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tr2:GetServiceCapabilitiesResponse>");
		offset += build_Media2ServicesCapabilities_xml(p_buf+offset, mlen-offset);
		offset += snprintf(p_buf+offset, mlen-offset, "</tr2:GetServiceCapabilitiesResponse>");
	}
#endif
#ifdef MEDIA_SUPPORT
	else if (CapabilityCategory_Media == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetServiceCapabilitiesResponse>");
		offset += build_MediaServicesCapabilities_xml(p_buf+offset, mlen-offset);
		offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetServiceCapabilitiesResponse>");
	}
#endif	
	else if (CapabilityCategory_Events == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tev:GetServiceCapabilitiesResponse>");
		offset += build_EventsServicesCapabilities_xml(p_buf+offset, mlen-offset);
		offset += snprintf(p_buf+offset, mlen-offset, "</tev:GetServiceCapabilitiesResponse>");
	}
#ifdef PTZ_SUPPORT	
	else if (CapabilityCategory_PTZ == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GetServiceCapabilitiesResponse>");
		offset += build_PTZServicesCapabilities_xml(p_buf+offset, mlen-offset);
		offset += snprintf(p_buf+offset, mlen-offset, "</tptz:GetServiceCapabilitiesResponse>");
	}
#endif
#ifdef IMAGE_SUPPORT
	else if (CapabilityCategory_Imaging == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<timg:GetServiceCapabilitiesResponse>");
		offset += build_ImagingServicesCapabilities_xml(p_buf+offset, mlen-offset);
		offset += snprintf(p_buf+offset, mlen-offset, "</timg:GetServiceCapabilitiesResponse>");
	}
#endif	
#ifdef VIDEO_ANALYTICS	
	else if (CapabilityCategory_Analytics == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tan:GetServiceCapabilitiesResponse>");
		offset += build_AnalyticsServicesCapabilities_xml(p_buf+offset, mlen-offset);
		offset += snprintf(p_buf+offset, mlen-offset, "</tan:GetServiceCapabilitiesResponse>");
	}	
#endif	
#ifdef PROFILE_G_SUPPORT
	else if (CapabilityCategory_Recording == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trc:GetServiceCapabilitiesResponse>");
		offset += build_RecordingServicesCapabilities_xml(p_buf+offset, mlen-offset);
		offset += snprintf(p_buf+offset, mlen-offset, "</trc:GetServiceCapabilitiesResponse>");
	}
	else if (CapabilityCategory_Search == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tse:GetServiceCapabilitiesResponse>");
		offset += build_SearchServicesCapabilities_xml(p_buf+offset, mlen-offset);
		offset += snprintf(p_buf+offset, mlen-offset, "</tse:GetServiceCapabilitiesResponse>");
	}
	else if (CapabilityCategory_Replay == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trp:GetServiceCapabilitiesResponse>");
		offset += build_ReplayServicesCapabilities_xml(p_buf+offset, mlen-offset);
		offset += snprintf(p_buf+offset, mlen-offset, "</trp:GetServiceCapabilitiesResponse>");
	}
#endif
#ifdef PROFILE_C_SUPPORT
	else if (CapabilityCategory_AccessControl == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tac:GetServiceCapabilitiesResponse>");
		offset += build_AccessControlServicesCapabilities_xml(p_buf+offset, mlen-offset);
		offset += snprintf(p_buf+offset, mlen-offset, "</tac:GetServiceCapabilitiesResponse>");
	}
	else if (CapabilityCategory_DoorControl == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tdc:GetServiceCapabilitiesResponse>");
		offset += build_DoorControlServicesCapabilities_xml(p_buf+offset, mlen-offset);
		offset += snprintf(p_buf+offset, mlen-offset, "</tdc:GetServiceCapabilitiesResponse>");
	}
#endif
#ifdef DEVICEIO_SUPPORT
	else if (CapabilityCategory_DeviceIO == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tmd:GetServiceCapabilitiesResponse>");
		offset += build_DeviceIOServicesCapabilities_xml(p_buf+offset, mlen-offset);
		offset += snprintf(p_buf+offset, mlen-offset, "</tmd:GetServiceCapabilitiesResponse>");
	}
#endif
#ifdef THERMAL_SUPPORT
	else if (CapabilityCategory_Thermal == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tth:GetServiceCapabilitiesResponse>");
		offset += build_ThermalServicesCapabilities_xml(p_buf+offset, mlen-offset);
		offset += snprintf(p_buf+offset, mlen-offset, "</tth:GetServiceCapabilitiesResponse>");
	}
#endif
#ifdef CREDENTIAL_SUPPORT
	else if (CapabilityCategory_Credential == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tcr:GetServiceCapabilitiesResponse>");
		offset += build_CredentialServicesCapabilities_xml(p_buf+offset, mlen-offset);
		offset += snprintf(p_buf+offset, mlen-offset, "</tcr:GetServiceCapabilitiesResponse>");
	}
#endif
#ifdef ACCESS_RULES
	else if (CapabilityCategory_AccessRules == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tar:GetServiceCapabilitiesResponse>");
		offset += build_AccessRulesServicesCapabilities_xml(p_buf+offset, mlen-offset);
		offset += snprintf(p_buf+offset, mlen-offset, "</tar:GetServiceCapabilitiesResponse>");
	}
#endif
#ifdef SCHEDULE_SUPPORT
	else if (CapabilityCategory_Schedule == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tsc:GetServiceCapabilitiesResponse>");
		offset += build_ScheduleServicesCapabilities_xml(p_buf+offset, mlen-offset);
		offset += snprintf(p_buf+offset, mlen-offset, "</tsc:GetServiceCapabilitiesResponse>");
	}
#endif
#ifdef RECEIVER_SUPPORT
	else if (CapabilityCategory_Receiver == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trv:GetServiceCapabilitiesResponse>");
		offset += build_ReceiverServicesCapabilities_xml(p_buf+offset, mlen-offset);
		offset += snprintf(p_buf+offset, mlen-offset, "</trv:GetServiceCapabilitiesResponse>");
	}
#endif
#ifdef PROVISIONING_SUPPORT
	else if (CapabilityCategory_Provisioning == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tpv:GetServiceCapabilitiesResponse>");
		offset += build_ProvisioningServicesCapabilities_xml(p_buf+offset, mlen-offset);
		offset += snprintf(p_buf+offset, mlen-offset, "</tpv:GetServiceCapabilitiesResponse>");
	}
#endif

#if 1
		else if (CapabilityCategory_Plus == category)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tpl:GetServiceCapabilitiesResponse>");
			offset += build_tpl_ServicesCapabilities_xml(p_buf+offset, mlen-offset);
			offset += snprintf(p_buf+offset, mlen-offset, "</tpl:GetServiceCapabilitiesResponse>");
		}
#endif

	return offset;
}

/***************************************************************************************/

int build_NetworkInterface_xml(char * p_buf, int mlen, onvif_NetworkInterface * p_res)
{
#ifdef DOT11_SUPPORT
	uint32 i;
#endif
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Enabled>%s</tt:Enabled>", p_res->Enabled ? "true" : "false");

	if (p_res->InfoFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Info>");

		if (p_res->Info.NameFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Name>%s</tt:Name>", p_res->Info.Name);
		}
		
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:HwAddress>%s</tt:HwAddress>", p_res->Info.HwAddress);

		if (p_res->Info.MTUFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:MTU>%d</tt:MTU>", p_res->Info.MTU);
		}
		
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Info>");
	}

	if (p_res->IPv4Flag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:IPv4>");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Enabled>%s</tt:Enabled>", p_res->IPv4.Enabled ? "true" : "false");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Config>");

		if (p_res->IPv4.Config.DHCP == FALSE)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:Manual>"
					"<tt:Address>%s</tt:Address>"
					"<tt:PrefixLength>%d</tt:PrefixLength>"
				"</tt:Manual>",
				p_res->IPv4.Config.Address, 
				p_res->IPv4.Config.PrefixLength);
		}
		else
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:FromDHCP>"
					"<tt:Address>%s</tt:Address>"
					"<tt:PrefixLength>%d</tt:PrefixLength>"
				"</tt:FromDHCP>",
				p_res->IPv4.Config.Address, 
				p_res->IPv4.Config.PrefixLength);
		}
		
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:DHCP>%s</tt:DHCP>", p_res->IPv4.Config.DHCP ? "true" : "false");

		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Config>");
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:IPv4>");
	}

	if (p_res->ExtensionFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Extension>");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:InterfaceType>%d</tt:InterfaceType>", p_res->Extension.InterfaceType);

#ifdef DOT11_SUPPORT
		for (i = 0; i < p_res->Extension.sizeDot11; i++)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Dot11>");
			offset += build_Dot11Configuration_xml(p_buf+offset, mlen-offset, &p_res->Extension.Dot11[i]);
			offset += snprintf(p_buf+offset, mlen-offset, "</tt:Dot11>");
		}
#endif

		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Extension>");
	}

	return offset;
}

int build_DeviceInformation_xml(char * p_buf, int mlen, onvif_DeviceInformation * p_res)
{
	int offset = 0;
	if (g_isHxVersion)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tds:Manufacturer>%s</tds:Manufacturer>\r\n"
			"<tds:Model>%s</tds:Model>\r\n"
			"<tds:FirmwareVersion>%s</tds:FirmwareVersion>\r\n"
			"<tds:SerialNumber>%s</tds:SerialNumber>\r\n"
			"<tds:HardwareId>%s</tds:HardwareId>\r\n",
			p_res->Manufacturer, 
			p_res->Model, 
			p_res->FirmwareVersion, 
			p_res->SerialNumber, 
			"88");
	}
	else
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tds:Manufacturer>%s</tds:Manufacturer>\r\n"
			"<tds:Model>%s</tds:Model>\r\n"
			"<tds:FirmwareVersion>%s</tds:FirmwareVersion>\r\n"
			"<tds:SerialNumber>%s</tds:SerialNumber>\r\n"
			"<tds:HardwareId>%s</tds:HardwareId>\r\n",
			p_res->Manufacturer, 
			p_res->Model, 
			p_res->FirmwareVersion, 
			p_res->SerialNumber, 
			p_res->HardwareId);
	}
	
	return offset;
}

int build_SystemDateTime_xml(char * p_buf, int mlen, onvif_SystemDateTime * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
						"<tt:DateTimeType>%s</tt:DateTimeType>\r\n"
						"<tt:DaylightSavings>%s</tt:DaylightSavings>\r\n",
						onvif_SetDateTimeTypeToString(p_res->DateTimeType), 
						p_res->DaylightSavings ? "true" : "false");

	if (p_res->TimeZoneFlag && p_res->TimeZone.TZ[0] != '\0')
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
							"<tt:TimeZone>\r\n"
								"<tt:TZ>%s</tt:TZ>\r\n"
							"</tt:TimeZone>\r\n", 
							p_res->TimeZone.TZ);
	}

	return offset;
}

int build_NetworkProtocol_xml(char * p_buf, int mlen, onvif_NetworkProtocol * p_res)
{
	uint32 i;
	int offset = 0;

	if (p_res->HTTPFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tds:NetworkProtocols>\r\n"
				"<tt:Name>HTTP</tt:Name>\r\n"
				"<tt:Enabled>%s</tt:Enabled>\r\n", 
			p_res->HTTPEnabled ? "true" : "false");

		for (i = 0; i < ARRAY_SIZE(p_res->HTTPPort); i++)
		{
			if (p_res->HTTPPort[i] == 0)
			{
				continue;
			}
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Port>%d</tt:Port>\r\n", p_res->HTTPPort[i]);
		}
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:NetworkProtocols>\r\n");
	}

	if (p_res->HTTPSFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tds:NetworkProtocols>\r\n"
				"<tt:Name>HTTPS</tt:Name>\r\n"
				"<tt:Enabled>%s</tt:Enabled>\r\n", 
			p_res->HTTPSEnabled ? "true" : "false");

		for (i = 0; i < ARRAY_SIZE(p_res->HTTPSPort); i++)
		{
			if (p_res->HTTPSPort[i] == 0)
			{
				continue;
			}
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Port>%d</tt:Port>\r\n", p_res->HTTPSPort[i]);
		}
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:NetworkProtocols>\r\n");
	}

	if (p_res->RTSPFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tds:NetworkProtocols>\r\n"
				"<tt:Name>RTSP</tt:Name>\r\n"
				"<tt:Enabled>%s</tt:Enabled>\r\n", 
			p_res->RTSPEnabled ? "true" : "false");

		for (i = 0; i < ARRAY_SIZE(p_res->RTSPPort); i++)
		{
			if (p_res->RTSPPort[i] == 0)
			{
				continue;
			}
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Port>%d</tt:Port>\r\n", p_res->RTSPPort[i]);
		}
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:NetworkProtocols>\r\n");
	}

	return offset;
}

int build_DNSInformation_xml(char * p_buf, int mlen, onvif_DNSInformation * p_res)
{
	int offset = 0;
	uint32 i;

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:FromDHCP>%s</tt:FromDHCP>\r\n", p_res->FromDHCP ? "true" : "false");

	if (p_res->SearchDomainFlag)
	{
		for (i = 0; i < ARRAY_SIZE(p_res->SearchDomain); i++)
		{
			if (p_res->SearchDomain[i][0] == '\0')
			{
				continue;
			}
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:SearchDomain>%s</tt:SearchDomain>\r\n", p_res->SearchDomain[i]);
		}
	}
	
	for (i = 0; i < ARRAY_SIZE(p_res->DNSServer); i++)
	{
		if (p_res->DNSServer[i][0] == '\0')
		{
			continue;
		}
		
		if (p_res->FromDHCP)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:DNSFromDHCP>\r\n");
		}
		else
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:DNSManual>\r\n");
		}

		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:Type>IPv4</tt:Type>\r\n"
			"<tt:IPv4Address>%s</tt:IPv4Address>\r\n",
			p_res->DNSServer[i]);

		if (p_res->FromDHCP)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "</tt:DNSFromDHCP>\r\n");
		}
		else
		{
			offset += snprintf(p_buf+offset, mlen-offset, "</tt:DNSManual>\r\n");
		}
	}

	return offset;
}

int build_NTPInformation_xml(char * p_buf, int mlen, onvif_NTPInformation * p_res)
{
	int offset = 0;
	uint32 i;

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:FromDHCP>%s</tt:FromDHCP>\r\n", p_res->FromDHCP ? "true" : "false");

	for (i = 0; i < ARRAY_SIZE(p_res->NTPServer); i++)
	{
		if (p_res->NTPServer[i][0] == '\0')
		{
			continue;
		}
		
		if (p_res->FromDHCP)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:NTPFromDHCP>\r\n");
		}
		else
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:NTPManual>\r\n");
		}
		
		if (is_ip_address(p_res->NTPServer[i]))
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:Type>IPv4</tt:Type>\r\n"
				"<tt:IPv4Address>%s</tt:IPv4Address>\r\n", 
				p_res->NTPServer[i]);
		}
		else
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:Type>DNS</tt:Type>\r\n"
				"<tt:DNSname>%s</tt:DNSname>\r\n", 
				p_res->NTPServer[i]);
		}

		if (p_res->FromDHCP)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "</tt:NTPFromDHCP>\r\n");
		}
		else
		{
			offset += snprintf(p_buf+offset, mlen-offset, "</tt:NTPManual>\r\n");
		}
	}
	
	return offset;
}

int build_NetworkGateway_xml(char * p_buf, int mlen, onvif_NetworkGateway * p_res)
{
	uint32 i;
	int offset = 0;

	for (i = 0; i < ARRAY_SIZE(p_res->IPv4Address); i++)
	{
		if (p_res->IPv4Address[i][0] != '\0')
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:IPv4Address>%s</tt:IPv4Address>\r\n", p_res->IPv4Address[i]);
		}
	}
	return offset;
}

int build_DynamicDNSInformation_xml(char * p_buf, int mlen, onvif_DynamicDNSInformation * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Type>%s</tt:Type>\r\n", onvif_DynamicDNSTypeToString(p_res->Type));

	if (p_res->NameFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Name>%s</tt:Name>\r\n", p_res->Name);
	}

	if (p_res->TTLFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:TTL>%d</tt:TTL>\r\n", p_res->TTL);
	}

	return offset;
}

int build_ZeroConfiguration_xml(char * p_buf, int mlen, onvif_NetworkZeroConfiguration * p_res)
{
	uint32 i;
	int offset = 0;

	offset += snprintf(p_buf + offset, mlen - offset, 
		"<tt:InterfaceToken>%s</tt:InterfaceToken>\r\n"
		"<tt:Enabled>%s</tt:Enabled>\r\n",
		p_res->InterfaceToken,
		p_res->Enabled ? "true" : "false");

	for (i = 0; i < p_res->sizeAddresses; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Addresses>%s</tt:Addresses>\r\n", p_res->Addresses[i]);
	}

	return offset;
}

int build_Scope_xml(char * p_buf, int mlen, onvif_Scope * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:ScopeDef>%s</tt:ScopeDef>\r\n"
		"<tt:ScopeItem>%s</tt:ScopeItem>\r\n",
		onvif_ScopeDefinitionToString(p_res->ScopeDef), 
		p_res->ScopeItem);
				
    return offset;
}

int build_tds_GetDeviceInformation_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetDeviceInformationResponse>\r\n");
	offset += build_DeviceInformation_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.DeviceInformation);
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetDeviceInformationResponse>\r\n");
	
	return offset;
}

int build_tds_GetSystemUris_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tds_GetSystemUris_RES * p_res = (tds_GetSystemUris_RES *)argv;
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetSystemUrisResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SystemLogUris>");

	if (p_res->SystemLogUriFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset,
			"<tt:SystemLog>"
				"<tt:Type>System</tt:Type>"
				"<tt:Uri>%s</tt:Uri>"
			"</tt:SystemLog>", 
			p_res->SystemLogUri);
	}

	if (p_res->AccessLogUriFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset,
			"<tt:SystemLog>"
				"<tt:Type>Access</tt:Type>"
				"<tt:Uri>%s</tt:Uri>"
			"</tt:SystemLog>", 
			p_res->AccessLogUri);
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tds:SystemLogUris>");

	if (p_res->SupportInfoUriFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset,"<tds:SupportInfoUri>%s</tds:SupportInfoUri>", p_res->SupportInfoUri);
	}

	if (p_res->SystemBackupUriFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset,"<tds:SystemBackupUri>%s</tds:SystemBackupUri>", p_res->SystemBackupUri);
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetSystemUrisResponse>");
	
	return offset;
}

int build_tds_GetCapabilities_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tds_GetCapabilities_REQ * p_res = (tds_GetCapabilities_REQ *)argv;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:GetCapabilitiesResponse>"
			"<tds:Capabilities>");

	if (CapabilityCategory_Device == p_res->Category)
	{
		offset += build_DeviceCapabilities_xml(p_user, p_buf+offset, mlen-offset);
	}
	else if (CapabilityCategory_Events == p_res->Category)
	{
		offset += build_EventsCapabilities_xml(p_user, p_buf+offset, mlen-offset);
	}
#ifdef MEDIA_SUPPORT
	else if (CapabilityCategory_Media == p_res->Category)
	{
		offset += build_MediaCapabilities_xml(p_user, p_buf+offset, mlen-offset);
	}
#endif	
#ifdef IMAGE_SUPPORT	
	else if (CapabilityCategory_Imaging == p_res->Category)
	{
		offset += build_ImagingCapabilities_xml(p_user, p_buf+offset, mlen-offset);
	}
#endif
#ifdef PTZ_SUPPORT
	else if (CapabilityCategory_PTZ == p_res->Category)
	{
		offset += build_PTZCapabilities_xml(p_user, p_buf+offset, mlen-offset);
	}
#endif
#ifdef VIDEO_ANALYTICS
	else if (CapabilityCategory_Analytics == p_res->Category)
	{
		offset += build_AnalyticsCapabilities_xml(p_user, p_buf+offset, mlen-offset);
	}
#endif
	else if (CapabilityCategory_All == p_res->Category)
	{
#ifdef VIDEO_ANALYTICS
		offset += build_AnalyticsCapabilities_xml(p_user, p_buf+offset, mlen-offset);
#endif	
		offset += build_DeviceCapabilities_xml(p_user, p_buf+offset, mlen-offset);
		offset += build_EventsCapabilities_xml(p_user, p_buf+offset, mlen-offset);
#ifdef IMAGE_SUPPORT
		offset += build_ImagingCapabilities_xml(p_user, p_buf+offset, mlen-offset);
#endif
#ifdef MEDIA_SUPPORT
		offset += build_MediaCapabilities_xml(p_user, p_buf+offset, mlen-offset);
#endif
#ifdef PTZ_SUPPORT
		if (!g_openeye_smw)
		{
			offset += build_PTZCapabilities_xml(p_user, p_buf+offset, mlen-offset);
		}
#endif
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Extension>");
#ifdef DEVICEIO_SUPPORT
		if (g_onvif_cfg.Capabilities.deviceIO.support)
		{
			offset += build_DeviceIOCapabilities_xml(p_user, p_buf+offset, mlen-offset);
		}
#endif
#ifdef PROFILE_G_SUPPORT
		if (g_onvif_cfg.Capabilities.recording.support)
		{
			offset += build_RecordingCapabilities_xml(p_user, p_buf+offset, mlen-offset);
		}
		if (g_onvif_cfg.Capabilities.search.support)
		{
			offset += build_SearchCapabilities_xml(p_user, p_buf+offset, mlen-offset);
		}
		if (g_onvif_cfg.Capabilities.replay.support)
		{
			offset += build_ReplayCapabilities_xml(p_user, p_buf+offset, mlen-offset);
		}
#endif
#ifdef RECEIVER_SUPPORT
		if (g_onvif_cfg.Capabilities.receiver.support && !g_openeye_smw)
		{
			offset += build_ReceiverCapabilities_xml(p_user, p_buf+offset, mlen-offset);
		}
#endif
		if (!g_openeye_smw)
		{
			offset += build_ExtensionCapabilities_xml(p_user, p_buf+offset, mlen-offset);
		}
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Extension>");
	}

	offset += snprintf(p_buf+offset, mlen-offset, 
			"</tds:Capabilities>"
		"</tds:GetCapabilitiesResponse>");
	
	return offset;
}

int build_tds_GetNetworkInterfaces_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	NetworkInterfaceList * p_net_inf = g_onvif_cfg.network.interfaces;
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetNetworkInterfacesResponse>");
	
	while (p_net_inf)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tds:NetworkInterfaces token=\"%s\">", p_net_inf->NetworkInterface.token);
		offset += build_NetworkInterface_xml(p_buf+offset, mlen-offset, &p_net_inf->NetworkInterface);
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:NetworkInterfaces>");
		
		p_net_inf = p_net_inf->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetNetworkInterfacesResponse>");
	
	return offset;
}

int build_tds_SetNetworkInterfaces_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tds_SetNetworkInterfaces_RES * p_res = (tds_SetNetworkInterfaces_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:SetNetworkInterfacesResponse>"
			"<tds:RebootNeeded>%s</tds:RebootNeeded>"
		"</tds:SetNetworkInterfacesResponse>",
		p_res->RebootNeeded ? "true" : "false");
	
	return offset;
}

int build_tds_SystemReboot_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:SystemRebootResponse>"
			"<tds:Message>Rebooting</tds:Message>"
		"</tds:SystemRebootResponse>");
	
	return offset;
}

int build_tds_SetSystemFactoryDefault_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;	
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetSystemFactoryDefaultResponse />");	    
	return offset;
}

int build_tds_GetSystemLog_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tds_GetSystemLog_RES * p_res = (tds_GetSystemLog_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:GetSystemLogResponse>"
			"<tds:SystemLog xsi:nil=\"true\">"
				"<tt:String>%s</tt:String>"
			"</tds:SystemLog>"
		"</tds:GetSystemLogResponse>",
		p_res->String);
	
	return offset;
}

int build_tds_GetSystemDateAndTime_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	time_t nowtime;
	struct tm *gtime;

	time(&nowtime);
	gtime = gmtime(&nowtime);

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:GetSystemDateAndTimeResponse>\r\n"
			"<tds:SystemDateAndTime>\r\n");

	offset += build_SystemDateTime_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.SystemDateTime);
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:UTCDateTime>\r\n"
			"<tt:Time>\r\n"
				"<tt:Hour>%d</tt:Hour>\r\n"
				"<tt:Minute>%d</tt:Minute>\r\n"
				"<tt:Second>%d</tt:Second>\r\n"
			"</tt:Time>\r\n"
			"<tt:Date>\r\n"
				"<tt:Year>%d</tt:Year>\r\n"
				"<tt:Month>%d</tt:Month>\r\n"
				"<tt:Day>%d</tt:Day>\r\n"
			"</tt:Date>\r\n"
		"</tt:UTCDateTime>\r\n",
		gtime->tm_hour, 
		gtime->tm_min, 
		gtime->tm_sec, 
		gtime->tm_year + 1900, 
		gtime->tm_mon + 1, 
		gtime->tm_mday);

	offset += snprintf(p_buf+offset, mlen-offset, 
			"</tds:SystemDateAndTime>\r\n"
		"</tds:GetSystemDateAndTimeResponse>\r\n");
	
	return offset;
}


int build_tds_SetSystemDateAndTime_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetSystemDateAndTimeResponse />");
	return offset;
}

int build_tds_GetServices_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	uint32 lip = 0;
	char saddr[256] = {'\0'};
	HTTPSRV * p_srv = (HTTPSRV *) p_user->http_srv;
	tds_GetServices_REQ * p_res = (tds_GetServices_REQ *)argv;

	if (p_srv->saddr == 0)
	{
		struct sockaddr_in addr;
		socklen_t addrlen = sizeof(addr);
		getsockname(p_user->cfd, (struct sockaddr *)&addr, &addrlen);
		lip = addr.sin_addr.s_addr;
	}
	else
	{
		lip = p_srv->saddr;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetServicesResponse>");

	if (1)
	{
		// device manager
		if (1)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tds:Service>"
				"<tds:Namespace>http://www.onvif.org/ver10/device/wsdl</tds:Namespace>"
				"<tds:XAddr>%s</tds:XAddr>", 
				onvif_get_service_addr(CapabilityCategory_Device, lip, p_user->rip, saddr, sizeof(saddr)-1));
			if (p_res->IncludeCapability)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
				offset += build_DeviceServicesCapabilities_xml(p_buf+offset, mlen-offset);
				offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
			}
			offset += build_Version_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.Capabilities.device.Version);
			offset += snprintf(p_buf+offset, mlen-offset, "</tds:Service>");
		}
		
		// event service
		if (g_onvif_cfg.Capabilities.events.support)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tds:Service>"
				"<tds:Namespace>http://www.onvif.org/ver10/events/wsdl</tds:Namespace>"
				"<tds:XAddr>%s</tds:XAddr>", 
				onvif_get_service_addr(CapabilityCategory_Events, lip, p_user->rip, saddr, sizeof(saddr)-1));
			if (p_res->IncludeCapability)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
				offset += build_EventsServicesCapabilities_xml(p_buf+offset, mlen-offset);
				offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
			}
			offset += build_Version_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.Capabilities.events.Version);
			offset += snprintf(p_buf+offset, mlen-offset, "</tds:Service>");
		}

#ifdef MEDIA2_SUPPORT
		// media service 2
		if (g_onvif_cfg.Capabilities.media2.support)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tds:Service>"
				"<tds:Namespace>http://www.onvif.org/ver20/media/wsdl</tds:Namespace>"
				"<tds:XAddr>%s</tds:XAddr>", 
				onvif_get_service_addr(CapabilityCategory_Media2, lip, p_user->rip, saddr, sizeof(saddr)-1));
			if (p_res->IncludeCapability)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
				offset += build_Media2ServicesCapabilities_xml(p_buf+offset, mlen-offset);
				offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
			}
			offset += build_Version_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.Capabilities.media2.Version);
			offset += snprintf(p_buf+offset, mlen-offset, "</tds:Service>");
		}
#endif

#ifdef MEDIA_SUPPORT
		// media service 1
		if (g_onvif_cfg.Capabilities.media.support)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tds:Service>"
				"<tds:Namespace>http://www.onvif.org/ver10/media/wsdl</tds:Namespace>"
				"<tds:XAddr>%s</tds:XAddr>", 
				onvif_get_service_addr(CapabilityCategory_Media, lip, p_user->rip, saddr, sizeof(saddr)-1));
			if (p_res->IncludeCapability)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
				offset += build_MediaServicesCapabilities_xml(p_buf+offset, mlen-offset);
				offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
			}
			offset += build_Version_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.Capabilities.media.Version);
			offset += snprintf(p_buf+offset, mlen-offset, "</tds:Service>");
		}
#endif

#ifdef PTZ_SUPPORT
		// ptz
		if (g_onvif_cfg.Capabilities.ptz.support)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tds:Service>"
				"<tds:Namespace>http://www.onvif.org/ver20/ptz/wsdl</tds:Namespace>"
				"<tds:XAddr>%s</tds:XAddr>", 
				onvif_get_service_addr(CapabilityCategory_PTZ, lip, p_user->rip, saddr, sizeof(saddr)-1));
			if (p_res->IncludeCapability)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
				offset += build_PTZServicesCapabilities_xml(p_buf+offset, mlen-offset);
				offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
			}
			offset += build_Version_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.Capabilities.ptz.Version);
			offset += snprintf(p_buf+offset, mlen-offset, "</tds:Service>");
		}
#endif

#ifdef IMAGE_SUPPORT
		// image
		if (g_onvif_cfg.Capabilities.image.support)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tds:Service>"
				"<tds:Namespace>http://www.onvif.org/ver20/imaging/wsdl</tds:Namespace>"
				"<tds:XAddr>%s</tds:XAddr>", 
				onvif_get_service_addr(CapabilityCategory_Imaging, lip, p_user->rip, saddr, sizeof(saddr)-1));
			if (p_res->IncludeCapability)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
				offset += build_ImagingServicesCapabilities_xml(p_buf+offset, mlen-offset);
				offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
			}
			offset += build_Version_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.Capabilities.image.Version);
			offset += snprintf(p_buf+offset, mlen-offset, "</tds:Service>");
		}
#endif

#ifdef VIDEO_ANALYTICS
		// analytics
		if (g_onvif_cfg.Capabilities.analytics.support)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tds:Service>"
				"<tds:Namespace>http://www.onvif.org/ver20/analytics/wsdl</tds:Namespace>"
				"<tds:XAddr>%s</tds:XAddr>", 
				onvif_get_service_addr(CapabilityCategory_Analytics, lip, p_user->rip, saddr, sizeof(saddr)-1));
			if (p_res->IncludeCapability)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
				offset += build_AnalyticsServicesCapabilities_xml(p_buf+offset, mlen-offset);
				offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
			}
			offset += build_Version_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.Capabilities.analytics.Version);
			offset += snprintf(p_buf+offset, mlen-offset, "</tds:Service>");
		}
#endif

#ifdef PROFILE_G_SUPPORT
		// recording
		if (g_onvif_cfg.Capabilities.recording.support)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tds:Service>"
				"<tds:Namespace>http://www.onvif.org/ver10/recording/wsdl</tds:Namespace>"
				"<tds:XAddr>%s</tds:XAddr>", 
				onvif_get_service_addr(CapabilityCategory_Recording, lip, p_user->rip, saddr, sizeof(saddr)-1));
			if (p_res->IncludeCapability)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
				offset += build_RecordingServicesCapabilities_xml(p_buf+offset, mlen-offset);
				offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
			}
			offset += build_Version_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.Capabilities.recording.Version);
			offset += snprintf(p_buf+offset, mlen-offset, "</tds:Service>");
		}

		// search
		if (g_onvif_cfg.Capabilities.search.support)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tds:Service>"
				"<tds:Namespace>http://www.onvif.org/ver10/search/wsdl</tds:Namespace>"
				"<tds:XAddr>%s</tds:XAddr>", 
				onvif_get_service_addr(CapabilityCategory_Search, lip, p_user->rip, saddr, sizeof(saddr)-1));
			if (p_res->IncludeCapability)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
				offset += build_SearchServicesCapabilities_xml(p_buf+offset, mlen-offset);
				offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
			}
			offset += build_Version_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.Capabilities.search.Version);
			offset += snprintf(p_buf+offset, mlen-offset, "</tds:Service>");
		}

		// replay
		if (g_onvif_cfg.Capabilities.replay.support)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tds:Service>"
				"<tds:Namespace>http://www.onvif.org/ver10/replay/wsdl</tds:Namespace>"
				"<tds:XAddr>%s</tds:XAddr>", 
				onvif_get_service_addr(CapabilityCategory_Replay, lip, p_user->rip, saddr, sizeof(saddr)-1));
			if (p_res->IncludeCapability)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
				offset += build_ReplayServicesCapabilities_xml(p_buf+offset, mlen-offset);
				offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
			}
			offset += build_Version_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.Capabilities.replay.Version);
			offset += snprintf(p_buf+offset, mlen-offset, "</tds:Service>");
		}

#endif // end of PROFILE_G_SUPPORT

#ifdef PROFILE_C_SUPPORT
		// access control
		if (g_onvif_cfg.Capabilities.accesscontrol.support)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tds:Service>"
				"<tds:Namespace>http://www.onvif.org/ver10/accesscontrol/wsdl</tds:Namespace>"
				"<tds:XAddr>%s</tds:XAddr>", 
				onvif_get_service_addr(CapabilityCategory_AccessControl, lip, p_user->rip, saddr, sizeof(saddr)-1));
			if (p_res->IncludeCapability)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
				offset += build_AccessControlServicesCapabilities_xml(p_buf+offset, mlen-offset);
				offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
			}
			offset += build_Version_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.Capabilities.accesscontrol.Version);
			offset += snprintf(p_buf+offset, mlen-offset, "</tds:Service>");
		}

		// door control
		if (g_onvif_cfg.Capabilities.doorcontrol.support)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tds:Service>"
				"<tds:Namespace>http://www.onvif.org/ver10/doorcontrol/wsdl</tds:Namespace>"
				"<tds:XAddr>%s</tds:XAddr>", 
				onvif_get_service_addr(CapabilityCategory_DoorControl, lip, p_user->rip, saddr, sizeof(saddr)-1));
			if (p_res->IncludeCapability)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
				offset += build_DoorControlServicesCapabilities_xml(p_buf+offset, mlen-offset);
				offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
			}
			offset += build_Version_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.Capabilities.doorcontrol.Version);
			offset += snprintf(p_buf+offset, mlen-offset, "</tds:Service>");
		}

#endif // end of PROFILE_C_SUPPORT

#ifdef DEVICEIO_SUPPORT
		// deviceio
		if (g_onvif_cfg.Capabilities.deviceIO.support)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tds:Service>"
				"<tds:Namespace>http://www.onvif.org/ver10/deviceIO/wsdl</tds:Namespace>"
				"<tds:XAddr>%s</tds:XAddr>", 
				onvif_get_service_addr(CapabilityCategory_DeviceIO, lip, p_user->rip, saddr, sizeof(saddr)-1));
			if (p_res->IncludeCapability)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
				offset += build_DeviceIOServicesCapabilities_xml(p_buf+offset, mlen-offset);
				offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
			}
			offset += build_Version_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.Capabilities.deviceIO.Version);
			offset += snprintf(p_buf+offset, mlen-offset, "</tds:Service>");
		}
#endif // end of DEVICEIO_SUPPORT

#ifdef THERMAL_SUPPORT
		// thermal
		if (g_onvif_cfg.Capabilities.thermal.support)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tds:Service>"
				"<tds:Namespace>http://www.onvif.org/ver10/thermal/wsdl</tds:Namespace>"
				"<tds:XAddr>%s</tds:XAddr>", 
				onvif_get_service_addr(CapabilityCategory_Thermal, lip, p_user->rip, saddr, sizeof(saddr)-1));
			if (p_res->IncludeCapability)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
				offset += build_ThermalServicesCapabilities_xml(p_buf+offset, mlen-offset);
				offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
			}
			offset += build_Version_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.Capabilities.thermal.Version);
			offset += snprintf(p_buf+offset, mlen-offset, "</tds:Service>");
		}
#endif // end of THERMAL_SUPPORT

#ifdef CREDENTIAL_SUPPORT
		// credential
		if (g_onvif_cfg.Capabilities.credential.support)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tds:Service>"
				"<tds:Namespace>http://www.onvif.org/ver10/credential/wsdl</tds:Namespace>"
				"<tds:XAddr>%s</tds:XAddr>", 
				onvif_get_service_addr(CapabilityCategory_Credential, lip, p_user->rip, saddr, sizeof(saddr)-1));
			if (p_res->IncludeCapability)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
				offset += build_CredentialServicesCapabilities_xml(p_buf+offset, mlen-offset);
				offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
			}
			offset += build_Version_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.Capabilities.credential.Version);
			offset += snprintf(p_buf+offset, mlen-offset, "</tds:Service>");
		}
#endif // end of CREDENTIAL_SUPPORT

#ifdef ACCESS_RULES
		// access rules
		if (g_onvif_cfg.Capabilities.accessrules.support)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tds:Service>"
				"<tds:Namespace>http://www.onvif.org/ver10/accessrules/wsdl</tds:Namespace>"
				"<tds:XAddr>%s</tds:XAddr>", 
				onvif_get_service_addr(CapabilityCategory_AccessRules, lip, p_user->rip, saddr, sizeof(saddr)-1));
			if (p_res->IncludeCapability)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
				offset += build_AccessRulesServicesCapabilities_xml(p_buf+offset, mlen-offset);
				offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
			}
			offset += build_Version_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.Capabilities.accessrules.Version);
			offset += snprintf(p_buf+offset, mlen-offset, "</tds:Service>");
		}
#endif // end of ACCESS_RULES

#ifdef SCHEDULE_SUPPORT
		// schedule
		if (g_onvif_cfg.Capabilities.schedule.support)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tds:Service>"
				"<tds:Namespace>http://www.onvif.org/ver10/schedule/wsdl</tds:Namespace>"
				"<tds:XAddr>%s</tds:XAddr>", 
				onvif_get_service_addr(CapabilityCategory_Schedule, lip, p_user->rip, saddr, sizeof(saddr)-1));
			if (p_res->IncludeCapability)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
				offset += build_ScheduleServicesCapabilities_xml(p_buf+offset, mlen-offset);
				offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
			}
			offset += build_Version_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.Capabilities.schedule.Version);
			offset += snprintf(p_buf+offset, mlen-offset, "</tds:Service>");
		}
#endif // end of SCHEDULE_SUPPORT

#ifdef RECEIVER_SUPPORT
		// receiver
		if (g_onvif_cfg.Capabilities.receiver.support)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tds:Service>"
				"<tds:Namespace>http://www.onvif.org/ver10/receiver/wsdl</tds:Namespace>"
				"<tds:XAddr>%s</tds:XAddr>", 
				onvif_get_service_addr(CapabilityCategory_Receiver, lip, p_user->rip, saddr, sizeof(saddr)-1));
			if (p_res->IncludeCapability)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
				offset += build_ReceiverServicesCapabilities_xml(p_buf+offset, mlen-offset);
				offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
			}
			offset += build_Version_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.Capabilities.receiver.Version);
			offset += snprintf(p_buf+offset, mlen-offset, "</tds:Service>");
		}
#endif // end of RECEIVER_SUPPORT

#ifdef PROVISIONING_SUPPORT
		// provisioning
		if (g_onvif_cfg.Capabilities.provisioning.support)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tds:Service>"
				"<tds:Namespace>http://www.onvif.org/ver10/provisioning/wsdl</tds:Namespace>"
				"<tds:XAddr>%s</tds:XAddr>", 
				onvif_get_service_addr(CapabilityCategory_Provisioning, lip, p_user->rip, saddr, sizeof(saddr)-1));
			if (p_res->IncludeCapability)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
				offset += build_ProvisioningServicesCapabilities_xml(p_buf+offset, mlen-offset);
				offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
			}
			offset += build_Version_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.Capabilities.provisioning.Version);
			offset += snprintf(p_buf+offset, mlen-offset, "</tds:Service>");
		}

		if (g_onvif_expand)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tds:Service>"
				"<tds:Namespace>http://www.onvif.org/ver10/plus/wsdl</tds:Namespace>"
				"<tds:XAddr>%s</tds:XAddr>",
				onvif_get_service_addr(CapabilityCategory_Plus, lip, p_user->rip, saddr, sizeof(saddr)-1));
			if (p_res->IncludeCapability)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
				offset += build_PlusServicesCapabilities_xml(p_buf+offset, mlen-offset);
				offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
			}
			offset += build_Version_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.Capabilities.provisioning.Version);
			offset += snprintf(p_buf+offset, mlen-offset, "</tds:Service>");
		}
#endif // end of PROVISIONING_SUPPORT
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetServicesResponse>");

	return offset;
}

int build_tds_GetScopes_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	uint32 i;
	int offset = 0;
	onvif_Scope scope;
	memset(&scope, 0, sizeof(scope));

	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetScopesResponse>");

#ifdef AUDIO_SUPPORT
	if (!onvif_is_scope_exist("onvif://www.onvif.org/type/audio_encoder"))
	{
		scope.ScopeDef = ScopeDefinition_Fixed;
		strcpy(scope.ScopeItem, "onvif://www.onvif.org/type/audio_encoder");

		offset += snprintf(p_buf+offset, mlen-offset, "<tds:Scopes>");
		offset += build_Scope_xml(p_buf+offset, mlen-offset, &scope);
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:Scopes>");
	}
#endif

#if defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)
	if (!onvif_is_scope_exist("onvif://www.onvif.org/type/video_encoder"))
	{
		scope.ScopeDef = ScopeDefinition_Fixed;
		strcpy(scope.ScopeItem, "onvif://www.onvif.org/type/video_encoder");

		offset += snprintf(p_buf+offset, mlen-offset, "<tds:Scopes>");
		offset += build_Scope_xml(p_buf+offset, mlen-offset, &scope);
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:Scopes>");
	}
#endif

#ifdef PTZ_SUPPORT
	if (!onvif_is_scope_exist("onvif://www.onvif.org/type/ptz"))
	{
		scope.ScopeDef = ScopeDefinition_Fixed;
		strcpy(scope.ScopeItem, "onvif://www.onvif.org/type/ptz");

		offset += snprintf(p_buf+offset, mlen-offset, "<tds:Scopes>");
		offset += build_Scope_xml(p_buf+offset, mlen-offset, &scope);
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:Scopes>");
	}
#endif

#ifdef MEDIA_SUPPORT
	if (!onvif_is_scope_exist("onvif://www.onvif.org/Profile/Streaming"))
	{
		scope.ScopeDef = ScopeDefinition_Fixed;
		strcpy(scope.ScopeItem, "onvif://www.onvif.org/Profile/Streaming");

		offset += snprintf(p_buf+offset, mlen-offset, "<tds:Scopes>");
		offset += build_Scope_xml(p_buf+offset, mlen-offset, &scope);
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:Scopes>");
	}
#endif

#ifdef MEDIA2_SUPPORT
	if (0)//!onvif_is_scope_exist("onvif://www.onvif.org/Profile/T"))
	{
		scope.ScopeDef = ScopeDefinition_Fixed;
		strcpy(scope.ScopeItem, "onvif://www.onvif.org/Profile/T");

		offset += snprintf(p_buf+offset, mlen-offset, "<tds:Scopes>");
		offset += build_Scope_xml(p_buf+offset, mlen-offset, &scope);
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:Scopes>");
	}
#endif

#ifdef PROFILE_G_SUPPORT
	if (!onvif_is_scope_exist("onvif://www.onvif.org/Profile/G"))
	{
		scope.ScopeDef = ScopeDefinition_Fixed;
		strcpy(scope.ScopeItem, "onvif://www.onvif.org/Profile/G");

		offset += snprintf(p_buf+offset, mlen-offset, "<tds:Scopes>");
		offset += build_Scope_xml(p_buf+offset, mlen-offset, &scope);
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:Scopes>");
	}
#endif

#ifdef PROFILE_C_SUPPORT
	if (!onvif_is_scope_exist("onvif://www.onvif.org/Profile/C"))
	{
		scope.ScopeDef = ScopeDefinition_Fixed;
		strcpy(scope.ScopeItem, "onvif://www.onvif.org/Profile/C");

		offset += snprintf(p_buf+offset, mlen-offset, "<tds:Scopes>");
		offset += build_Scope_xml(p_buf+offset, mlen-offset, &scope);
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:Scopes>");
	}
#endif

#ifdef ACCESS_RULES
	if (0)//!onvif_is_scope_exist("onvif://www.onvif.org/Profile/A"))
	{
		scope.ScopeDef = ScopeDefinition_Fixed;
		strcpy(scope.ScopeItem, "onvif://www.onvif.org/Profile/A");

		offset += snprintf(p_buf+offset, mlen-offset, "<tds:Scopes>");
		offset += build_Scope_xml(p_buf+offset, mlen-offset, &scope);
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:Scopes>");
	}
#endif

#ifdef VIDEO_ANALYTICS
	if (0)//!onvif_is_scope_exist("onvif://www.onvif.org/Profile/M"))
	{
		scope.ScopeDef = ScopeDefinition_Fixed;
		strcpy(scope.ScopeItem, "onvif://www.onvif.org/Profile/M");

		offset += snprintf(p_buf+offset, mlen-offset, "<tds:Scopes>");
		offset += build_Scope_xml(p_buf+offset, mlen-offset, &scope);
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:Scopes>");
	}
#endif

#ifdef PROFILE_Q_SUPPORT
	strcpy(scope.ScopeItem, "onvif://www.onvif.org/Profile/Q/");

	if (g_onvif_cfg.device_state)
	{
		strcat(scope.ScopeItem, "Operational");
	}
	else
	{
		strcat(scope.ScopeItem, "FactoryDefault");
	}

	if (!onvif_is_scope_exist(scope.ScopeItem))
	{
		scope.ScopeDef = ScopeDefinition_Fixed;

		offset += snprintf(p_buf+offset, mlen-offset, "<tds:Scopes>");
		offset += build_Scope_xml(p_buf+offset, mlen-offset, &scope);
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:Scopes>");
	}
#endif

	for (i = 0; i < ARRAY_SIZE(g_onvif_cfg.scopes); i++)
	{
		if (g_onvif_cfg.scopes[i].ScopeItem[0] != '\0')
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tds:Scopes>");
			offset += build_Scope_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.scopes[i]);
			offset += snprintf(p_buf+offset, mlen-offset, "</tds:Scopes>");
		}
	}
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetScopesResponse>");

	return offset;
}

int build_tds_AddScopes_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:AddScopesResponse />");
	return offset;
}

int build_tds_SetScopes_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetScopesResponse />");
	return offset;
}

int build_tds_RemoveScopes_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	uint32 i;
	int offset = 0;
	tds_RemoveScopes_REQ * p_res = (tds_RemoveScopes_REQ *)argv;
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:RemoveScopesResponse>");

	for (i = 0; i < ARRAY_SIZE(p_res->ScopeItem); i++)
	{
		if (p_res->ScopeItem[i][0] == '\0')
		{
			break;
		}
		offset += snprintf(p_buf+offset, mlen-offset, "<tds:ScopeItem>%s</tds:ScopeItem>", p_res->ScopeItem[i]);
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:RemoveScopesResponse>");

	return offset;
}

int build_tds_GetHostname_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:GetHostnameResponse>"
			"<tds:HostnameInformation>"
				"<tt:FromDHCP>%s</tt:FromDHCP>"
				"<tt:Name>%s</tt:Name>"
			"</tds:HostnameInformation>"
		"</tds:GetHostnameResponse>",
		g_onvif_cfg.network.HostnameInformation.FromDHCP ? "true" : "false", 
		g_onvif_cfg.network.HostnameInformation.Name);

	return offset;
}

int build_tds_SetHostname_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetHostnameResponse />");
	return offset;
}

int build_tds_SetHostnameFromDHCP_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:SetHostnameFromDHCPResponse>"
			"<tds:RebootNeeded>%s</tds:RebootNeeded>"
		"</tds:SetHostnameFromDHCPResponse>",
		g_onvif_cfg.network.HostnameInformation.RebootNeeded ? "true" : "false");

	return offset;
}

int build_tds_GetNetworkProtocols_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetNetworkProtocolsResponse>\r\n");
	offset += build_NetworkProtocol_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.network.NetworkProtocol);
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetNetworkProtocolsResponse>\r\n");

	return offset;
}

int build_tds_SetNetworkProtocols_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetNetworkProtocolsResponse />");
	return offset;
}

int build_tds_GetNetworkDefaultGateway_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetNetworkDefaultGatewayResponse>\r\n");
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:NetworkGateway>\r\n");
	offset += build_NetworkGateway_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.network.NetworkGateway);
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:NetworkGateway>\r\n");
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetNetworkDefaultGatewayResponse>\r\n");

	return offset;
}

int build_tds_SetNetworkDefaultGateway_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;	
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetNetworkDefaultGatewayResponse />");
	return offset;
}

int build_tds_GetDiscoveryMode_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:GetDiscoveryModeResponse>"
			"<tds:DiscoveryMode>%s</tds:DiscoveryMode>"
		"</tds:GetDiscoveryModeResponse>", 
		onvif_DiscoveryModeToString(g_onvif_cfg.network.DiscoveryMode));

	return offset;
}

int build_tds_SetDiscoveryMode_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetDiscoveryModeResponse />");	
	return offset;
}

int build_tds_GetDNS_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetDNSResponse>\r\n");
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:DNSInformation>\r\n");
	offset += build_DNSInformation_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.network.DNSInformation);
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:DNSInformation>\r\n");
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetDNSResponse>\r\n");

	return offset;
}

int build_tds_SetDNS_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetDNSResponse />");	
	return offset;
}

int build_tds_GetDynamicDNS_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetDynamicDNSResponse>\r\n");
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:DynamicDNSInformation>\r\n");
	offset += build_DynamicDNSInformation_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.network.DynamicDNSInformation);
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:DynamicDNSInformation>\r\n");
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetDynamicDNSResponse>\r\n");
	
	return offset;
}

int build_tds_SetDynamicDNS_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetDynamicDNSResponse />");	
	return offset;
}

int build_tds_GetNTP_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetNTPResponse>\r\n");
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:NTPInformation>\r\n");
	offset += build_NTPInformation_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.network.NTPInformation);
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:NTPInformation>\r\n");
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetNTPResponse>\r\n");	

	return offset;
}

int build_tds_SetNTP_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetNTPResponse />");	
	return offset;
}

int build_tds_GetZeroConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetZeroConfigurationResponse>\r\n");
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:ZeroConfiguration>\r\n");
	offset += build_ZeroConfiguration_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.network.ZeroConfiguration);
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:ZeroConfiguration>\r\n");
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetZeroConfigurationResponse>\r\n");

	return offset;
}

int build_tds_SetZeroConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetZeroConfigurationResponse />");	
	return offset;
}

int build_tds_GetUsers_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	uint32 i;
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetUsersResponse>");

	for (i = 0; i < ARRAY_SIZE(g_onvif_cfg.users); i++)
	{
		if (g_onvif_cfg.users[i].Username[0] != '\0')
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tds:User>"
					"<tt:Username>%s</tt:Username>"
					"<tt:UserLevel>%s</tt:UserLevel>"
				"</tds:User>", 
				g_onvif_cfg.users[i].Username, 
				onvif_UserLevelToString(g_onvif_cfg.users[i].UserLevel));
		}
	}
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetUsersResponse>");

	return offset;
}

int build_tds_CreateUsers_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:CreateUsersResponse />");
	return offset;
}

int build_tds_DeleteUsers_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:DeleteUsersResponse />");
	return offset;
}

int build_tds_SetUser_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetUserResponse />");
	return offset;
}

int build_tds_GetRemoteUser_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tds_GetRemoteUser_RES * p_res = (tds_GetRemoteUser_RES *) argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetRemoteUserResponse>");

	if (p_res->RemoteUserFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tds:RemoteUser>"
				"<tt:Username>%s</tt:Username>"
				"<tt:UseDerivedPassword>%s</tt:UseDerivedPassword>"
			"</tds:RemoteUser>",
			p_res->RemoteUser.Username,
			p_res->RemoteUser.UseDerivedPassword ? "true" : "false");
	}
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetRemoteUserResponse>");
	
	return offset;
}

int build_tds_SetRemoteUser_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetRemoteUserResponse />");
	return offset;
}

int build_tds_StartFirmwareUpgrade_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tds_StartFirmwareUpgrade_RES * p_res = (tds_StartFirmwareUpgrade_RES *) argv;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:StartFirmwareUpgradeResponse>"
			"<tds:UploadUri>%s</tds:UploadUri>"
			"<tds:UploadDelay>PT%dS</tds:UploadDelay>"
			"<tds:ExpectedDownTime>PT%dS</tds:ExpectedDownTime>"
		"</tds:StartFirmwareUpgradeResponse>", 
		p_res->UploadUri, 
		p_res->UploadDelay, 
		p_res->ExpectedDownTime);
	
	return offset;
}

int build_tds_StartSystemRestore_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tds_StartSystemRestore_RES * p_res = (tds_StartSystemRestore_RES *) argv;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:StartSystemRestoreResponse>"
			"<tds:UploadUri>%s</tds:UploadUri>"
			"<tds:ExpectedDownTime>PT%dS</tds:ExpectedDownTime>"
		"</tds:StartSystemRestoreResponse>", 
		p_res->UploadUri, 
		p_res->ExpectedDownTime);
	
	return offset;
}

int build_tds_GetWsdlUrl_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:GetWsdlUrlResponse>"
			"<tds:WsdlUrl>http://www.onvif.org/</tds:WsdlUrl>"
		"</tds:GetWsdlUrlResponse>");
		
	return offset;
}

int build_tds_GetEndpointReference_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:GetEndpointReferenceResponse>"
			"<tds:GUID>%s</tds:GUID>"
		"</tds:GetEndpointReferenceResponse>",
		g_onvif_cfg.EndpointReference);
	
	return offset;
}

int build_tds_SetHashingAlgorithm_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetHashingAlgorithmResponse />");
	return offset;
}

#ifdef DEVICEIO_SUPPORT

int build_tds_SetRelayOutputSettings_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetRelayOutputSettingsResponse />");
	return offset;
}

int build_tds_SetRelayOutputState_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetRelayOutputStateResponse />");
	return offset;
}

int build_tds_GetRelayOutputs_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	RelayOutputList * p_output = g_onvif_cfg.relay_output;

	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetRelayOutputsResponse>");
	
	while (p_output)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tds:RelayOutputs token=\"%s\">", p_output->RelayOutput.token);

		offset += build_RelayOutput_xml(p_buf+offset, mlen-offset, &p_output->RelayOutput);

		offset += snprintf(p_buf+offset, mlen-offset, "</tds:RelayOutputs>");

		p_output = p_output->next;
	}
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetRelayOutputsResponse>");
	return offset;
}

#endif // DEVICEIO_SUPPORT

#ifdef IPFILTER_SUPPORT	

int build_IPAddressFilter_xml(char * p_buf, int mlen, onvif_IPAddressFilter * p_res)
{
	int i;
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Type>%s</tt:Type>\r\n", onvif_IPAddressFilterTypeToString(p_res->Type));

	for (i = 0; i < ARRAY_SIZE(p_res->IPv4Address); i++)
	{
		if (p_res->IPv4Address[i].Address[0] == '\0')
		{
			continue;
		}

		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:IPv4Address>\r\n"
				"<tt:Address>%s</tt:Address>\r\n"
				"<tt:PrefixLength>%d</tt:PrefixLength>\r\n"
			"</tt:IPv4Address>\r\n", 
			p_res->IPv4Address[i].Address,
			p_res->IPv4Address[i].PrefixLength);
	}

	for (i = 0; i < ARRAY_SIZE(p_res->IPv6Address); i++)
	{
		if (p_res->IPv6Address[i].Address[0] == '\0')
		{
			continue;
		}

		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:IPv6Address>\r\n"
				"<tt:Address>%s</tt:Address>\r\n"
				"<tt:PrefixLength>%d</tt:PrefixLength>\r\n"
			"</tt:IPv6Address>\r\n", 
			p_res->IPv6Address[i].Address,
			p_res->IPv6Address[i].PrefixLength);
	}

	return offset;
}

int build_tds_GetIPAddressFilter_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:GetIPAddressFilterResponse>"
		"<tds:IPAddressFilter>");
	offset += build_IPAddressFilter_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.ipaddr_filter);    
	offset += snprintf(p_buf+offset, mlen-offset, 
		"</tds:IPAddressFilter>"
		"</tds:GetIPAddressFilterResponse>");

	return offset;
}

int build_tds_SetIPAddressFilter_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetIPAddressFilterResponse />");
	return offset;
}

int build_tds_AddIPAddressFilter_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    offset += snprintf(p_buf+offset, mlen-offset, "<tds:AddIPAddressFilterResponse />");
	return offset;
}

int build_tds_RemoveIPAddressFilter_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    offset += snprintf(p_buf+offset, mlen-offset, "<tds:RemoveIPAddressFilterResponse />");
	return offset;
}

#endif // end of IPFILTER_SUPPORT

#ifdef STORAGE_SUPPORT

int build_UserCredential_xml(char * p_buf, int mlen, onvif_UserCredential * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tds:UserName>%s</tds:UserName>", p_res->UserName);

	if (p_res->PasswordFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tds:Password>%s</tds:Password>", p_res->Password);
	}

	return offset;
}

int build_StorageConfigurationData_xml(char * p_buf, int mlen, onvif_StorageConfigurationData * p_res)
{
	int offset = 0;

	if (p_res->LocalPathFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tds:LocalPath>%s</tds:LocalPath>", p_res->LocalPath);
	}

	if (p_res->StorageUriFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tds:StorageUri>%s</tds:StorageUri>", p_res->StorageUri);
	}

	if (p_res->UserFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tds:User>");
		offset += build_UserCredential_xml(p_buf+offset, mlen-offset, &p_res->User);
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:User>");
	}

	return offset;
}

int build_StorageConfiguration_xml(char * p_buf, int mlen, onvif_StorageConfiguration * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tds:Data type=\"%s\"", p_res->Data.type);

	if (p_res->Data.RegionFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, " Region=\"%s\"", p_res->Data.Region);
	}

	offset += snprintf(p_buf+offset, mlen-offset, ">");

	offset += build_StorageConfigurationData_xml(p_buf+offset, mlen-offset, &p_res->Data);

	offset += snprintf(p_buf+offset, mlen-offset, "</tds:Data>");

	return offset;
}

int build_tds_GetStorageConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	StorageConfigurationList * p_storage = g_onvif_cfg.storage;

	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetStorageConfigurationsResponse>");

	while (p_storage)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tds:StorageConfigurations token=\"%s\">", p_storage->Configuration.token);

		offset += build_StorageConfiguration_xml(p_buf+offset, mlen-offset, &p_storage->Configuration);

		offset += snprintf(p_buf+offset, mlen-offset, "</tds:StorageConfigurations>");

		p_storage = p_storage->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetStorageConfigurationsResponse>");

	return offset;
}

int build_tds_GetStorageConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tds_GetStorageConfiguration_REQ * p_res = (tds_GetStorageConfiguration_REQ *) argv;
	StorageConfigurationList * p_storage = onvif_find_StorageConfiguration(g_onvif_cfg.storage, p_res->Token);
	if (NULL == p_storage)
	{
		return ONVIF_ERR_NoConfig;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetStorageConfigurationResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, "<tds:StorageConfiguration token=\"%s\">", p_storage->Configuration.token);

	offset += build_StorageConfiguration_xml(p_buf+offset, mlen-offset, &p_storage->Configuration);

	offset += snprintf(p_buf+offset, mlen-offset, "</tds:StorageConfiguration>");

	offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetStorageConfigurationResponse>"); 

	return offset;
}

int build_tds_CreateStorageConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tds_CreateStorageConfiguration_RES * p_res = (tds_CreateStorageConfiguration_RES *) argv;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:CreateStorageConfigurationResponse>"
	   		"<tds:Token>%s</tds:Token>"
  		"</tds:CreateStorageConfigurationResponse>", 
  		p_res->Token);
	
	return offset;
}

int build_tds_SetStorageConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetStorageConfigurationResponse />");
	return offset;
}

int build_tds_DeleteStorageConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    offset += snprintf(p_buf+offset, mlen-offset, "<tds:DeleteStorageConfigurationResponse />");
	return offset;
}

#endif // STORAGE_SUPPORT

#ifdef GEOLOCATION_SUPPORT

int build_LocationEntity_xml(char * p_buf, int mlen, onvif_LocationEntity * p_res)
{
	int offset = 0;

	if (p_res->GeoLocationFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:GeoLocation");

		if (p_res->GeoLocation.lonFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " lon=\"%f\"", p_res->GeoLocation.lon);
		}

		if (p_res->GeoLocation.latFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " lat=\"%f\"", p_res->GeoLocation.lat);
		}

		if (p_res->GeoLocation.elevationFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " elevation=\"%f\"", p_res->GeoLocation.elevation);
		}

		offset += snprintf(p_buf+offset, mlen-offset, "></tt:GeoLocation>");
	}

	if (p_res->GeoOrientationFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:GeoOrientation");

		if (p_res->GeoOrientation.rollFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " roll=\"%f\"", p_res->GeoOrientation.roll);
		}

		if (p_res->GeoOrientation.pitchFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " pitch=\"%f\"", p_res->GeoOrientation.pitch);
		}

		if (p_res->GeoOrientation.yawFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " yaw=\"%f\"", p_res->GeoOrientation.yaw);
		}

		offset += snprintf(p_buf+offset, mlen-offset, "></tt:GeoOrientation>");
	}

	if (p_res->LocalLocationFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:LocalLocation");

		if (p_res->LocalLocation.xFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " x=\"%f\"", p_res->LocalLocation.x);
		}

		if (p_res->LocalLocation.yFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " y=\"%f\"", p_res->LocalLocation.y);
		}

		if (p_res->LocalLocation.zFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " z=\"%f\"", p_res->LocalLocation.z);
		}

		offset += snprintf(p_buf+offset, mlen-offset, "></tt:LocalLocation>");
	}

	if (p_res->LocalOrientationFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:LocalOrientation");

		if (p_res->LocalOrientation.panFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " pan=\"%f\"", p_res->LocalOrientation.pan);
		}

		if (p_res->LocalOrientation.tiltFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " tilt=\"%f\"", p_res->LocalOrientation.tilt);
		}

		if (p_res->LocalOrientation.rollFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " roll=\"%f\"", p_res->LocalOrientation.roll);
		}

		offset += snprintf(p_buf+offset, mlen-offset, "></tt:LocalOrientation>");
	}

	return offset;    
}

int build_tds_GetGeoLocation_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	LocationEntityList * p_info = g_onvif_cfg.location;

	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetGeoLocationResponse>");
	
	while (p_info)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tds:Location");

		if (p_info->Location.EntityFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " Entity=\"%s\"", p_info->Location.Entity);
		}

		if (p_info->Location.TokenFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " Token=\"%s\"", p_info->Location.Token);
		}

		if (p_info->Location.FixedFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " Fixed=\"%s\"", p_info->Location.Fixed ? "true" : "false");
		}

		if (p_info->Location.GeoSourceFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " GeoSource=\"%s\"", p_info->Location.GeoSource);
		}

		if (p_info->Location.AutoGeoFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " AutoGeo=\"%s\"", p_info->Location.AutoGeo ? "true" : "false");
		}

		offset += snprintf(p_buf+offset, mlen-offset, ">");
		offset += build_LocationEntity_xml(p_buf+offset, mlen-offset, &p_info->Location);
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:Location>");

		p_info = p_info->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetGeoLocationResponse>");

	return offset;
}

int build_tds_SetGeoLocation_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetGeoLocationResponse />");
	return offset;
}

int build_tds_DeleteGeoLocation_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:DeleteGeoLocationResponse />");
	return offset;
}

#endif // GEOLOCATION_SUPPORT

#ifdef DOT11_SUPPORT

int build_Dot11Configuration_xml(char * p_buf, int mlen, onvif_Dot11Configuration * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:SSID>%s</tt:SSID>"
		"<tt:Mode>%s</tt:Mode>"
		"<tt:Alias>%s</tt:Alias>"
		"<tt:Priority>%d</tt:Priority>", 
		p_res->SSID,
		onvif_Dot11StationModeToString(p_res->Mode),
		p_res->Alias,
		p_res->Priority);

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Security>");

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>%s</tt:Mode>", onvif_Dot11SecurityModeToString(p_res->Security.Mode));

	if (p_res->Security.AlgorithmFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Algorithm>%s</tt:Algorithm>", onvif_Dot11CipherToString(p_res->Security.Algorithm));
	}

	if (p_res->Security.PSKFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:PSK>");

		if (p_res->Security.PSK.KeyFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Key>%s</tt:Key>", p_res->Security.PSK.Key);
		}

		if (p_res->Security.PSK.PassphraseFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Passphrase>%s</tt:Passphrase>", p_res->Security.PSK.Passphrase);
		}

		offset += snprintf(p_buf+offset, mlen-offset, "</tt:PSK>");
	}

	if (p_res->Security.Dot1XFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Dot1X>%s</tt:Dot1X>", p_res->Security.Dot1X);
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tt:Security>");

	return offset;
}

int build_Dot11AvailableNetworks_xml(char * p_buf, int mlen, onvif_Dot11AvailableNetworks * p_res)
{
	uint32 i;
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:SSID>%s</tt:SSID>", p_res->SSID);

	if (p_res->BSSIDFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:BSSID>%s</tt:BSSID>", p_res->BSSID);
	}

	for (i = 0; i < p_res->sizeAuthAndMangementSuite; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:AuthAndMangementSuite>%s</tt:AuthAndMangementSuite>", onvif_Dot11AuthAndMangementSuiteToString(p_res->AuthAndMangementSuite[i]));
	}

	for (i = 0; i < p_res->sizePairCipher; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:PairCipher>%s</tt:PairCipher>", onvif_Dot11CipherToString(p_res->PairCipher[i]));
	}

	for (i = 0; i < p_res->sizeGroupCipher; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:GroupCipher>%s</tt:GroupCipher>", onvif_Dot11CipherToString(p_res->GroupCipher[i]));
	}

	if (p_res->SignalStrengthFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:SignalStrength>%s</tt:SignalStrength>", onvif_Dot11SignalStrengthToString(p_res->SignalStrength));
	}

	return offset;
}

int build_tds_GetDot11Capabilities_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	onvif_Dot11Capabilities * p_cap = &g_onvif_cfg.Capabilities.dot11;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:GetDot11CapabilitiesResponse>"
			"<tds:Capabilities>"
				"<tt:TKIP>%s</tt:TKIP>"
				"<tt:ScanAvailableNetworks>%s</tt:ScanAvailableNetworks>"
				"<tt:MultipleConfiguration>%s</tt:MultipleConfiguration>"
				"<tt:AdHocStationMode>%s</tt:AdHocStationMode>"
				"<tt:WEP>%s</tt:WEP>"
			"</tds:Capabilities>"    
		"</tds:GetDot11CapabilitiesResponse>",
		p_cap->TKIP ? "true" : "false",
		p_cap->ScanAvailableNetworks ? "true" : "false",
		p_cap->MultipleConfiguration ? "true" : "false",
		p_cap->AdHocStationMode ? "true" : "false",
		p_cap->WEP ? "true" : "false");

	return offset;
}

int build_tds_GetDot11Status_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tds_GetDot11Status_RES * p_res = (tds_GetDot11Status_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:GetDot11StatusResponse>"
		"<tds:Status>");

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:SSID>%s</tt:SSID>", p_res->Status.SSID);

	if (p_res->Status.BSSIDFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:BSSID>%s</tt:BSSID>", p_res->Status.BSSID);
	}

	if (p_res->Status.PairCipherFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:PairCipher>%s</tt:PairCipher>", onvif_Dot11CipherToString(p_res->Status.PairCipher));
	}

	if (p_res->Status.GroupCipherFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:GroupCipher>%s</tt:GroupCipher>", onvif_Dot11CipherToString(p_res->Status.GroupCipher));
	}

	if (p_res->Status.SignalStrengthFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:SignalStrength>%s</tt:SignalStrength>", onvif_Dot11SignalStrengthToString(p_res->Status.SignalStrength));
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:ActiveConfigAlias>%s</tt:ActiveConfigAlias>", p_res->Status.ActiveConfigAlias);

	offset += snprintf(p_buf+offset, mlen-offset, 
		"</tds:Status>"
		"</tds:GetDot11StatusResponse>");

	return offset;    
}

int build_tds_ScanAvailableDot11Networks_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int i;
	int offset = 0;
	tds_ScanAvailableDot11Networks_RES * p_res = (tds_ScanAvailableDot11Networks_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tds:ScanAvailableDot11NetworksResponse>");

	for (i = 0; i < p_res->sizeNetworks; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tds:Networks>");
		offset += build_Dot11AvailableNetworks_xml(p_buf+offset, mlen-offset, &p_res->Networks[i]);
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:Networks>");
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:ScanAvailableDot11NetworksResponse>");

	return offset; 
}

#endif // DOT11_SUPPORT

/***************************************************************************************/

#ifdef PROFILE_Q_SUPPORT

int build_Base_EventProperties_xml(char * p_buf, int mlen)
{
	int offset = snprintf(p_buf, mlen, 
		"<tns1:Monitoring wstop:topic=\"true\">"
			// tns1:Monitoring/ProcessorUsage
			"<ProcessorUsage wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"Token\" Type=\"tt:ReferenceToken\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:SimpleItemDescription Name=\"Value\" Type=\"xs:float\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</ProcessorUsage>"

			"<OperatingTime wstop:topic=\"true\">"
				// tns1:Monitoring/OperatingTime/LastReset
				"<LastReset wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"true\">"
				"<tt:Source>"
				"</tt:Source>"
				"<tt:Data>"
				    "<tt:SimpleItemDescription Name=\"Status\" Type=\"xs:dateTime\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</LastReset>"

				// tns1:Monitoring/OperatingTime/LastReboot
				"<LastReboot wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"true\">"
				"<tt:Source>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"Status\" Type=\"xs:dateTime\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</LastReboot>"

				// tns1:Monitoring/OperatingTime/LastClockSynchronization
				"<LastClockSynchronization wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"true\">"
				"<tt:Source>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"Status\" Type=\"xs:dateTime\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</LastClockSynchronization>"
			"</OperatingTime>"
		"</tns1:Monitoring>"
	);

	return offset;
}

#endif // PROFILE_Q_SUPPORT

int build_Imaging_EventProperties_xml(char * p_buf, int mlen)
{
	int offset = snprintf(p_buf, mlen, 
		"<tns1:VideoSource wstop:topic=\"true\">"
			// tns1:VideoSource/ImageTooBlurry/ImagingService
			"<ImageTooBlurry wstop:topic=\"true\">"
				"<ImagingService wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"true\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"Source\" Type=\"tt:ReferenceToken\"/>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"State\" Type=\"xs:boolean\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</ImagingService>"
			"</ImageTooBlurry>"

			// tns1:VideoSource/ImageTooDark/ImagingService
			"<ImageTooDark wstop:topic=\"true\">"
				"<ImagingService wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"true\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"Source\" Type=\"tt:ReferenceToken\"/>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"State\" Type=\"xs:boolean\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</ImagingService>"
			"</ImageTooDark>"

			// tns1:VideoSource/ImageTooBright/ImagingService
			"<ImageTooBright wstop:topic=\"true\">"
				"<ImagingService wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"true\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"Source\" Type=\"tt:ReferenceToken\"/>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"State\" Type=\"xs:boolean\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</ImagingService>"
			"</ImageTooBright>"

			// tns1:VideoSource/GlobalSceneChange/ImagingService
			"<GlobalSceneChange wstop:topic=\"true\">"
				"<ImagingService wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"true\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"Source\" Type=\"tt:ReferenceToken\"/>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"State\" Type=\"xs:boolean\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</ImagingService>"
			"</GlobalSceneChange>"

			// tns1:VideoSource/SignalLoss
			"<SignalLoss wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"Source\" Type=\"tt:ReferenceToken\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:SimpleItemDescription Name=\"State\" Type=\"xs:boolean\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</SignalLoss>"

			// tns1:VideoSource/MotionAlarm
			"<MotionAlarm wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"Source\" Type=\"tt:ReferenceToken\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:SimpleItemDescription Name=\"State\" Type=\"xs:boolean\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"	
			"</MotionAlarm>"
		"</tns1:VideoSource>"
	);

	return offset;
}

#ifdef MEDIA_SUPPORT

int build_Media_EventProperties_xml(char * p_buf, int mlen)
{
	int offset = snprintf(p_buf, mlen, 
		"<tns1:Configuration wstop:topic=\"true\">"
			// tns1:Configuration/Profile
			"<Profile wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"Token\" Type=\"tt:ReferenceToken\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:ElementItemDescription Name=\"Configuration\" Type=\"tt:Profile\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</Profile>"

			// tns1:Configuration/VideoEncoderConfiguration
			"<VideoEncoderConfiguration wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"Token\" Type=\"tt:ReferenceToken\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:ElementItemDescription Name=\"Configuration\" Type=\"tt:VideoEncoderConfiguration\"/>"
			"</tt:Data>"				
			"</tt:MessageDescription>"
			"</VideoEncoderConfiguration>"

			// tns1:Configuration/VideoSourceConfiguration/MediaService
			"<VideoSourceConfiguration wstop:topic=\"true\">"
			"<MediaService wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"Token\" Type=\"tt:ReferenceToken\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:ElementItemDescription Name=\"Configuration\" Type=\"tt:VideoSourceConfiguration\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</MediaService>"
			"</VideoSourceConfiguration>"

#ifdef DEVICEIO_SUPPORT
			// tns1:Configuration/VideoOutputConfiguration/MediaService
			"<VideoOutputConfiguration wstop:topic=\"true\">"
			"<MediaService wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"Token\" Type=\"tt:ReferenceToken\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:ElementItemDescription Name=\"Configuration\" Type=\"tt:VideoOutputConfiguration\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</MediaService>"
			"</VideoOutputConfiguration>"
#endif

			// tns1:Configuration/MetadataConfiguration
			"<MetadataConfiguration wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"Token\" Type=\"tt:ReferenceToken\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:ElementItemDescription Name=\"Configuration\" Type=\"tt:MetadataConfiguration\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</MetadataConfiguration>"
			
#ifdef AUDIO_SUPPORT
			// tns1:Configuration/AudioEncoderConfiguration
			"<AudioEncoderConfiguration wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"Token\" Type=\"tt:ReferenceToken\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:ElementItemDescription Name=\"Configuration\" Type=\"tt:AudioEncoderConfiguration\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</AudioEncoderConfiguration>"

			// tns1:Configuration/AudioSourceConfiguration/MediaService
			"<AudioSourceConfiguration wstop:topic=\"true\">"
			"<MediaService wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"Token\" Type=\"tt:ReferenceToken\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:ElementItemDescription Name=\"Configuration\" Type=\"tt:AudioSourceConfiguration\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</MediaService>"
			"</AudioSourceConfiguration>"

#ifdef DEVICEIO_SUPPORT
			// tns1:Configuration/AudioOutputConfiguration/MediaService
			"<AudioOutputConfiguration wstop:topic=\"true\">"
			"<MediaService wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"Token\" Type=\"tt:ReferenceToken\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:ElementItemDescription Name=\"Configuration\" Type=\"tt:AudioOutputConfiguration\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</MediaService>"
			"</AudioOutputConfiguration>"
#endif
#endif

#ifdef PTZ_SUPPORT
			// tns1:Configuration/PTZConfiguration
			"<PTZConfiguration wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"Token\" Type=\"tt:ReferenceToken\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:ElementItemDescription Name=\"Configuration\" Type=\"tt:PTZConfiguration\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</PTZConfiguration>"
#endif

#ifdef VIDEO_ANALYTICS
			// tns1:Configuration/VideoAnalyticsConfiguration
			"<VideoAnalyticsConfiguration wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"Token\" Type=\"tt:ReferenceToken\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:ElementItemDescription Name=\"Configuration\" Type=\"tt:VideoAnalyticsConfiguration\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</VideoAnalyticsConfiguration>"
#endif

		"</tns1:Configuration>"
	);

	return offset; 
}

#endif // MEDIA_SUPPORT

#ifdef MEDIA2_SUPPORT

int build_Media2_EventProperties_xml(char * p_buf, int mlen)
{
	int offset = snprintf(p_buf, mlen, 
		"<tns1:Media wstop:topic=\"true\">"
			// tns1:Media/ProfileChanged
			"<ProfileChanged wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"Token\" Type=\"tt:ReferenceToken\"/>"
			"</tt:Source>"
			"</tt:MessageDescription>"
			"</ProfileChanged>"

			// tns1:Media/ConfigurationChanged
			"<ConfigurationChanged wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"Token\" Type=\"tt:ReferenceToken\"/>"
				"<tt:SimpleItemDescription Name=\"Type\" Type=\"xs:string\"/>"
			"</tt:Source>"
			"</tt:MessageDescription>"
			"</ConfigurationChanged>"
		"</tns1:Media>"
	);

	return offset; 
}

#endif // end of MEDIA2_SUPPORT

#ifdef PTZ_SUPPORT

int build_PTZ_EventProperties_xml(char * p_buf, int mlen)
{
	int offset = snprintf(p_buf, mlen, 
		"<tns1:PTZController wstop:topic=\"true\">"
			"<PTZPresets wstop:topic=\"true\">"
				// tns1:PTZController/PTZPresets/Invoked
				"<Invoked wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"true\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"PTZConfigurationToken\" Type=\"tt:ReferenceToken\"/>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"PresetToken\" Type=\"tt:ReferenceToken\"/>"
					"<tt:SimpleItemDescription Name=\"PresetName\" Type=\"tt:Name\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"	
				"</Invoked>"

				// tns1:PTZController/PTZPresets/Reached
				"<Reached wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"true\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"PTZConfigurationToken\" Type=\"tt:ReferenceToken\"/>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"PresetToken\" Type=\"tt:ReferenceToken\"/>"
					"<tt:SimpleItemDescription Name=\"PresetName\" Type=\"tt:Name\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"	
				"</Reached>"
				
				// tns1:PTZController/PTZPresets/Aborted
				"<Aborted wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"true\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"PTZConfigurationToken\" Type=\"tt:ReferenceToken\"/>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"PresetToken\" Type=\"tt:ReferenceToken\"/>"
					"<tt:SimpleItemDescription Name=\"PresetName\" Type=\"tt:Name\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"	
				"</Aborted>"
				
				// tns1:PTZController/PTZPresets/Left
				"<Left wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"true\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"PTZConfigurationToken\" Type=\"tt:ReferenceToken\"/>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"PresetToken\" Type=\"tt:ReferenceToken\"/>"
					"<tt:SimpleItemDescription Name=\"PresetName\" Type=\"tt:Name\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"	
				"</Left>"
			"</PTZPresets>"
		"</tns1:PTZController>"
	);

	return offset; 
}

#endif // PTZ_SUPPORT

#ifdef VIDEO_ANALYTICS

int build_Analytics_EventProperties_xml(char * p_buf, int mlen)
{
	int offset = snprintf(p_buf, mlen, 
		"<tns1:RuleEngine wstop:topic=\"true\">"
			// tns1:RuleEngine/MotionRegionDetector/Motion
			"<MotionRegionDetector wstop:topic=\"true\">"
			"<Motion wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"VideoSource\" Type=\"tt:ReferenceToken\"/>"
				"<tt:SimpleItemDescription Name=\"RuleName\" Type=\"xs:string\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:SimpleItemDescription Name=\"State\" Type=\"xs:boolean\"/>"
			"</tt:Data>"					
			"</tt:MessageDescription>"					
			"</Motion>"
			"</MotionRegionDetector>"

			// tns1:RuleEngine/CellMotionDetector/Motion
			"<CellMotionDetector wstop:topic=\"true\">"
			"<Motion wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"VideoSourceConfigurationToken\" Type=\"tt:ReferenceToken\"/>"
				"<tt:SimpleItemDescription Name=\"VideoAnalyticsConfigurationToken\" Type=\"tt:ReferenceToken\"/>"
				"<tt:SimpleItemDescription Name=\"Rule\" Type=\"xs:string\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:SimpleItemDescription Name=\"IsMotion\" Type=\"xs:boolean\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</Motion>"
			"</CellMotionDetector>"

			// tns1:RuleEngine/Recognition/Face
			"<Recognition wstop:topic=\"true\">"
			"<Face wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"false\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"VideoSource\" Type=\"tt:ReferenceToken\"/>"
				"<tt:SimpleItemDescription Name=\"AnalyticsConfiguration\" Type=\"tt:ReferenceToken\"/>"
				"<tt:SimpleItemDescription Name=\"Rule\" Type=\"xs:string\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:SimpleItemDescription Name=\"Likelihood\" Type=\"xs:float\"/>"
				"<tt:SimpleItemDescription Name=\"Label\" Type=\"xs:string\"/>"
				"<tt:SimpleItemDescription Name=\"ImageUri\" Type=\"xs:anyURI\"/>"
				"<tt:SimpleItemDescription Name=\"EnrollmentID\" Type=\"xs:string\"/>"
				"<tt:SimpleItemDescription Name=\"RefImageUri\" Type=\"xs:anyURI\"/>"
				"<tt:ElementItemDescription Name=\"Image\" Type=\"xs:base64Binary\"/>"
				"<tt:ElementItemDescription Name=\"BoundingBox\" Type=\"tt:Rectangle\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</Face>"
			"</Recognition>"

			// tns1:RuleEngine/Recognition/LicensePlate
			"<Recognition wstop:topic=\"true\">"
			"<LicensePlate wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"false\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"VideoSource\" Type=\"tt:ReferenceToken\"/>"
				"<tt:SimpleItemDescription Name=\"AnalyticsConfiguration\" Type=\"tt:ReferenceToken\"/>"
				"<tt:SimpleItemDescription Name=\"Rule\" Type=\"xs:string\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:SimpleItemDescription Name=\"Likelihood\" Type=\"xs:float\"/>"
				"<tt:SimpleItemDescription Name=\"Label\" Type=\"xs:string\"/>"
				"<tt:SimpleItemDescription Name=\"ImageUri\" Type=\"xs:anyURI\"/>"
				"<tt:SimpleItemDescription Name=\"VehicleImageURI\" Type=\"xs:anyURI\"/>"
				"<tt:ElementItemDescription Name=\"Image\" Type=\"xs:base64Binary\"/>"
				"<tt:ElementItemDescription Name=\"BoundingBox\" Type=\"tt:Rectangle\"/>"
				"<tt:ElementItemDescription Name=\"LicensePlateInfo\" Type=\"tt:LicensePlateInfo\"/>"
				"<tt:ElementItemDescription Name=\"VehicleInfo\" Type=\"tt:VehicleInfo\"/>"
				"<tt:ElementItemDescription Name=\"VehicleImage\" Type=\"xs:base64Binary\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</LicensePlate>"
			"</Recognition>"

			// tns1:RuleEngine/CountAggregation/Counter
			"<CountAggregation wstop:topic=\"true\">"
			"<Counter wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"VideoSource\" Type=\"tt:ReferenceToken\"/>"
				"<tt:SimpleItemDescription Name=\"AnalyticsConfiguration\" Type=\"tt:ReferenceToken\"/>"
				"<tt:SimpleItemDescription Name=\"Rule\" Type=\"xs:string\"/>"
			"</tt:Source>"
			"<tt:Key>"
				"<tt:SimpleItemDescription Name=\"ObjectId\" Type=\"xs:integer\"/>"
			"</tt:Key>"
			"<tt:Data>"
				"<tt:SimpleItemDescription Name=\"Count\" Type=\"xs:int\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</Counter>"
			"</CountAggregation>"
		"</tns1:RuleEngine>"
	);

	return offset;        
}

#endif

#ifdef DEVICEIO_SUPPORT

int build_DeviceIO_EventProperties_xml(char * p_buf, int mlen)
{
	int offset = snprintf(p_buf, mlen, 
		"<tns1:Device wstop:topic=\"true\">"
			"<tnshik:AlarmIn wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"true\">"
					"<tt:Source>"
						"<tt:SimpleItemDescription Name=\"AlarmInToken\" Type=\"tt:ReferenceToken\"/>"
					"</tt:Source>"
					"<tt:Data>"
						"<tt:SimpleItemDescription Name=\"State\" Type=\"xs:boolean\"/>"
					"</tt:Data>"
				"</tt:MessageDescription>"
			"</tnshik:AlarmIn>"
			"<Trigger wstop:topic=\"true\">"
				// tns1:Device/Trigger/DigitalInput
				"<DigitalInput wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"true\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"InputToken\" Type=\"tt:ReferenceToken\" />"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"LogicalState\" Type=\"xs:boolean\" />"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</DigitalInput>"

				// tns1:Device/Trigger/Relay  
				"<Relay wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"true\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"RelayToken\" Type=\"tt:ReferenceToken\" />"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"LogicalState\" Type=\"tt:RelayLogicalState\" />"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</Relay>"
			"</Trigger>"
		"</tns1:Device>"
	);

	return offset;
}

#endif // end if DEVICEIO_SUPPORT

#ifdef PROFILE_G_SUPPORT

int build_ProfileG_EventProperties_xml(char * p_buf, int mlen)
{
	int offset = snprintf(p_buf, mlen, 
		"<tns1:RecordingConfig wstop:topic=\"true\">"

			// tns1:RecordingConfig/CreateRecording    
			"<CreateRecording wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"false\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"RecordingToken\" Type=\"tt:RecordingReference\"/>"
			"</tt:Source>"
			"<tt:Data>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</CreateRecording>"

			// tns1:RecordingConfig/DeleteRecording
			"<DeleteRecording wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"false\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"RecordingToken\" Type=\"tt:RecordingReference\"/>"
			"</tt:Source>"
			"<tt:Data>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</DeleteRecording>"

			// tns1:RecordingConfig/CreateTrack
			"<CreateTrack wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"false\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"RecordingToken\" Type=\"tt:RecordingReference\"/>"
				"<tt:SimpleItemDescription Name=\"TrackToken\" Type=\"tt:TrackReference\"/>"
			"</tt:Source>"
			"<tt:Data>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</CreateTrack>"

			// tns1:RecordingConfig/DeleteTrack
			"<DeleteTrack wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"false\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"RecordingToken\" Type=\"tt:RecordingReference\"/>"
				"<tt:SimpleItemDescription Name=\"TrackToken\" Type=\"tt:TrackReference\"/>"
			"</tt:Source>"
			"<tt:Data>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</DeleteTrack>"

			// tns1:RecordingConfig/JobState
			"<JobState wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"RecordingJobToken\" Type=\"tt:RecordingJobReference\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:SimpleItemDescription Name=\"State\" Type=\"xs:string\" />"
				"<tt:ElementItemDescription Name=\"Information\" Type=\"tt:RecordingJobStateInformation\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</JobState>"

			// tns1:RecordingConfig/RecordingConfiguration
			"<RecordingConfiguration wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"false\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"RecordingToken\" Type=\"tt:RecordingReference\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:ElementItemDescription Name=\"Configuration\" Type=\"tt:RecordingConfiguration\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</RecordingConfiguration>"

			// tns1:RecordingConfig/TrackConfiguration
			"<TrackConfiguration wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"false\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"RecordingToken\" Type=\"tt:RecordingReference\"/>"
			"<tt:SimpleItemDescription Name=\"TrackToken\" Type=\"tt:TrackReference\" />"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:ElementItemDescription Name=\"Configuration\" Type=\"tt:TrackConfiguration\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</TrackConfiguration>"

			// tns1:RecordingConfig/RecordingJobConfiguration
			"<RecordingJobConfiguration wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"false\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"RecordingJobToken\" Type=\"tt:RecordingJobReference\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:ElementItemDescription Name=\"Configuration\" Type=\"tt:RecordingJobConfiguration\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</RecordingJobConfiguration>"

			// tns1:RecordingConfig/DeleteTrackData
			"<DeleteTrackData wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"false\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"RecordingToken\" Type=\"tt:RecordingReference\"/>"
				"<tt:SimpleItemDescription Name=\"TrackToken\" Type=\"tt:TrackReference\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:SimpleItemDescription Name=\"StartTime\" Type=\"xs:dateTime\"/>"
				"<tt:SimpleItemDescription Name=\"EndTime\" Type=\"xs:dateTime\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</DeleteTrackData>"
			
		"</tns1:RecordingConfig>"

		"<tns1:RecordingHistory wstop:topic=\"true\">"

			// tns1:RecordingHistory/Recording/State
			"<Recording wstop:topic=\"true\">"
			"<State wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"RecordingToken\" Type=\"tt:ReferenceToken\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:SimpleItemDescription Name=\"IsRecording\" Type=\"xs:boolean\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</State>"
			"</Recording>"

			// tns1:RecordingHistory/Track/State
			"<Track wstop:topic=\"true\">"
			"<State wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"RecordingToken\" Type=\"tt:ReferenceToken\"/>"
				"<tt:SimpleItemDescription Name=\"Track\" Type=\"tt:ReferenceToken\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:SimpleItemDescription Name=\"IsDataPresent\" Type=\"xs:boolean\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</State>"
			"</Track>"
			
		"</tns1:RecordingHistory>"
	);

	return offset;
}

#endif // end of PROFILE_G_SUPPORT

#ifdef PROFILE_C_SUPPORT

int build_ProfileC_EventProperties_xml(char * p_buf, int mlen)
{
	int offset = snprintf(p_buf, mlen, 
		"<tns1:AccessControl wstop:topic=\"true\">"
			"<AccessGranted wstop:topic=\"true\">"
				// tns1:AccessControl/AccessGranted/Anonymous
				"<Anonymous wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"AccessPointToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"External\" Type=\"xs:boolean\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</Anonymous>"

				// tns1:AccessControl/AccessGranted/Credential
				"<Credential wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"AccessPointToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"External\" Type=\"xs:boolean\"/>"
					"<tt:SimpleItemDescription Name=\"CredentialToken\" Type=\"pt:ReferenceToken\"/>"
					"<tt:SimpleItemDescription Name=\"CredentialHolderName\" Type=\"xs:string\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</Credential>"

				// tns1:AccessControl/AccessGranted/Identifier
				"<Identifier wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"AccessPointToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"IdentifierType\" Type=\"xs:string\"/>"
					"<tt:SimpleItemDescription Name=\"FormatType\" Type=\"xs:string\"/>"
					"<tt:SimpleItemDescription Name=\"IdentifierValue\" Type=\"xs:hexBinary\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</Identifier>"
			"</AccessGranted>"
			
			"<AccessTaken wstop:topic=\"true\">"
				// tns1:AccessControl/AccessTaken/Anonymous
				"<Anonymous wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"AccessPointToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"
				"</tt:MessageDescription>"
				"</Anonymous>"

				// tns1:AccessControl/AccessTaken/Credential
	 			"<Credential wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"AccessPointToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"CredentialToken\" Type=\"pt:ReferenceToken\"/>"
					"<tt:SimpleItemDescription Name=\"CredentialHolderName\" Type=\"xs:string\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</Credential>"

				// tns1:AccessControl/AccessTaken/Identifier
				"<Identifier wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"AccessPointToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"IdentifierType\" Type=\"xs:string\"/>"
					"<tt:SimpleItemDescription Name=\"FormatType\" Type=\"xs:string\"/>"
					"<tt:SimpleItemDescription Name=\"IdentifierValue\" Type=\"xs:hexBinary\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</Identifier>"
			"</AccessTaken>"

			"<AccessNotTaken wstop:topic=\"true\">"
				// tns1:AccessControl/AccessNotTaken/Anonymous
				"<Anonymous wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"AccessPointToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"
				"</tt:MessageDescription>"
				"</Anonymous>"

				// tns1:AccessControl/AccessNotTaken/Credential
				"<Credential wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"AccessPointToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"CredentialToken\" Type=\"pt:ReferenceToken\"/>"
					"<tt:SimpleItemDescription Name=\"CredentialHolderName\" Type=\"xs:string\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</Credential>"

				// tns1:AccessControl/AccessNotTaken/Identifier
				"<Identifier wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"AccessPointToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"IdentifierType\" Type=\"xs:string\"/>"
					"<tt:SimpleItemDescription Name=\"FormatType\" Type=\"xs:string\"/>"
					"<tt:SimpleItemDescription Name=\"IdentifierValue\" Type=\"xs:hexBinary\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</Identifier>"
			"</AccessNotTaken>"

			"<Denied wstop:topic=\"true\">"
				// tns1:tns1:AccessControl/Denied/Anonymous
				"<Anonymous wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"AccessPointToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"External\" Type=\"xs:boolean\"/>"
					"<tt:SimpleItemDescription Name=\"Reason\" Type=\"xs:string\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</Anonymous>"

				// tns1:AccessControl/Denied/Credential
				"<Credential wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"AccessPointToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"External\" Type=\"xs:boolean\"/>"
					"<tt:SimpleItemDescription Name=\"CredentialToken\" Type=\"pt:ReferenceToken\"/>"
					"<tt:SimpleItemDescription Name=\"CredentialHolderName\" Type=\"xs:string\"/>"
					"<tt:SimpleItemDescription Name=\"Reason\" Type=\"xs:string\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</Credential>"

				// tns1:AccessControl/Denied/Identifier
				"<Identifier wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"AccessPointToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"IdentifierType\" Type=\"xs:string\"/>"
					"<tt:SimpleItemDescription Name=\"FormatType\" Type=\"xs:string\"/>"
					"<tt:SimpleItemDescription Name=\"IdentifierValue\" Type=\"xs:hexBinary\"/>"
					"<tt:SimpleItemDescription Name=\"Reason\" Type=\"xs:string\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</Identifier>"

				// tns1:AccessControl/Denied/CredentialNotFound
				"<CredentialNotFound wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"AccessPointToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"IdentifierType\" Type=\"xs:string\"/>"
					"<tt:SimpleItemDescription Name=\"IdentifierValue\" Type=\"xs:hexBinary\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</CredentialNotFound>"

				// tns1:AccessControl/Denied/CredentialNotFound/Card
				"<CredentialNotFound wstop:topic=\"true\">"
				"<Card wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"AccessPointToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"Card\" Type=\"xs:string\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</Card>"
				"</CredentialNotFound>"
			"</Denied>"

			// tns1:AccessControl/Duress
			"<Duress wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"false\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"AccessPointToken\" Type=\"pt:ReferenceToken\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:SimpleItemDescription Name=\"CredentialToken\" Type=\"pt:ReferenceToken\"/>"
				"<tt:SimpleItemDescription Name=\"CredentialHolderName\" Type=\"xs:string\"/>"
				"<tt:SimpleItemDescription Name=\"Reason\" Type=\"xs:string\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</Duress>"
		"</tns1:AccessControl>"
		
		// tns1:AccessPoint/State/Enabled
		"<tns1:AccessPoint wstop:topic=\"true\">"
			"<State wstop:topic=\"true\">"
			"<Enabled wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"AccessPointToken\" Type=\"pt:ReferenceToken\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:SimpleItemDescription Name=\"State\" Type=\"xs:boolean\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</Enabled>"
			"</State>"
		"</tns1:AccessPoint>"

		"<tns1:Configuration wstop:topic=\"true\">"
			"<AccessPoint wstop:topic=\"true\">"
				// tns1:Configuration/AccessPoint/Changed
				"<Changed wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"AccessPointToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"
				"</tt:MessageDescription>"
				"</Changed>"

				// tns1:Configuration/AccessPoint/Removed
				"<Removed wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"AccessPointToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"
				"</tt:MessageDescription>"
				"</Removed>"
			"</AccessPoint>"

			"<Area wstop:topic=\"true\">"
				// tns1:Configuration/Area/Changed
				"<Changed wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"AreaToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"
				"</tt:MessageDescription>"
				"</Changed>"

				// tns1:Configuration/Area/Removed
				"<Removed wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"AreaToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"
				"</tt:MessageDescription>"
				"</Removed>"
			"</Area>"

			"<Door wstop:topic=\"true\">"
				// tns1:Configuration/Door/Changed
				"<Changed wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"DoorToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"
				"</tt:MessageDescription>"
				"</Changed>"

				// tns1:Configuration/Door/Removed
				"<Removed wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"DoorToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"
				"</tt:MessageDescription>"
				"</Removed>"
			"</Door>"
		"</tns1:Configuration>"
		
		"<tns1:Door wstop:topic=\"true\">"
			"<State wstop:topic=\"true\">"
				// tns1:Door/State/DoorMode
				"<DoorMode wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"true\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"DoorToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"	
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"State\" Type=\"tdc:DoorMode\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</DoorMode>"

				// tns1:Door/State/DoorPhysicalState
				"<DoorPhysicalState wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"true\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"DoorToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"	
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"State\" Type=\"tdc:DoorPhysicalState\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</DoorPhysicalState>"

				// tns1:Door/State/LockPhysicalState
				"<LockPhysicalState wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"true\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"DoorToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"State\" Type=\"tdc:LockPhysicalState\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</LockPhysicalState>"

				// tns1:Door/State/DoubleLockPhysicalState
				"<DoubleLockPhysicalState wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"true\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"DoorToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"	
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"State\" Type=\"tdc:LockPhysicalState\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</DoubleLockPhysicalState>"

				// tns1:Door/State/DoorAlarm
				"<DoorAlarm wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"true\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"DoorToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"State\" Type=\"tdc:DoorAlarmState\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</DoorAlarm>"

				// tns1:Door/State/DoorTamper
				"<DoorTamper wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"true\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"DoorToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"	
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"State\" Type=\"tdc:DoorTamperState\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</DoorTamper>"

				// tns1:Door/State/DoorFault
				"<DoorFault wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"true\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"DoorToken\" Type=\"pt:ReferenceToken\"/>"
				"</tt:Source>"	
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"State\" Type=\"tdc:DoorFaultState\"/>"
					"<tt:SimpleItemDescription Name=\"Reason\" Type=\"xs:string\"/>"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</DoorFault>"
			"</State>"
		"</tns1:Door>"
	);

	return offset;				
}

#endif // end of PROFILE_C_SUPPORT

#ifdef CREDENTIAL_SUPPORT

int build_Credential_EventProperties_xml(char * p_buf, int mlen)
{
	int offset = snprintf(p_buf, mlen, 
		"<tns1:Credential wstop:topic=\"true\">"
			"<State wstop:topic=\"true\">"
				// tns1:Credential/State/Enabled
				"<Enabled wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"CredentialToken\" Type=\"pt:ReferenceToken\" />"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"State\" Type=\"xs:boolean\" />"
					"<tt:SimpleItemDescription Name=\"Reason\" Type=\"xs:string\" />"
					"<tt:SimpleItemDescription Name=\"ClientUpdated\" Type=\"xs:boolean\" />"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</Enabled>"

				// tns1:Credential/State/ApbViolation
				"<ApbViolation wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"CredentialToken\" Type=\"pt:ReferenceToken\" />"
				"</tt:Source>"
				"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"ApbViolation\" Type=\"xs:boolean\" />"
					"<tt:SimpleItemDescription Name=\"ClientUpdated\" Type=\"xs:boolean\" />"
				"</tt:Data>"
				"</tt:MessageDescription>"
				"</ApbViolation>"
			"</State>"
		"</tns1:Credential>"

		"<tns1:Configuration wstop:topic=\"true\">"
			"<Credential wstop:topic=\"true\">"
				// tns1:Configuration/Credential/Changed
				"<Changed wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"CredentialToken\" Type=\"pt:ReferenceToken\" />"
				"</tt:Source>"
				"</tt:MessageDescription>"
				"</Changed>"

				// tns1:Configuration/Credential/Removed
				"<Removed wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"CredentialToken\" Type=\"pt:ReferenceToken\" />"
				"</tt:Source>"
				"</tt:MessageDescription>"
				"</Removed>"
			"</Credential>"
		"</tns1:Configuration>"
	);

	return offset;
}

#endif // end of CREDENTIAL_SUPPORT

#ifdef ACCESS_RULES

int build_AccessRules_EventProperties_xml(char * p_buf, int mlen)
{
	int offset = snprintf(p_buf, mlen, 
		"<tns1:Configuration wstop:topic=\"true\">"
			"<AccessProfile wstop:topic=\"true\">"
				// tns1:Configuration/AccessProfile/Changed
				"<Changed wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"AccessProfileToken\" Type=\"pt:ReferenceToken\" />"
				"</tt:Source>"
				"</tt:MessageDescription>"
				"</Changed>"

				// tns1:Configuration/AccessProfile/Removed
				"<Removed wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"AccessProfileToken\" Type=\"pt:ReferenceToken\" />"
				"</tt:Source>"
				"</tt:MessageDescription>"
				"</Removed>"
			"</AccessProfile>"
		"</tns1:Configuration>"
	);

	return offset;
}

#endif // end of ACCESS_RULES

#ifdef SCHEDULE_SUPPORT

int build_Schedule_EventProperties_xml(char * p_buf, int mlen)
{
	int offset = snprintf(p_buf, mlen, 

		// tns1:Schedule/State/Active
		"<tns1:Schedule wstop:topic=\"true\">"
			"<State wstop:topic=\"true\">"
			"<Active wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"ScheduleToken\" Type=\"pt:ReferenceToken\" />"
				"<tt:SimpleItemDescription Name=\"Name\" Type=\"xs:string\" />"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:SimpleItemDescription Name=\"Active\" Type=\"xs:boolean\" />"
				"<tt:SimpleItemDescription Name=\"SpecialDay\" Type=\"xs:boolean\" />"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</Active>"
			"</State>"
		"</tns1:Schedule>"

		"<tns1:Configuration wstop:topic=\"true\">"
			"<Schedule wstop:topic=\"true\">"
				// tns1:Configuration/Schedule/Changed
				"<Changed wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"ScheduleToken\" Type=\"pt:ReferenceToken\" />"
				"</tt:Source>"
				"</tt:MessageDescription>"
				"</Changed>"

				// tns1:Configuration/Schedule/Removed
				"<Removed wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"ScheduleToken\" Type=\"pt:ReferenceToken\" />"
				"</tt:Source>"
				"</tt:MessageDescription>"
				"</Removed>"
			"</Schedule>"
			
			"<SpecialDays wstop:topic=\"true\">"
				// tns1:Configuration/SpecialDays/Changed
				"<Changed wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"SpecialDaysToken\" Type=\"pt:ReferenceToken\" />"
				"</tt:Source>"
				"</tt:MessageDescription>"
				"</Changed>"

				// tns1:Configuration/SpecialDays/Removed
				"<Removed wstop:topic=\"true\">"
				"<tt:MessageDescription IsProperty=\"false\">"
				"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"SpecialDaysToken\" Type=\"pt:ReferenceToken\" />"
				"</tt:Source>"
				"</tt:MessageDescription>"
				"</Removed>"
			"</SpecialDays>"
		"</tns1:Configuration>"
	);

	return offset;
}

#endif // end of SCHEDULE_SUPPORT

#ifdef RECEIVER_SUPPORT

int build_Receiver_EventProperties_xml(char * p_buf, int mlen)
{
	int offset = snprintf(p_buf, mlen, 
		"<tns1:Receiver wstop:topic=\"true\">"
			// tns1:Receiver/ChangeState
			"<ChangeState wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"false\">"
			"<tt:Source>"
			    "<tt:SimpleItemDescription Name=\"ReceiverToken\" Type=\"tt:ReferenceToken\" />"
			"</tt:Source>"
			"<tt:Data>"
			    "<tt:SimpleItemDescription Name=\"NewState\" Type=\"tt:ReceiverState\" />"
			    "<tt:SimpleItemDescription Name=\"MediaUri\" Type=\"tt:MediaUri\" />"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</ChangeState>"

			// tns1:Receiver/ConnectionFailed
			"<ConnectionFailed wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"false\">"
			"<tt:Source>"
			    "<tt:SimpleItemDescription Name=\"ReceiverToken\" Type=\"tt:ReferenceToken\" />"
			"</tt:Source>"
			"<tt:Data>"
			    "<tt:SimpleItemDescription Name=\"MediaUri\" Type=\"tt:MediaUri\" />"
			"</tt:Data>"
			"</tt:MessageDescription>"					
			"</ConnectionFailed>"
		"</tns1:Receiver>"
	);

	return offset;
}

#endif // end of RECEIVER_SUPPORT

#ifdef THERMAL_SUPPORT

int build_Thermal_EventProperties_xml(char * p_buf, int mlen)
{
	int offset = snprintf(p_buf, mlen, 
		"<tns1:VideoSource wstop:topic=\"true\">"
			// tns1:VideoSource/RadiometryAlarm
			"<RadiometryAlarm wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"false\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"VideoSource\" Type=\"tt:ReferenceToken\" />"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:SimpleItemDescription Name=\"State\" Type=\"xs:boolean\" />"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</RadiometryAlarm>"
			"<MotionAlarm  wstop:topic=\"true\">"
			"<tt:MessageDescription IsProperty=\"true\">"
			"<tt:Source>"
				"<tt:SimpleItemDescription Name=\"Source\" Type=\"tt:ReferenceToken\"/>"
			"</tt:Source>"
			"<tt:Data>"
				"<tt:SimpleItemDescription Name=\"State\" Type=\"xs:boolean\"/>"
			"</tt:Data>"
			"</tt:MessageDescription>"
			"</MotionAlarm>"
		"</tns1:VideoSource>"
		//"<tns1:VideoSource>"
			
		//"</tns1:VideoSource>"
	);

	return offset;
}

#endif // end of THERMAL_SUPPORT

int build_ONVIF_EXPAND_xml(char * p_buf, int mlen)
{
	int offset = snprintf(p_buf, mlen, 
				"<tns1:UserAlarm>"
					"<tns1:IVA>"
						"<tns1:HumanShapeDetect wstop:topic=\"true\">"
							"<tt:MessageDescription IsProperty=\"false\">"
								"<tt:Source>"
									"<tt:SimpleItemDescription Name=\"VideoSourceConfigurationToken\" Type=\"tt:ReferenceToken\"/>"
								"</tt:Source>"
								"<tt:Data>"
									"<tt:SimpleItemDescription Name=\"State\" Type=\"xsd:boolean\"/>"
								"</tt:Data>"
							"</tt:MessageDescription>"
						"</tns1:HumanShapeDetect>"
					"</tns1:IVA>"
				"</tns1:UserAlarm>"
	);
	return offset;
}
/***************************************************************************************/

int build_alarm_SimpleItem_xml(char * p_buf, int mlen, onvif_SimpleItem * p_res, char *p_alarm_level)
{
	int offset = 0;
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:SimpleItem Name=\"%s\" Value=\"%s\" Level=\"%s\" />\r\n", p_res->Name, p_res->Value, p_alarm_level);

	return offset;
}

int build_SimpleItem_xml(char * p_buf, int mlen, onvif_SimpleItem * p_res)
{
	int offset = 0;
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:SimpleItem Name=\"%s\" Value=\"%s\" />\r\n", p_res->Name, p_res->Value);

	return offset;
}

int build_ElementItem_xml(char * p_buf, int mlen, onvif_ElementItem * p_res)
{
	int offset = 0;

	if (p_res->AnyFlag)
	{
	    offset += snprintf(p_buf+offset, mlen-offset, 
	        "<tt:ElementItem Name=\"%s\">%s</tt:ElementItem>", 
	        p_res->Name, p_res->Any);
	}
	else
	{
	    offset += snprintf(p_buf+offset, mlen-offset, 
	        "<tt:ElementItem Name=\"%s\" />", 
	        p_res->Name);
	}

	return offset;
}

int build_Message_xml(char * p_buf, int mlen, onvif_Message * p_res)
{
	int offset = 0;
	char utctime[64];
	SimpleItemList * p_simpleitem;
	ElementItemList * p_elementitem;
	struct tm *gtime;
	
	gtime = gmtime(&p_res->UtcTime);
	
	snprintf(utctime, sizeof(utctime), 
		"%04d-%02d-%02dT%02d:%02d:%02dZ", 
		gtime->tm_year+1900, 
		gtime->tm_mon+1, 
		gtime->tm_mday, 
		gtime->tm_hour, 
		gtime->tm_min, 
		gtime->tm_sec);
		
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Message UtcTime=\"%s\"", utctime);

	if (p_res->PropertyOperationFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, " PropertyOperation=\"%s\"", onvif_PropertyOperationToString(p_res->PropertyOperation));
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, ">");

	if (p_res->SourceFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Source>");

		p_simpleitem = p_res->Source.SimpleItem;
		while (p_simpleitem)
		{
			offset += build_SimpleItem_xml(p_buf+offset, mlen-offset, &p_simpleitem->SimpleItem);
			p_simpleitem = p_simpleitem->next;
		}
		
		p_elementitem = p_res->Source.ElementItem;
		while (p_elementitem)
		{
			offset += build_ElementItem_xml(p_buf+offset, mlen-offset, &p_elementitem->ElementItem);
			p_elementitem = p_elementitem->next;
		}
		
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Source>");
	}

	if (p_res->KeyFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Key>");

		p_simpleitem = p_res->Key.SimpleItem;
		while (p_simpleitem)
		{
			offset += build_SimpleItem_xml(p_buf+offset, mlen-offset, &p_simpleitem->SimpleItem);
			p_simpleitem = p_simpleitem->next;
		}

		p_elementitem = p_res->Key.ElementItem;
		while (p_elementitem)
		{
			offset += build_ElementItem_xml(p_buf+offset, mlen-offset, &p_elementitem->ElementItem);
			p_elementitem = p_elementitem->next;
		}
		
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Key>");
	}

	if (p_res->DataFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Data>");

		p_simpleitem = p_res->Data.SimpleItem;
		while (p_simpleitem)
		{
			offset += build_SimpleItem_xml(p_buf+offset, mlen-offset, &p_simpleitem->SimpleItem);
			p_simpleitem = p_simpleitem->next;
		}

		p_elementitem = p_res->Data.ElementItem;
		while (p_elementitem)
		{
			offset += build_ElementItem_xml(p_buf+offset, mlen-offset, &p_elementitem->ElementItem);
			p_elementitem = p_elementitem->next;
		}
		
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Data>");
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:Message>");
	
	return offset;
}

int build_alarm_notify_message_xml(char * p_buf, int mlen, onvif_Message * p_res, char *p_alarm_level)
{
	int offset = 0;
	char utctime[64];
	SimpleItemList * p_simpleitem = NULL;
	ElementItemList * p_elementitem = NULL;
	struct tm *gtime;
	
	gtime = gmtime(&p_res->UtcTime);
	
	snprintf(utctime, sizeof(utctime), 
		"%04d-%02d-%02dT%02d:%02d:%02dZ", 
		gtime->tm_year+1900, 
		gtime->tm_mon+1, 
		gtime->tm_mday, 
		gtime->tm_hour, 
		gtime->tm_min, 
		gtime->tm_sec);
		
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Message UtcTime=\"%s\"", utctime);

	if (p_res->PropertyOperationFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, " PropertyOperation=\"%s\"", onvif_PropertyOperationToString(p_res->PropertyOperation));
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, ">");

	if (p_res->SourceFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Source>");

		p_simpleitem = p_res->Source.SimpleItem;
		while (p_simpleitem)
		{
			offset += build_SimpleItem_xml(p_buf+offset, mlen-offset, &p_simpleitem->SimpleItem);
			p_simpleitem = p_simpleitem->next;
		}
		
		p_elementitem = p_res->Source.ElementItem;
		while (p_elementitem)
		{
			offset += build_ElementItem_xml(p_buf+offset, mlen-offset, &p_elementitem->ElementItem);
			p_elementitem = p_elementitem->next;
		}
		
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Source>");
	}

	if (p_res->KeyFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Key>");

		p_simpleitem = p_res->Key.SimpleItem;
		while (p_simpleitem)
		{
			offset += build_SimpleItem_xml(p_buf+offset, mlen-offset, &p_simpleitem->SimpleItem);
			p_simpleitem = p_simpleitem->next;
		}

		p_elementitem = p_res->Key.ElementItem;
		while (p_elementitem)
		{
			offset += build_ElementItem_xml(p_buf+offset, mlen-offset, &p_elementitem->ElementItem);
			p_elementitem = p_elementitem->next;
		}
		
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Key>");
	}

	if (p_res->DataFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Data>");

		p_simpleitem = p_res->Data.SimpleItem;
		while (p_simpleitem)
		{
			offset += build_alarm_SimpleItem_xml(p_buf+offset, mlen-offset, &p_simpleitem->SimpleItem, p_alarm_level);
			p_simpleitem = p_simpleitem->next;
		}

		p_elementitem = p_res->Data.ElementItem;
		while (p_elementitem)
		{
			offset += build_ElementItem_xml(p_buf+offset, mlen-offset, &p_elementitem->ElementItem);
			p_elementitem = p_elementitem->next;
		}
		
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Data>");
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:Message>");
	
	return offset;
}


int build_NotificationMessage_xml(char * p_buf, int mlen, onvif_NotificationMessage * p_res)
{
	int offset = 0;
		
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<wsnt:Topic Dialect=\"%s\">%s</wsnt:Topic>",
		p_res->Dialect,
		p_res->Topic);

	offset += snprintf(p_buf+offset, mlen-offset, "<wsnt:Message>");

    if (p_res->AlarmDataLevel != NULL)
    {
        offset += build_alarm_notify_message_xml(p_buf+offset, mlen-offset, &p_res->Message, p_res->AlarmDataLevel);
    }
	else
	{
	    offset += build_Message_xml(p_buf+offset, mlen-offset, &p_res->Message);
	}
	offset += snprintf(p_buf+offset, mlen-offset, "</wsnt:Message>");

	return offset;
}

int build_Notify_xml(char * p_buf, int mlen, const char * argv)
{
	NotificationMessageList * p_message = (NotificationMessageList *)argv;
	NotificationMessageList * p_tmp = p_message;
	
	int offset = snprintf(p_buf, mlen, "%s", xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, "%s", onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_head, "http://docs.oasis-open.org/wsn/bw-2/NotificationConsumer/Notify");
	offset += snprintf(p_buf+offset, mlen-offset, "%s", soap_body);
		
	offset += snprintf(p_buf+offset, mlen-offset, "<wsnt:Notify>");
	while (p_tmp)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<wsnt:NotificationMessage>");   
		offset += build_NotificationMessage_xml(p_buf+offset, mlen-offset, &p_tmp->NotificationMessage);
		offset += snprintf(p_buf+offset, mlen-offset, "</wsnt:NotificationMessage>");
		
		p_tmp = p_tmp->next;
	}			
	offset += snprintf(p_buf+offset, mlen-offset, "</wsnt:Notify>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "%s", soap_tailer);

	return offset;
}

int build_tev_GetEventProperties_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tev:GetEventPropertiesResponse>"
			"<tev:TopicNamespaceLocation>"
				"http://www.onvif.org/onvif/ver10/topics/topicns.xml"
			"</tev:TopicNamespaceLocation>"
			"<wsnt:FixedTopicSet>true</wsnt:FixedTopicSet>"
			"<wstop:TopicSet xmlns=\"\">");

#ifdef PROFILE_Q_SUPPORT
	offset += build_Base_EventProperties_xml(p_buf+offset, mlen-offset);
#endif

	offset += build_Imaging_EventProperties_xml(p_buf+offset, mlen-offset);

#ifdef MEDIA_SUPPORT
	offset += build_Media_EventProperties_xml(p_buf+offset, mlen-offset);
#endif

#ifdef MEDIA2_SUPPORT
	offset += build_Media2_EventProperties_xml(p_buf+offset, mlen-offset);
#endif

#ifdef PTZ_SUPPORT
	offset += build_PTZ_EventProperties_xml(p_buf+offset, mlen-offset);
#endif

#ifdef VIDEO_ANALYTICS
	offset += build_Analytics_EventProperties_xml(p_buf+offset, mlen-offset);            
#endif

#ifdef DEVICEIO_SUPPORT
	offset += build_DeviceIO_EventProperties_xml(p_buf+offset, mlen-offset);
#endif

#ifdef PROFILE_G_SUPPORT
	offset += build_ProfileG_EventProperties_xml(p_buf+offset, mlen-offset);                
#endif

#ifdef PROFILE_C_SUPPORT
	offset += build_ProfileC_EventProperties_xml(p_buf+offset, mlen-offset);                
#endif

#ifdef CREDENTIAL_SUPPORT
	offset += build_Credential_EventProperties_xml(p_buf+offset, mlen-offset);  
#endif

#ifdef ACCESS_RULES
	offset += build_AccessRules_EventProperties_xml(p_buf+offset, mlen-offset);
#endif

#ifdef SCHEDULE_SUPPORT
	offset += build_Schedule_EventProperties_xml(p_buf+offset, mlen-offset);
#endif

#ifdef RECEIVER_SUPPORT
	offset += build_Receiver_EventProperties_xml(p_buf+offset, mlen-offset);
#endif

#ifdef THERMAL_SUPPORT
	if (g_onvif_cfg.Capabilities.thermal.Radiometry)
	{
		offset += build_Thermal_EventProperties_xml(p_buf+offset, mlen-offset);
	}
#endif
	if (g_onvif_expand)
	{
		offset += build_ONVIF_EXPAND_xml(p_buf+offset, mlen-offset);
	}
	offset += snprintf(p_buf+offset, mlen-offset,
			"</wstop:TopicSet>"	

			"<wsnt:TopicExpressionDialect>"
				"http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet"
			"</wsnt:TopicExpressionDialect>"
			"<wsnt:TopicExpressionDialect>"
				"http://docs.oasis-open.org/wsnt/t-1/TopicExpression/ConcreteSet"
			"</wsnt:TopicExpressionDialect>"
			"<wsnt:TopicExpressionDialect>"
				"http://docs.oasis-open.org/wsn/t-1/TopicExpression/Concrete"
			"</wsnt:TopicExpressionDialect>"	
			"<tev:MessageContentFilterDialect>"
				"http://www.onvif.org/ver10/tev/messageContentFilter/ItemFilter"
			"</tev:MessageContentFilterDialect>"
			"<tev:MessageContentSchemaLocation>"
				"http://www.onvif.org/onvif/ver10/schema/onvif.xsd"
			"</tev:MessageContentSchemaLocation>"
		"</tev:GetEventPropertiesResponse>");

	return offset;
}

int build_tev_Subscribe_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	EUA * p_eua = (EUA *) argv;
	char cur_time[100], term_time[100];
	
	if (NULL == p_eua)
	{
		return -1;
	}

	onvif_get_time_str(cur_time, sizeof(cur_time), 0);	
	
	if (g_onvif_cfg.evt_renew_time < p_eua->init_term_time)
	{
		onvif_get_time_str(term_time, sizeof(term_time), g_onvif_cfg.evt_renew_time);
	}
	else
	{
		onvif_get_time_str(term_time, sizeof(term_time), p_eua->init_term_time);
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<wsnt:SubscribeResponse>"
			"<wsnt:SubscriptionReference>"
				"<wsa:Address>%s</wsa:Address>"
			"</wsnt:SubscriptionReference>"
			"<wsnt:CurrentTime>%s</wsnt:CurrentTime>"
			"<wsnt:TerminationTime>%s</wsnt:TerminationTime>"
		"</wsnt:SubscribeResponse>",
		p_eua->producter_addr, 
		cur_time, 
		term_time);

	return offset;
}

int build_tev_Unsubscribe_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<wsnt:UnsubscribeResponse />");
	return offset;
}

int build_tev_Renew_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	char cur_time[100], term_time[100];

	onvif_get_time_str(cur_time, sizeof(cur_time), 0);
	onvif_get_time_str(term_time, sizeof(term_time), g_onvif_cfg.evt_renew_time);
		
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<wsnt:RenewResponse>"
			"<wsnt:TerminationTime>%s</wsnt:TerminationTime>"
			"<wsnt:CurrentTime>%s</wsnt:CurrentTime>"
		"</wsnt:RenewResponse>",
		term_time, 
		cur_time);

	return offset;
}

int build_tev_CreatePullPointSubscription_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	char cur_time[100], term_time[100];
	EUA * p_eua = (EUA *) argv;

	onvif_get_time_str(cur_time, sizeof(cur_time), 0);

	if (g_onvif_cfg.evt_renew_time < p_eua->init_term_time)
	{
		onvif_get_time_str(term_time, sizeof(term_time), g_onvif_cfg.evt_renew_time);
	}
	else
	{
		onvif_get_time_str(term_time, sizeof(term_time), p_eua->init_term_time);
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tev:CreatePullPointSubscriptionResponse>"	
			"<tev:SubscriptionReference>"
				"<wsa:Address>%s</wsa:Address>"
			"</tev:SubscriptionReference>"	        
			"<wsnt:CurrentTime>%s</wsnt:CurrentTime>"
			"<wsnt:TerminationTime>%s</wsnt:TerminationTime>"
		"</tev:CreatePullPointSubscriptionResponse>", 
		p_eua->producter_addr,
		cur_time, 
		term_time);

	return offset;
}

int build_tev_PullMessages_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0, msg_nums = 0;
	char cur_time[100], term_time[100];
	LINKED_NODE *p_node, *p_next;
	NotificationMessageList * p_tmp;
	NotificationMessageList * p_message;
	
	tev_PullMessages_REQ * p_res = (tev_PullMessages_REQ *)argv;
	EUA * p_eua = onvif_get_eua_by_index(p_res->eua_idx);

	onvif_get_time_str(cur_time, sizeof(cur_time), 0);
	onvif_get_time_str(term_time, sizeof(term_time), g_onvif_cfg.evt_renew_time);

	offset += snprintf(p_buf+offset, mlen-offset, "<tev:PullMessagesResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tev:CurrentTime>%s</tev:CurrentTime>"
		"<tev:TerminationTime>%s</tev:TerminationTime>",
		cur_time, term_time);

	// get notify message from message list

	if (p_eua)
	{
		if (0)//test
		{
			NotificationMessageList *p_message;
			p_message = onvif_init_NotificationMessage_Motion(TRUE);
			if (p_message)
			{
				onvif_put_NotificationMessage(p_message);
			}
		}

		p_node = h_list_lookback_start(p_eua->msg_list);
		while (p_node && msg_nums < p_res->MessageLimit)
		{
			p_message = (NotificationMessageList *) p_node->p_data;
			
			p_tmp = p_message;
			while (p_tmp)
			{
				if (onvif_event_filter(p_tmp, p_eua))
				{
					offset += snprintf(p_buf + offset, mlen - offset, "<wsnt:NotificationMessage>");
					offset += build_NotificationMessage_xml(p_buf + offset, mlen - offset, &p_tmp->NotificationMessage);
					offset += snprintf(p_buf+offset, mlen-offset, "</wsnt:NotificationMessage>");

					msg_nums ++;
				}
				else
				{
					log_print(HT_LOG_INFO, "onvif_event_filter failed\n");
				}
				p_tmp = p_tmp->next;
			}

			p_next = h_list_lookback_next(p_eua->msg_list, p_node);

			h_list_remove(p_eua->msg_list, p_node);
			onvif_free_NotificationMessage(p_message);

			p_node = p_next;
		}
		h_list_lookback_end(p_eua->msg_list);
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tev:PullMessagesResponse>");

	return offset;
}

int build_tev_SetSynchronizationPoint_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;		
	offset += snprintf(p_buf+offset, mlen-offset, "<tev:SetSynchronizationPointResponse />");
	return offset;
}

/***************************************************************************************/

#ifdef IMAGE_SUPPORT

int build_ImageSettings_xml(char * p_buf, int mlen, onvif_ImagingSettings * p_res)
{
	int offset = 0;

	if (p_res->BacklightCompensationFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:BacklightCompensation>\r\n");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>%s</tt:Mode>\r\n", onvif_BacklightCompensationModeToString(p_res->BacklightCompensation.Mode));

		if (p_res->BacklightCompensation.LevelFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Level>%0.1f</tt:Level>\r\n", p_res->BacklightCompensation.Level);
		}
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:BacklightCompensation>\r\n");
	}

	if (p_res->BrightnessFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Brightness>%0.1f</tt:Brightness>\r\n", p_res->Brightness);
	}
	if (p_res->ColorSaturationFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:ColorSaturation>%0.1f</tt:ColorSaturation>\r\n", p_res->ColorSaturation);
	}
	if (p_res->ContrastFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Contrast>%0.1f</tt:Contrast>\r\n", p_res->Contrast);
	}

	if (p_res->ExposureFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Exposure>\r\n");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>%s</tt:Mode>\r\n", onvif_ExposureModeToString(p_res->Exposure.Mode));

		if (p_res->Exposure.PriorityFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Priority>%s</tt:Priority>\r\n", onvif_ExposurePriorityToString(p_res->Exposure.Priority));
		}

		/*offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:Window "
				"bottom=\"%0.1f\" "
				"top=\"%0.1f\" "
				"right=\"%0.1f\" "
				"left=\"%0.1f\">"
			"</tt:Window>\r\n",
			p_res->Exposure.Window.bottom, 
			p_res->Exposure.Window.top,
			p_res->Exposure.Window.right, 
			p_res->Exposure.Window.left);*/
		
		if (p_res->Exposure.MinExposureTimeFlag)
		{
			if (g_onvif_expand)
				offset += snprintf(p_buf+offset, mlen-offset, "<tt:MinExposureTime>%0.1f</tt:MinExposureTime>\r\n", 125.0);
			else
				offset += snprintf(p_buf+offset, mlen-offset, "<tt:MinExposureTime>%0.1f</tt:MinExposureTime>\r\n", p_res->Exposure.MinExposureTime);
		}
		if (p_res->Exposure.MaxExposureTimeFlag)
		{
			if (g_onvif_expand)
				offset += snprintf(p_buf+offset, mlen-offset, "<tt:MaxExposureTime>%0.1f</tt:MaxExposureTime>\r\n", 20000.0);
			else
				offset += snprintf(p_buf+offset, mlen-offset, "<tt:MaxExposureTime>%0.1f</tt:MaxExposureTime>\r\n", p_res->Exposure.MaxExposureTime);
		}
		if (p_res->Exposure.MinGainFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:MinGain>%0.1f</tt:MinGain>\r\n", p_res->Exposure.MinGain);
		}
		if (p_res->Exposure.MaxGainFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:MaxGain>%0.1f</tt:MaxGain>\r\n", p_res->Exposure.MaxGain);
		}
		if (p_res->Exposure.MinIrisFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:MinIris>%0.1f</tt:MinIris>\r\n", p_res->Exposure.MinIris);
		}
		if (p_res->Exposure.MaxIrisFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:MaxIris>%0.1f</tt:MaxIris>\r\n", p_res->Exposure.MaxIris);
		}
		if (p_res->Exposure.ExposureTimeFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:ExposureTime>%0.1f</tt:ExposureTime>\r\n", p_res->Exposure.ExposureTime);
		}
		if (p_res->Exposure.GainFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Gain>%0.1f</tt:Gain>\r\n", p_res->Exposure.Gain);
		}
		if (p_res->Exposure.IrisFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Iris>%0.1f</tt:Iris>\r\n", p_res->Exposure.Iris);
		}
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Exposure>\r\n");			
	}

	if (p_res->FocusFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Focus>\r\n");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:AutoFocusMode>%s</tt:AutoFocusMode>\r\n", onvif_AutoFocusModeToString(p_res->Focus.AutoFocusMode));

		if (p_res->Focus.DefaultSpeedFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:DefaultSpeed>%0.1f</tt:DefaultSpeed>\r\n", p_res->Focus.DefaultSpeed);
		}
		if (p_res->Focus.NearLimitFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:NearLimit>%0.1f</tt:NearLimit>\r\n", p_res->Focus.NearLimit);
		}
		if (p_res->Focus.FarLimitFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:FarLimit>%0.1f</tt:FarLimit>\r\n", p_res->Focus.FarLimit);
		}
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Focus>\r\n");
	}

	if (p_res->IrCutFilterFlag)
	{
		VideoCaptureCfg videoCfg;
		memcpy(&videoCfg, &((MediaConfig *)getMediaConfig())->videoConfig[0].videoCapture, sizeof(videoCfg));
		VideoCaptureCfg *capCfg = &videoCfg;
		if (g_onvif_expand && (capCfg->ircut_mode == IRCUT_Mode_Manual &&  capCfg->ircut_keepcolor == 0))
		{
			if ((access("/tmp/g_onvif_expand_ircut_off", F_OK) == 0) && (access("/tmp/g_onvif_expand_ircut_on", F_OK) != 0))//����
				offset += snprintf(p_buf+offset, mlen-offset, "<tt:IrCutFilter>%s</tt:IrCutFilter>\r\n", onvif_IrCutFilterModeToString(IrCutFilterMode_OFF));
			else if((access("/tmp/g_onvif_expand_ircut_off", F_OK) != 0) && (access("/tmp/g_onvif_expand_ircut_on", F_OK) == 0))
				offset += snprintf(p_buf+offset, mlen-offset, "<tt:IrCutFilter>%s</tt:IrCutFilter>\r\n", onvif_IrCutFilterModeToString(IrCutFilterMode_ON));
			else if((access("/tmp/ircut_night_mode", F_OK) == 0))
				offset += snprintf(p_buf+offset, mlen-offset, "<tt:IrCutFilter>%s</tt:IrCutFilter>\r\n", onvif_IrCutFilterModeToString(IrCutFilterMode_OFF));
			else if((access("/tmp/ircut_night_mode", F_OK) != 0))
				offset += snprintf(p_buf+offset, mlen-offset, "<tt:IrCutFilter>%s</tt:IrCutFilter>\r\n", onvif_IrCutFilterModeToString(IrCutFilterMode_ON));
		}
		else
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:IrCutFilter>%s</tt:IrCutFilter>\r\n", onvif_IrCutFilterModeToString(p_res->IrCutFilter));
	}

	if (p_res->SharpnessFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Sharpness>%0.1f</tt:Sharpness>\r\n", p_res->Sharpness);
	}

	if (p_res->WideDynamicRangeFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:WideDynamicRange>\r\n");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>%s</tt:Mode>\r\n", onvif_WideDynamicModeToString(p_res->WideDynamicRange.Mode));

		if (p_res->WideDynamicRange.LevelFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Level>%0.1f</tt:Level>\r\n", p_res->WideDynamicRange.Level);
		}
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:WideDynamicRange>\r\n");	
	}

	if (p_res->WhiteBalanceFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:WhiteBalance>\r\n");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>%s</tt:Mode>\r\n", onvif_WhiteBalanceModeToString(p_res->WhiteBalance.Mode));

		if (p_res->WhiteBalance.CrGainFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:CrGain>%0.1f</tt:CrGain>\r\n", p_res->WhiteBalance.CrGain);
		}
		if (p_res->WhiteBalance.CbGainFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:CbGain>%0.1f</tt:CbGain>\r\n", p_res->WhiteBalance.CbGain);
		}
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:WhiteBalance>\r\n");	
	}

	if (p_res->ExtensionFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Extension>\r\n");
		if (p_res->Extension.Extension202Flag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Extension>\r\n");
			if (p_res->Extension.Extension.Extension203Flag)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tt:Extension>\r\n");
				if (p_res->Extension.Extension.Extension.DefoggingFlag)
				{
					offset += snprintf(p_buf+offset, mlen-offset, "<tt:Defogging>\r\n");
						offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>%s</tt:Mode>\r\n", p_res->Extension.Extension.Extension.Defogging.Mode);
						offset += snprintf(p_buf+offset, mlen-offset, "<tt:Level>%f</tt:Level>\r\n", p_res->Extension.Extension.Extension.Defogging.Level);
					offset += snprintf(p_buf+offset, mlen-offset, "</tt:Defogging>\r\n");
				}
				if (p_res->Extension.Extension.Extension.NoiseReductionFlag)
				{
					offset += snprintf(p_buf+offset, mlen-offset, "<tt:NoiseReduction>\r\n");
						offset += snprintf(p_buf+offset, mlen-offset, "<tt:Level>%f</tt:Level>\r\n", p_res->Extension.Extension.Extension.NoiseReduction.Level);
					offset += snprintf(p_buf+offset, mlen-offset, "</tt:NoiseReduction>\r\n");
				}
				offset += snprintf(p_buf+offset, mlen-offset, "</tt:Extension>\r\n");
			}
			offset += snprintf(p_buf+offset, mlen-offset, "</tt:Extension>\r\n");
		}
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Extension>\r\n");
	}
	return offset;
}

int build_ImageOptions_xml(char * p_buf, int mlen, onvif_ImagingOptions * p_res)
{
	int offset = 0;

	if (p_res->BacklightCompensationFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:BacklightCompensation>\r\n");
		if (p_res->BacklightCompensation.Mode_OFF)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>OFF</tt:Mode>\r\n");
		}
		if (p_res->BacklightCompensation.Mode_ON)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>ON</tt:Mode>\r\n");
		}
		if (p_res->BacklightCompensation.LevelFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:Level>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:Level>\r\n",
				p_res->BacklightCompensation.Level.Min,
				p_res->BacklightCompensation.Level.Max);
		}
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:BacklightCompensation>\r\n");
	}

	if (p_res->BrightnessFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:Brightness>\r\n"
				"<tt:Min>%0.1f</tt:Min>\r\n"
				"<tt:Max>%0.1f</tt:Max>\r\n"
			"</tt:Brightness>\r\n",
			p_res->Brightness.Min, 
			p_res->Brightness.Max);
	}

	if (p_res->ColorSaturationFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:ColorSaturation>\r\n"
				"<tt:Min>%0.1f</tt:Min>\r\n"
				"<tt:Max>%0.1f</tt:Max>\r\n"
			"</tt:ColorSaturation>\r\n",
			p_res->ColorSaturation.Min, 
			p_res->ColorSaturation.Max);
	}

	if (p_res->ContrastFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:Contrast>\r\n"
				"<tt:Min>%0.1f</tt:Min>\r\n"
				"<tt:Max>%0.1f</tt:Max>\r\n"
			"</tt:Contrast>\r\n",
			p_res->Contrast.Min, 
			p_res->Contrast.Max);
	}

	if (p_res->ExposureFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Exposure>\r\n");
		if (p_res->Exposure.Mode_AUTO)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>AUTO</tt:Mode>\r\n");
		}
		if (p_res->Exposure.Mode_MANUAL)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>MANUAL</tt:Mode>\r\n");
		}
		if (p_res->Exposure.Priority_LowNoise)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Priority>LowNoise</tt:Priority>\r\n");
		}
		if (p_res->Exposure.Priority_FrameRate)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Priority>FrameRate</tt:Priority>\r\n");
		}
		if (p_res->Exposure.MinExposureTimeFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:MinExposureTime>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:MinExposureTime>\r\n",
				p_res->Exposure.MinExposureTime.Min, 
				p_res->Exposure.MinExposureTime.Max);
		}
		if (p_res->Exposure.MaxExposureTimeFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:MaxExposureTime>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:MaxExposureTime>\r\n",
				p_res->Exposure.MaxExposureTime.Min, 
				p_res->Exposure.MaxExposureTime.Max);	
		}	
		if (p_res->Exposure.MinGainFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:MinGain>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:MinGain>\r\n",
				p_res->Exposure.MinGain.Min, 
				p_res->Exposure.MinGain.Max);
		}	
		if (p_res->Exposure.MaxGainFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:MaxGain>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:MaxGain>\r\n",
				p_res->Exposure.MaxGain.Min, 
				p_res->Exposure.MaxGain.Max);
		}
		if (p_res->Exposure.MinIrisFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:MinIris>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:MinIris>\r\n",
				p_res->Exposure.MinIris.Min, 
				p_res->Exposure.MinIris.Max);
		}
		if (p_res->Exposure.MaxIrisFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:MaxIris>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:MaxIris>\r\n",
				p_res->Exposure.MaxIris.Min, 
				p_res->Exposure.MaxIris.Max);	
		}	
		if (p_res->Exposure.ExposureTimeFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:ExposureTime>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:ExposureTime>\r\n",
				p_res->Exposure.ExposureTime.Min, 
				p_res->Exposure.ExposureTime.Max);	
		}
		if (p_res->Exposure.GainFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:Gain>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:Gain>\r\n",
				p_res->Exposure.Gain.Min, 
				p_res->Exposure.Gain.Max);	
		}	
		if (p_res->Exposure.IrisFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:Iris>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:Iris>\r\n",
				p_res->Exposure.Iris.Min, 
				p_res->Exposure.Iris.Max);	
		}	
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Exposure>\r\n");
	}	

	if (p_res->FocusFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Focus>\r\n");
		if (p_res->Focus.AutoFocusModes_AUTO)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:AutoFocusModes>AUTO</tt:AutoFocusModes>\r\n");
		}
		if (p_res->Focus.AutoFocusModes_MANUAL)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:AutoFocusModes>MANUAL</tt:AutoFocusModes>\r\n");
		}
		if (p_res->Focus.DefaultSpeedFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:DefaultSpeed>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:DefaultSpeed>\r\n",
				p_res->Focus.DefaultSpeed.Min, 
				p_res->Focus.DefaultSpeed.Max);
		}	
		if (p_res->Focus.NearLimitFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:NearLimit>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:NearLimit>\r\n",
				p_res->Focus.NearLimit.Min, 
				p_res->Focus.NearLimit.Max);
		}	
		if (p_res->Focus.FarLimitFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:FarLimit>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:FarLimit>\r\n",
				p_res->Focus.FarLimit.Min, 
				p_res->Focus.FarLimit.Max);
		}	
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Focus>\r\n");
	}	
	
	if (p_res->IrCutFilterMode_ON)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:IrCutFilterModes>ON</tt:IrCutFilterModes>\r\n");
	}
	if (p_res->IrCutFilterMode_OFF)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:IrCutFilterModes>OFF</tt:IrCutFilterModes>\r\n");
	}
	if (p_res->IrCutFilterMode_AUTO)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:IrCutFilterModes>AUTO</tt:IrCutFilterModes>\r\n");
	}

	if (p_res->SharpnessFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:Sharpness>\r\n"
				"<tt:Min>%0.1f</tt:Min>\r\n"
				"<tt:Max>%0.1f</tt:Max>\r\n"
			"</tt:Sharpness>\r\n",
			p_res->Sharpness.Min, 
			p_res->Sharpness.Max);
	}	

	if (p_res->WideDynamicRangeFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:WideDynamicRange>\r\n");
		if (p_res->WideDynamicRange.Mode_ON)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>ON</tt:Mode>\r\n");
		}
		if (p_res->WideDynamicRange.Mode_OFF)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>OFF</tt:Mode>\r\n");
		}
		if (p_res->WideDynamicRange.LevelFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:Level>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:Level>\r\n",
				p_res->WideDynamicRange.Level.Min, 
				p_res->WideDynamicRange.Level.Max);
		}	
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:WideDynamicRange>\r\n");
	}

	if (p_res->WhiteBalanceFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:WhiteBalance>\r\n");
		if (p_res->WhiteBalance.Mode_AUTO)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>AUTO</tt:Mode>\r\n");
		}
		if (p_res->WhiteBalance.Mode_MANUAL)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>MANUAL</tt:Mode>\r\n");
		}
		if (p_res->WhiteBalance.YrGainFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:YrGain>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:YrGain>\r\n",
				p_res->WhiteBalance.YrGain.Min, 
				p_res->WhiteBalance.YrGain.Max);
		}	
		if (p_res->WhiteBalance.YbGainFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:YbGain>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:YbGain>\r\n",
				p_res->WhiteBalance.YbGain.Min, 
				p_res->WhiteBalance.YbGain.Max);
		}	
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:WhiteBalance>\r\n");
	}

	if (p_res->ExtensionFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Extension>\r\n");
		if (p_res->Extension.Extension202Flag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Extension>\r\n");
			if (p_res->Extension.Extension.Extension203Flag)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tt:Extension>\r\n");
				if (p_res->Extension.Extension.Extension.DefoggingOptionsFlag)
				{
					offset += snprintf(p_buf+offset, mlen-offset, "<tt:DefoggingOptions>\r\n");
					int i;
					for (i = 0; i < p_res->Extension.Extension.Extension.DefoggingOptions.__sizeMode ; i++)
					{
						offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>%s</tt:Mode>\r\n", p_res->Extension.Extension.Extension.DefoggingOptions.Mode[i]);
					}
					if (p_res->Extension.Extension.Extension.DefoggingOptions.Level)
						offset += snprintf(p_buf+offset, mlen-offset, " <tt:Level>true</tt:Level>\r\n");
					else
						offset += snprintf(p_buf+offset, mlen-offset, " <tt:Level>false</tt:Level>\r\n");
					offset += snprintf(p_buf+offset, mlen-offset, "</tt:DefoggingOptions>\r\n");
				}
				if (p_res->Extension.Extension.Extension.NoiseReductionOptionsFlag)
				{
					offset += snprintf(p_buf+offset, mlen-offset, "<tt:NoiseReductionOptions>\r\n");
						if (p_res->Extension.Extension.Extension.DefoggingOptions.Level)
							offset += snprintf(p_buf+offset, mlen-offset, " <tt:Level>true</tt:Level>\r\n");
						else
							offset += snprintf(p_buf+offset, mlen-offset, " <tt:Level>false</tt:Level>\r\n");
					offset += snprintf(p_buf+offset, mlen-offset, "</tt:NoiseReductionOptions>\r\n");
				}
				offset += snprintf(p_buf+offset, mlen-offset, "</tt:Extension>\r\n");
			}
			offset += snprintf(p_buf+offset, mlen-offset, "</tt:Extension>\r\n");
		}
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Extension>\r\n");
	}
	return offset;
}

int build_ImagePreset_xml(char * p_buf, int mlen, onvif_ImagingPreset * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<timg:Preset token=\"%s\" type=\"%s\">\r\n"
			"<timg:Name>%s</timg:Name>\r\n"
		"</timg:Preset>\r\n",
		p_res->token, 
		p_res->type, 
		p_res->Name);

	return offset;
}

int build_img_GetImagingSettings_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	img_GetImagingSettings_REQ * p_res = (img_GetImagingSettings_REQ *)argv;
	VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_res->VideoSourceToken);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NoSource;
	}
	onvif_refresh_ImagingSettings(&p_v_src->ImagingSettings);
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<timg:GetImagingSettingsResponse>"
		"<timg:ImagingSettings>");
	offset += build_ImageSettings_xml(p_buf+offset, mlen-offset, &p_v_src->ImagingSettings);
	offset += snprintf(p_buf+offset, mlen-offset, 
		"</timg:ImagingSettings>"
		"</timg:GetImagingSettingsResponse>");

	return offset;
}

int build_img_GetOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	img_GetOptions_REQ * p_res = (img_GetOptions_REQ *)argv;
	VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_res->VideoSourceToken);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NoSource;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<timg:GetOptionsResponse>"
		"<timg:ImagingOptions>");
	offset += build_ImageOptions_xml(p_buf+offset, mlen-offset, &p_v_src->ImagingOptions);
	offset += snprintf(p_buf+offset, mlen-offset, 
		"</timg:ImagingOptions>"
		"</timg:GetOptionsResponse>");

	return offset;
}

int build_img_SetImagingSettings_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<timg:SetImagingSettingsResponse />");
	return offset;
}

int build_img_GetMoveOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{	
	int offset = 0;
	img_GetMoveOptions_RES * p_res = (img_GetMoveOptions_RES *)argv;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<timg:GetMoveOptionsResponse>"
		"<timg:MoveOptions>");

	if (p_res->MoveOptions.AbsoluteFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Absolute>");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Position>");
		offset += build_FloatRange_xml(p_buf+offset, mlen-offset, &p_res->MoveOptions.Absolute.Position);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Position>");
		if (p_res->MoveOptions.Absolute.SpeedFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Speed>");
			offset += build_FloatRange_xml(p_buf+offset, mlen-offset, &p_res->MoveOptions.Absolute.Speed);
			offset += snprintf(p_buf+offset, mlen-offset, "</tt:Speed>");
		}
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Absolute>");
	}
	
	if (p_res->MoveOptions.RelativeFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Relative>");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Distance>");
		offset += build_FloatRange_xml(p_buf+offset, mlen-offset, &p_res->MoveOptions.Relative.Distance);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Distance>");
		if (p_res->MoveOptions.Absolute.SpeedFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Speed>");
			offset += build_FloatRange_xml(p_buf+offset, mlen-offset, &p_res->MoveOptions.Relative.Speed);
			offset += snprintf(p_buf+offset, mlen-offset, "</tt:Speed>");
		}
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Relative>");
	}
	
	if (p_res->MoveOptions.ContinuousFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:Continuous>"
			"<tt:Speed>");
		offset += build_FloatRange_xml(p_buf+offset, mlen-offset, &p_res->MoveOptions.Continuous.Speed);
		offset += snprintf(p_buf+offset, mlen-offset, 
			"</tt:Speed>"
			"</tt:Continuous>");
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"</timg:MoveOptions>"
		"</timg:GetMoveOptionsResponse>");
	
	return offset;
}

int build_img_Move_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<timg:MoveResponse />");
	return offset;
}

int build_img_GetStatus_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	img_GetStatus_RES * p_res = (img_GetStatus_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<timg:GetStatusResponse>");
	
	if (p_res->Status.FocusStatusFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<timg:Status>"
			"<tt:FocusStatus20>");

		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:Position>%0.1f</tt:Position>"
			"<tt:MoveStatus>%s</tt:MoveStatus>",
			p_res->Status.FocusStatus.Position,
			onvif_MoveStatusToString(p_res->Status.FocusStatus.MoveStatus));
			
		if (p_res->Status.FocusStatus.ErrorFlag && p_res->Status.FocusStatus.Error[0] != '\0')
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Error>%s</tt:Error>", p_res->Status.FocusStatus.Error);
		}
		
		offset += snprintf(p_buf+offset, mlen-offset, 
			"</tt:FocusStatus20>"
			"</timg:Status>");
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</timg:GetStatusResponse>");	
	
	return offset;
}

int build_img_Stop_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<timg:StopResponse />");
	return offset;
}

int build_img_GetPresets_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	img_GetPresets_REQ * p_res = (img_GetPresets_REQ *)argv;
	ImagingPresetList * p_preset;
	VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_res->VideoSourceToken);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NoVideoSource;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<timg:GetPresetsResponse>");

	p_preset = p_v_src->Presets;
	while (p_preset)
	{
		build_ImagePreset_xml(p_buf+offset, mlen-offset, &p_preset->Preset);

		p_preset = p_preset->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</timg:GetPresetsResponse>");

	return offset;
}

int build_img_GetCurrentPreset_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	img_GetCurrentPreset_REQ * p_res = (img_GetCurrentPreset_REQ *)argv;
	ImagingPresetList * p_preset;
	VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_res->VideoSourceToken);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NoVideoSource;
	}

	p_preset = onvif_find_ImagingPreset(p_v_src->Presets, p_v_src->CurrentPresetToken);

	offset += snprintf(p_buf+offset, mlen-offset, "<timg:GetCurrentPresetResponse>");

	if (p_preset)
	{
		build_ImagePreset_xml(p_buf+offset, mlen-offset, &p_preset->Preset);
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</timg:GetCurrentPresetResponse>");

	return offset;
}

int build_img_SetCurrentPreset_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<timg:SetCurrentPresetResponse />");
	return offset;
}

#endif // IMAGE_SUPPORT

/***************************************************************************************/

#if defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)

int build_MulticastConfiguration_xml(char * p_buf, int mlen, onvif_MulticastConfiguration * p_res)
{
	int offset = 0;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Multicast>\r\n"
			"<tt:Address>\r\n"
				"<tt:Type>IPv4</tt:Type>\r\n"
				"<tt:IPv4Address>%s</tt:IPv4Address>\r\n"
			"</tt:Address>\r\n"
			"<tt:Port>%d</tt:Port>\r\n"
			"<tt:TTL>%d</tt:TTL>\r\n"
			"<tt:AutoStart>%s</tt:AutoStart>\r\n"
		"</tt:Multicast>\r\n", 
		p_res->IPv4Address,
		p_res->Port,
		p_res->TTL,
		p_res->AutoStart ? "true" : "false");

	return offset;	    
}

int build_OSDConfiguration_xml(char * p_buf, int mlen, onvif_OSDConfiguration * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:VideoSourceConfigurationToken>%s</tt:VideoSourceConfigurationToken>\r\n", p_res->VideoSourceConfigurationToken);
		
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Type>%s</tt:Type>\r\n", onvif_OSDTypeToString(p_res->Type));

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Position>\r\n"
		"<tt:Type>%s</tt:Type>\r\n", 
		onvif_OSDPosTypeToString(p_res->Position.Type));

	if (p_res->Position.PosFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Pos x=\"%0.2f\" y=\"%0.2f\"></tt:Pos>\r\n", p_res->Position.Pos.x, p_res->Position.Pos.y);
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:Position>\r\n");

	if (p_res->Type == OSDType_Text)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:TextString>\r\n");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Type>%s</tt:Type>\r\n", onvif_OSDTextTypeToString(p_res->TextString.Type));

		if (p_res->TextString.Type == OSDTextType_Date)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:DateFormat>%s</tt:DateFormat>\r\n", p_res->TextString.DateFormat);
		}
		else if (p_res->TextString.Type == OSDTextType_Time)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:TimeFormat>%s</tt:TimeFormat>\r\n", p_res->TextString.TimeFormat);
		}
		else if (p_res->TextString.Type == OSDTextType_DateAndTime)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:DateFormat>%s</tt:DateFormat>\r\n", p_res->TextString.DateFormat);
			
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:TimeFormat>%s</tt:TimeFormat>\r\n", p_res->TextString.TimeFormat);
		}

		if (p_res->TextString.FontSizeFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:FontSize>%d</tt:FontSize>\r\n", p_res->TextString.FontSize);
		}

		if (p_res->TextString.FontColorFlag)
		{
			if (p_res->TextString.FontColor.TransparentFlag)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tt:FontColor Transparent=\"%d\">\r\n", p_res->TextString.FontColor.Transparent);
			}	
			else
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tt:FontColor>\r\n");
			}

			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Color X=\"%0.1f\" Y=\"%0.1f\" Z=\"%0.1f\"></tt:Color>\r\n", 
				p_res->TextString.FontColor.X, p_res->TextString.FontColor.Y, p_res->TextString.FontColor.Z);

			offset += snprintf(p_buf+offset, mlen-offset, "</tt:FontColor>\r\n");	
		}

		if (p_res->TextString.BackgroundColorFlag)
		{
			if (p_res->TextString.BackgroundColor.TransparentFlag)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tt:BackgroundColor Transparent=\"%d\">\r\n", p_res->TextString.BackgroundColor.Transparent);
			}	
			else
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tt:BackgroundColor>");
			}

			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Color X=\"%0.1f\" Y=\"%0.1f\" Z=\"%0.1f\"></tt:Color>", 
				p_res->TextString.BackgroundColor.X, 
				p_res->TextString.BackgroundColor.Y, 
				p_res->TextString.BackgroundColor.Z);

			offset += snprintf(p_buf+offset, mlen-offset, "</tt:BackgroundColor>\r\n");	
		}

		if (p_res->TextString.Type == OSDTextType_Plain)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:PlainText>%s</tt:PlainText>\r\n", p_res->TextString.PlainText);
		}

		offset += snprintf(p_buf+offset, mlen-offset, "</tt:TextString>\r\n");
	}
	else if (p_res->Type == OSDType_Image)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Image><tt:ImgPath>%s</tt:ImgPath></tt:Image>\r\n", p_res->Image.ImgPath);
	}	
	
	return offset;
}

int build_VideoSourceMode_xml(char * p_buf, int mlen, onvif_VideoSourceMode * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<trt:MaxFramerate>%0.1f</trt:MaxFramerate>\r\n"
		"<trt:MaxResolution>\r\n"
			"<tt:Width>%d</tt:Width>\r\n"
			"<tt:Height>%d</tt:Height>\r\n"
		"</trt:MaxResolution>\r\n"
		"<trt:Encodings>%s</trt:Encodings>\r\n"
		"<trt:Reboot>%s</trt:Reboot>\r\n",
		p_res->MaxFramerate,
		p_res->MaxResolution.Width,
		p_res->MaxResolution.Height,
		p_res->Encodings,
		p_res->Reboot ? "true" : "false");

	if (p_res->DescriptionFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:Description>%s</trt:Description>\r\n", p_res->Description);
	}
	
	return offset;
}

int build_VideoSourceConfiguration_xml_by_profile(char * p_buf, int mlen, ONVIF_PROFILE * p_profile)
{
	log_print(HT_LOG_INFO, "p_profile->token:%s\n", p_profile->token);
	
	int offset = 0;
	int width, height, iIndex = 0;

	if (g_stereo)
	{
		if (strstr(p_profile->token, "ProfileToken_1_1") || strstr(p_profile->token, "ProfileToken_2_1"))
			iIndex = 0;
		else if (strstr(p_profile->token, "ProfileToken_1_2") || strstr(p_profile->token, "ProfileToken_2_2"))
			iIndex = 1;
		else if (strstr(p_profile->token, "ProfileToken_1_3") || strstr(p_profile->token, "ProfileToken_2_3"))
			iIndex = 2;
	}
	else
	{
		if (strstr(p_profile->token, "ProfileToken_1"))
			iIndex = 0;
		else if (strstr(p_profile->token, "ProfileToken_2"))
			iIndex = 1;
		else if (strstr(p_profile->token, "ProfileToken_3"))
			iIndex = 2;
	}
	
	memcpy(&gMediaCfg, getMediaConfig(), sizeof(gMediaCfg));
	OnvifGetVideoSize(gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex].resolution.name, gMediaCfg.videoConfig[0].videoCapture.tvsystem, &width, &height);//???????????????

	offset += snprintf(p_buf+offset, mlen-offset,
		"<tt:Name>%s</tt:Name>\r\n"
		"<tt:UseCount>%d</tt:UseCount>\r\n"
		"<tt:SourceToken>%s</tt:SourceToken>\r\n"
		"<tt:Bounds x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" />\r\n", 
		p_profile->v_src_cfg->Configuration.Name, 
		p_profile->v_src_cfg->Configuration.UseCount, 
		p_profile->v_src_cfg->Configuration.SourceToken, 
		0, 
		0, 
		width, 
		height
		);

	return offset;
}

int build_VideoSourceConfiguration_xml(char * p_buf, int mlen, onvif_VideoSourceConfiguration * p_res)
{
	int offset = 0;
	log_print(HT_LOG_INFO, "p_res->token:%s\n", p_res->token);
	log_print(HT_LOG_INFO, "p_res->SourceToken:%s\n", p_res->SourceToken);
	int width, height, iIndex = 0;
	if (1)
	{
		memcpy(&gMediaCfg, getMediaConfig(), sizeof(gMediaCfg));
		iIndex = 0;
		OnvifGetVideoSize(gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex].resolution.name, gMediaCfg.videoConfig[0].videoCapture.tvsystem, &width, &height);//???????????????
	}
		
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Name>%s</tt:Name>\r\n"
		"<tt:UseCount>%d</tt:UseCount>\r\n"
		"<tt:SourceToken>%s</tt:SourceToken>\r\n"
		"<tt:Bounds x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" />\r\n",
		p_res->Name, 
		p_res->UseCount, 
		p_res->SourceToken,
		//p_res->Bounds.x,
		//p_res->Bounds.y,
		//p_res->Bounds.width, 
		//p_res->Bounds.height
		0,
		0,
		width, 
		height
		);

	return offset;
}

int build_RotateOptions_xml(char * p_buf, int mlen, onvif_RotateOptions * p_res)
{
	uint32 i;
	int offset = 0;

	if (p_res->RotateMode_ON)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>ON</tt:Mode>");
	}

	if (p_res->RotateMode_OFF)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>OFF</tt:Mode>");
	}

	if (p_res->RotateMode_AUTO)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>AUTO</tt:Mode>");
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:DegreeList>");

	for (i = 0; i < p_res->sizeDegreeList; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Items>%d</tt:Items>", p_res->DegreeList[i]);
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tt:DegreeList>");

	return offset;
}

int build_VideoSourceConfigurationOptionsExtension_xml(char * p_buf, int mlen, onvif_VideoSourceConfigurationOptionsExtension * p_res)
{
	int offset = 0;

	if (p_res->RotateFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Rotate Reboot=\"%s\">", p_res->Rotate.Reboot ? "true" : "false");

		offset += build_RotateOptions_xml(p_buf+offset, mlen-offset, &p_res->Rotate);

		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Rotate>");
	}

	return offset;
}

int build_VideoSourceConfigurationOptions_xml(char * p_buf, int mlen, onvif_VideoSourceConfigurationOptions * p_res)
{
	uint32 i;
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:BoundsRange>"
			"<tt:XRange>"
				"<tt:Min>%d</tt:Min>"
				"<tt:Max>%d</tt:Max>"
			"</tt:XRange>"
			"<tt:YRange>"
				"<tt:Min>%d</tt:Min>"
				"<tt:Max>%d</tt:Max>"
			"</tt:YRange>"
			"<tt:WidthRange>"
				"<tt:Min>%d</tt:Min>"
				"<tt:Max>%d</tt:Max>"
			"</tt:WidthRange>"
			"<tt:HeightRange>"
				"<tt:Min>%d</tt:Min>"
				"<tt:Max>%d</tt:Max>"
			"</tt:HeightRange>"
		"</tt:BoundsRange>", 
		p_res->BoundsRange.XRange.Min, 
		p_res->BoundsRange.XRange.Max,
		p_res->BoundsRange.YRange.Min, 
		p_res->BoundsRange.YRange.Max,
		p_res->BoundsRange.WidthRange.Min, 
		p_res->BoundsRange.WidthRange.Max,
		p_res->BoundsRange.HeightRange.Min, 
		p_res->BoundsRange.HeightRange.Max);

	for (i = 0; i < p_res->sizeVideoSourceTokensAvailable; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:VideoSourceTokensAvailable>%s</tt:VideoSourceTokensAvailable>", p_res->VideoSourceTokensAvailable[i]);
	}

	if (p_res->ExtensionFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Extension>");
		offset += build_VideoSourceConfigurationOptionsExtension_xml(p_buf+offset, mlen-offset, &p_res->Extension);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Extension>");
	}

	return offset;
}

int build_VideoEncoder2Configuration_xml(char * p_buf, int mlen, onvif_VideoEncoder2Configuration * p_res)
{
	int width, height, iIndex = 0;
	memcpy(&gMediaCfg, getMediaConfig(), sizeof(gMediaCfg));
	if (g_stereo)
	{
		if(strstr(p_res->token, "VideoEncoderConfigurationToken_1_1") != NULL || strstr(p_res->token, "VideoEncoderConfigurationToken_2_1") != NULL)
			iIndex = 0;
		else if(strstr(p_res->token, "VideoEncoderConfigurationToken_1_2") != NULL || strstr(p_res->token, "VideoEncoderConfigurationToken_2_2") != NULL)
			iIndex = 1;
		else if(strstr(p_res->token, "VideoEncoderConfigurationToken_1_3") != NULL || strstr(p_res->token, "VideoEncoderConfigurationToken_2_3") != NULL)
			iIndex = 2;
	}
	else
	{
		if(strstr(p_res->token, "VideoEncoderConfigurationToken_1") != NULL)
			iIndex = 0;
		else if(strstr(p_res->token, "VideoEncoderConfigurationToken_2") != NULL)
			iIndex = 1;
		else if(strstr(p_res->token, "VideoEncoderConfigurationToken_3") != NULL)
			iIndex = 2;
	}
	OnvifGetVideoSize(gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex].resolution.name, gMediaCfg.videoConfig[0].videoCapture.tvsystem, &width, &height);//???????????????
	
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Name>%s</tt:Name>\r\n"
		"<tt:UseCount>%d</tt:UseCount>\r\n"
		"<tt:Encoding>%s</tt:Encoding>\r\n"
		"<tt:Resolution>\r\n"
			"<tt:Width>%d</tt:Width>\r\n"
			"<tt:Height>%d</tt:Height>\r\n"
		"</tt:Resolution>\r\n",
		p_res->Name, 
		p_res->UseCount, 
		onvif_Video2EncodingToString(onvif_StringToVideoEncoding(gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex].encodeFormat.name)), 
		width, 
		height);
		//p_res->Encoding, 
		//p_res->Resolution.Width, 
		//p_res->Resolution.Height);

	if (p_res->RateControlFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:RateControl");
		if (p_res->RateControl.ConstantBitRateFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " ConstantBitRate=\"%s\"", p_res->RateControl.ConstantBitRate ? "true" : "false");
		}
		offset += snprintf(p_buf+offset, mlen-offset, ">\r\n");

		offset += snprintf(p_buf+offset, mlen-offset,  
				"<tt:FrameRateLimit>%0.1f</tt:FrameRateLimit>\r\n"
				"<tt:BitrateLimit>%d</tt:BitrateLimit>\r\n"
			"</tt:RateControl>\r\n",
			p_res->RateControl.FrameRateLimit,
			p_res->RateControl.BitrateLimit);
	}

	if (p_res->MulticastFlag)
	{
		offset += build_MulticastConfiguration_xml(p_buf+offset, mlen-offset, &p_res->Multicast);
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Quality>%0.2f</tt:Quality>\r\n", p_res->Quality);

	return offset;  
}

int build_MetadataConfiguration_xml(char * p_buf, int mlen, onvif_MetadataConfiguration * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset,
		"<tt:Name>%s</tt:Name>\r\n"
		"<tt:UseCount>%d</tt:UseCount>\r\n",
		p_res->Name,
		p_res->UseCount);

	if (p_res->PTZStatusFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset,
			"<tt:PTZStatus>\r\n"
				"<tt:Status>%s</tt:Status>\r\n"
				"<tt:Position>%s</tt:Position>\r\n"
			"</tt:PTZStatus>\r\n",
			p_res->PTZStatus.Status ? "true" : "false",
			p_res->PTZStatus.Position ? "true" : "false");
	}

	if (p_res->EventsFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset,
			"<tt:Events>\r\n"
				"<tt:Filter>\r\n"
					"<wsnt:TopicExpression Dialect=\"%s\">%s</wsnt:TopicExpression>\r\n"
				"</tt:Filter>\r\n"
			"</tt:Events>\r\n",
			p_res->Events.Dialect,
			p_res->Events.TopicExpression);
	}

	if (p_res->AnalyticsFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset,
			"<tt:Analytics>%s</tt:Analytics>\r\n",
			p_res->Analytics ? "true" : "false");
	}

	offset += build_MulticastConfiguration_xml(p_buf+offset, mlen-offset, &p_res->Multicast);

	offset += snprintf(p_buf+offset, mlen-offset,
			"<tt:SessionTimeout>PT%dS</tt:SessionTimeout>\r\n",
			p_res->SessionTimeout);

#ifdef VIDEO_ANALYTICS
	if (p_res->AnalyticsEngineConfigurationFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:AnalyticsEngineConfiguration>");
		offset += build_AnalyticsEngineConfiguration_xml(p_buf+offset, mlen-offset, &p_res->AnalyticsEngineConfiguration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:AnalyticsEngineConfiguration>");
	}
#endif

	return offset;
}

int build_PTZStatusFilterOptions_xml(char * p_buf, int mlen, onvif_PTZStatusFilterOptions * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:PanTiltStatusSupported>%s</tt:PanTiltStatusSupported>"
		"<tt:ZoomStatusSupported>%s</tt:ZoomStatusSupported>"
		"<tt:PanTiltPositionSupported>%s</tt:PanTiltPositionSupported>"
		"<tt:ZoomPositionSupported>%s</tt:ZoomPositionSupported>",
		p_res->PanTiltStatusSupported ? "true" : "false",
		p_res->ZoomStatusSupported ? "true" : "false",
		p_res->PanTiltPositionSupported ? "true" : "false",
		p_res->ZoomPositionSupported ? "true" : "false");

	return offset;		
}

int build_MetadataConfigurationOptionsExtension_xml(char * p_buf, int mlen, onvif_MetadataConfigurationOptionsExtension * p_res)
{
	uint32 i;
	int offset = 0;

	for (i = 0; i < p_res->sizeCompressionType; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:CompressionType>%s</tt:CompressionType>", p_res->CompressionType[i]);
	}

	return offset;
}

int build_MetadataConfigurationOptions_xml(char * p_buf, int mlen, onvif_MetadataConfigurationOptions * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:PTZStatusFilterOptions>");
	offset += build_PTZStatusFilterOptions_xml(p_buf+offset, mlen-offset, &p_res->PTZStatusFilterOptions);
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:PTZStatusFilterOptions>");

	if (p_res->ExtensionFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Extension>");
		offset += build_MetadataConfigurationOptionsExtension_xml(p_buf+offset, mlen-offset, &p_res->Extension);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Extension>");
	}

	return offset;
}

#ifdef AUDIO_SUPPORT

int build_AudioSource_xml(char * p_buf, int mlen, onvif_AudioSource * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<trt:AudioSources token=\"%s\">\r\n"
			"<tt:Channels>%d</tt:Channels>\r\n"
		"</trt:AudioSources>\r\n", 
		p_res->token,
		p_res->Channels);

	return offset;
}

int build_AudioSourceConfiguration_xml(char * p_buf, int mlen, onvif_AudioSourceConfiguration * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Name>%s</tt:Name>\r\n"
		"<tt:UseCount>%d</tt:UseCount>\r\n"
		"<tt:SourceToken>%s</tt:SourceToken>\r\n", 
		p_res->Name, 
		p_res->UseCount, 
		p_res->SourceToken);

	return offset;
}

int build_AudioEncoderConfiguration_xml(char * p_buf, int mlen, onvif_AudioEncoder2Configuration * p_res)
{
	int offset = 0;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Name>%s</tt:Name>"
		"<tt:UseCount>%d</tt:UseCount>"
		"<tt:Encoding>%s</tt:Encoding>"
		"<tt:Bitrate>%d</tt:Bitrate>"
		"<tt:SampleRate>%d</tt:SampleRate>", 
		p_res->Name, 
		p_res->UseCount, 
		onvif_AudioEncodingToString(p_res->AudioEncoding), 
		p_res->Bitrate, 
		p_res->SampleRate); 

	offset += build_MulticastConfiguration_xml(p_buf+offset, mlen-offset, &p_res->Multicast);

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:SessionTimeout>PT%dS</tt:SessionTimeout>", p_res->SessionTimeout);
		
	return offset;
}

int build_AudioDecoderConfiguration_xml(char * p_buf, int mlen, onvif_AudioDecoderConfiguration * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Name>%s</tt:Name>\r\n"
		"<tt:UseCount>%d</tt:UseCount>\r\n",
		p_res->Name,
		p_res->UseCount);

	return offset;
}

int build_AudioSourceConfigurationOptions_xml(char * p_buf, int mlen)
{
	int offset = 0;	
	AudioSourceList * p_a_src = g_onvif_cfg.a_src;
	
	while (p_a_src)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:InputTokensAvailable>%s</tt:InputTokensAvailable>", p_a_src->AudioSource.token);
		
		p_a_src = p_a_src->next;
	}
	
	return offset;
}

int build_AudioEncoder2Configuration_xml(char * p_buf, int mlen, onvif_AudioEncoder2Configuration * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Name>%s</tt:Name>\r\n"
		"<tt:UseCount>%d</tt:UseCount>\r\n"
		"<tt:Encoding>%s</tt:Encoding>\r\n"
		"<tt:Bitrate>%d</tt:Bitrate>\r\n"
		"<tt:SampleRate>%d</tt:SampleRate>\r\n", 
		p_res->Name, 
		p_res->UseCount, 
		onvif_AudioEncodingToString(p_res->AudioEncoding), 
		p_res->Bitrate, 
		p_res->SampleRate);

	if (p_res->MulticastFlag)
	{
		offset += build_MulticastConfiguration_xml(p_buf+offset, mlen-offset, &p_res->Multicast);
	}
		
	return offset;
}

#endif // AUDIO_SUPPORT

#endif // defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)

/***************************************************************************************/

#ifdef MEDIA_SUPPORT

int build_VideoEncoderConfiguration_xml(char * p_buf, int mlen, onvif_VideoEncoder2Configuration * p_res)
{
	int width, height, iIndex = 0;
	memcpy(&gMediaCfg, getMediaConfig(), sizeof(gMediaCfg));
	if (g_stereo)
	{
		if(strstr(p_res->token, "VideoEncoderConfigurationToken_1_1") != NULL || strstr(p_res->token, "VideoEncoderConfigurationToken_2_1") != NULL)
			iIndex = 0;
		else if(strstr(p_res->token, "VideoEncoderConfigurationToken_1_2") != NULL || strstr(p_res->token, "VideoEncoderConfigurationToken_2_2") != NULL)
			iIndex = 1;
		else if(strstr(p_res->token, "VideoEncoderConfigurationToken_1_3") != NULL || strstr(p_res->token, "VideoEncoderConfigurationToken_2_3") != NULL)
			iIndex = 2;
	}
	else
	{
		if(strstr(p_res->token, "VideoEncoderConfigurationToken_1") != NULL)
			iIndex = 0;
		else if(strstr(p_res->token, "VideoEncoderConfigurationToken_2") != NULL)
			iIndex = 1;
		else if(strstr(p_res->token, "VideoEncoderConfigurationToken_3") != NULL)
			iIndex = 2;
	}
	OnvifGetVideoSize(gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex].resolution.name, gMediaCfg.videoConfig[0].videoCapture.tvsystem, &width, &height);//???????????????
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Name>%s</tt:Name>\r\n"
		"<tt:UseCount>%d</tt:UseCount>\r\n"
		"<tt:Encoding>%s</tt:Encoding>\r\n"
		"<tt:Resolution>\r\n"
			"<tt:Width>%d</tt:Width>\r\n"
			"<tt:Height>%d</tt:Height>\r\n"
		"</tt:Resolution>\r\n"
		"<tt:Quality>%d</tt:Quality>\r\n",
		p_res->Name, 
		p_res->UseCount, 
		//onvif_VideoEncodingToString(p_res->VideoEncoding), 
		//p_res->Resolution.Width, 
		//p_res->Resolution.Height, 
		onvif_VideoEncodingToString(onvif_StringToVideoEncoding(gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex].encodeFormat.name)), 
		//gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex].encodeFormat.name,
		width, 
		height,
		(int)p_res->Quality);

	if (p_res->RateControlFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset,
			"<tt:RateControl>\r\n"
				"<tt:FrameRateLimit>%d</tt:FrameRateLimit>\r\n"
				"<tt:EncodingInterval>%d</tt:EncodingInterval>\r\n"
				"<tt:BitrateLimit>%d</tt:BitrateLimit>\r\n"
			"</tt:RateControl>\r\n",
			gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex].display_frameRate,//(int)p_res->RateControl.FrameRateLimit,
			p_res->RateControl.EncodingInterval, 
			gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex].bitRate//p_res->RateControl.BitrateLimit
			);
	}
	
	if (p_res->VideoEncoding == VideoEncoding_H264)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:H264>\r\n"
				"<tt:GovLength>%d</tt:GovLength>\r\n"
				"<tt:H264Profile>%s</tt:H264Profile>\r\n"
			"</tt:H264>\r\n", 
			p_res->GovLength,
			p_res->Profile);
	}
	else if (p_res->VideoEncoding == VideoEncoding_MPEG4)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:MPEG4>\r\n"
				"<tt:GovLength>%d</tt:GovLength>\r\n"
				"<tt:Mpeg4Profile>%s</tt:Mpeg4Profile>\r\n"
			"</tt:MPEG4>\r\n", 
			p_res->GovLength,
			p_res->Profile);
	}
	else
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:H264>\r\n"
				"<tt:GovLength>%d</tt:GovLength>\r\n"
				"<tt:H264Profile>%s</tt:H264Profile>\r\n"
			"</tt:H264>\r\n", 
			p_res->GovLength,
			p_res->Profile);
	}
	
	offset += build_MulticastConfiguration_xml(p_buf+offset, mlen-offset, &p_res->Multicast);
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:SessionTimeout>PT%dS</tt:SessionTimeout>\r\n", p_res->SessionTimeout);

	return offset;    
}

int build_Profile_xml(char * p_buf, int mlen, ONVIF_PROFILE * p_profile)
{
	int offset = 0;
	int extension = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Name>%s</tt:Name>", p_profile->name);

	if (p_profile->v_src_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:VideoSourceConfiguration token=\"%s\">", p_profile->v_src_cfg->Configuration.token);
		offset += build_VideoSourceConfiguration_xml_by_profile(p_buf+offset, mlen-offset, p_profile);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:VideoSourceConfiguration>");
	}

#ifdef AUDIO_SUPPORT
	if (p_profile->a_src_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:AudioSourceConfiguration token=\"%s\">", p_profile->a_src_cfg->Configuration.token);
		offset += build_AudioSourceConfiguration_xml(p_buf+offset, mlen-offset, &p_profile->a_src_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:AudioSourceConfiguration>");
	}
#endif

	if (p_profile->v_enc_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:VideoEncoderConfiguration token=\"%s\">", p_profile->v_enc_cfg->Configuration.token);
		offset += build_VideoEncoderConfiguration_xml(p_buf+offset, mlen-offset, &p_profile->v_enc_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:VideoEncoderConfiguration>");
	}

#ifdef AUDIO_SUPPORT
	if (p_profile->a_enc_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:AudioEncoderConfiguration token=\"%s\">", p_profile->a_enc_cfg->Configuration.token);
		offset += build_AudioEncoderConfiguration_xml(p_buf+offset, mlen-offset, &p_profile->a_enc_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:AudioEncoderConfiguration>");
	}
#endif

#ifdef VIDEO_ANALYTICS
	if (p_profile->va_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:VideoAnalyticsConfiguration token=\"%s\">", p_profile->va_cfg->Configuration.token);
		offset += build_VideoAnalyticsConfiguration_xml(p_buf+offset, mlen-offset, &p_profile->va_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:VideoAnalyticsConfiguration>");
	}
#endif

#ifdef PTZ_SUPPORT
	if (p_profile->ptz_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:PTZConfiguration token=\"%s\" "
				"MoveRamp=\"%d\" "
				"PresetRamp=\"%d\" "
				"PresetTourRamp=\"%d\">", 
			p_profile->ptz_cfg->Configuration.token, 
			p_profile->ptz_cfg->Configuration.MoveRamp,
			p_profile->ptz_cfg->Configuration.PresetRamp, 
			p_profile->ptz_cfg->Configuration.PresetTourRamp);
		offset += build_PTZConfiguration_xml(p_buf+offset, mlen-offset, &p_profile->ptz_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:PTZConfiguration>");
	}
#endif

	if (p_profile->metadata_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:MetadataConfiguration token=\"%s\" "
				"CompressionType=\"%s\" "
				"GeoLocation=\"%s\" "
				"ShapePolygon=\"%s\">", 
			p_profile->metadata_cfg->Configuration.token,
			p_profile->metadata_cfg->Configuration.CompressionType,
			p_profile->metadata_cfg->Configuration.GeoLocation ? "true" : "false",
			p_profile->metadata_cfg->Configuration.ShapePolygon ? "true" : "false");
		offset += build_MetadataConfiguration_xml(p_buf+offset, mlen-offset, &p_profile->metadata_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:MetadataConfiguration>");
	}

#ifdef DEVICEIO_SUPPORT
	if (p_profile->a_output_cfg)
	{
		extension = 1;
	}
#endif

#ifdef AUDIO_SUPPORT
	if (p_profile->a_dec_cfg)
	{
		extension = 1;
	}
#endif

	if (extension)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Extension>");
	}

#ifdef DEVICEIO_SUPPORT
	if (p_profile->a_output_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:AudioOutputConfiguration token=\"%s\">", p_profile->a_output_cfg->Configuration.token);
		offset += build_AudioOutputConfiguration_xml(p_buf+offset, mlen-offset, &p_profile->a_output_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:AudioOutputConfiguration>");
	}
#endif

#ifdef AUDIO_SUPPORT
	if (p_profile->a_dec_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:AudioDecoderConfiguration token=\"%s\">", p_profile->a_dec_cfg->Configuration.token);
		offset += build_AudioDecoderConfiguration_xml(p_buf+offset, mlen-offset, &p_profile->a_dec_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:AudioDecoderConfiguration>");
	}
#endif

	if (extension)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Extension>");
	}

	return offset;
}

int build_JpegOptions_xml(char * p_buf, int mlen, onvif_JpegOptions * p_options)
{
	uint32 i;
	int offset = 0;

	for (i = 0; i < ARRAY_SIZE(p_options->ResolutionsAvailable); i++)
	{
		if (p_options->ResolutionsAvailable[i].Width == 0 || p_options->ResolutionsAvailable[i].Height == 0)
		{
			continue;
		}
		offset += build_VideoResolution_xml(p_buf+offset, mlen-offset, &p_options->ResolutionsAvailable[i]);
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:FrameRateRange>"
			"<tt:Min>%d</tt:Min>"
			"<tt:Max>%d</tt:Max>"
		"</tt:FrameRateRange>"
		"<tt:EncodingIntervalRange>"
			"<tt:Min>%d</tt:Min>"
			"<tt:Max>%d</tt:Max>"
		"</tt:EncodingIntervalRange>",
		p_options->FrameRateRange.Min, 
		p_options->FrameRateRange.Max,
		p_options->EncodingIntervalRange.Min, 
		p_options->EncodingIntervalRange.Max);

	return offset;		
}

int build_Mpeg4Options_xml(char * p_buf, int mlen, onvif_Mpeg4Options * p_options)
{
	uint32 i;
	int offset = 0;

	for (i = 0; i < ARRAY_SIZE(p_options->ResolutionsAvailable); i++)
	{
		if (p_options->ResolutionsAvailable[i].Width == 0 || p_options->ResolutionsAvailable[i].Height == 0)
		{
			continue;
		}
		offset += build_VideoResolution_xml(p_buf+offset, mlen-offset, &p_options->ResolutionsAvailable[i]);
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:GovLengthRange>"
			"<tt:Min>%d</tt:Min>"
			"<tt:Max>%d</tt:Max>"
		"</tt:GovLengthRange>"
		"<tt:FrameRateRange>"
			"<tt:Min>%d</tt:Min>"
			"<tt:Max>%d</tt:Max>"
		"</tt:FrameRateRange>"
		"<tt:EncodingIntervalRange>"
			"<tt:Min>%d</tt:Min>"
			"<tt:Max>%d</tt:Max>"
		"</tt:EncodingIntervalRange>",
		p_options->GovLengthRange.Min, 
		p_options->GovLengthRange.Max, 
		p_options->FrameRateRange.Min, 
		p_options->FrameRateRange.Max,
		p_options->EncodingIntervalRange.Min, 
		p_options->EncodingIntervalRange.Max);

	if (p_options->Mpeg4Profile_SP)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:Mpeg4ProfilesSupported>SP</tt:Mpeg4ProfilesSupported>");
	}
	
	if (p_options->Mpeg4Profile_ASP)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:Mpeg4ProfilesSupported>ASP</tt:Mpeg4ProfilesSupported>");
	}

	return offset;
}

int build_H264Options_xml(char * p_buf, int mlen, onvif_H264Options * p_options)
{
	uint32 i;
	int offset = 0;
	log_print(HT_LOG_INFO, "ARRAY_SIZE(p_options->ResolutionsAvailable):%d\n", ARRAY_SIZE(p_options->ResolutionsAvailable));
	for (i = 0; i < ARRAY_SIZE(p_options->ResolutionsAvailable); i++)
	{
		if (p_options->ResolutionsAvailable[i].Width == 0 || p_options->ResolutionsAvailable[i].Height == 0)
		{
			continue;
		}
		offset += build_VideoResolution_xml(p_buf+offset, mlen-offset, &p_options->ResolutionsAvailable[i]);
	}

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:GovLengthRange>"
			"<tt:Min>%d</tt:Min>"
			"<tt:Max>%d</tt:Max>"
		"</tt:GovLengthRange>"
		"<tt:FrameRateRange>"
			"<tt:Min>%d</tt:Min>"
			"<tt:Max>%d</tt:Max>"
		"</tt:FrameRateRange>"
		"<tt:EncodingIntervalRange>"
			"<tt:Min>%d</tt:Min>"
			"<tt:Max>%d</tt:Max>"
		"</tt:EncodingIntervalRange>",
		p_options->GovLengthRange.Min, 
		p_options->GovLengthRange.Max, 
		p_options->FrameRateRange.Min, 
		p_options->FrameRateRange.Max,
		p_options->EncodingIntervalRange.Min, 
		p_options->EncodingIntervalRange.Max);

	if (p_options->H264Profile_Baseline)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:H264ProfilesSupported>Baseline</tt:H264ProfilesSupported>");
	}
	
	if (p_options->H264Profile_Main)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:H264ProfilesSupported>Main</tt:H264ProfilesSupported>");
	}

	if (p_options->H264Profile_Extended)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:H264ProfilesSupported>Extended</tt:H264ProfilesSupported>");
	}

	if (p_options->H264Profile_High)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:H264ProfilesSupported>High</tt:H264ProfilesSupported>");
	}

	return offset;
}

int build_BitrateRange_xml(char * p_buf, int mlen, onvif_IntRange * p_res)
{
	int offset = 0;

	offset = snprintf(p_buf+offset, mlen-offset, 
		"<tt:BitrateRange>"
			"<tt:Min>%d</tt:Min>"
			"<tt:Max>%d</tt:Max>"
		"</tt:BitrateRange>",
		p_res->Min, p_res->Max);

	return offset;
}

int build_trt_GetProfiles_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	ONVIF_PROFILE * profile = g_onvif_cfg.profiles;

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetProfilesResponse>");
	
	while (profile)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:Profiles token=\"%s\" fixed=\"%s\">", profile->token, profile->fixed ? "true" : "false");

		offset += build_Profile_xml(p_buf+offset, mlen-offset, profile);

		offset += snprintf(p_buf+offset, mlen-offset, "</trt:Profiles>");

		profile = profile->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetProfilesResponse>");

	return offset;
}

int build_trt_GetProfile_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{	
	int offset = 0;
	trt_GetProfile_REQ * p_res = (trt_GetProfile_REQ *) argv;
	ONVIF_PROFILE * profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->ProfileToken);
	if (NULL == profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetProfileResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:Profile token=\"%s\" fixed=\"%s\">", profile->token, profile->fixed ? "true" : "false");

	offset += build_Profile_xml(p_buf+offset, mlen-offset, profile);

	offset += snprintf(p_buf+offset, mlen-offset, "</trt:Profile>");

	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetProfileResponse>");

	return offset;
}

int build_trt_CreateProfile_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	ONVIF_PROFILE * profile = onvif_find_profile(g_onvif_cfg.profiles, argv);
	if (NULL == profile)
	{
		return -1;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:CreateProfileResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:Profile token=\"%s\" fixed=\"%s\">", profile->token, profile->fixed ? "true" : "false");

	offset += build_Profile_xml(p_buf+offset, mlen-offset, profile);

	offset += snprintf(p_buf+offset, mlen-offset, "</trt:Profile>");

	offset += snprintf(p_buf+offset, mlen-offset, "</trt:CreateProfileResponse>");

	return offset;
}

int build_trt_DeleteProfile_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:DeleteProfileResponse />");
	return offset;
}

int build_trt_AddVideoSourceConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:AddVideoSourceConfigurationResponse />");
	return offset;
}

int build_trt_RemoveVideoSourceConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
    offset += snprintf(p_buf+offset, mlen-offset, "<trt:RemoveVideoSourceConfigurationResponse />");
	return offset;
}

int build_trt_AddVideoEncoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:AddVideoEncoderConfigurationResponse />");
	return offset;
}

int build_trt_RemoveVideoEncoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:RemoveVideoEncoderConfigurationResponse />");
	return offset;
}

int build_trt_GetStreamUri_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	trt_GetStreamUri_RES * p_res = (trt_GetStreamUri_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<trt:GetStreamUriResponse>"
			"<trt:MediaUri>"
				"<tt:Uri>%s</tt:Uri>"
				"<tt:InvalidAfterConnect>%s</tt:InvalidAfterConnect>"
				"<tt:InvalidAfterReboot>%s</tt:InvalidAfterReboot>"
				"<tt:Timeout>PT%dS</tt:Timeout>"
			"</trt:MediaUri>"
		"</trt:GetStreamUriResponse>", 
		p_res->MediaUri.Uri,
		p_res->MediaUri.InvalidAfterConnect ? "true" : "false",
		p_res->MediaUri.InvalidAfterReboot ? "true" : "false",
		p_res->MediaUri.Timeout);

	log_print(HT_LOG_INFO, "rtspuri : %s\n", p_res->MediaUri.Uri);

	return offset;
}

int build_trt_GetSnapshotUri_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	trt_GetSnapshotUri_RES * p_res = (trt_GetSnapshotUri_RES *) argv;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<trt:GetSnapshotUriResponse>"
			"<trt:MediaUri>"
				"<tt:Uri>%s</tt:Uri>"
				"<tt:InvalidAfterConnect>%s</tt:InvalidAfterConnect>"
				"<tt:InvalidAfterReboot>%s</tt:InvalidAfterReboot>"
				"<tt:Timeout>PT%dS</tt:Timeout>"
			"</trt:MediaUri>"
		"</trt:GetSnapshotUriResponse>",
		p_res->MediaUri.Uri, 
		p_res->MediaUri.InvalidAfterConnect ? "true" : "false",
		p_res->MediaUri.InvalidAfterReboot ? "true" : "false",
		p_res->MediaUri.Timeout);

	return offset;
}

int build_trt_GetVideoSources_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	VideoSourceList * p_v_src = g_onvif_cfg.v_src;
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetVideoSourcesResponse>");

	while (p_v_src)
	{
		if (g_onvif_expand)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
			"<trt:VideoSources token=\"%s\">"
				"<tt:Framerate>%0.1f</tt:Framerate>"
				"<tt:Resolution>"
					"<tt:Width>%d</tt:Width>"
					"<tt:Height>%d</tt:Height>"
				"</tt:Resolution>", 
			p_v_src->VideoSource.token, 
			p_v_src->VideoSourceMode.MaxFramerate, 
			p_v_src->VideoSourceMode.MaxResolution.Width, 
			p_v_src->VideoSourceMode.MaxResolution.Height); 
		}
		else
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
			"<trt:VideoSources token=\"%s\">"
				"<tt:Framerate>%0.1f</tt:Framerate>"
				"<tt:Resolution>"
					"<tt:Width>%d</tt:Width>"
					"<tt:Height>%d</tt:Height>"
				"</tt:Resolution>", 
			p_v_src->VideoSource.token, 
			p_v_src->VideoSource.Framerate, 
			p_v_src->VideoSource.Resolution.Width, 
			p_v_src->VideoSource.Resolution.Height); 
		}

#ifdef IMAGE_SUPPORT
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Imaging>");
		offset += build_ImageSettings_xml(p_buf+offset, mlen-offset, &p_v_src->ImagingSettings);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Imaging>");
#endif
		offset += snprintf(p_buf+offset, mlen-offset, "</trt:VideoSources>");
		p_v_src = p_v_src->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetVideoSourcesResponse>");
	
	return offset;
}

int build_trt_GetVideoEncoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	VideoEncoder2ConfigurationList * p_v_enc_cfg = onvif_find_VideoEncoder2Configuration(g_onvif_cfg.v_enc_cfg, argv);
	if (NULL == p_v_enc_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetVideoEncoderConfigurationResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configuration token=\"%s\">", p_v_enc_cfg->Configuration.token);
	offset += build_VideoEncoderConfiguration_xml(p_buf+offset, mlen-offset, &p_v_enc_cfg->Configuration);
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configuration>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetVideoEncoderConfigurationResponse>");

	return offset;
}

int build_trt_GetVideoEncoderConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	VideoEncoder2ConfigurationList * p_v_enc_cfg = g_onvif_cfg.v_enc_cfg;

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetVideoEncoderConfigurationsResponse>");

	while (p_v_enc_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configurations token=\"%s\">", p_v_enc_cfg->Configuration.token);
		offset += build_VideoEncoderConfiguration_xml(p_buf + offset, mlen - offset, &p_v_enc_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configurations>");
		
		p_v_enc_cfg = p_v_enc_cfg->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetVideoEncoderConfigurationsResponse>");

	return offset;
}

int build_trt_GetCompatibleVideoEncoderConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	VideoEncoder2ConfigurationList * p_v_enc_cfg;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, argv);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	
	p_v_enc_cfg = g_onvif_cfg.v_enc_cfg;

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetCompatibleVideoEncoderConfigurationsResponse>");

	while (p_v_enc_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configurations token=\"%s\">", p_v_enc_cfg->Configuration.token);
		offset += build_VideoEncoderConfiguration_xml(p_buf+offset, mlen-offset, &p_v_enc_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configurations>");

		p_v_enc_cfg = p_v_enc_cfg->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetCompatibleVideoEncoderConfigurationsResponse>");

	return offset;
}

int build_trt_GetVideoSourceConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	VideoSourceConfigurationList * p_v_src_cfg = g_onvif_cfg.v_src_cfg;

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetVideoSourceConfigurationsResponse>");

	while (p_v_src_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configurations token=\"%s\">", p_v_src_cfg->Configuration.token);
		offset += build_VideoSourceConfiguration_xml(p_buf+offset, mlen-offset, &p_v_src_cfg->Configuration);	
		offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configurations>");

		p_v_src_cfg = p_v_src_cfg->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetVideoSourceConfigurationsResponse>");
	
	return offset;
}

int build_trt_GetVideoSourceConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	VideoSourceConfigurationList * p_v_src_cfg = onvif_find_VideoSourceConfiguration(g_onvif_cfg.v_src_cfg, argv);
	if (NULL == p_v_src_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetVideoSourceConfigurationResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configuration token=\"%s\">", p_v_src_cfg->Configuration.token);
	offset += build_VideoSourceConfiguration_xml(p_buf+offset, mlen-offset, &p_v_src_cfg->Configuration);	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configuration>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetVideoSourceConfigurationResponse>");
	
	return offset;
}

int build_trt_SetVideoSourceConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:SetVideoSourceConfigurationResponse />");	
	return offset;
}

int build_trt_GetVideoSourceConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	VideoSourceConfigurationList * p_v_src_cfg = NULL;
	trt_GetVideoSourceConfigurationOptions_REQ * p_res = (trt_GetVideoSourceConfigurationOptions_REQ *)argv;

	if (p_res->ProfileTokenFlag && p_res->ProfileToken[0] != '\0')
	{
		ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->ProfileToken);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NoProfile;
		}

		p_v_src_cfg = p_profile->v_src_cfg;
	}

	if (p_res->ConfigurationTokenFlag && p_res->ConfigurationToken[0] != '\0')
	{
		p_v_src_cfg = onvif_find_VideoSourceConfiguration(g_onvif_cfg.v_src_cfg, p_res->ConfigurationToken);
		if (NULL == p_v_src_cfg)
		{
			return ONVIF_ERR_NoConfig;
		}
	}

	if (NULL == p_v_src_cfg)
	{
		p_v_src_cfg = g_onvif_cfg.v_src_cfg;
	}
	
	if (NULL == p_v_src_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetVideoSourceConfigurationOptionsResponse>");

	if (p_v_src_cfg->Options.MaximumNumberOfProfilesFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:Options MaximumNumberOfProfiles=\"%d\">", p_v_src_cfg->Options.MaximumNumberOfProfiles);
	}
	else
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:Options>");
	}

	offset += build_VideoSourceConfigurationOptions_xml(p_buf+offset, mlen-offset, &p_v_src_cfg->Options);

	offset += snprintf(p_buf+offset, mlen-offset, "</trt:Options>");
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetVideoSourceConfigurationOptionsResponse>");
	
	return offset;
}

int build_trt_GetCompatibleVideoSourceConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	VideoSourceConfigurationList * p_v_src_cfg;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, argv);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	p_v_src_cfg = g_onvif_cfg.v_src_cfg;
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetCompatibleVideoSourceConfigurationsResponse>");

	while (p_v_src_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configurations token=\"%s\">", p_v_src_cfg->Configuration.token);
		offset += build_VideoSourceConfiguration_xml(p_buf+offset, mlen-offset, &p_v_src_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configurations>");

		p_v_src_cfg = p_v_src_cfg->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetCompatibleVideoSourceConfigurationsResponse>");
	
	return offset;
}

int build_trt_GetVideoEncoderConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	trt_GetVideoEncoderConfigurationOptions_RES * p_res = (trt_GetVideoEncoderConfigurationOptions_RES *) argv;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<trt:GetVideoEncoderConfigurationOptionsResponse>"
		"<trt:Options>");

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:QualityRange>"
			"<tt:Min>%d</tt:Min>"
			"<tt:Max>%d</tt:Max>"
		"</tt:QualityRange>",
		p_res->Options.QualityRange.Min, 
		p_res->Options.QualityRange.Max);

	if (p_res->Options.JPEGFlag)
	{
		// JPEG options	
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:JPEG>");
		offset += build_JpegOptions_xml(p_buf+offset, mlen-offset, &p_res->Options.JPEG);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:JPEG>");
	}

	if (p_res->Options.MPEG4Flag)
	{
		// MPEG4 options
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:MPEG4>");
		offset += build_Mpeg4Options_xml(p_buf+offset, mlen-offset, &p_res->Options.MPEG4);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:MPEG4>");
	}

	if (p_res->Options.H264Flag)
	{
		// H264 options
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:H264>");
		offset += build_H264Options_xml(p_buf+offset, mlen-offset, &p_res->Options.H264);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:H264>");
	}

	if (p_res->Options.ExtensionFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Extension>");

		if (p_res->Options.Extension.JPEGFlag)
		{
			// JPEG options	
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:JPEG>");
			
			offset += build_JpegOptions_xml(p_buf+offset, mlen-offset, &p_res->Options.Extension.JPEG.JpegOptions);
			offset += build_BitrateRange_xml(p_buf+offset, mlen-offset, &p_res->Options.Extension.JPEG.BitrateRange);

			offset += snprintf(p_buf+offset, mlen-offset, "</tt:JPEG>");
		}

		if (p_res->Options.Extension.MPEG4Flag)
		{
			// MPEG4 options
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:MPEG4>");
			
			offset += build_Mpeg4Options_xml(p_buf+offset, mlen-offset, &p_res->Options.Extension.MPEG4.Mpeg4Options);
			offset += build_BitrateRange_xml(p_buf+offset, mlen-offset, &p_res->Options.Extension.MPEG4.BitrateRange);
				
			offset += snprintf(p_buf+offset, mlen-offset, "</tt:MPEG4>");
		}

		if (p_res->Options.Extension.H264Flag)
		{
			// H264 options
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:H264>");
			
			offset += build_H264Options_xml(p_buf+offset, mlen-offset, &p_res->Options.Extension.H264.H264Options);
			offset += build_BitrateRange_xml(p_buf+offset, mlen-offset, &p_res->Options.Extension.H264.BitrateRange);
				
			offset += snprintf(p_buf+offset, mlen-offset, "</tt:H264>");
		}

		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Extension>");
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"</trt:Options>"
		"</trt:GetVideoEncoderConfigurationOptionsResponse>");
	
	return offset;
}

int build_trt_SetVideoEncoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:SetVideoEncoderConfigurationResponse />");		    
	return offset;
}

int build_trt_SetSynchronizationPoint_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:SetSynchronizationPointResponse />");		    
	return offset;
}

int build_trt_GetGuaranteedNumberOfVideoEncoderInstances_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	int jpeg = 1;
	int h264 = 4;
#ifdef MPEG4_SUPPORT	
	int mpeg4= 2;
#endif

	VideoSourceConfigurationList * p_v_src_cfg = onvif_find_VideoSourceConfiguration(g_onvif_cfg.v_src_cfg, argv);
	if (NULL == p_v_src_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<trt:GetGuaranteedNumberOfVideoEncoderInstancesResponse>"
		    "<trt:TotalNumber>%d</trt:TotalNumber>"
		    "<trt:JPEG>%d</trt:JPEG>"
		    "<trt:H264>%d</trt:H264>"
#ifdef MPEG4_SUPPORT		    
		    "<trt:MPEG4>%d</trt:MPEG4>"
#endif		    
		"</trt:GetGuaranteedNumberOfVideoEncoderInstancesResponse>"
		, 2
		, jpeg
		, h264
#ifdef MPEG4_SUPPORT		
		, mpeg4
#endif		
		);	

	return offset;
}

int build_trt_GetOSDs_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	OSDConfigurationList * p_osd;
	trt_GetOSDs_REQ * p_res = (trt_GetOSDs_REQ *) argv;

	if (p_res->ConfigurationTokenFlag)
	{
		VideoSourceConfigurationList * p_v_src = onvif_find_VideoSourceConfiguration(g_onvif_cfg.v_src_cfg, p_res->ConfigurationToken);
		if (NULL == p_v_src)
		{
			return ONVIF_ERR_NoConfig;
		}
	}
	
	p_osd = g_onvif_cfg.OSDs;

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetOSDsResponse>");

	while (p_osd)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:OSDs token=\"%s\">", p_osd->OSD.token);
		offset += build_OSDConfiguration_xml(p_buf+offset, mlen-offset, &p_osd->OSD);
		offset += snprintf(p_buf+offset, mlen-offset, "</trt:OSDs>");

		p_osd = p_osd->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetOSDsResponse>");
	
	return offset;
}

int build_trt_GetOSD_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	trt_GetOSD_REQ * p_res = (trt_GetOSD_REQ *) argv;
	OSDConfigurationList * p_osd = onvif_find_OSDConfiguration(g_onvif_cfg.OSDs, p_res->OSDToken);
	if (NULL == p_osd)
	{
		return ONVIF_ERR_NoConfig;
	}
	
	onvif_renew_OSDConfigurations(p_osd);
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetOSDResponse>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:OSD token=\"%s\">", p_osd->OSD.token);
	offset += build_OSDConfiguration_xml(p_buf+offset, mlen-offset, &p_osd->OSD);
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:OSD>");

	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetOSDResponse>");
	
	return offset;
}

int build_trt_SetOSD_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:SetOSDResponse />");
	return offset;
}

int build_trt_CreateOSD_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<trt:CreateOSDResponse>"
			"<trt:OSDToken>%s</trt:OSDToken>"
		"</trt:CreateOSDResponse>", 
		argv);

	return offset;
}

int build_trt_DeleteOSD_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:DeleteOSDResponse />");
	return offset;
}

int build_trt_GetOSDOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	uint32 i;
	int offset = 0;
	onvif_OSDConfigurationOptions * p_opt = &g_onvif_cfg.OSDConfigurationOptions;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<trt:GetOSDOptionsResponse>"
		"<trt:OSDOptions>");

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:MaximumNumberOfOSDs Total=\"%d\"", p_opt->MaximumNumberOfOSDs.Total);
		
	if (p_opt->MaximumNumberOfOSDs.ImageFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, " Image=\"%d\"", p_opt->MaximumNumberOfOSDs.Image);
	}
	
	if (p_opt->MaximumNumberOfOSDs.PlainTextFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, " PlainText=\"%d\"", p_opt->MaximumNumberOfOSDs.PlainText);
	}
	
	if (p_opt->MaximumNumberOfOSDs.DateFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, " Date=\"%d\"", p_opt->MaximumNumberOfOSDs.Date);
	}
	
	if (p_opt->MaximumNumberOfOSDs.TimeFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, " Time=\"%d\"", p_opt->MaximumNumberOfOSDs.Time);
	}
	
	if (p_opt->MaximumNumberOfOSDs.DateAndTimeFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, " DateAndTime=\"%d\"", p_opt->MaximumNumberOfOSDs.DateAndTime);
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "></tt:MaximumNumberOfOSDs>");

	if (p_opt->OSDType_Text)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Type>%s</tt:Type>", onvif_OSDTypeToString(OSDType_Text));
	}
	
	if (p_opt->OSDType_Image)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Type>%s</tt:Type>", onvif_OSDTypeToString(OSDType_Image));
	}
	
	if (p_opt->OSDType_Extended)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Type>%s</tt:Type>", onvif_OSDTypeToString(OSDType_Extended));
	}

	if (p_opt->OSDPosType_LowerLeft)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:PositionOption>%s</tt:PositionOption>", onvif_OSDPosTypeToString(OSDPosType_LowerLeft));
	}
	
	if (p_opt->OSDPosType_LowerRight)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:PositionOption>%s</tt:PositionOption>", onvif_OSDPosTypeToString(OSDPosType_LowerRight));
	}
	
	if (p_opt->OSDPosType_UpperLeft)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:PositionOption>%s</tt:PositionOption>", onvif_OSDPosTypeToString(OSDPosType_UpperLeft));
	}
	
	if (p_opt->OSDPosType_UpperRight)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:PositionOption>%s</tt:PositionOption>", onvif_OSDPosTypeToString(OSDPosType_UpperRight));
	}
	
	/*if (p_opt->OSDPosType_Custom)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:PositionOption>%s</tt:PositionOption>", onvif_OSDPosTypeToString(OSDPosType_Custom));
	}*/

	if (p_opt->TextOptionFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:TextOption>");
		
		if (p_opt->TextOption.OSDTextType_Plain)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Type>%s</tt:Type>", onvif_OSDTextTypeToString(OSDTextType_Plain));
		}
		
		if (p_opt->TextOption.OSDTextType_Date)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Type>%s</tt:Type>", onvif_OSDTextTypeToString(OSDTextType_Date));
		}
		
		if (p_opt->TextOption.OSDTextType_Time)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Type>%s</tt:Type>", onvif_OSDTextTypeToString(OSDTextType_Time));
		}
		
		if (p_opt->TextOption.OSDTextType_DateAndTime)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Type>%s</tt:Type>", onvif_OSDTextTypeToString(OSDTextType_DateAndTime));
		}

		if (p_opt->TextOption.FontSizeRangeFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:FontSizeRange>"
					"<tt:Min>%d</tt:Min>"
					"<tt:Max>%d</tt:Max>"
				"</tt:FontSizeRange>", 
				p_opt->TextOption.FontSizeRange.Min,
				p_opt->TextOption.FontSizeRange.Max);
		}

		for (i = 0; i < p_opt->TextOption.DateFormatSize; i++)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:DateFormat>%s</tt:DateFormat>", p_opt->TextOption.DateFormat[i]);
		}
		
		for (i = 0; i < p_opt->TextOption.TimeFormatSize; i++)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:TimeFormat>%s</tt:TimeFormat>", p_opt->TextOption.TimeFormat[i]);
		}
		// build onvif color options ...
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:TextOption>");
	}

	if (p_opt->ImageOptionFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:ImageOption>");

		for (i = 0; i < p_opt->ImageOption.ImagePathSize; i++)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:ImagePath>%s</tt:ImagePath>", p_opt->ImageOption.ImagePath[i]);
		}
		
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:ImageOption>");
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"</trt:OSDOptions>"
		"</trt:GetOSDOptionsResponse>");

	return offset;
}

int build_trt_StartMulticastStreaming_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:StartMulticastStreamingResponse />");
	return offset;
}

int build_trt_StopMulticastStreaming_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:StopMulticastStreamingResponse />");
	return offset;
}

int build_trt_GetMetadataConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	MetadataConfigurationList * p_cfg = g_onvif_cfg.metadata_cfg;

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetMetadataConfigurationsResponse>");

	while (p_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset,
			"<trt:Configurations token=\"%s\" "
				"CompressionType=\"%s\" "
				"GeoLocation=\"%s\" "
				"ShapePolygon=\"%s\">", 
			p_cfg->Configuration.token,
			p_cfg->Configuration.CompressionType,
			p_cfg->Configuration.GeoLocation ? "true" : "false",
			p_cfg->Configuration.ShapePolygon ? "true" : "false");
		offset += build_MetadataConfiguration_xml(p_buf+offset, mlen-offset, &p_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, 
			"</trt:Configurations>");
		
		p_cfg = p_cfg->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetMetadataConfigurationsResponse>");
	
	return offset;
}

int build_trt_GetMetadataConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
    MetadataConfigurationList * p_cfg = onvif_find_MetadataConfiguration(g_onvif_cfg.metadata_cfg, argv);
    if (NULL == p_cfg)
    {
    	return ONVIF_ERR_NoConfig;
    }

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetMetadataConfigurationResponse>");
	offset += snprintf(p_buf+offset, mlen-offset,
	    "<trt:Configuration token=\"%s\" "
            "CompressionType=\"%s\" "
            "GeoLocation=\"%s\" "
            "ShapePolygon=\"%s\">", 
        p_cfg->Configuration.token,
        p_cfg->Configuration.CompressionType,
        p_cfg->Configuration.GeoLocation ? "true" : "false",
        p_cfg->Configuration.ShapePolygon ? "true" : "false");
	offset += build_MetadataConfiguration_xml(p_buf+offset, mlen-offset, &p_cfg->Configuration);
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</trt:Configuration>");
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetMetadataConfigurationResponse>");

	return offset;
}

int build_trt_GetCompatibleMetadataConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;	
	MetadataConfigurationList * p_cfg = g_onvif_cfg.metadata_cfg;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, argv);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<trt:GetCompatibleMetadataConfigurationsResponse>");
	    
	while (p_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset,
		    "<trt:Configurations token=\"%s\" "
                "CompressionType=\"%s\" "
                "GeoLocation=\"%s\" "
                "ShapePolygon=\"%s\">", 
            p_cfg->Configuration.token,
            p_cfg->Configuration.CompressionType,
            p_cfg->Configuration.GeoLocation ? "true" : "false",
            p_cfg->Configuration.ShapePolygon ? "true" : "false");
		offset += build_MetadataConfiguration_xml(p_buf+offset, mlen-offset, &p_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "</trt:Configurations>");
		
		p_cfg = p_cfg->next;
	}
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</trt:GetCompatibleMetadataConfigurationsResponse>");
	
	return offset;
}

int build_trt_GetMetadataConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	ONVIF_PROFILE * p_profile = NULL;
	trt_GetMetadataConfigurationOptions_REQ * p_res = (trt_GetMetadataConfigurationOptions_REQ *) argv;

	if (p_res->ProfileTokenFlag)
	{
		p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->ProfileToken);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NoProfile;
		}
	}

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<trt:GetMetadataConfigurationOptionsResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<trt:Options GeoLocation=\"%s\">", 
	    g_onvif_cfg.MetadataConfigurationOptions.GeoLocation ? "true" : "false");

	offset += build_MetadataConfigurationOptions_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.MetadataConfigurationOptions);
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</trt:Options>");

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</trt:GetMetadataConfigurationOptionsResponse>");
	
	return offset;
}

int build_trt_SetMetadataConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:SetMetadataConfigurationResponse />");
	return offset;
}

int build_trt_AddMetadataConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:AddMetadataConfigurationResponse />");
	return offset;
}

int build_trt_RemoveMetadataConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:RemoveMetadataConfigurationResponse />");
	return offset;
}

int build_trt_GetVideoSourceModes_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	trt_GetVideoSourceModes_REQ * p_res = (trt_GetVideoSourceModes_REQ *)argv;

	VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_res->VideoSourceToken);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NoVideoSource;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetVideoSourceModesResponse>\r\n");

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:VideoSourceModes token=\"%s\" Enabled=\"%s\">\r\n", p_v_src->VideoSourceMode.token, p_v_src->VideoSourceMode.Enabled ? "true" : "false");

	offset += build_VideoSourceMode_xml(p_buf+offset, mlen-offset, &p_v_src->VideoSourceMode);

	offset += snprintf(p_buf+offset, mlen-offset, "</trt:VideoSourceModes>\r\n");

	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetVideoSourceModesResponse>\r\n");

	return offset;
}

int build_trt_SetVideoSourceMode_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	trt_SetVideoSourceMode_RES * p_res = (trt_SetVideoSourceMode_RES *) argv;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<trt:SetVideoSourceModeResponse>"
			"<trt:Reboot>%s</trt:Reboot>"
		"</trt:SetVideoSourceModeResponse>",
		p_res->Reboot ? "true" : "false");

	return offset;
}

#ifdef AUDIO_SUPPORT

int build_AudioDecoderConfigurationOptions_xml(char * p_buf, int mlen, onvif_AudioDecoderConfigurationOptions * p_res)
{
	int offset = 0;

	if (p_res->AACDecOptionsFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:AACDecOptions>\r\n");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Bitrate>\r\n");
		offset += build_IntList_xml(p_buf+offset, mlen-offset, &p_res->AACDecOptions.Bitrate);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Bitrate>\r\n");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:SampleRateRange>\r\n");
		offset += build_IntList_xml(p_buf+offset, mlen-offset, &p_res->AACDecOptions.SampleRateRange);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:SampleRateRange>\r\n");
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:AACDecOptions>\r\n");
	}
	
	if (p_res->G711DecOptionsFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:G711DecOptions>\r\n");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Bitrate>\r\n");
		offset += build_IntList_xml(p_buf+offset, mlen-offset, &p_res->G711DecOptions.Bitrate);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Bitrate>\r\n");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:SampleRateRange>\r\n");
		offset += build_IntList_xml(p_buf+offset, mlen-offset, &p_res->G711DecOptions.SampleRateRange);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:SampleRateRange>\r\n");
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:G711DecOptions>\r\n");
	}
	
	if (p_res->G726DecOptionsFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:G726DecOptions>\r\n");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Bitrate>\r\n");
		offset += build_IntList_xml(p_buf+offset, mlen-offset, &p_res->G726DecOptions.Bitrate);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Bitrate>\r\n");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:SampleRateRange>\r\n");
		offset += build_IntList_xml(p_buf+offset, mlen-offset, &p_res->G726DecOptions.SampleRateRange);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:SampleRateRange>\r\n");
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:G726DecOptions>\r\n");
	}

	return offset;
}

int build_trt_AddAudioSourceConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:AddAudioSourceConfigurationResponse />");
	return offset;
}

int build_trt_RemoveAudioSourceConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:RemoveAudioSourceConfigurationResponse />");
	return offset;
}

int build_trt_AddAudioEncoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:AddAudioEncoderConfigurationResponse />");
	return offset;
}

int build_trt_RemoveAudioEncoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:RemoveAudioEncoderConfigurationResponse />");
	return offset;
}

int build_trt_GetAudioSources_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	AudioSourceList * p_a_src = g_onvif_cfg.a_src;

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetAudioSourcesResponse>");
	
	while (p_a_src)
	{
		offset += build_AudioSource_xml(p_buf+offset, mlen-offset, &p_a_src->AudioSource);

		p_a_src = p_a_src->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetAudioSourcesResponse>");
	
	return offset;
}

int build_trt_GetAudioEncoderConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	onvif_renew_AudioEncoderConfiguration();
	
	AudioEncoder2ConfigurationList * p_a_enc_cfg = g_onvif_cfg.a_enc_cfg;

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetAudioEncoderConfigurationsResponse>");

	while (p_a_enc_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configurations token=\"%s\">", p_a_enc_cfg->Configuration.token);
		offset += build_AudioEncoderConfiguration_xml(p_buf+offset, mlen-offset, &p_a_enc_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configurations>");
		
		p_a_enc_cfg = p_a_enc_cfg->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetAudioEncoderConfigurationsResponse>");

	return offset;
}

int build_trt_GetAudioEncoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	
	onvif_renew_AudioEncoderConfiguration();
	
	AudioEncoder2ConfigurationList * p_a_enc_cfg = onvif_find_AudioEncoder2Configuration(g_onvif_cfg.a_enc_cfg, argv);
	if (NULL == p_a_enc_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetAudioEncoderConfigurationResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configuration token=\"%s\">", p_a_enc_cfg->Configuration.token);
	offset += build_AudioEncoderConfiguration_xml(p_buf+offset, mlen-offset, &p_a_enc_cfg->Configuration);
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configuration>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetAudioEncoderConfigurationResponse>");

	return offset;
}

int build_trt_SetAudioEncoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:SetAudioEncoderConfigurationResponse />");		    
	return offset;
}

int build_trt_GetAudioSourceConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	AudioSourceConfigurationList * p_a_src_cfg = g_onvif_cfg.a_src_cfg;

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetAudioSourceConfigurationsResponse>");

	while (p_a_src_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configurations token=\"%s\">", p_a_src_cfg->Configuration.token);
		offset += build_AudioSourceConfiguration_xml(p_buf+offset, mlen-offset, &p_a_src_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configurations>");

		p_a_src_cfg = p_a_src_cfg->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetAudioSourceConfigurationsResponse>");
	
	return offset;
}

int build_trt_GetCompatibleAudioSourceConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	AudioSourceConfigurationList * p_a_src_cfg;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, argv);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	p_a_src_cfg = g_onvif_cfg.a_src_cfg;

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetCompatibleAudioSourceConfigurationsResponse>");

	while (p_a_src_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configurations token=\"%s\">", p_a_src_cfg->Configuration.token);
		offset += build_AudioSourceConfiguration_xml(p_buf+offset, mlen-offset, &p_a_src_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configurations>");

		p_a_src_cfg = p_a_src_cfg->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetCompatibleAudioSourceConfigurationsResponse>");
	
	return offset;
}

int build_trt_GetAudioSourceConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	AudioSourceConfigurationList * p_a_src_cfg = NULL;
	trt_GetAudioSourceConfigurationOptions_REQ * p_res = (trt_GetAudioSourceConfigurationOptions_REQ *)argv;

	if (p_res->ProfileTokenFlag && p_res->ProfileToken[0] != '\0')
	{
		ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->ProfileToken);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NoProfile;
		}

		p_a_src_cfg = p_profile->a_src_cfg;
	}

	if (p_res->ConfigurationTokenFlag && p_res->ConfigurationToken[0] != '\0')
	{
		p_a_src_cfg = onvif_find_AudioSourceConfiguration(g_onvif_cfg.a_src_cfg, p_res->ConfigurationToken);
		if (NULL == p_a_src_cfg)
		{
			return ONVIF_ERR_NoConfig;
		}
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetAudioSourceConfigurationOptionsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:Options>");

	offset += build_AudioSourceConfigurationOptions_xml(p_buf+offset, mlen-offset);

    offset += snprintf(p_buf+offset, mlen-offset, "</trt:Options>");
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetAudioSourceConfigurationOptionsResponse>");
	
	return offset;
}

int build_trt_GetAudioEncoderConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	trt_GetAudioEncoderConfigurationOptions_REQ * p_res = (trt_GetAudioEncoderConfigurationOptions_REQ *) argv;
	AudioEncoder2ConfigurationList * p_a_enc_cfg = NULL;
	AudioEncoder2ConfigurationOptionsList * p_option;

	if (p_res->ProfileTokenFlag && p_res->ProfileToken[0] != '\0')
	{
		ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->ProfileToken);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NoProfile;
		}

		p_a_enc_cfg = p_profile->a_enc_cfg;
	}

	if (p_res->ConfigurationTokenFlag && p_res->ConfigurationToken[0] != '\0')
	{
		AudioEncoder2ConfigurationList * p_a_enc_cfg = onvif_find_AudioEncoder2Configuration(g_onvif_cfg.a_enc_cfg, p_res->ConfigurationToken);
		if (NULL == p_a_enc_cfg)
		{
			return ONVIF_ERR_NoConfig;
		}
	}

	if (NULL == p_a_enc_cfg)
	{
		p_a_enc_cfg = g_onvif_cfg.a_enc_cfg;
	}

	if (NULL == p_a_enc_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}
		
	p_option = p_a_enc_cfg->Options;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<trt:GetAudioEncoderConfigurationOptionsResponse>"
		"<trt:Options>");

	while (p_option)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Options>");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Encoding>%s</tt:Encoding>", onvif_AudioEncodingToString(p_option->Options.AudioEncoding));
			
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:BitrateList>");
		offset += build_IntList_xml(p_buf+offset, mlen-offset, &p_option->Options.BitrateList);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:BitrateList>");
		
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:SampleRateList>");
		offset += build_IntList_xml(p_buf+offset, mlen-offset, &p_option->Options.SampleRateList);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:SampleRateList>");
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Options>");

		p_option = p_option->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"</trt:Options>"
		"</trt:GetAudioEncoderConfigurationOptionsResponse>");
	
	return offset;
}

int build_trt_GetAudioSourceConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
    AudioSourceConfigurationList * p_a_src_cfg = onvif_find_AudioSourceConfiguration(g_onvif_cfg.a_src_cfg, argv);
    if (NULL == p_a_src_cfg)
    {
    	return ONVIF_ERR_NoConfig;
    }
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetAudioSourceConfigurationResponse>");

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<trt:Configuration token=\"%s\">", 
        p_a_src_cfg->Configuration.token);
    offset += build_AudioSourceConfiguration_xml(p_buf+offset, mlen-offset, &p_a_src_cfg->Configuration);
    offset += snprintf(p_buf+offset, mlen-offset, 
        "</trt:Configuration>");	    
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetAudioSourceConfigurationResponse>");
	
	return offset;
}

int build_trt_SetAudioSourceConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:SetAudioSourceConfigurationResponse />");
	return offset;
}

int build_trt_GetCompatibleAudioEncoderConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	onvif_renew_AudioEncoderConfiguration();
	
	AudioEncoder2ConfigurationList * p_a_enc_cfg;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, argv);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	p_a_enc_cfg = g_onvif_cfg.a_enc_cfg;
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetCompatibleAudioEncoderConfigurationsResponse>");

	while (p_a_enc_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configurations token=\"%s\">", p_a_enc_cfg->Configuration.token);
		offset += build_AudioEncoderConfiguration_xml(p_buf+offset, mlen-offset, &p_a_enc_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configurations>");

		p_a_enc_cfg = p_a_enc_cfg->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetCompatibleAudioEncoderConfigurationsResponse>");

	return offset;
}

int build_trt_AddAudioDecoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:AddAudioDecoderConfigurationResponse />");
	return offset;
}

int build_trt_GetAudioDecoderConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	AudioDecoderConfigurationList * p_a_dec_cfg;

	p_a_dec_cfg = g_onvif_cfg.a_dec_cfg;
    
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetAudioDecoderConfigurationsResponse>");

	while (p_a_dec_cfg)
	{
	    offset += snprintf(p_buf+offset, mlen-offset, 
	        "<trt:Configurations token=\"%s\">", 
	        p_a_dec_cfg->Configuration.token);
    	offset += build_AudioDecoderConfiguration_xml(p_buf+offset, mlen-offset, &p_a_dec_cfg->Configuration);
    	offset += snprintf(p_buf+offset, mlen-offset, 
    	    "</trt:Configurations>");
    	
    	p_a_dec_cfg = p_a_dec_cfg->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetAudioDecoderConfigurationsResponse>");

	return offset;
}

int build_trt_GetAudioDecoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	AudioDecoderConfigurationList * p_a_dec_cfg;

	p_a_dec_cfg = onvif_find_AudioDecoderConfiguration(g_onvif_cfg.a_dec_cfg, argv);
	if (NULL == p_a_dec_cfg)
	{
	    return ONVIF_ERR_InvalidToken;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetAudioDecoderConfigurationResponse>");

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<trt:Configuration token=\"%s\">", 
        p_a_dec_cfg->Configuration.token);
	offset += build_AudioDecoderConfiguration_xml(p_buf+offset, mlen-offset, &p_a_dec_cfg->Configuration);
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</trt:Configuration>");
	
	p_a_dec_cfg = p_a_dec_cfg->next;
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetAudioDecoderConfigurationResponse>");

	return offset;
}

int build_trt_RemoveAudioDecoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:RemoveAudioDecoderConfigurationResponse />");
	return offset;
}

int build_trt_SetAudioDecoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:SetAudioDecoderConfigurationResponse />");
	return offset;
}

int build_trt_GetAudioDecoderConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    trt_GetAudioDecoderConfigurationOptions_REQ * p_res = (trt_GetAudioDecoderConfigurationOptions_REQ *)argv;
    ONVIF_PROFILE * p_profile = NULL;
    AudioDecoderConfigurationList * p_a_dec_cfg = NULL;
    
    if (p_res->ProfileTokenFlag && p_res->ProfileToken[0] != '\0')
	{
		p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->ProfileToken);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NoProfile;
		}
	}

	if (p_res->ConfigurationTokenFlag && p_res->ConfigurationToken[0] != '\0')
	{
		p_a_dec_cfg = onvif_find_AudioDecoderConfiguration(g_onvif_cfg.a_dec_cfg, p_res->ConfigurationToken);
		if (NULL == p_a_dec_cfg)
		{
			return ONVIF_ERR_NoConfig;
		}
	}
	else if (p_profile && p_profile->a_dec_cfg)
	{
	    p_a_dec_cfg = p_profile->a_dec_cfg;
	}
	else if (g_onvif_cfg.a_dec_cfg)
	{
	    p_a_dec_cfg = g_onvif_cfg.a_dec_cfg;
	}

	if (NULL == p_a_dec_cfg)
	{
	    return ONVIF_ERR_InvalidArgVal;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<trt:GetAudioDecoderConfigurationOptionsResponse>\r\n");

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:Options>\r\n");
	offset += build_AudioDecoderConfigurationOptions_xml(p_buf+offset, mlen-offset, &p_a_dec_cfg->Options);
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:Options>\r\n");

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</trt:GetAudioDecoderConfigurationOptionsResponse>\r\n");
	
	return offset;
}

int build_trt_GetCompatibleAudioDecoderConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	AudioDecoderConfigurationList * p_a_dec_cfg;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, argv);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	p_a_dec_cfg = g_onvif_cfg.a_dec_cfg;
    
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<trt:GetCompatibleAudioDecoderConfigurationsResponse>");

	while (p_a_dec_cfg)
	{
	    offset += snprintf(p_buf+offset, mlen-offset, 
	        "<trt:Configurations token=\"%s\">", 
	        p_a_dec_cfg->Configuration.token);
    	offset += build_AudioDecoderConfiguration_xml(p_buf+offset, mlen-offset, &p_a_dec_cfg->Configuration);
    	offset += snprintf(p_buf+offset, mlen-offset, 
    	    "</trt:Configurations>");
    	
    	p_a_dec_cfg = p_a_dec_cfg->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</trt:GetCompatibleAudioDecoderConfigurationsResponse>");

	return offset;

}

#endif // AUDIO_SUPPORT

#ifdef DEVICEIO_SUPPORT

int build_trt_GetAudioOutputs_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    AudioOutputList * p_output = g_onvif_cfg.a_output;

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetAudioOutputsResponse>");

	while (p_output)
	{
	    offset += snprintf(p_buf+offset, mlen-offset, 
	        "<trt:AudioOutputs token=\"%s\" />",
	        p_output->AudioOutput.token);

        p_output = p_output->next;
	}
	        
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetAudioOutputsResponse>");

	return offset;
}

int build_trt_AddAudioOutputConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:AddAudioOutputConfigurationResponse />");
	return offset;
}

int build_trt_RemoveAudioOutputConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:RemoveAudioOutputConfigurationResponse />");
	return offset;
}

int build_trt_GetAudioOutputConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    AudioOutputConfigurationList * p_cfg = g_onvif_cfg.a_output_cfg;

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetAudioOutputConfigurationsResponse>");

    while (p_cfg)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<trt:Configurations token=\"%s\">", 
            p_cfg->Configuration.token);
        
        offset += build_AudioOutputConfiguration_xml(p_buf+offset, mlen-offset, &p_cfg->Configuration);
        
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</trt:Configurations>");
            
        p_cfg = p_cfg->next;
    }
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetAudioOutputConfigurationsResponse>");

	return offset;
}

int build_trt_GetCompatibleAudioOutputConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    trt_GetCompatibleAudioOutputConfigurations_REQ * p_res = (trt_GetCompatibleAudioOutputConfigurations_REQ *)argv;
    AudioOutputConfigurationList * p_cfg = g_onvif_cfg.a_output_cfg;
    ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->ProfileToken);
    if (NULL == p_profile)
    {
        return ONVIF_ERR_NoProfile;
    }

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<trt:GetCompatibleAudioOutputConfigurationsResponse>");

    while (p_cfg)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<trt:Configurations token=\"%s\">", 
            p_cfg->Configuration.token);
        
        offset += build_AudioOutputConfiguration_xml(p_buf+offset, mlen-offset, &p_cfg->Configuration);
        
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</trt:Configurations>");
            
        p_cfg = p_cfg->next;
    }
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</trt:GetCompatibleAudioOutputConfigurationsResponse>");

	return offset;
}

int build_trt_GetAudioOutputConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    trt_GetAudioOutputConfiguration_REQ * p_res = (trt_GetAudioOutputConfiguration_REQ *)argv; 
    AudioOutputConfigurationList * p_cfg = onvif_find_AudioOutputConfiguration(g_onvif_cfg.a_output_cfg, p_res->ConfigurationToken);
    if (NULL == p_cfg)
    {
        return ONVIF_ERR_NoConfig;
    }

    offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetAudioOutputConfigurationResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<trt:Configuration token=\"%s\">", 
	    p_cfg->Configuration.token);
    offset += build_AudioOutputConfiguration_xml(p_buf+offset, mlen-offset, &p_cfg->Configuration);
    offset += snprintf(p_buf+offset, mlen-offset, 
        "</trt:Configuration>");

	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetAudioOutputConfigurationResponse>");

	return offset;
}

int build_trt_GetAudioOutputConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    AudioOutputConfigurationList * p_cfg = NULL;
    trt_GetAudioOutputConfigurationOptions_REQ * p_res = (trt_GetAudioOutputConfigurationOptions_REQ *)argv; 
    if (p_res->ConfigurationTokenFlag)
    {
        p_cfg = onvif_find_AudioOutputConfiguration(g_onvif_cfg.a_output_cfg, p_res->ConfigurationToken);
        if (NULL == p_cfg)
        {
            return ONVIF_ERR_NoConfig;
        }
    }

    if (p_res->ProfileTokenFlag)
    {
        ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->ProfileToken);
        if (NULL == p_profile)
        {
            return ONVIF_ERR_NoProfile;
        }
    }

    if (NULL == p_cfg)
    {
        p_cfg = g_onvif_cfg.a_output_cfg;
    }

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<trt:GetAudioOutputConfigurationOptionsResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:Options>");

	if (p_cfg)
	{
        offset += build_AudioOutputConfigurationOptions_xml(p_buf+offset, mlen-offset, &p_cfg->Options);
    }
    
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:Options>");

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</trt:GetAudioOutputConfigurationOptionsResponse>");

	return offset;
}

int build_trt_SetAudioOutputConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:SetAudioOutputConfigurationResponse />");
	return offset;
}

#endif // DEVICEIO_SUPPORT

#ifdef PTZ_SUPPORT

int build_trt_AddPTZConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:AddPTZConfigurationResponse />");	
	return offset;
}

int build_trt_RemovePTZConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:RemovePTZConfigurationResponse />");	
	return offset;
}

#endif // PTZ_SUPPORT

#ifdef VIDEO_ANALYTICS

int build_trt_GetVideoAnalyticsConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	VideoAnalyticsConfigurationList * p_va_cfg = g_onvif_cfg.va_cfg;
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetVideoAnalyticsConfigurationsResponse>");

	while (p_va_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configurations token=\"%s\">", p_va_cfg->Configuration.token);
		offset += build_VideoAnalyticsConfiguration_xml(p_buf+offset, mlen-offset, &p_va_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configurations>");

		p_va_cfg = p_va_cfg->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetVideoAnalyticsConfigurationsResponse>");

	return offset;
}

int build_trt_AddVideoAnalyticsConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:AddVideoAnalyticsConfigurationResponse />");
	return offset;
}

int build_trt_GetVideoAnalyticsConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	trt_GetVideoAnalyticsConfiguration_REQ * p_res = (trt_GetVideoAnalyticsConfiguration_REQ *)argv;
	VideoAnalyticsConfigurationList * p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_res->ConfigurationToken);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetVideoAnalyticsConfigurationResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<trt:Configuration token=\"%s\">", 
	    p_va_cfg->Configuration.token);
	offset += build_VideoAnalyticsConfiguration_xml(p_buf+offset, mlen-offset, &p_va_cfg->Configuration);
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</trt:Configuration>");

	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetVideoAnalyticsConfigurationResponse>");

	return offset;
}

int build_trt_RemoveVideoAnalyticsConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:RemoveVideoAnalyticsConfigurationResponse />");
	return offset;
}

int build_trt_SetVideoAnalyticsConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:SetVideoAnalyticsConfigurationResponse />");
	return offset;
}

int build_trt_GetAnalyticsConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    VideoAnalyticsConfigurationList * p_cfg = g_onvif_cfg.va_cfg;
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetAnalyticsConfigurationsResponse>");

	while (p_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<trt:Configurations token=\"%s\">", 
		    p_cfg->Configuration.token);
		offset += build_VideoAnalyticsConfiguration_xml(p_buf+offset, mlen-offset, &p_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "</trt:Configurations>");
		
		p_cfg = p_cfg->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetAnalyticsConfigurationsResponse>");

	return offset;
}

int build_trt_GetCompatibleVideoAnalyticsConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    trt_GetCompatibleVideoAnalyticsConfigurations_REQ * p_res = (trt_GetCompatibleVideoAnalyticsConfigurations_REQ *) argv;
    ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->ProfileToken);
	VideoAnalyticsConfigurationList * p_va_cfg = g_onvif_cfg.va_cfg;

    if (NULL == p_profile)
    {
        return ONVIF_ERR_NoProfile;
    }
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<trt:GetCompatibleVideoAnalyticsConfigurationsResponse>");

	while (p_va_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<trt:Configurations token=\"%s\">", 
		    p_va_cfg->Configuration.token);
		offset += build_VideoAnalyticsConfiguration_xml(p_buf+offset, mlen-offset, &p_va_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "</trt:Configurations>");

		p_va_cfg = p_va_cfg->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</trt:GetCompatibleVideoAnalyticsConfigurationsResponse>");

	return offset;
}

#endif // VIDEO_ANALYTICS

#endif // MEDIA_SUPPORT

/***************************************************************************************/

#ifdef PTZ_SUPPORT

int build_PTZSpaces_xml(char * p_buf, int mlen, onvif_PTZSpaces * p_res)
{
	int offset = 0;

	if (p_res->AbsolutePanTiltPositionSpaceFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:AbsolutePanTiltPositionSpace>\r\n"
				"<tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace</tt:URI>\r\n"
				"<tt:XRange>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:XRange>\r\n"
				"<tt:YRange>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:YRange>\r\n"
			"</tt:AbsolutePanTiltPositionSpace>\r\n", 
			p_res->AbsolutePanTiltPositionSpace.XRange.Min,
			p_res->AbsolutePanTiltPositionSpace.XRange.Max,
			p_res->AbsolutePanTiltPositionSpace.YRange.Min,
			p_res->AbsolutePanTiltPositionSpace.YRange.Max);
	}

	if (p_res->AbsoluteZoomPositionSpaceFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:AbsoluteZoomPositionSpace>\r\n"
				"<tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/PositionGenericSpace</tt:URI>\r\n"
				"<tt:XRange>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:XRange>\r\n"
			"</tt:AbsoluteZoomPositionSpace>\r\n", 
			p_res->AbsoluteZoomPositionSpace.XRange.Min,
			p_res->AbsoluteZoomPositionSpace.XRange.Max);
	}

	if (p_res->RelativePanTiltTranslationSpaceFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:RelativePanTiltTranslationSpace>\r\n"
				"<tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/TranslationGenericSpace</tt:URI>\r\n"
				"<tt:XRange>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:XRange>\r\n"
				"<tt:YRange>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:YRange>\r\n"
			"</tt:RelativePanTiltTranslationSpace>\r\n",
			p_res->RelativePanTiltTranslationSpace.XRange.Min,
			p_res->RelativePanTiltTranslationSpace.XRange.Max,
			p_res->RelativePanTiltTranslationSpace.YRange.Min,
			p_res->RelativePanTiltTranslationSpace.YRange.Max);
	}

	if (p_res->RelativeZoomTranslationSpaceFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:RelativeZoomTranslationSpace>\r\n"
				"<tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/TranslationGenericSpace</tt:URI>\r\n"
				"<tt:XRange>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:XRange>\r\n"
			"</tt:RelativeZoomTranslationSpace>\r\n", 
			p_res->RelativeZoomTranslationSpace.XRange.Min,
			p_res->RelativeZoomTranslationSpace.XRange.Max);
	}

	if (p_res->ContinuousPanTiltVelocitySpaceFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:ContinuousPanTiltVelocitySpace>\r\n"
				"<tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace</tt:URI>\r\n"
				"<tt:XRange>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:XRange>\r\n"
				"<tt:YRange>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:YRange>\r\n"
			"</tt:ContinuousPanTiltVelocitySpace>\r\n",
			p_res->ContinuousPanTiltVelocitySpace.XRange.Min,
			p_res->ContinuousPanTiltVelocitySpace.XRange.Max,
			p_res->ContinuousPanTiltVelocitySpace.YRange.Min,
			p_res->ContinuousPanTiltVelocitySpace.YRange.Max);
	}

	if (p_res->ContinuousZoomVelocitySpaceFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:ContinuousZoomVelocitySpace>\r\n"
				"<tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/VelocityGenericSpace</tt:URI>\r\n"
				"<tt:XRange>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:XRange>\r\n"
			"</tt:ContinuousZoomVelocitySpace>\r\n",
			p_res->ContinuousZoomVelocitySpace.XRange.Min,
			p_res->ContinuousZoomVelocitySpace.XRange.Max);
	}

	if (p_res->PanTiltSpeedSpaceFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:PanTiltSpeedSpace>\r\n"
				"<tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/GenericSpeedSpace</tt:URI>\r\n"
				"<tt:XRange>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:XRange>\r\n"
			"</tt:PanTiltSpeedSpace>\r\n",  
			p_res->PanTiltSpeedSpace.XRange.Min,
			p_res->PanTiltSpeedSpace.XRange.Max);
	}

	if (p_res->ZoomSpeedSpaceFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:ZoomSpeedSpace>\r\n"
				"<tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/ZoomGenericSpeedSpace</tt:URI>\r\n"
				"<tt:XRange>\r\n"
					"<tt:Min>%0.1f</tt:Min>\r\n"
					"<tt:Max>%0.1f</tt:Max>\r\n"
				"</tt:XRange>\r\n"
			"</tt:ZoomSpeedSpace>\r\n",
			p_res->ZoomSpeedSpace.XRange.Min,
			p_res->ZoomSpeedSpace.XRange.Max);
	}

	return offset;
}

int build_PTZNodeExtension_xml(char * p_buf, int mlen, onvif_PTZNodeExtension * p_res)
{
	int offset = 0;

	if (p_res->SupportedPresetTourFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:SupportedPresetTour>\r\n");

		offset += snprintf(p_buf+offset, mlen-offset, "<tt:MaximumNumberOfPresetTours>%d</tt:MaximumNumberOfPresetTours>\r\n", 
						p_res->SupportedPresetTour.MaximumNumberOfPresetTours);

		if (p_res->SupportedPresetTour.PTZPresetTourOperation_Start)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:PTZPresetTourOperation>Start</tt:PTZPresetTourOperation>\r\n");
		}
		if (p_res->SupportedPresetTour.PTZPresetTourOperation_Stop)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:PTZPresetTourOperation>Stop</tt:PTZPresetTourOperation>\r\n");
		}
		if (p_res->SupportedPresetTour.PTZPresetTourOperation_Pause)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:PTZPresetTourOperation>Pause</tt:PTZPresetTourOperation>\r\n");
		}
		if (p_res->SupportedPresetTour.PTZPresetTourOperation_Extended)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:PTZPresetTourOperation>Extended</tt:PTZPresetTourOperation>\r\n");
		}

		offset += snprintf(p_buf+offset, mlen-offset, "</tt:SupportedPresetTour>\r\n");
	}

	return offset;
}

int build_PTZNode_xml(char * p_buf, int mlen, onvif_PTZNode * p_res)
{
	uint32 i;
	int offset = 0;
		
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Name>%s</tt:Name>\r\n", p_res->Name);

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:SupportedPTZSpaces>\r\n");
	offset += build_PTZSpaces_xml(p_buf+offset, mlen-offset, &p_res->SupportedPTZSpaces);	
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:SupportedPTZSpaces>\r\n");
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:MaximumNumberOfPresets>%d</tt:MaximumNumberOfPresets>\r\n", p_res->MaximumNumberOfPresets);

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:HomeSupported>%s</tt:HomeSupported>\r\n", p_res->HomeSupported ? "true" : "false");

	for (i = 0; i < p_res->sizeAuxiliaryCommands; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:AuxiliaryCommands>%s</tt:AuxiliaryCommands>\r\n", p_res->AuxiliaryCommands[i]);
	}

	if (p_res->ExtensionFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Extension>\r\n"); 
		offset += build_PTZNodeExtension_xml(p_buf+offset, mlen-offset, &p_res->Extension);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Extension>\r\n"); 
	}

	return offset;
}

int build_Vector_xml(char * p_buf, int mlen, onvif_Vector * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:x>%f</tt:x>\r\n"
		"<tt:y>%f</tt:y>\r\n",
		p_res->x, p_res->y);

	return offset;
}

int build_Vector1D_xml(char * p_buf, int mlen, onvif_Vector1D * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:x>%f</tt:x>\r\n",
		p_res->x);

	return offset;
}

int build_PTZVector_xml(char * p_buf, int mlen, onvif_PTZVector * p_res)
{
	int offset = 0;

	if (p_res->PanTiltFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:PanTilt>\r\n");
		offset += build_Vector_xml(p_buf+offset, mlen-offset, &p_res->PanTilt);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:PanTilt>\r\n");
	}

	if (p_res->ZoomFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Zoom>\r\n");
		offset += build_Vector1D_xml(p_buf+offset, mlen-offset, &p_res->Zoom);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Zoom>\r\n");
	}

	return offset;
}

int build_PTZPresetTourPresetDetail_xml(char * p_buf, int mlen, onvif_PTZPresetTourPresetDetail * p_res)
{
	int offset = 0;

	if (p_res->PresetTokenFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:PresetToken>%s</tt:PresetToken>\r\n", p_res->PresetToken);
	}
	else if (p_res->HomeFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Home>%s</tt:Home>\r\n", p_res->Home ? "true" : "false");
	}
	else if (p_res->PTZPositionFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:PTZPosition>\r\n");
		offset += build_PTZVector_xml(p_buf+offset, mlen-offset, &p_res->PTZPosition);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:PTZPosition>\r\n");
	}

	return offset;
}

int build_PTZSpeed_xml(char * p_buf, int mlen, onvif_PTZSpeed * p_res)
{
	int offset = 0;

	if (p_res->PanTiltFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:PanTilt>\r\n");
		offset += build_Vector_xml(p_buf+offset, mlen-offset, &p_res->PanTilt);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:PanTilt>\r\n");
	}

	if (p_res->ZoomFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Zoom>\r\n");
		offset += build_Vector1D_xml(p_buf+offset, mlen-offset, &p_res->Zoom);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Zoom>\r\n");
	}

	return offset;
}

int build_PTZPreset_xml(char * p_buf, int mlen, onvif_PTZPreset * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Name>%s</tt:Name>\r\n", p_res->Name);
		
	if (p_res->PTZPositionFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:PTZPosition>\r\n");
		
		if (p_res->PTZPosition.PanTiltFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:PanTilt x=\"%0.1f\" y=\"%0.1f\" />\r\n",
				p_res->PTZPosition.PanTilt.x,
				p_res->PTZPosition.PanTilt.y);
		}

		if (p_res->PTZPosition.ZoomFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Zoom x=\"%0.1f\" />\r\n", p_res->PTZPosition.Zoom.x);
		}

		offset += snprintf(p_buf+offset, mlen-offset, "</tt:PTZPosition>\r\n");
	}

	return offset;
}

int build_PTZPresetTourSpot_xml(char * p_buf, int mlen, onvif_PTZPresetTourSpot * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:PresetDetail>\r\n");
	offset += build_PTZPresetTourPresetDetail_xml(p_buf+offset, mlen-offset, &p_res->PresetDetail);
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:PresetDetail>\r\n");

	if (p_res->SpeedFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Speed>\r\n");
		offset += build_PTZSpeed_xml(p_buf+offset, mlen-offset, &p_res->Speed);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Speed>\r\n");
	}

	if (p_res->StayTimeFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:StayTime>PT%dS</tt:StayTime>\r\n", p_res->StayTime);
	}

	return offset;
}

int build_PTZPresetTourStatus(char * p_buf, int mlen, onvif_PTZPresetTourStatus * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:State>%s</tt:State>\r\n", onvif_PTZPresetTourStateToString(p_res->State));

	if (p_res->CurrentTourSpotFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:CurrentTourSpot>\r\n");
		offset += build_PTZPresetTourSpot_xml(p_buf+offset, mlen-offset, &p_res->CurrentTourSpot);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:CurrentTourSpot>\r\n");
	}

	return offset;
}

int build_PTZPresetTourStartingCondition(char * p_buf, int mlen, onvif_PTZPresetTourStartingCondition * p_res)
{
	int offset = 0;

	if (p_res->RandomPresetOrderFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:StartingCondition RandomPresetOrder=\"%s\">\r\n", p_res->RandomPresetOrder ? "true" : "false");
	}
	else
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:StartingCondition>\r\n");
	}

	if (p_res->RecurringTimeFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:RecurringTime>%d</tt:RecurringTime>\r\n", p_res->RecurringTime);
	}

	if (p_res->RecurringDurationFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:RecurringDuration>PT%dS</tt:RecurringDuration>\r\n", p_res->RecurringDuration);
	}

	if (p_res->DirectionFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Direction>%s</tt:Direction>\r\n", onvif_PTZPresetTourDirectionToString(p_res->Direction));
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tt:StartingCondition>\r\n");

	return offset;
}

int build_PresetTour_by_g_tour_ctx_xml(char * p_buf, int mlen, ptz_tour_t * p_res)
{
	int offset = 0;
	ptz_tour_t * p_spot = p_res;

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Status>\r\n");
	if(p_spot->active)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:State>%s</tt:State>\r\n", "Touring");
	}
	else
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:State>%s</tt:State>\r\n", "Idle");
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:Status>\r\n");

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:AutoStart>%s</tt:AutoStart>\r\n", "false");

	return offset;
}

int build_PresetTour_xml(char * p_buf, int mlen, onvif_PresetTour * p_res)
{
	int offset = 0;
	PTZPresetTourSpotList * p_spot = p_res->TourSpot;

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Name>%s</tt:Name>\r\n", p_res->Name);

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Status>\r\n");
	offset += build_PTZPresetTourStatus(p_buf+offset, mlen-offset, &p_res->Status);
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:Status>\r\n");

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:AutoStart>%s</tt:AutoStart>\r\n", p_res->AutoStart ? "true" : "false");

	offset += build_PTZPresetTourStartingCondition(p_buf+offset, mlen-offset, &p_res->StartingCondition);

	while (p_spot)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:TourSpot>\r\n");
		offset += build_PTZPresetTourSpot_xml(p_buf+offset, mlen-offset, &p_spot->PTZPresetTourSpot);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:TourSpot>\r\n");

		p_spot = p_spot->next;
	}

	return offset;
}

int build_IntRange_xml(char * p_buf, int mlen, onvif_IntRange * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Min>%d</tt:Min>\r\n"
		"<tt:Max>%d</tt:Max>\r\n",
		p_res->Min, p_res->Max);

	return offset;
}

int build_DurationRange_xml(char * p_buf, int mlen, onvif_DurationRange * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Min>PT%dS</tt:Min>\r\n"
		"<tt:Max>PT%dS</tt:Max>\r\n",
		p_res->Min, p_res->Max);

	return offset;
}

int build_PTZPresetTourStartingConditionOptions_xml(char * p_buf, int mlen, onvif_PTZPresetTourStartingConditionOptions * p_res)
{
	int offset = 0;

	if (p_res->RecurringTimeFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:RecurringTime>\r\n");
		offset += build_IntRange_xml(p_buf+offset, mlen-offset, &p_res->RecurringTime);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:RecurringTime>\r\n");
	}

	if (p_res->RecurringDurationFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:RecurringDuration>\r\n");
		offset += build_DurationRange_xml(p_buf+offset, mlen-offset, &p_res->RecurringDuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:RecurringDuration>\r\n");
	}

	if (p_res->PTZPresetTourDirection_Forward)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Direction>Forward</tt:Direction>\r\n");
	}
	if (p_res->PTZPresetTourDirection_Backward)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Direction>Backward</tt:Direction>\r\n");
	}
	if (p_res->PTZPresetTourDirection_Extended)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Direction>Extended</tt:Direction>\r\n");
	}

	return offset;
}

int build_Space2DDescription_xml(char * p_buf, int mlen, onvif_Space2DDescription * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tt:URI>%s</tt:URI>\r\n", 
        p_res->URI);        

    offset += snprintf(p_buf+offset, mlen-offset, "<tt:XRange>\r\n");
    offset += build_FloatRange_xml(p_buf+offset, mlen-offset, &p_res->XRange);
    offset += snprintf(p_buf+offset, mlen-offset, "</tt:XRange>\r\n");

    offset += snprintf(p_buf+offset, mlen-offset, "<tt:YRange>\r\n");
    offset += build_FloatRange_xml(p_buf+offset, mlen-offset, &p_res->YRange);
    offset += snprintf(p_buf+offset, mlen-offset, "</tt:YRange>\r\n");

    return offset;
}

int build_Space1DDescription_xml(char * p_buf, int mlen, onvif_Space1DDescription * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tt:URI>%s</tt:URI>\r\n\r\n", 
        p_res->URI);        

    offset += snprintf(p_buf+offset, mlen-offset, "<tt:XRange>\r\n");
    offset += build_FloatRange_xml(p_buf+offset, mlen-offset, &p_res->XRange);
    offset += snprintf(p_buf+offset, mlen-offset, "</tt:XRange>\r\n");

    return offset;
}

int build_PTZPresetTourPresetDetailOptions_xml(char * p_buf, int mlen, onvif_PTZPresetTourPresetDetailOptions * p_res)
{
	uint32 i = 0;
    int offset = 0;

    for (i = 0; i < p_res->sizePresetToken; i++)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tt:PresetToken>%s</tt:PresetToken>\r\n", 
            p_res->PresetToken[i]);
    }

    if (p_res->HomeFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tt:Home>%s</tt:Home>\r\n", 
            p_res->Home ? "true" : "false");
    }

    if (p_res->PanTiltPositionSpaceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, "<tt:PanTiltPositionSpace>\r\n");
        offset += build_Space2DDescription_xml(p_buf+offset, mlen-offset, &p_res->PanTiltPositionSpace);
        offset += snprintf(p_buf+offset, mlen-offset, "</tt:PanTiltPositionSpace>\r\n");
    }

    if (p_res->ZoomPositionSpaceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, "<tt:ZoomPositionSpace>\r\n");
        offset += build_Space1DDescription_xml(p_buf+offset, mlen-offset, &p_res->ZoomPositionSpace);
        offset += snprintf(p_buf+offset, mlen-offset, "</tt:ZoomPositionSpace>\r\n");
    }

    return offset;
}

int build_PTZPresetTourSpotOptions_xml(char * p_buf, int mlen, onvif_PTZPresetTourSpotOptions * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, "<tt:PresetDetail>\r\n");
    offset += build_PTZPresetTourPresetDetailOptions_xml(p_buf+offset, mlen-offset, &p_res->PresetDetail);
    offset += snprintf(p_buf+offset, mlen-offset, "</tt:PresetDetail>\r\n");

    offset += snprintf(p_buf+offset, mlen-offset, "<tt:StayTime>\r\n");
    offset += build_DurationRange_xml(p_buf+offset, mlen-offset, &p_res->StayTime);
    offset += snprintf(p_buf+offset, mlen-offset, "</tt:StayTime>\r\n");

    return offset;
}

int build_PTZPresetTourOptions_xml(char * p_buf, int mlen, onvif_PTZPresetTourOptions * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:AutoStart>%s</tt:AutoStart>\r\n", p_res->AutoStart ? "true" : "false");

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:StartingCondition>\r\n");
	offset += build_PTZPresetTourStartingConditionOptions_xml(p_buf+offset, mlen-offset, &p_res->StartingCondition);
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:StartingCondition>\r\n");

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:TourSpot>\r\n");
	offset += build_PTZPresetTourSpotOptions_xml(p_buf+offset, mlen-offset, &p_res->TourSpot);
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:TourSpot>\r\n");

	return offset;
}

int build_PTZConfiguration_xml(char * p_buf, int mlen, onvif_PTZConfiguration * p_ptz_cfg)
{
	int offset = 0;
	
	offset += snprintf(p_buf+offset, mlen-offset,
		"<tt:Name>%s</tt:Name>\r\n"
		"<tt:UseCount>%d</tt:UseCount>\r\n"
		"<tt:NodeToken>%s</tt:NodeToken>\r\n",
		p_ptz_cfg->Name, 
		p_ptz_cfg->UseCount,
		p_ptz_cfg->NodeToken);

	offset += snprintf(p_buf+offset, mlen-offset,  	
		"<tt:DefaultAbsolutePantTiltPositionSpace>"
			"http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace"
		"</tt:DefaultAbsolutePantTiltPositionSpace>\r\n"
		"<tt:DefaultAbsoluteZoomPositionSpace>"
			"http://www.onvif.org/ver10/tptz/ZoomSpaces/PositionGenericSpace"
		"</tt:DefaultAbsoluteZoomPositionSpace>\r\n"
		"<tt:DefaultRelativePanTiltTranslationSpace>"
			"http://www.onvif.org/ver10/tptz/PanTiltSpaces/TranslationGenericSpace"
		"</tt:DefaultRelativePanTiltTranslationSpace>\r\n"
		"<tt:DefaultRelativeZoomTranslationSpace>"
			"http://www.onvif.org/ver10/tptz/ZoomSpaces/TranslationGenericSpace"
		"</tt:DefaultRelativeZoomTranslationSpace>\r\n"
		"<tt:DefaultContinuousPanTiltVelocitySpace>"
				"http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace"
		"</tt:DefaultContinuousPanTiltVelocitySpace>\r\n"
		"<tt:DefaultContinuousZoomVelocitySpace>"
			"http://www.onvif.org/ver10/tptz/ZoomSpaces/VelocityGenericSpace"
		"</tt:DefaultContinuousZoomVelocitySpace>\r\n");

	if (p_ptz_cfg->DefaultPTZSpeedFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:DefaultPTZSpeed>\r\n");

		if (p_ptz_cfg->DefaultPTZSpeed.PanTiltFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:PanTilt x=\"%0.1f\" y=\"%0.1f\" "
					"space=\"http://www.onvif.org/ver10/tptz/PanTiltSpaces/GenericSpeedSpace\" />\r\n",
				p_ptz_cfg->DefaultPTZSpeed.PanTilt.x, 
				p_ptz_cfg->DefaultPTZSpeed.PanTilt.y);
		}
		
		if (p_ptz_cfg->DefaultPTZSpeed.ZoomFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:Zoom x=\"%0.1f\" "
					"space=\"http://www.onvif.org/ver10/tptz/ZoomSpaces/ZoomGenericSpeedSpace\" />\r\n",
				p_ptz_cfg->DefaultPTZSpeed.Zoom.x);
		}
		
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:DefaultPTZSpeed>\r\n"); 
	}

	if (p_ptz_cfg->DefaultPTZTimeoutFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:DefaultPTZTimeout>PT%dS</tt:DefaultPTZTimeout>\r\n", p_ptz_cfg->DefaultPTZTimeout);
	}

	if (p_ptz_cfg->PanTiltLimitsFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset,
			"<tt:PanTiltLimits>\r\n"
				"<tt:Range>"
					"<tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace</tt:URI>\r\n"
					"<tt:XRange>\r\n"
						"<tt:Min>%0.1f</tt:Min>\r\n"
						"<tt:Max>%0.1f</tt:Max>\r\n"
					"</tt:XRange>\r\n"
					"<tt:YRange>\r\n"
						"<tt:Min>%0.1f</tt:Min>\r\n"
						"<tt:Max>%0.1f</tt:Max>\r\n"
					"</tt:YRange>\r\n"
				"</tt:Range>\r\n"
			"</tt:PanTiltLimits>\r\n",
			p_ptz_cfg->PanTiltLimits.XRange.Min, 
			p_ptz_cfg->PanTiltLimits.XRange.Max,
			p_ptz_cfg->PanTiltLimits.YRange.Min, 
			p_ptz_cfg->PanTiltLimits.YRange.Max);
	}

	if (p_ptz_cfg->ZoomLimitsFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset,  	 	
			"<tt:ZoomLimits>\r\n"
				"<tt:Range>\r\n"
					"<tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/PositionGenericSpace</tt:URI>\r\n"
					"<tt:XRange>\r\n"
						"<tt:Min>%0.1f</tt:Min>\r\n"
						"<tt:Max>%0.1f</tt:Max>\r\n"
					"</tt:XRange>\r\n"
				"</tt:Range>\r\n"
			"</tt:ZoomLimits>\r\n",
			p_ptz_cfg->ZoomLimits.XRange.Min,
			p_ptz_cfg->ZoomLimits.XRange.Max);
	}

	if (p_ptz_cfg->ExtensionFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Extension>\r\n");

		if (p_ptz_cfg->Extension.PTControlDirectionFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:PTControlDirection>\r\n");

			if (p_ptz_cfg->Extension.PTControlDirection.EFlipFlag)
			{
				offset += snprintf(p_buf+offset, mlen-offset, 
					"<tt:EFlip>\r\n"
						"<tt:Mode>%s</tt:Mode>\r\n"
					"</tt:EFlip>\r\n",
					onvif_EFlipModeToString(p_ptz_cfg->Extension.PTControlDirection.EFlip));
			}

			if (p_ptz_cfg->Extension.PTControlDirection.ReverseFlag)
			{
				offset += snprintf(p_buf+offset, mlen-offset, 
					"<tt:Reverse>\r\n"
						"<tt:Mode>%s</tt:Mode>\r\n"
					"</tt:Reverse>\r\n",
					onvif_ReverseModeToString(p_ptz_cfg->Extension.PTControlDirection.Reverse));
			}

			offset += snprintf(p_buf+offset, mlen-offset, "</tt:PTControlDirection>\r\n");
		} 

		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Extension>\r\n");
	}

	return offset;
}

int build_ptz_GetNodes_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	PTZNodeList * p_node = g_onvif_cfg.ptz_node;

	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GetNodesResponse>\r\n");

	while (p_node)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tptz:PTZNode token=\"%s\" "
				"FixedHomePosition=\"%s\" "
				"GeoMove=\"%s\">\r\n",
			p_node->PTZNode.token, 
			p_node->PTZNode.FixedHomePosition ? "true" : "false",
			p_node->PTZNode.GeoMove ? "true" : "false");
		
		offset += build_PTZNode_xml(p_buf+offset, mlen-offset, &p_node->PTZNode);
		offset += snprintf(p_buf+offset, mlen-offset, "</tptz:PTZNode>\r\n");

		p_node = p_node->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:GetNodesResponse>\r\n");	

	return offset;
}


int build_ptz_GetNode_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	PTZNodeList * p_node = onvif_find_PTZNode(g_onvif_cfg.ptz_node, argv);
	if (NULL == p_node)
	{
		return ONVIF_ERR_NoEntity;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GetNodeResponse>\r\n");

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tptz:PTZNode token=\"%s\" "
			"FixedHomePosition=\"%s\" "
			"GeoMove=\"%s\">\r\n",
		p_node->PTZNode.token, 
		p_node->PTZNode.FixedHomePosition ? "true" : "false",
		p_node->PTZNode.GeoMove ? "true" : "false");
	offset += build_PTZNode_xml(p_buf+offset, mlen-offset, &p_node->PTZNode);
	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:PTZNode>\r\n");

	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:GetNodeResponse>\r\n");

	return offset;
}

int build_ptz_GetConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	PTZConfigurationList * p_ptz_cfg = g_onvif_cfg.ptz_cfg;

	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GetConfigurationsResponse>\r\n");

	while (p_ptz_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tptz:PTZConfiguration token=\"%s\" "
				"MoveRamp=\"%d\" "
				"PresetRamp=\"%d\" "
				"PresetTourRamp=\"%d\">\r\n", 
			p_ptz_cfg->Configuration.token, 
			p_ptz_cfg->Configuration.MoveRamp,
			p_ptz_cfg->Configuration.PresetRamp, 
			p_ptz_cfg->Configuration.PresetTourRamp);
		
		offset += build_PTZConfiguration_xml(p_buf+offset, mlen-offset, &p_ptz_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tptz:PTZConfiguration>\r\n");

		p_ptz_cfg = p_ptz_cfg->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:GetConfigurationsResponse>\r\n");	

	return offset;
}

int build_ptz_GetCompatibleConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	ptz_GetCompatibleConfigurations_REQ * p_res = (ptz_GetCompatibleConfigurations_REQ *)argv;
	PTZConfigurationList * p_ptz_cfg = g_onvif_cfg.ptz_cfg;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GetCompatibleConfigurationsResponse>\r\n");

	while (p_ptz_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tptz:PTZConfiguration token=\"%s\" "
				"MoveRamp=\"%d\" "
				"PresetRamp=\"%d\" "
				"PresetTourRamp=\"%d\">\r\n", 
			p_ptz_cfg->Configuration.token, 
			p_ptz_cfg->Configuration.MoveRamp,
			p_ptz_cfg->Configuration.PresetRamp, 
			p_ptz_cfg->Configuration.PresetTourRamp);
		
		offset += build_PTZConfiguration_xml(p_buf+offset, mlen-offset, &p_ptz_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tptz:PTZConfiguration>\r\n");

		p_ptz_cfg = p_ptz_cfg->next;
	}	

	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:GetCompatibleConfigurationsResponse>\r\n");	

	return offset;   
}

int build_ptz_GetConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	PTZConfigurationList * p_ptz_cfg = onvif_find_PTZConfiguration(g_onvif_cfg.ptz_cfg, argv);
	if (NULL == p_ptz_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GetConfigurationResponse>\r\n");

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tptz:PTZConfiguration token=\"%s\" "
			"MoveRamp=\"%d\" "
			"PresetRamp=\"%d\" "
			"PresetTourRamp=\"%d\">\r\n", 
		p_ptz_cfg->Configuration.token,
		p_ptz_cfg->Configuration.MoveRampFlag ? p_ptz_cfg->Configuration.MoveRamp : 0,
		p_ptz_cfg->Configuration.PresetRampFlag ? p_ptz_cfg->Configuration.PresetRamp : 0,
		p_ptz_cfg->Configuration.PresetTourRampFlag ? p_ptz_cfg->Configuration.PresetTourRamp : 0);
	
	offset += build_PTZConfiguration_xml(p_buf+offset, mlen-offset, &p_ptz_cfg->Configuration);
	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:PTZConfiguration>\r\n");
		
	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:GetConfigurationResponse>\r\n");	

	return offset;
}

int build_ptz_GetConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	PTZNodeList * p_node;
	PTZConfigurationList * p_ptz_cfg = onvif_find_PTZConfiguration(g_onvif_cfg.ptz_cfg, argv);
	if (NULL == p_ptz_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	p_node = onvif_find_PTZNode(g_onvif_cfg.ptz_node, p_ptz_cfg->Configuration.NodeToken);
	assert(p_node);
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GetConfigurationOptionsResponse>\r\n");
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:PTZConfigurationOptions>\r\n");
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Spaces>\r\n");
	offset += build_PTZSpaces_xml(p_buf+offset, mlen-offset, &p_node->PTZNode.SupportedPTZSpaces);
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:Spaces>\r\n");

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:PTZTimeout>\r\n"
			"<tt:Min>PT%dS</tt:Min>\r\n"
			"<tt:Max>PT%dS</tt:Max>\r\n"
		"</tt:PTZTimeout>\r\n", 
		p_ptz_cfg->Options.PTZTimeout.Min, 
		p_ptz_cfg->Options.PTZTimeout.Max);

	if (p_ptz_cfg->Options.PTControlDirectionFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:PTControlDirection>\r\n");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:EFlip>\r\n");
		if (p_ptz_cfg->Options.PTControlDirection.EFlipMode_OFF)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>OFF</tt:Mode>\r\n");
		}
		if (p_ptz_cfg->Options.PTControlDirection.EFlipMode_ON)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>ON</tt:Mode>\r\n");
		}
		if (p_ptz_cfg->Options.PTControlDirection.EFlipMode_Extended)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>Extended</tt:Mode>\r\n");
		}
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:EFlip>\r\n");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Reverse>\r\n");
		if (p_ptz_cfg->Options.PTControlDirection.ReverseMode_OFF)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>OFF</tt:Mode>\r\n");
		}
		if (p_ptz_cfg->Options.PTControlDirection.ReverseMode_ON)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>ON</tt:Mode>\r\n");
		}
		if (p_ptz_cfg->Options.PTControlDirection.ReverseMode_AUTO)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>AUTO</tt:Mode>\r\n");
		}
		if (p_ptz_cfg->Options.PTControlDirection.ReverseMode_Extended)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>Extended</tt:Mode>\r\n");
		}
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Reverse>\r\n");
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:PTControlDirection>\r\n");
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:PTZConfigurationOptions>\r\n");	
	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:GetConfigurationOptionsResponse>\r\n");	

	return offset;
}


int build_ptz_GetStatus_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	char str[100] = {'\0'};
	onvif_PTZStatus ptz_status;
	/*
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, argv);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	*/
	memset(&ptz_status, 0, sizeof(onvif_PTZStatus));

	if (ONVIF_OK != onvif_ptz_GetStatus(&ptz_status))
	{
		return -1;
	}

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tptz:GetStatusResponse>\r\n"
		"<tptz:PTZStatus>\r\n");

	if (ptz_status.PositionFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Position>\r\n");
		if (ptz_status.Position.PanTiltFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:PanTilt x=\"%0.1f\" y=\"%0.1f\" "
					"space=\"http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace\" />\r\n",
				ptz_status.Position.PanTilt.x,
				ptz_status.Position.PanTilt.y);
		}
		if (ptz_status.Position.ZoomFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:Zoom x=\"%0.1f\" "
					"space=\"http://www.onvif.org/ver10/tptz/ZoomSpaces/PositionGenericSpace\" />\r\n",
				ptz_status.Position.Zoom.x);
		}
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Position>\r\n");
	}

	if (ptz_status.MoveStatusFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:MoveStatus>\r\n");
		if (ptz_status.MoveStatus.PanTiltFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:PanTilt>%s</tt:PanTilt>\r\n", onvif_MoveStatusToString(ptz_status.MoveStatus.PanTilt));
		}
		if (ptz_status.MoveStatus.ZoomFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Zoom>%s</tt:Zoom>\r\n", onvif_MoveStatusToString(ptz_status.MoveStatus.Zoom));
		}
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:MoveStatus>\r\n");
	}
	
	if (ptz_status.ErrorFlag && strlen(ptz_status.Error) > 0)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Error>%s</tt:Error>\r\n", ptz_status.Error);
	}

	onvif_format_datetime_str(ptz_status.UtcTime, 1, "%Y-%m-%dT%H:%M:%SZ", str, sizeof(str));

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:UtcTime>%s</tt:UtcTime>\r\n", str);
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"</tptz:PTZStatus>\r\n"
		"</tptz:GetStatusResponse>\r\n");
	
	return offset;
}


int build_ptz_ContinuousMove_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:ContinuousMoveResponse />");	
	return offset;
}

int build_ptz_Stop_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:StopResponse />");	
	return offset;
}

int build_ptz_AbsoluteMove_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:AbsoluteMoveResponse />");	
	return offset;
}

int build_ptz_RelativeMove_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:RelativeMoveResponse />");	
	return offset;
}

int build_ptz_SetPreset_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tptz:SetPresetResponse>\r\n"
			"<tptz:PresetToken>%s</tptz:PresetToken>\r\n"
		"</tptz:SetPresetResponse>\r\n", 
		argv);

	return offset;
}

int build_ptz_GetPresets_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	PTZPresetList * p_preset;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, argv);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GetPresetsResponse>");

	p_preset = p_profile->presets;
	while (p_preset)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tptz:Preset token=\"%s\">\r\n", p_preset->PTZPreset.token);
		offset += build_PTZPreset_xml(p_buf + offset, mlen - offset, &p_preset->PTZPreset);
		offset += snprintf(p_buf+offset, mlen-offset, "</tptz:Preset>\r\n");

		p_preset = p_preset->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:GetPresetsResponse>");
	
	return offset;
}

int build_ptz_RemovePreset_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:RemovePresetResponse />");	
	return offset;
}

int build_ptz_GotoPreset_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GotoPresetResponse />");	
	return offset;
}

int build_ptz_GotoHomePosition_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GotoHomePositionResponse />");	
	return offset;
}

int build_ptz_SetHomePosition_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:SetHomePositionResponse />");	
	return offset;
}

int build_ptz_SetConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:SetConfigurationResponse />");
	return offset;
}

int build_ptz_GetPresetTours_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	PresetTourList * p_tour;
	ptz_GetPresetTours_REQ * p_res = (ptz_GetPresetTours_REQ *) argv;

	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GetPresetToursResponse>\r\n");
	
	p_tour = p_profile->preset_tour;
	while (p_tour)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tptz:PresetTour token=\"%s\">\r\n", p_tour->PresetTour.token);
		offset += build_PresetTour_xml(p_buf+offset, mlen-offset, &p_tour->PresetTour);
		offset += snprintf(p_buf+offset, mlen-offset, "</tptz:PresetTour>\r\n");

		p_tour = p_tour->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:GetPresetToursResponse>\r\n");

	return offset;
}

int build_ptz_GetPresetTours_new_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	ptz_GetPresetTours_REQ * p_res = (ptz_GetPresetTours_REQ *) argv;

	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GetPresetToursResponse>\r\n");
	if (1)
	{
		int tourCnt = get_tour_cnt();
		log_print(HT_LOG_INFO, "get_tour_cnt:%d\n", tourCnt);
		if (tourCnt == 0)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "</tptz:GetPresetToursResponse>\r\n");
			return offset;
		}
		int tourIdx = 0;
		
		int i;
		for (i = 0; i < MAX_TOUR_CNT; i++)
		{
			ptz_tour_t * ptour = &g_tour_ctx.ptz_tours[i];
			if (ptour->free == 1)
			{
				log_print(HT_LOG_DBG, "continue\n");
				continue;
			}

			if (tourIdx >= tourCnt)
			{
				break;
			}
			log_print(HT_LOG_INFO, "tour(idx :%d, token:%s, free:%d, active:%d, spotcnt:%d)\n", ptour->index, ptour->token, ptour->free, ptour->active, ptour->spot_cnt);

			offset += snprintf(p_buf+offset, mlen-offset, "<tptz:PresetTour token=\"%s\">\r\n", ptour->token);
			offset += build_PresetTour_by_g_tour_ctx_xml(p_buf+offset, mlen-offset, ptour);
			offset += snprintf(p_buf+offset, mlen-offset, "</tptz:PresetTour>\r\n");
			

		}
		
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:GetPresetToursResponse>\r\n");

	return offset;
}

int build_ptz_GetPresetTour_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	ptz_GetPresetTour_REQ * p_res = (ptz_GetPresetTour_REQ *) argv;

	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}

	
	ptz_tour_t * ptour = find_ptz_tour(p_res->PresetTourToken);
	//p_tour = onvif_find_PresetTour(p_profile->preset_tour, p_res->PresetTourToken);
	if (NULL == ptour)
	{
		return ONVIF_ERR_NoToken;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GetPresetTourResponse>\r\n");	

	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:PresetTour token=\"%s\">\r\n", ptour->token);
	offset += build_PresetTour_by_g_tour_ctx_xml(p_buf+offset, mlen-offset, ptour);
	//offset += build_PresetTour_xml(p_buf+offset, mlen-offset, ptour);
	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:PresetTour>\r\n");

	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:GetPresetTourResponse>\r\n");

	return offset;
}

int build_ptz_GetPresetTourOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	ptz_GetPresetTourOptions_RES * p_res = (ptz_GetPresetTourOptions_RES *) argv;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tptz:GetPresetTourOptionsResponse>\r\n"
		"<tptz:Options>\r\n");
	offset += build_PTZPresetTourOptions_xml(p_buf+offset, mlen-offset, &p_res->Options);    
	offset += snprintf(p_buf+offset, mlen-offset, 
		"</tptz:Options>\r\n"
		"</tptz:GetPresetTourOptionsResponse>\r\n");

	return offset;
}

int build_ptz_CreatePresetTour_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	ptz_CreatePresetTour_RES * p_res = (ptz_CreatePresetTour_RES *) argv;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tptz:CreatePresetTourResponse>\r\n"
		"<tptz:PresetTourToken>%s</tptz:PresetTourToken>\r\n"
		"</tptz:CreatePresetTourResponse>\r\n",
		p_res->PresetTourToken);

	return offset;
}

int build_ptz_ModifyPresetTour_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:ModifyPresetTourResponse />");	
	return offset;
}

int build_ptz_OperatePresetTour_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:OperatePresetTourResponse />");	
	return offset;
}

int build_ptz_RemovePresetTour_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:RemovePresetTourResponse />");	
	return offset;
}

int build_ptz_SendAuxiliaryCommand_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;    
	ptz_SendAuxiliaryCommand_RES * p_res = (ptz_SendAuxiliaryCommand_RES *) argv;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tptz:SendAuxiliaryCommandResponse>\r\n"
			"<tptz:AuxiliaryResponse>%s</tptz:AuxiliaryResponse>\r\n"
		"</tptz:SendAuxiliaryCommandResponse>\r\n",
		p_res->AuxiliaryResponse);

	return offset;
}

int build_ptz_GeoMove_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GeoMoveResponse />");	
	return offset;
}

#endif // end of PTZ_SUPPORT

/***************************************************************************************/

#ifdef VIDEO_ANALYTICS

int build_Config_xml(char * p_buf, int mlen, onvif_Config * p_res)
{
	int offset = 0;
	SimpleItemList * p_simpleitem;
	ElementItemList * p_elementitem;

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Parameters>\r\n");

	p_simpleitem = p_res->Parameters.SimpleItem;
	while (p_simpleitem)
	{
		offset += build_SimpleItem_xml(p_buf+offset, mlen-offset, &p_simpleitem->SimpleItem);
		
		p_simpleitem = p_simpleitem->next;
	}

	p_elementitem = p_res->Parameters.ElementItem;
	while (p_elementitem)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:ElementItem Name=\"%s\">\r\n", p_elementitem->ElementItem.Name);

		if (p_elementitem->ElementItem.AnyFlag && p_elementitem->ElementItem.Any)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "%s", p_elementitem->ElementItem.Any);
		}

		offset += snprintf(p_buf+offset, mlen-offset, "</tt:ElementItem>\r\n");
		
		p_elementitem = p_elementitem->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:Parameters>\r\n");
	
	return offset;
}

int build_AnalyticsEngineConfiguration_xml(char * p_buf, int mlen, onvif_AnalyticsEngineConfiguration * p_res)
{
	int offset = 0;
	ConfigList * p_config;
	
	p_config = p_res->AnalyticsModule;
	while (p_config)
	{
		if (0)//p_config->Config.attrFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:AnalyticsModule Name=\"%s\" Type=\"%s\" %s>\r\n", p_config->Config.Name, p_config->Config.Type, p_config->Config.attr);
		}
		else
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:AnalyticsModule Name=\"%s\" Type=\"%s\">\r\n", p_config->Config.Name, p_config->Config.Type);
		}
		
		offset += build_Config_xml(p_buf+offset, mlen-offset, &p_config->Config);

		offset += snprintf(p_buf+offset, mlen-offset, "</tt:AnalyticsModule>\r\n");

		p_config = p_config->next;
	}

	return offset;
}

int build_RuleEngineConfiguration_xml(char * p_buf, int mlen, onvif_RuleEngineConfiguration * p_res)
{
	int offset = 0;
	ConfigList * p_config;
	
	p_config = p_res->Rule;
	while (p_config)
	{
		if (0)//p_config->Config.attrFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Rule Name=\"%s\" Type=\"%s\" %s>\r\n", p_config->Config.Name, p_config->Config.Type, p_config->Config.attr);
		}
		else
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Rule Name=\"%s\" Type=\"%s\">\r\n", p_config->Config.Name, p_config->Config.Type);
		}
		
		offset += build_Config_xml(p_buf+offset, mlen-offset, &p_config->Config);

		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Rule>\r\n");

		p_config = p_config->next;
	}

	return offset;
}

int build_VideoAnalyticsConfiguration_xml(char * p_buf, int mlen, onvif_VideoAnalyticsConfiguration * p_res)
{
	int offset = 0;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Name>%s</tt:Name>\r\n"
		"<tt:UseCount>%d</tt:UseCount>\r\n", 
		p_res->Name, 
		p_res->UseCount);

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:AnalyticsEngineConfiguration>\r\n");
	offset += build_AnalyticsEngineConfiguration_xml(p_buf+offset, mlen-offset, &p_res->AnalyticsEngineConfiguration);
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:AnalyticsEngineConfiguration>\r\n");

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:RuleEngineConfiguration>\r\n");
	offset += build_RuleEngineConfiguration_xml(p_buf+offset, mlen-offset, &p_res->RuleEngineConfiguration);
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:RuleEngineConfiguration>\r\n");
	
	return offset;
}

int build_ItemListDescription_xml(char * p_buf, int mlen, onvif_ItemListDescription * p_res)
{
	int offset = 0;
	SimpleItemDescriptionList * p_simpleitem_desc;
	
	p_simpleitem_desc = p_res->SimpleItemDescription;
	while (p_simpleitem_desc)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:SimpleItemDescription Name=\"%s\" Type=\"%s\" />",
			p_simpleitem_desc->SimpleItemDescription.Name, 
			p_simpleitem_desc->SimpleItemDescription.Type);

		p_simpleitem_desc = p_simpleitem_desc->next;
	}

	p_simpleitem_desc = p_res->ElementItemDescription;
	while (p_simpleitem_desc)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:ElementItemDescription Name=\"%s\" Type=\"%s\" />",
			p_simpleitem_desc->SimpleItemDescription.Name, 
			p_simpleitem_desc->SimpleItemDescription.Type);

		p_simpleitem_desc = p_simpleitem_desc->next;
	}

	return offset;
}

int build_ConfigDescription_Messages_xml(char * p_buf, int mlen, onvif_ConfigDescription_Messages * p_res)
{
	int offset = 0;

	if (p_res->SourceFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Source>");
		offset += build_ItemListDescription_xml(p_buf+offset, mlen-offset, &p_res->Source);		
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Source>");
	}

	if (p_res->KeyFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Key>");
		offset += build_ItemListDescription_xml(p_buf+offset, mlen-offset, &p_res->Key);		
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Key>");
	}

	if (p_res->DataFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Data>");
		offset += build_ItemListDescription_xml(p_buf+offset, mlen-offset, &p_res->Data);		
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Data>");
	}

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:ParentTopic>%s</tt:ParentTopic>", 
		p_res->ParentTopic);
	
	return offset;
}

int build_ConfigDescription_xml(char * p_buf, int mlen, onvif_ConfigDescription * p_res)
{
	int offset = 0;
	SimpleItemDescriptionList * p_simpleitem_desc;
	ConfigDescription_MessagesList * p_cfg_desc_msg;

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Parameters>");

	p_simpleitem_desc = p_res->Parameters.SimpleItemDescription;
	while (p_simpleitem_desc)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:SimpleItemDescription Name=\"%s\" Type=\"%s\" />", 
			p_simpleitem_desc->SimpleItemDescription.Name, 
			p_simpleitem_desc->SimpleItemDescription.Type);

		p_simpleitem_desc = p_simpleitem_desc->next;
	}

	p_simpleitem_desc = p_res->Parameters.ElementItemDescription;
	while (p_simpleitem_desc)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:ElementItemDescription Name=\"%s\" Type=\"%s\" />", 
			p_simpleitem_desc->SimpleItemDescription.Name, 
			p_simpleitem_desc->SimpleItemDescription.Type);

		p_simpleitem_desc = p_simpleitem_desc->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:Parameters>");

	p_cfg_desc_msg = p_res->Messages;
	while (p_cfg_desc_msg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:Messages IsProperty=\"%s\">", 
			p_cfg_desc_msg->Messages.IsProperty ? "true" : "false");
		offset += build_ConfigDescription_Messages_xml(p_buf+offset, mlen-offset, &p_cfg_desc_msg->Messages);
		offset += snprintf(p_buf+offset, mlen-offset, 
			"</tt:Messages>");
		
		p_cfg_desc_msg = p_cfg_desc_msg->next;
	}

	return offset;
}

int build_SupportedRules_xml(char * p_buf, int mlen, onvif_SupportedRules * p_res)
{
	uint32 i;
	int offset = 0;
	ConfigDescriptionList * p_cfg_desc;

	for (i = 0; i < p_res->sizeRuleContentSchemaLocation; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:RuleContentSchemaLocation>%s</tt:RuleContentSchemaLocation>", p_res->RuleContentSchemaLocation[i]);
	}

	p_cfg_desc = p_res->RuleDescription;
	while (p_cfg_desc)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:RuleDescription Name=\"%s\" fixed=\"%s\" maxInstances=\"%d\">", 
			p_cfg_desc->ConfigDescription.Name,
			p_cfg_desc->ConfigDescription.fixed ? "true" : "false",
			p_cfg_desc->ConfigDescription.maxInstances);

		offset += build_ConfigDescription_xml(p_buf+offset, mlen-offset, &p_cfg_desc->ConfigDescription);
		
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:RuleDescription>");
		
		p_cfg_desc = p_cfg_desc->next;
	}

	return offset;
}

int build_SupportedAnalyticsModules_xml(char * p_buf, int mlen, onvif_SupportedAnalyticsModules * p_res)
{
	uint32 i;
	int offset = 0;
	ConfigDescriptionList * p_cfg_desc;

	for (i = 0; i < p_res->sizeAnalyticsModuleContentSchemaLocation; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:AnalyticsModuleContentSchemaLocation>%s</tt:AnalyticsModuleContentSchemaLocation>",
			p_res->AnalyticsModuleContentSchemaLocation[i]);
	}

	p_cfg_desc = p_res->AnalyticsModuleDescription;
	while (p_cfg_desc)
	{		
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:AnalyticsModuleDescription Name=\"%s\" fixed=\"%s\" maxInstances=\"%d\">", 
			p_cfg_desc->ConfigDescription.Name,
			p_cfg_desc->ConfigDescription.fixed ? "true" : "false",
			p_cfg_desc->ConfigDescription.maxInstances);

		offset += build_ConfigDescription_xml(p_buf+offset, mlen-offset, &p_cfg_desc->ConfigDescription);
		
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:AnalyticsModuleDescription>");
		
		p_cfg_desc = p_cfg_desc->next;
	}

	return offset;
}

int build_tan_GetSupportedRules_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tan_GetSupportedRules_RES * p_res = (tan_GetSupportedRules_RES *)argv;
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tan:GetSupportedRulesResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tan:SupportedRules>");
	offset += build_SupportedRules_xml(p_buf+offset, mlen-offset, &p_res->SupportedRules);
	offset += snprintf(p_buf+offset, mlen-offset, "</tan:SupportedRules>");
	offset += snprintf(p_buf+offset, mlen-offset, "</tan:GetSupportedRulesResponse>");

	return offset;
}

int build_tan_CreateRules_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tan:CreateRulesResponse />");
	return offset;
}

int build_tan_DeleteRules_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tan:DeleteRulesResponse />");
	return offset;
}

int build_tan_GetRules_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tan_GetRules_RES * p_res = (tan_GetRules_RES *)argv;
	ConfigList * p_config;

	onvif_renew_VideoAnalyticsConfiguration();
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tan:GetRulesResponse>\r\n");

	p_config = p_res->Rule;
	while (p_config)
	{
		if (0)//p_config->Config.attrFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tan:Rule Name=\"%s\" Type=\"%s\" %s>\r\n", 
				p_config->Config.Name, 
				p_config->Config.Type, 
				p_config->Config.attr);
		}
		else
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tan:Rule Name=\"%s\" Type=\"%s\">\r\n", 
				p_config->Config.Name, 
				p_config->Config.Type);
		}

		offset += build_Config_xml(p_buf+offset, mlen-offset, &p_config->Config);

		offset += snprintf(p_buf+offset, mlen-offset, "</tan:Rule>\r\n");

		p_config = p_config->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tan:GetRulesResponse>");

	return offset;
}

int build_tan_ModifyRules_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tan:ModifyRulesResponse />");
	return offset;
}

int build_tan_CreateAnalyticsModules_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tan:CreateAnalyticsModulesResponse />");
	return offset;
}

int build_tan_DeleteAnalyticsModules_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tan:DeleteAnalyticsModulesResponse />");
	return offset;
}

int build_tan_GetAnalyticsModules_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tan_GetAnalyticsModules_RES * p_res = (tan_GetAnalyticsModules_RES *)argv;
	ConfigList * p_config;

	onvif_renew_VideoAnalyticsConfiguration();
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tan:GetAnalyticsModulesResponse>");

	p_config = p_res->AnalyticsModule;
	while (p_config)
	{
		if (0)//p_config->Config.attrFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tan:AnalyticsModule Name=\"%s\" Type=\"%s\" %s>", 
				p_config->Config.Name, 
				p_config->Config.Type, 
				p_config->Config.attr);
		}
		else
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tan:AnalyticsModule Name=\"%s\" Type=\"%s\">", 
				p_config->Config.Name, 
				p_config->Config.Type);
		}

		offset += build_Config_xml(p_buf+offset, mlen-offset, &p_config->Config);
		offset += snprintf(p_buf+offset, mlen-offset, "</tan:AnalyticsModule>");
		
		p_config = p_config->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tan:GetAnalyticsModulesResponse>");

	return offset;
}

int build_tan_ModifyAnalyticsModules_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tan:ModifyAnalyticsModulesResponse />");
	return offset;
}

int build_tan_GetRuleOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    ConfigDescriptionList * p_desc;
    ConfigOptionsList * p_options;
    tan_GetRuleOptions_REQ * p_res = (tan_GetRuleOptions_REQ *)argv;
    VideoAnalyticsConfigurationList * p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_res->ConfigurationToken);
    if (NULL == p_va_cfg)
    {
        return ONVIF_ERR_NoConfig;
    }

    offset += snprintf(p_buf+offset, mlen-offset, "<tan:GetRuleOptionsResponse>");

    p_desc = p_va_cfg->SupportedRules.RuleDescription;
    while (p_desc)
    {
        if (p_res->RuleType[0] == '\0' || 
            soap_strcmp(p_res->RuleType, p_desc->ConfigDescription.Name) == 0)
        {
            p_options = p_desc->ConfigOptions;
            while (p_options)
            {
                offset += snprintf(p_buf+offset, mlen-offset, "<tan:RuleOptions");

                if (p_options->Options.RuleTypeFlag)
                {
                    offset += snprintf(p_buf+offset, mlen-offset, 
                        " RuleType=\"%s\"", 
                        p_options->Options.RuleType);
                }
                
                offset += snprintf(p_buf+offset, mlen-offset, 
                    " Name=\"%s\" Type=\"%s\"",
                    p_options->Options.Name, 
                    p_options->Options.Type);

                if (p_options->Options.AnalyticsModuleFlag)
                {
                    offset += snprintf(p_buf+offset, mlen-offset, 
                        " AnalyticsModule=\"%s\"", 
                        p_options->Options.AnalyticsModule);
                }

                offset += snprintf(p_buf+offset, mlen-offset, ">");
                
                if (p_options->Options.any)
                {
                    offset += snprintf(p_buf+offset, mlen-offset, "%s", p_options->Options.any);
                }
                
                offset += snprintf(p_buf+offset, mlen-offset, "</tan:RuleOptions>");
                
                p_options = p_options->next;
            }
        }
        
        p_desc = p_desc->next;
    }    

    offset += snprintf(p_buf+offset, mlen-offset, "</tan:GetRuleOptionsResponse>");

    return offset;
}

int build_tan_GetSupportedAnalyticsModules_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tan_GetSupportedAnalyticsModules_REQ * p_res = (tan_GetSupportedAnalyticsModules_REQ *)argv;
	VideoAnalyticsConfigurationList * p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_res->ConfigurationToken);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tan:GetSupportedAnalyticsModulesResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tan:SupportedAnalyticsModules>");
	offset += build_SupportedAnalyticsModules_xml(p_buf+offset, mlen-offset, &p_va_cfg->SupportedAnalyticsModules);
	offset += snprintf(p_buf+offset, mlen-offset, "</tan:SupportedAnalyticsModules>");
	offset += snprintf(p_buf+offset, mlen-offset, "</tan:GetSupportedAnalyticsModulesResponse>");

	return offset;
}

int build_tan_GetAnalyticsModuleOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	ConfigDescriptionList * p_desc;
	ConfigOptionsList * p_options;
	tan_GetAnalyticsModuleOptions_REQ * p_res = (tan_GetAnalyticsModuleOptions_REQ *)argv;
	VideoAnalyticsConfigurationList * p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_res->ConfigurationToken);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tan:GetAnalyticsModuleOptionsResponse>");

	p_desc = p_va_cfg->SupportedAnalyticsModules.AnalyticsModuleDescription;
	while (p_desc)
	{
		if (p_res->Type[0] != 0 && soap_strcmp(p_desc->ConfigDescription.Name, p_res->Type))
		{
			p_desc = p_desc->next;
			continue;
		}

		p_options = p_desc->ConfigOptions;
		while (p_options)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tan:Options");

			if (p_options->Options.RuleTypeFlag)
			{
				offset += snprintf(p_buf+offset, mlen-offset, " RuleType=\"%s\"", p_options->Options.RuleType);
			}

			offset += snprintf(p_buf+offset, mlen-offset, " Name=\"%s\" Type=\"%s\"", p_options->Options.Name, p_options->Options.Type);

			if (p_options->Options.AnalyticsModuleFlag)
			{
				offset += snprintf(p_buf+offset, mlen-offset, " AnalyticsModule=\"%s\"", p_options->Options.AnalyticsModule);
			}

			offset += snprintf(p_buf+offset, mlen-offset, ">");

			if (p_options->Options.any)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "%s", p_options->Options.any);
			}

			offset += snprintf(p_buf+offset, mlen-offset, "</tan:Options>");

			p_options = p_options->next;
		}

		p_desc = p_desc->next;
	}    

	offset += snprintf(p_buf+offset, mlen-offset, "</tan:GetAnalyticsModuleOptionsResponse>");

	return offset;
}

int build_tan_GetSupportedMetadata_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int i;
	int offset = 0;
	tan_GetSupportedMetadata_RES * p_res = (tan_GetSupportedMetadata_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tan:GetSupportedMetadataResponse>");

	for (i = 0; i < p_res->sizeAnalyticsModule; i++)
	{
		if (p_res->AnalyticsModule[i].attrFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tan:AnalyticsModule Type=\"%s\" %s>", 
				p_res->AnalyticsModule[i].Type, 
				p_res->AnalyticsModule[i].attr);
		}
		else
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tan:AnalyticsModule Type=\"%s\">", p_res->AnalyticsModule[i].Type);
		}

		if (p_res->AnalyticsModule[i].Frame)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "%s", p_res->AnalyticsModule[i].Frame);
		}

		offset += snprintf(p_buf+offset, mlen-offset, "</tan:AnalyticsModule>");
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tan:GetSupportedMetadataResponse>");

	return offset;
}

#endif // end of VIDEO_ANALYTICS

/***************************************************************************************/

#ifdef PROFILE_G_SUPPORT

int build_RecordingSourceInformation_xml(char * p_buf, int mlen, onvif_RecordingSourceInformation * p_res)
{
	int offset = 0;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Source>\r\n"
			"<tt:SourceId>%s</tt:SourceId>\r\n"
			"<tt:Name>%s</tt:Name>\r\n"
			"<tt:Location>%s</tt:Location>\r\n"
			"<tt:Description>%s</tt:Description>\r\n"
			"<tt:Address>%s</tt:Address>\r\n"
		"</tt:Source>\r\n",
		p_res->SourceId,
		p_res->Name,
		p_res->Location,
		p_res->Description,
		p_res->Address);
		
	return offset;		
}

int build_TrackInformation_xml(char * p_buf, int mlen, onvif_TrackInformation * p_res)
{
	int offset = 0;
	char DataFrom[64], DataTo[64];

	onvif_get_time_str_s(DataFrom, sizeof(DataFrom), p_res->DataFrom, 0);
	onvif_get_time_str_s(DataTo, sizeof(DataTo), p_res->DataTo, 0);

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:TrackToken>%s</tt:TrackToken>"
		"<tt:TrackType>%s</tt:TrackType>"
		"<tt:Description>%s</tt:Description>"
		"<tt:DataFrom>%s</tt:DataFrom>"
		"<tt:DataTo>%s</tt:DataTo>",
		p_res->TrackToken,
		onvif_TrackTypeToString(p_res->TrackType),
		p_res->Description,
		DataFrom,
		DataTo);

	return offset;			
}

int build_RecordingInformation_xml(char * p_buf, int mlen, onvif_RecordingInformation * p_res)
{
	uint32 i;
	int offset = 0;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tt:RecordingToken>%s</tt:RecordingToken>", 
	    p_res->RecordingToken);

	offset += build_RecordingSourceInformation_xml(p_buf+offset, mlen-offset, &p_res->Source);

	if (p_res->EarliestRecordingFlag)
	{
		char EarliestRecording[64];
		onvif_get_time_str_s(EarliestRecording, sizeof(EarliestRecording), p_res->EarliestRecording, 0);

		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:EarliestRecording>%s</tt:EarliestRecording>", 
		    EarliestRecording);
	}

	if (p_res->LatestRecordingFlag)
	{
		char LatestRecording[64];
		onvif_get_time_str_s(LatestRecording, sizeof(LatestRecording), p_res->LatestRecording, 0);

		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:LatestRecording>%s</tt:LatestRecording>", 
		    LatestRecording);
	}

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tt:Content>%s</tt:Content>", 
	    p_res->Content);

	for (i = 0; i < p_res->sizeTrack; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Track>");
		offset += build_TrackInformation_xml(p_buf+offset, mlen-offset, &p_res->Track[i]);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Track>");
	}

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tt:RecordingStatus>%s</tt:RecordingStatus>", 
		onvif_RecordingStatusToString(p_res->RecordingStatus));

	return offset;		
}

int build_TrackAttributes_xml(char * p_buf, int mlen, onvif_TrackAttributes * p_res)
{
	int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, "<tt:TrackInformation>");
	offset += build_TrackInformation_xml(p_buf+offset, mlen-offset, &p_res->TrackInformation);
    offset += snprintf(p_buf+offset, mlen-offset, "</tt:TrackInformation>");

	if (p_res->VideoAttributesFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:VideoAttributes>");

		if (p_res->VideoAttributes.BitrateFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
			    "<tt:Bitrate>%d</tt:Bitrate>", 
			    p_res->VideoAttributes.Bitrate);
		}
		
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:Width>%d</tt:Width>"
			"<tt:Height>%d</tt:Height>"
			"<tt:Encoding>%s</tt:Encoding>"
			"<tt:Framerate>%0.1f</tt:Framerate>",
			p_res->VideoAttributes.Width,
			p_res->VideoAttributes.Height,
			onvif_VideoEncodingToString(p_res->VideoAttributes.Encoding),
			p_res->VideoAttributes.Framerate);			

		offset += snprintf(p_buf+offset, mlen-offset, "</tt:VideoAttributes>");		
	}

	if (p_res->AudioAttributesFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:AudioAttributes>");

		if (p_res->AudioAttributes.BitrateFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
			    "<tt:Bitrate>%d</tt:Bitrate>", 
			    p_res->AudioAttributes.Bitrate);
		}
		
		offset += snprintf(p_buf+offset, mlen-offset,
			"<tt:Encoding>%s</tt:Encoding>"
			"<tt:Samplerate>%d</tt:Samplerate>",
			onvif_AudioEncodingToString(p_res->AudioAttributes.Encoding),
			p_res->AudioAttributes.Samplerate);			

		offset += snprintf(p_buf+offset, mlen-offset, "</tt:AudioAttributes>");
	}

	if (p_res->MetadataAttributesFlag)
	{		
		if (p_res->MetadataAttributes.PtzSpacesFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
			    "<tt:MetadataAttributes PtzSpaces=\"%s\">", 
			    p_res->MetadataAttributes.PtzSpaces);
		}
		else
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
			    "<tt:MetadataAttributes>");
		}
		
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:CanContainPTZ>%s</tt:CanContainPTZ>"
			"<tt:CanContainAnalytics>%s</tt:CanContainAnalytics>"
			"<tt:CanContainNotifications>%s</tt:CanContainNotifications>",
			p_res->MetadataAttributes.CanContainPTZ ? "true" : "false",
			p_res->MetadataAttributes.CanContainAnalytics ? "true" : "false",
			p_res->MetadataAttributes.CanContainNotifications ? "true" : "false");

		offset += snprintf(p_buf+offset, mlen-offset, 
		    "</tt:MetadataAttributes>");
	}

	return offset;
}

int build_MediaAttributes_xml(char * p_buf, int mlen, onvif_MediaAttributes * p_res)
{
	uint32 i;
    int offset = 0;
	char From[64], Until[64];

	onvif_get_time_str_s(From, sizeof(From), p_res->From, 0);
	onvif_get_time_str_s(Until, sizeof(Until), p_res->Until, 0);
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tt:RecordingToken>%s</tt:RecordingToken>", 
	    p_res->RecordingToken);

	for (i = 0; i < p_res->sizeTrackAttributes; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:TrackAttributes>");
		offset += build_TrackAttributes_xml(p_buf+offset, mlen-offset, &p_res->TrackAttributes[i]);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:TrackAttributes>");
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:From>%s</tt:From>"
    	"<tt:Until>%s</tt:Until>", 
    	From,
    	Until);

	return offset;    	
}

int build_EndpointReferenceType_xml(char * p_buf, int mlen, onvif_EndpointReferenceType * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tt:Address>%s</tt:Address>", 
        p_res->Address);

    return offset;        
}

int build_TopicExpressionType_xml(char * p_buf, int mlen, onvif_EndpointReferenceType * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tt:Address>%s</tt:Address>", 
        p_res->Address);

    return offset;
}

int build_NotificationMessageHolderType_xml(char * p_buf, int mlen, onvif_NotificationMessageHolderType * p_res)
{
    int offset = 0;

    if (p_res->SubscriptionReferenceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, "<wsnt:SubscriptionReference>");
        offset += build_EndpointReferenceType_xml(p_buf+offset, mlen-offset, &p_res->SubscriptionReference);
        offset += snprintf(p_buf+offset, mlen-offset, "</wsnt:SubscriptionReference>");
    }

    if (p_res->TopicFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<wsnt:Topic Dialect=\"%s\">%s</wsnt:Topic>",
            p_res->Topic.Dialect,
            p_res->Topic.Topic);
    }

    if (p_res->ProducerReferenceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, "<wsnt:ProducerReference>");
        offset += build_EndpointReferenceType_xml(p_buf+offset, mlen-offset, &p_res->ProducerReference);
        offset += snprintf(p_buf+offset, mlen-offset, "</wsnt:ProducerReference>");
    }

    offset += snprintf(p_buf+offset, mlen-offset, "<wsnt:Message>");
    offset += build_Message_xml(p_buf+offset, mlen-offset, &p_res->Message);
    offset += snprintf(p_buf+offset, mlen-offset, "</wsnt:Message>");
    
    return offset;
}

int build_FindEventResult_xml(char * p_buf, int mlen, onvif_FindEventResult * p_res)
{
	int offset = 0;
	char TimeBuff[64];

	onvif_get_time_str_s(TimeBuff, sizeof(TimeBuff), p_res->Time, 0);

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:RecordingToken>%s</tt:RecordingToken>"
     	"<tt:TrackToken>%s</tt:TrackToken>"
     	"<tt:Time>%s</tt:Time>",
     	p_res->RecordingToken, 
     	p_res->TrackToken, 
     	TimeBuff);

    offset += snprintf(p_buf+offset, mlen-offset, "<tt:Event>");
	offset += build_NotificationMessageHolderType_xml(p_buf+offset, mlen-offset, &p_res->Event);
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:Event>");
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:StartStateEvent>%s</tt:StartStateEvent>",
		p_res->StartStateEvent ? "true" : "false");
	
	return offset;
}

int build_FindPTZPositionResult_xml(char * p_buf, int mlen, onvif_FindPTZPositionResult * p_res)
{
    int offset = 0;
	char TimeBuff[64];

	onvif_get_time_str_s(TimeBuff, sizeof(TimeBuff), p_res->Time, 0);

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:RecordingToken>%s</tt:RecordingToken>"
     	"<tt:TrackToken>%s</tt:TrackToken>"
     	"<tt:Time>%s</tt:Time>",
     	p_res->RecordingToken, 
     	p_res->TrackToken, 
     	TimeBuff);

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Position>");

	if (p_res->Position.PanTiltFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:PanTilt x=\"%0.1f\" y=\"%0.1f\" "
			    "space=\"http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace\" />",
			p_res->Position.PanTilt.x,
			p_res->Position.PanTilt.y);
	}
	
	if (p_res->Position.ZoomFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:Zoom x=\"%0.1f\" "
			    "space=\"http://www.onvif.org/ver10/tptz/ZoomSpaces/PositionGenericSpace\" />",
			p_res->Position.Zoom.x);
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:Position>");	
	
	return offset;
}

int build_tse_GetRecordingInformation_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	tse_GetRecordingInformation_RES * p_res = (tse_GetRecordingInformation_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tse:GetRecordingInformationResponse>"
	    "<tse:RecordingInformation>");	
	offset += build_RecordingInformation_xml(p_buf+offset, mlen-offset, &p_res->RecordingInformation);
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</tse:RecordingInformation>"
	    "</tse:GetRecordingInformationResponse>");

	return offset;
}

int build_tse_GetRecordingSummary_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	char DataFrom[64];
	char DataUntil[64];

	int offset = 0;
	tse_GetRecordingSummary_RES * p_res = (tse_GetRecordingSummary_RES *)argv;

	onvif_get_time_str_s(DataFrom, sizeof(DataFrom), p_res->Summary.DataFrom, 0);
	onvif_get_time_str_s(DataUntil, sizeof(DataUntil), p_res->Summary.DataUntil, 0);
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tse:GetRecordingSummaryResponse>"
			"<tse:Summary>"
				"<tt:DataFrom>%s</tt:DataFrom>"
				"<tt:DataUntil>%s</tt:DataUntil>"
				"<tt:NumberRecordings>%d</tt:NumberRecordings>"
			"</tse:Summary>"
		"</tse:GetRecordingSummaryResponse>",
		DataFrom,
		DataUntil,
		p_res->Summary.NumberRecordings);

	return offset;
}

int build_tse_GetMediaAttributes_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int i;
	int offset = 0;
	tse_GetMediaAttributes_RES * p_res = (tse_GetMediaAttributes_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tse:GetMediaAttributesResponse>");

	for (i = 0; i < p_res->sizeMediaAttributes; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tse:MediaAttributes>");
		offset += build_MediaAttributes_xml(p_buf+offset, mlen-offset, &p_res->MediaAttributes[i]);
		offset += snprintf(p_buf+offset, mlen-offset, "</tse:MediaAttributes>");
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tse:GetMediaAttributesResponse>");
	
	return offset;
}

int build_tse_FindRecordings_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	tse_FindRecordings_RES * p_res = (tse_FindRecordings_RES *)argv;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tse:FindRecordingsResponse>"
			"<tse:SearchToken>%s</tse:SearchToken>"
		"</tse:FindRecordingsResponse>",
		p_res->SearchToken);

	return offset;
}

int build_tse_GetRecordingSearchResults_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	RecordingInformationList * p_RecInf;
	tse_GetRecordingSearchResults_RES * p_res = (tse_GetRecordingSearchResults_RES *)argv;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tse:GetRecordingSearchResultsResponse>"
	    "<tse:ResultList>");

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tt:SearchState>%s</tt:SearchState>", 
	    onvif_SearchStateToString(p_res->ResultList.SearchState));

	p_RecInf = p_res->ResultList.RecordInformation;
	while (p_RecInf)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:RecordingInformation>");
		offset += build_RecordingInformation_xml(p_buf+offset, mlen-offset, &p_RecInf->RecordingInformation);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:RecordingInformation>");

		p_RecInf = p_RecInf->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</tse:ResultList>"
	    "</tse:GetRecordingSearchResultsResponse>");
	
	return offset;
}

int build_tse_FindEvents_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	tse_FindEvents_RES * p_res = (tse_FindEvents_RES *)argv;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tse:FindEventsResponse>"
			"<tse:SearchToken>%s</tse:SearchToken>"
		"</tse:FindEventsResponse>",
		p_res->SearchToken);

	return offset;
}

int build_tse_GetEventSearchResults_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	FindEventResultList * p_EventResult;
	tse_GetEventSearchResults_RES * p_res = (tse_GetEventSearchResults_RES *)argv;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tse:GetEventSearchResultsResponse>"
	    "<tse:ResultList>");

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tt:SearchState>%s</tt:SearchState>", 
	    onvif_SearchStateToString(p_res->ResultList.SearchState));

	p_EventResult = p_res->ResultList.Result;
	while (p_EventResult)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Result>");
		offset += build_FindEventResult_xml(p_buf+offset, mlen-offset, &p_EventResult->Result);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Result>");

		p_EventResult = p_EventResult->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</tse:ResultList>"
	    "</tse:GetEventSearchResultsResponse>");
	
	return offset;
}

int build_tse_FindMetadata_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	tse_FindMetadata_RES * p_res = (tse_FindMetadata_RES *)argv;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tse:FindMetadataResponse>"
			"<tse:SearchToken>%s</tse:SearchToken>"
		"</tse:FindMetadataResponse>",
		p_res->SearchToken);

	return offset;
}

int build_tse_FindMetadataResult_xml(char * p_buf, int mlen, onvif_FindMetadataResult * p_res)
{
    int offset = 0;
	char TimeBuff[64];

	onvif_get_time_str_s(TimeBuff, sizeof(TimeBuff), p_res->Time, 0);

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:RecordingToken>%s</tt:RecordingToken>"
     	"<tt:TrackToken>%s</tt:TrackToken>"
     	"<tt:Time>%s</tt:Time>",
     	p_res->RecordingToken, 
     	p_res->TrackToken, 
     	TimeBuff);	
	
	return offset;
}

int build_tse_GetMetadataSearchResults_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    FindMetadataResultList * p_Result;
	tse_GetMetadataSearchResults_RES * p_res = (tse_GetMetadataSearchResults_RES *)argv;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tse:GetMetadataSearchResultsResponse>"
	    "<tse:ResultList>");

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tt:SearchState>%s</tt:SearchState>", 
	    onvif_SearchStateToString(p_res->ResultList.SearchState));

	p_Result = p_res->ResultList.Result;
	while (p_Result)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Result>");
		offset += build_tse_FindMetadataResult_xml(p_buf+offset, mlen-offset, &p_Result->Result);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Result>");

		p_Result = p_Result->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</tse:ResultList>"
	    "</tse:GetMetadataSearchResultsResponse>");
	
	return offset;
}

int build_tse_FindPTZPosition_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tse_FindPTZPosition_RES * p_res = (tse_FindPTZPosition_RES *)argv;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tse:FindPTZPositionResponse>"
			"<tse:SearchToken>%s</tse:SearchToken>"
		"</tse:FindPTZPositionResponse>",
		p_res->SearchToken);

	return offset;
}

int build_tse_GetPTZPositionSearchResults_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    FindPTZPositionResultList * p_Result;
	tse_GetPTZPositionSearchResults_RES * p_res = (tse_GetPTZPositionSearchResults_RES *)argv;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tse:GetPTZPositionSearchResultsResponse>"
	    "<tse:ResultList>");

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tt:SearchState>%s</tt:SearchState>", 
	    onvif_SearchStateToString(p_res->ResultList.SearchState));

	p_Result = p_res->ResultList.Result;
	while (p_Result)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Result>");
		offset += build_FindPTZPositionResult_xml(p_buf+offset, mlen-offset, &p_Result->Result);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Result>");

		p_Result = p_Result->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</tse:ResultList>"
	    "</tse:GetPTZPositionSearchResultsResponse>");
	
	return offset;
}

int build_tse_EndSearch_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	char EndPoint[64];
	tse_EndSearch_RES * p_res = (tse_EndSearch_RES *)argv;
	
	onvif_get_time_str_s(EndPoint, sizeof(EndPoint), p_res->Endpoint, 0);

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tse:EndSearchResponse>"
			"<tse:Endpoint>%s</tse:Endpoint>"
		"</tse:EndSearchResponse>",
		EndPoint);

	return offset;
}

int build_tse_GetSearchState_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	tse_GetSearchState_RES * p_res = (tse_GetSearchState_RES *)argv;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tse:GetSearchStateResponse>"
			"<tse:State>%s</tse:State>"
		"</tse:GetSearchStateResponse>",
		onvif_SearchStateToString(p_res->State));

	return offset;
}

int build_trc_CreateRecording_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<trc:CreateRecordingResponse>"
			"<trc:RecordingToken>%s</trc:RecordingToken>"
		"</trc:CreateRecordingResponse>",
		argv);

	return offset;
}

int build_trc_DeleteRecording_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trc:DeleteRecordingResponse />");
	return offset;
}

int build_RecordingEncryption_xml(char * p_buf, int mlen, onvif_RecordingEncryption * p_res)
{
    uint32 i;
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tt:KID>%s</tt:KID>\r\n", 
	    p_res->KID);

    if (p_res->KeyFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
    	    "<tt:Key>%s</tt:Key>\r\n", 
    	    p_res->Key);
    }

    for (i = 0; i < p_res->sizeTrack; i++)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
    	    "<tt:Track>%s</tt:Track>\r\n", 
    	    p_res->Track[i]);
    }
   
    return offset;
}

int build_RecordingTargetConfiguration_xml(char * p_buf, int mlen, onvif_RecordingTargetConfiguration * p_res)
{
    uint32 i;
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tt:Storage>%s</tt:Storage>\r\n"
	    "<tt:Format>%s</tt:Format>\r\n", 
	    p_res->Storage,
	    p_res->Format);
    
    if (p_res->PrefixFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
    	    "<tt:Prefix>%s</tt:Prefix>\r\n", 
    	    p_res->Prefix);
    }

    if (p_res->PostfixFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
    	    "<tt:Postfix>%s</tt:Postfix>\r\n", 
    	    p_res->Postfix);
    }

    if (p_res->SpanDurationFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
    	    "<tt:SpanDuration>PT%uS</tt:SpanDuration>\r\n", 
    	    p_res->SpanDuration);
    }

    if (p_res->SegmentDurationFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
    	    "<tt:SegmentDuration>PT%uS</tt:SegmentDuration>\r\n", 
    	    p_res->SegmentDuration);
    }

    for (i = 0; i < p_res->sizeEncryption; i++)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
    	    "<tt:Encryption Mode=\"%s\">\r\n", 
    	    p_res->Encryption[i].Mode);
        offset += build_RecordingEncryption_xml(p_buf+offset, mlen-offset, &p_res->Encryption[i]);
    	offset += snprintf(p_buf+offset, mlen-offset, 
    	    "<tt:Encryption>\r\n");
    }

    return offset;
}

int build_RecordingConfiguration_xml(char * p_buf, int mlen, onvif_RecordingConfiguration * p_res)
{
	int offset = 0;
	
	offset += build_RecordingSourceInformation_xml(p_buf+offset, mlen-offset, &p_res->Source);

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tt:Content>%s</tt:Content>\r\n", 
	    p_res->Content);
	
	if (p_res->MaximumRetentionTimeFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:MaximumRetentionTime>PT%dS</tt:MaximumRetentionTime>\r\n", 
		    p_res->MaximumRetentionTime);
	}

    if (p_res->TargetFlag)
	{
	    offset += snprintf(p_buf+offset, mlen-offset, "<tt:Target>\r\n");
	    offset += build_RecordingTargetConfiguration_xml(p_buf+offset, mlen-offset, &p_res->Target);
	    offset += snprintf(p_buf+offset, mlen-offset, "</tt:Target>\r\n");
	}

	return offset;
}

int build_TrackConfiguration_xml(char * p_buf, int mlen, onvif_TrackConfiguration * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:TrackType>%s</tt:TrackType>\r\n"
		"<tt:Description>%s</tt:Description>\r\n",
		onvif_TrackTypeToString(p_res->TrackType),
		p_res->Description);

	return offset;		
}

int build_Recording_xml(char * p_buf, int mlen, onvif_Recording * p_res)
{
    int offset = 0;

    TrackList * p_track = p_res->Tracks;
		
    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tt:RecordingToken>%s</tt:RecordingToken>\r\n", 
        p_res->RecordingToken);
        
    offset += snprintf(p_buf+offset, mlen-offset, "<tt:Configuration>\r\n");
    offset += build_RecordingConfiguration_xml(p_buf+offset, mlen-offset, &p_res->Configuration);
    offset += snprintf(p_buf+offset, mlen-offset, "</tt:Configuration>\r\n");

    offset += snprintf(p_buf+offset, mlen-offset, "<tt:Tracks>\r\n");
    while (p_track)
    {
    	offset += snprintf(p_buf+offset, mlen-offset, 
    	    "<tt:Track>\r\n"
    	    "<tt:TrackToken>%s</tt:TrackToken>\r\n"
    	    "<tt:Configuration>\r\n", 
    	    p_track->Track.TrackToken);
    	offset += build_TrackConfiguration_xml(p_buf+offset, mlen-offset, &p_track->Track.Configuration);
    	offset += snprintf(p_buf+offset, mlen-offset, 
    	    "</tt:Configuration>\r\n"
    	    "</tt:Track>\r\n");
    	
    	p_track = p_track->next;
    }
    offset += snprintf(p_buf+offset, mlen-offset, "</tt:Tracks>\r\n");
		
    return offset;
}

int build_RecordingJobConfiguration_xml(char * p_buf, int mlen, onvif_RecordingJobConfiguration * p_res)
{
	uint32 i, j;
    int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:RecordingToken>%s</tt:RecordingToken>\r\n"
		"<tt:Mode>%s</tt:Mode>\r\n"
		"<tt:Priority>%d</tt:Priority>\r\n", 
		p_res->RecordingToken,
		p_res->Mode, 
		p_res->Priority);

	for (i = 0; i < p_res->sizeSource; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Source>\r\n");
		
		if (p_res->Source[i].SourceTokenFlag)
		{
			if (p_res->Source[i].SourceToken.TypeFlag)
			{
				offset += snprintf(p_buf+offset, mlen-offset, 
				    "<tt:SourceToken Type=\"%s\">\r\n", 
				    p_res->Source[i].SourceToken.Type);
			}
			else
			{
				offset += snprintf(p_buf+offset, mlen-offset, 
				    "<tt:SourceToken>\r\n");
			}

			offset += snprintf(p_buf+offset, mlen-offset, 
			    "<tt:Token>%s</tt:Token>\r\n", 
			    p_res->Source[i].SourceToken.Token);

			offset += snprintf(p_buf+offset, mlen-offset, 
			    "</tt:SourceToken>\r\n");
		}

		for (j = 0; j < p_res->Source[i].sizeTracks; j++)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:Tracks>\r\n"
					"<tt:SourceTag>%s</tt:SourceTag>\r\n"
	      			"<tt:Destination>%s</tt:Destination>\r\n"
      			"</tt:Tracks>\r\n",
      			p_res->Source[i].Tracks[j].SourceTag,
      			p_res->Source[i].Tracks[j].Destination);
		}
		
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Source>\r\n");
	}
	
	return offset;
}

int build_RecordingJob_xml(char * p_buf, int mlen, onvif_RecordingJob * p_res)
{
    int offset = 0;
    
    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tt:JobToken>%s</tt:JobToken>\r\n", 
        p_res->JobToken);
        
    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tt:JobConfiguration ScheduleToken=\"%s\">\r\n",
        p_res->JobConfiguration.ScheduleToken);
        
    offset += build_RecordingJobConfiguration_xml(p_buf+offset, mlen-offset, &p_res->JobConfiguration);

    offset += snprintf(p_buf+offset, mlen-offset, 
        "</tt:JobConfiguration>\r\n");

    return offset;
}

int build_RecordingJobStateInformation_xml(char * p_buf, int mlen, onvif_RecordingJobStateInformation * p_res)
{
	uint32 i, j;
    int offset = 0;
    
    offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:RecordingToken>%s</tt:RecordingToken>" 
		"<tt:State>%s</tt:State>", 
		p_res->RecordingToken,
		p_res->State);

    for (i = 0; i < p_res->sizeSources; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Sources>");
		
		if (p_res->Sources[i].SourceToken.TypeFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
			    "<tt:SourceToken Type=\"%s\">", 
				p_res->Sources[i].SourceToken.Type);
		}
		else
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
			    "<tt:SourceToken>");
		}
		
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:Token>%s</tt:Token>",
			p_res->Sources[i].SourceToken.Token);

		offset += snprintf(p_buf+offset, mlen-offset, 
		    "</tt:SourceToken>");

		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:State>%s</tt:State>",
			p_res->Sources[i].State);

		// tracks
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Tracks>");

		for (j = 0; j < p_res->Sources[i].sizeTrack; j++)
		{
	      	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Track>");
	      	offset += snprintf(p_buf+offset, mlen-offset, 
	      		"<tt:SourceTag>%s</tt:SourceTag>"
	      		"<tt:Destination>%s</tt:Destination>", 
	      		p_res->Sources[i].Track[j].SourceTag,
	      		p_res->Sources[i].Track[j].Destination);

	      	if (p_res->Sources[i].Track[j].ErrorFlag)
	      	{
	      		offset += snprintf(p_buf+offset, mlen-offset, 
	      		    "<tt:Error>%s</tt:Error>", 
		      		p_res->Sources[i].Track[j].Error);
	      	}

      		offset += snprintf(p_buf+offset, mlen-offset, 
      		    "<tt:State>%s</tt:State>", 
	      		p_res->Sources[i].Track[j].State);

	      	offset += snprintf(p_buf+offset, mlen-offset, "</tt:Track>");
		}
		
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Tracks>");
		// end of tracks
		
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Sources>");	
	}  

	return offset;
}

int build_FileProgress_xml(char * p_buf, int mlen, onvif_FileProgress * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tt:FileName>%s</tt:FileName>"
        "<tt:Progress>%f</tt:Progress>",
        p_res->FileName,
        p_res->Progress);

    return offset;
}

int build_ArrayOfFileProgress_xml(char * p_buf, int mlen, onvif_ArrayOfFileProgress * p_res)
{
	uint32 i;
    int offset = 0;

    for (i = 0; i < p_res->sizeFileProgress; i++)
    {
        offset += snprintf(p_buf+offset, mlen-offset, "<tt:FileProgress>");
        offset += build_FileProgress_xml(p_buf+offset, mlen-offset, &p_res->FileProgress[i]);
        offset += snprintf(p_buf+offset, mlen-offset, "</tt:FileProgress>");
    }

    return offset;
}

int build_trc_GetRecordings_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	RecordingList * p_recording = g_onvif_cfg.recordings;
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trc:GetRecordingsResponse>");
	while (p_recording)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trc:RecordingItem>");
		offset += build_Recording_xml(p_buf+offset, mlen-offset, &p_recording->Recording);
		offset += snprintf(p_buf+offset, mlen-offset, "</trc:RecordingItem>");
		
		p_recording = p_recording->next;
	}
	offset += snprintf(p_buf+offset, mlen-offset, "</trc:GetRecordingsResponse>");

	return offset;
}

int build_trc_SetRecordingConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trc:SetRecordingConfigurationResponse />");
	return offset;
}

int build_trc_GetRecordingConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	RecordingList * p_recording = onvif_find_Recording(g_onvif_cfg.recordings, argv);
	if (NULL == p_recording)
	{
		return ONVIF_ERR_NoRecording;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<trc:GetRecordingConfigurationResponse>"
	    "<trc:RecordingConfiguration>");
	offset += build_RecordingConfiguration_xml(p_buf+offset, mlen-offset, &p_recording->Recording.Configuration);
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</trc:RecordingConfiguration>"
	    "</trc:GetRecordingConfigurationResponse>");

	return offset;
}

int build_trc_CreateTrack_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<trc:CreateTrackResponse>"
			"<trc:TrackToken>%s</trc:TrackToken>"
		"</trc:CreateTrackResponse>",
		argv);

	return offset;
}

int build_trc_DeleteTrack_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trc:DeleteTrackResponse />");
	return offset;
}

int build_trc_GetTrackConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	TrackList * p_track;
	RecordingList * p_recording;
	trc_GetTrackConfiguration_REQ * p_res = (trc_GetTrackConfiguration_REQ *)argv;

	p_recording = onvif_find_Recording(g_onvif_cfg.recordings, p_res->RecordingToken);
	if (NULL == p_recording)
	{
		return ONVIF_ERR_NoRecording;
	}

	p_track = onvif_find_Track(p_recording->Recording.Tracks, p_res->TrackToken);
	if (NULL == p_track)
	{
		return ONVIF_ERR_NoTrack;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<trc:GetTrackConfigurationResponse>"
	    "<trc:TrackConfiguration>");
	offset += build_TrackConfiguration_xml(p_buf+offset, mlen-offset, &p_track->Track.Configuration);
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</trc:TrackConfiguration>"
	    "</trc:GetTrackConfigurationResponse>");

	return offset;
}

int build_trc_SetTrackConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trc:SetTrackConfigurationResponse />");
	return offset;
}

int build_trc_CreateRecordingJob_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	trc_CreateRecordingJob_REQ * p_res = (trc_CreateRecordingJob_REQ *) argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<trc:CreateRecordingJobResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<trc:JobToken>%s</trc:JobToken>", 
	    p_res->JobToken);

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<trc:JobConfiguration ScheduleToken=\"%s\">",
	    p_res->JobConfiguration.ScheduleToken);
	offset += build_RecordingJobConfiguration_xml(p_buf+offset, mlen-offset, &p_res->JobConfiguration);
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</trc:JobConfiguration>");

	offset += snprintf(p_buf+offset, mlen-offset, "</trc:CreateRecordingJobResponse>");

	return offset;
}

int build_trc_DeleteRecordingJob_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trc:DeleteRecordingJobResponse />");
	return offset;
}

int build_trc_GetRecordingJobs_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	RecordingJobList * p_recordingjob = g_onvif_cfg.recording_jobs;
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trc:GetRecordingJobsResponse>");
	while (p_recordingjob)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trc:JobItem>");
		offset += build_RecordingJob_xml(p_buf+offset, mlen-offset, &p_recordingjob->RecordingJob);
		offset += snprintf(p_buf+offset, mlen-offset, "</trc:JobItem>");

		p_recordingjob = p_recordingjob->next;
	}
	offset += snprintf(p_buf+offset, mlen-offset, "</trc:GetRecordingJobsResponse>");

	return offset;
}

int build_trc_SetRecordingJobConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	trc_SetRecordingJobConfiguration_REQ * p_res = (trc_SetRecordingJobConfiguration_REQ *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<trc:SetRecordingJobConfigurationResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<trc:JobConfiguration ScheduleToken=\"%s\">",
	    p_res->JobConfiguration.ScheduleToken);
	offset += build_RecordingJobConfiguration_xml(p_buf+offset, mlen-offset, &p_res->JobConfiguration);
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</trc:JobConfiguration>");
	offset += snprintf(p_buf+offset, mlen-offset, "</trc:SetRecordingJobConfigurationResponse>");
    
	return offset;
}

int build_trc_GetRecordingJobConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	RecordingJobList * p_recordingjob = onvif_find_RecordingJob(g_onvif_cfg.recording_jobs, argv);
	if (NULL == p_recordingjob)
	{
		return ONVIF_ERR_NoRecordingJob;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trc:GetRecordingJobConfigurationResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<trc:JobConfiguration ScheduleToken=\"%s\">",
	    p_recordingjob->RecordingJob.JobConfiguration.ScheduleToken);
	offset += build_RecordingJobConfiguration_xml(p_buf+offset, mlen-offset, &p_recordingjob->RecordingJob.JobConfiguration);
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</trc:JobConfiguration>");
	offset += snprintf(p_buf+offset, mlen-offset, "</trc:GetRecordingJobConfigurationResponse>");
	
	return offset;
}

int build_trc_SetRecordingJobMode_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trc:SetRecordingJobModeResponse />");
	return offset;
}

int build_trc_GetRecordingJobState_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	onvif_RecordingJobStateInformation * p_res = (onvif_RecordingJobStateInformation *) argv;

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<trc:GetRecordingJobStateResponse>"
	    "<trc:State>");
    offset += build_RecordingJobStateInformation_xml(p_buf+offset, mlen-offset, p_res);
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</trc:State>"
	    "</trc:GetRecordingJobStateResponse>");

	return offset;
}

int build_trc_GetRecordingOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	onvif_RecordingOptions * p_res = (onvif_RecordingOptions *) argv;

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<trc:GetRecordingOptionsResponse>"
	    "<trc:Options>");

	// job options
	offset += snprintf(p_buf+offset, mlen-offset, "<trc:Job");
	if (p_res->Job.SpareFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    " Spare=\"%d\" ", 
		    p_res->Job.Spare);
	}
	if (p_res->Job.CompatibleSourcesFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    " CompatibleSources=\"%s\"", 
		    p_res->Job.CompatibleSources);
	}
	offset += snprintf(p_buf+offset, mlen-offset, "></trc:Job>");

	// track options
	offset += snprintf(p_buf+offset, mlen-offset, "<trc:Track");
	if (p_res->Track.SpareTotalFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    " SpareTotal=\"%d\"", 
		    p_res->Track.SpareTotal);
	}
	if (p_res->Track.SpareVideoFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    " SpareVideo=\"%d\"", 
		    p_res->Track.SpareVideo);
	}
	if (p_res->Track.SpareAudioFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    " SpareAudio=\"%d\"", 
		    p_res->Track.SpareAudio);
	}
	if (p_res->Track.SpareMetadataFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    " SpareMetadata=\"%d\"", 
		    p_res->Track.SpareMetadata);
	}
	offset += snprintf(p_buf+offset, mlen-offset, "></trc:Track>");
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</trc:Options>"
	    "</trc:GetRecordingOptionsResponse>");

	return offset;
}

int build_trc_ExportRecordedData_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i;
    int offset = 0;
	trc_ExportRecordedData_RES * p_res = (trc_ExportRecordedData_RES *) argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<trc:ExportRecordedDataResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<trc:OperationToken>%s</trc:OperationToken>",
	    p_res->OperationToken);

    for (i = 0; i < p_res->sizeFileNames; i++)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
    	    "<trc:FileNames>%s</trc:FileNames>",
    	    p_res->FileNames[i]);
    }
    
	offset += snprintf(p_buf+offset, mlen-offset, "</trc:ExportRecordedDataResponse>");

	return offset;
}

int build_trc_StopExportRecordedData_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	trc_StopExportRecordedData_RES * p_res = (trc_StopExportRecordedData_RES *) argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<trc:StopExportRecordedDataResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<trc:Progress>%f</trc:Progress>",
	    p_res->Progress);
	    
    offset += snprintf(p_buf+offset, mlen-offset, "<trc:FileProgressStatus>");
    offset += build_ArrayOfFileProgress_xml(p_buf+offset, mlen-offset, &p_res->FileProgressStatus);
    offset += snprintf(p_buf+offset, mlen-offset, "</trc:FileProgressStatus>");
	    
	offset += snprintf(p_buf+offset, mlen-offset, "</trc:StopExportRecordedDataResponse>");

	return offset;
}

int build_trc_GetExportRecordedDataState_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	trc_GetExportRecordedDataState_RES * p_res = (trc_GetExportRecordedDataState_RES *) argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<trc:GetExportRecordedDataStateResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<trc:Progress>%f</trc:Progress>",
	    p_res->Progress);
	    
    offset += snprintf(p_buf+offset, mlen-offset, "<trc:FileProgressStatus>");
    offset += build_ArrayOfFileProgress_xml(p_buf+offset, mlen-offset, &p_res->FileProgressStatus);
    offset += snprintf(p_buf+offset, mlen-offset, "</trc:FileProgressStatus>");
	    
	offset += snprintf(p_buf+offset, mlen-offset, "</trc:GetExportRecordedDataStateResponse>");

	return offset;
}

int build_trp_GetReplayUri_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	trp_GetReplayUri_RES * p_res = (trp_GetReplayUri_RES *)argv;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<trp:GetReplayUriResponse>"
			"<trp:Uri>%s</trp:Uri>"
		"</trp:GetReplayUriResponse>",
		p_res->Uri);

	return offset;
}

int build_trp_GetReplayConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	trp_GetReplayConfiguration_RES * p_res = (trp_GetReplayConfiguration_RES *)argv;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<trp:GetReplayConfigurationResponse>"
			"<trp:Configuration>"
				"<tt:SessionTimeout>PT%dS</tt:SessionTimeout>"
			"</trp:Configuration>"
		"</trp:GetReplayConfigurationResponse>",
		p_res->SessionTimeout);

	return offset;
}

int build_trp_SetReplayConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<trp:SetReplayConfigurationResponse />");
	return offset;
}

#endif // end of PROFILE_G_SUPPORT

#ifdef PROFILE_C_SUPPORT

int build_AccessPointInfo_xml(char * p_buf, int mlen, onvif_AccessPointInfo * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tac:Name>%s</tac:Name>\r\n", 
        p_res->Name);

    if (p_res->DescriptionFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tac:Description>%s</tac:Description>\r\n", 
            p_res->Description);
    }
    
    if (p_res->AreaFromFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tac:AreaFrom>%s</tac:AreaFrom>\r\n", 
            p_res->AreaFrom);
    }
    
    if (p_res->AreaToFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tac:AreaTo>%s</tac:AreaTo>\r\n", 
            p_res->AreaTo);
    }
    
    if (p_res->EntityTypeFlag)
    {
        if (p_res->EntityTypeAttrFlag)
        {
            offset += snprintf(p_buf+offset, mlen-offset, 
                "<tac:EntityType %s>%s</tac:EntityType>\r\n", 
                p_res->EntityTypeAttr, 
                p_res->EntityType);
        }
        else
        {
            offset += snprintf(p_buf+offset, mlen-offset, 
                "<tac:EntityType>%s</tac:EntityType>\r\n", 
                p_res->EntityType);
        }
    }

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tac:Entity>%s</tac:Entity>\r\n", 
        p_res->Entity);

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tac:Capabilities "
            "DisableAccessPoint=\"%s\" "
            "Duress=\"%s\" "
            "AnonymousAccess=\"%s\" "
            "AccessTaken=\"%s\" "
            "ExternalAuthorization=\"%s\" "
            "IdentifierAccess=\"%s\" ",
        p_res->Capabilities.DisableAccessPoint ? "true" : "false",
        p_res->Capabilities.Duress ? "true" : "false",
        p_res->Capabilities.AnonymousAccess ? "true" : "false",
        p_res->Capabilities.AccessTaken ? "true" : "false",
        p_res->Capabilities.ExternalAuthorization ? "true" : "false",
        p_res->Capabilities.IdentifierAccess ? "true" : "false");

    if (p_res->Capabilities.SupportedRecognitionTypesFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "SupportedRecognitionTypes=\"%s\" ",
            p_res->Capabilities.SupportedRecognitionTypes);
    }

    if (p_res->Capabilities.SupportedFeedbackTypesFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "SupportedFeedbackTypes=\"%s\" ",
            p_res->Capabilities.SupportedFeedbackTypes);
    }

    offset += snprintf(p_buf+offset, mlen-offset, " />\r\n");
    
    return offset;
}

int build_AreaInfo_xml(char * p_buf, int mlen, onvif_AreaInfo * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tac:Name>%s</tac:Name>\r\n", 
        p_res->Name);

    if (p_res->DescriptionFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tac:Description>%s</tac:Description>\r\n", 
            p_res->Description);
    }

    return offset;
}

int build_DoorInfo_xml(char * p_buf, int mlen, onvif_DoorInfo * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tdc:Name>%s</tdc:Name>\r\n", 
        p_res->Name);

    if (p_res->DescriptionFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tdc:Description>%s</tdc:Description>\r\n", 
            p_res->Description);
    }

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tdc:Capabilities "
            "Access=\"%s\" "
            "AccessTimingOverride=\"%s\" "
            "Lock=\"%s\" "
            "Unlock=\"%s\" "
            "Block=\"%s\" "
            "DoubleLock=\"%s\" "
            "LockDown=\"%s\" "
            "LockOpen=\"%s\" "
            "DoorMonitor=\"%s\" "
            "LockMonitor=\"%s\" "
            "DoubleLockMonitor=\"%s\" "
            "Alarm=\"%s\" "
            "Tamper=\"%s\" "
            "Fault=\"%s\" />\r\n",
        p_res->Capabilities.Access ? "true" : "false",
        p_res->Capabilities.AccessTimingOverride ? "true" : "false",
        p_res->Capabilities.Lock ? "true" : "false",
        p_res->Capabilities.Unlock ? "true" : "false",
        p_res->Capabilities.Block ? "true" : "false",
        p_res->Capabilities.DoubleLock ? "true" : "false",
        p_res->Capabilities.LockDown ? "true" : "false",
        p_res->Capabilities.LockOpen ? "true" : "false",
        p_res->Capabilities.DoorMonitor ? "true" : "false",
        p_res->Capabilities.LockMonitor ? "true" : "false",
        p_res->Capabilities.DoubleLockMonitor ? "true" : "false",
        p_res->Capabilities.Alarm ? "true" : "false",
        p_res->Capabilities.Tamper ? "true" : "false",
        p_res->Capabilities.Fault ? "true" : "false");

    return offset;
}

int build_Timings_xml(char * p_buf, int mlen, onvif_Timings * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tdc:ReleaseTime>PT%uS</tdc:ReleaseTime>\r\n"
        "<tdc:OpenTime>PT%uS</tdc:OpenTime>\r\n",
        p_res->ReleaseTime, 
        p_res->OpenTime);

    if (p_res->ExtendedReleaseTimeFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tdc:ExtendedReleaseTime>PT%uS</tdc:ExtendedReleaseTime>\r\n",
            p_res->ExtendedReleaseTime);
    }

    if (p_res->DelayTimeBeforeRelockFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tdc:DelayTimeBeforeRelock>PT%uS</tdc:DelayTimeBeforeRelock>\r\n",
            p_res->DelayTimeBeforeRelock);
    }

    if (p_res->ExtendedOpenTimeFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tdc:ExtendedOpenTime>PT%uS</tdc:ExtendedOpenTime>\r\n",
            p_res->ExtendedOpenTime);
    }

    if (p_res->PreAlarmTimeFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tdc:PreAlarmTime>PT%uS</tdc:PreAlarmTime>\r\n",
            p_res->PreAlarmTime);
    }
    
    return offset;
}

int build_Door_xml(char * p_buf, int mlen, onvif_Door * p_res)
{
    int offset = 0;

    offset += build_DoorInfo_xml(p_buf+offset, mlen-offset, &p_res->DoorInfo);

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tdc:DoorType>%s</tdc:DoorType>\r\n", 
        p_res->DoorType);

    offset += snprintf(p_buf+offset, mlen-offset, "<tdc:Timings>\r\n");
    offset += build_Timings_xml(p_buf+offset, mlen-offset, &p_res->Timings);
    offset += snprintf(p_buf+offset, mlen-offset, "</tdc:Timings>\r\n");
    
    return offset;
}

int build_DoorState_xml(char * p_buf, int mlen, onvif_DoorState * p_res)
{
    int offset = 0;
    
    if (p_res->DoorPhysicalStateFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tdc:DoorPhysicalState>%s</tdc:DoorPhysicalState>\r\n",
            onvif_DoorPhysicalStateToString(p_res->DoorPhysicalState));
    }

    if (p_res->LockPhysicalStateFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tdc:LockPhysicalState>%s</tdc:LockPhysicalState>\r\n",
            onvif_LockPhysicalStateToString(p_res->LockPhysicalState));
    }

    if (p_res->DoubleLockPhysicalStateFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tdc:DoubleLockPhysicalState>%s</tdc:DoubleLockPhysicalState>\r\n",
            onvif_LockPhysicalStateToString(p_res->DoubleLockPhysicalState));
    }

    if (p_res->AlarmFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tdc:Alarm>%s</tdc:Alarm>\r\n",
            onvif_DoorAlarmStateToString(p_res->Alarm));
    }

    if (p_res->TamperFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, "<tdc:Tamper>\r\n");

        if (p_res->Tamper.ReasonFlag)
        {
            offset += snprintf(p_buf+offset, mlen-offset, 
                "<tdc:Reason>%s</tdc:Reason>\r\n", 
                p_res->Tamper.Reason);
        }
        
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tdc:State>%s</tdc:State>\r\n",
            onvif_DoorTamperStateToString(p_res->Tamper.State));

        offset += snprintf(p_buf+offset, mlen-offset, "</tdc:Tamper>\r\n");
    }

    if (p_res->FaultFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, "<tdc:Fault>\r\n");

        if (p_res->Fault.ReasonFlag)
        {
            offset += snprintf(p_buf+offset, mlen-offset, 
                "<tdc:Reason>%s</tdc:Reason>\r\n", 
                p_res->Fault.Reason);
        }

        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tdc:State>%s</tdc:State>\r\n",
            onvif_DoorFaultStateToString(p_res->Fault.State));

        offset += snprintf(p_buf+offset, mlen-offset, "</tdc:Fault>\r\n");
    }

	offset += snprintf(p_buf+offset, mlen-offset, 
        "<tdc:DoorMode>%s</tdc:DoorMode>\r\n",
        onvif_DoorModeToString(p_res->DoorMode));

    return offset;        
}

int build_tac_GetAccessPointList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tac_GetAccessPointList_RES * p_res = (tac_GetAccessPointList_RES *)argv;
    AccessPointList * p_info = p_res->AccessPoint;

	offset += snprintf(p_buf+offset, mlen-offset, "<tac:GetAccessPointListResponse>");

    if (p_res->NextStartReferenceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tac:NextStartReference>%s</tac:NextStartReference>",
            p_res->NextStartReference);
    }

    while (p_info)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tac:AccessPoint token=\"%s\">", 
            p_info->AccessPointInfo.token);
        offset += build_AccessPointInfo_xml(p_buf+offset, mlen-offset, &p_info->AccessPointInfo);
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tac:AuthenticationProfileToken>%s</tac:AuthenticationProfileToken>",
            p_info->AuthenticationProfileToken);
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tac:AccessPoint>");
        
        p_info = p_info->next;
    }

	offset += snprintf(p_buf+offset, mlen-offset, "</tac:GetAccessPointListResponse>");

	return offset;
}

int build_tac_GetAccessPoints_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i = 0;
    int offset = 0;
    tac_GetAccessPoints_REQ * p_res = (tac_GetAccessPoints_REQ *)argv;
    AccessPointList * p_accesspoint;

	offset += snprintf(p_buf+offset, mlen-offset, "<tac:GetAccessPointsResponse>");

    for (i = 0; i < ARRAY_SIZE(p_res->token); i++)
    {
        if (p_res->token[i][0] == '\0')
        {
            break;
        }

        p_accesspoint = onvif_find_AccessPoint(g_onvif_cfg.access_points, p_res->token[i]);
        if (p_accesspoint)
        {
            offset += snprintf(p_buf+offset, mlen-offset, 
                "<tac:AccessPoint token=\"%s\">", 
                p_accesspoint->AccessPointInfo.token);
            offset += build_AccessPointInfo_xml(p_buf+offset, mlen-offset, &p_accesspoint->AccessPointInfo);
            offset += snprintf(p_buf+offset, mlen-offset, 
                "<tac:AuthenticationProfileToken>%s</tac:AuthenticationProfileToken>",
                p_accesspoint->AuthenticationProfileToken);
            offset += snprintf(p_buf+offset, mlen-offset, 
                "</tac:AccessPoint>");
        }
    }
	
    offset += snprintf(p_buf+offset, mlen-offset, "</tac:GetAccessPointsResponse>");

	return offset;
}

int build_tac_CreateAccessPoint_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tac_CreateAccessPoint_RES * p_res = (tac_CreateAccessPoint_RES *)argv;
    
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tac:CreateAccessPointResponse>"
	        "<tac:Token>%s</tac:Token>"
	    "</tac:CreateAccessPointResponse>",
	    p_res->Token);
	
	return offset;
}

int build_tac_SetAccessPoint_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tac:SetAccessPointResponse />");
	return offset;
}

int build_tac_ModifyAccessPoint_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tac:ModifyAccessPointResponse />");
	return offset;
}

int build_tac_DeleteAccessPoint_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tac:DeleteAccessPointResponse />");
	return offset;
}

int build_tac_GetAccessPointInfoList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tac_GetAccessPointInfoList_RES * p_res = (tac_GetAccessPointInfoList_RES *)argv;
    AccessPointList * p_info = p_res->AccessPointInfo;

	offset += snprintf(p_buf+offset, mlen-offset, "<tac:GetAccessPointInfoListResponse>");

    if (p_res->NextStartReferenceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tac:NextStartReference>%s</tac:NextStartReference>",
            p_res->NextStartReference);
    }

    while (p_info)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tac:AccessPointInfo token=\"%s\">", 
            p_info->AccessPointInfo.token);
        offset += build_AccessPointInfo_xml(p_buf+offset, mlen-offset, &p_info->AccessPointInfo);
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tac:AccessPointInfo>");
        
        p_info = p_info->next;
    }

	offset += snprintf(p_buf+offset, mlen-offset, "</tac:GetAccessPointInfoListResponse>");

	return offset;
}

int build_tac_GetAccessPointInfo_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i = 0;
    int offset = 0;
    tac_GetAccessPointInfo_REQ * p_res = (tac_GetAccessPointInfo_REQ *)argv;
    AccessPointList * p_accesspoint;

	offset += snprintf(p_buf+offset, mlen-offset, "<tac:GetAccessPointInfoResponse>");

    for (i = 0; i < ARRAY_SIZE(p_res->token); i++)
    {
        if (p_res->token[i][0] == '\0')
        {
            break;
        }

        p_accesspoint = onvif_find_AccessPoint(g_onvif_cfg.access_points, p_res->token[i]);
        if (p_accesspoint)
        {
            offset += snprintf(p_buf+offset, mlen-offset, 
                "<tac:AccessPointInfo token=\"%s\">", 
                p_accesspoint->AccessPointInfo.token);
            offset += build_AccessPointInfo_xml(p_buf+offset, mlen-offset, &p_accesspoint->AccessPointInfo);
            offset += snprintf(p_buf+offset, mlen-offset, 
                "</tac:AccessPointInfo>");
        }
    }
	
    offset += snprintf(p_buf+offset, mlen-offset, "</tac:GetAccessPointInfoResponse>");

	return offset;
}

int build_tac_GetAreaList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tac_GetAreaList_RES * p_res = (tac_GetAreaList_RES *)argv;
    AreaList * p_info = p_res->Area;

	offset += snprintf(p_buf+offset, mlen-offset, "<tac:GetAreaListResponse>");

    if (p_res->NextStartReferenceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tac:NextStartReference>%s</tac:NextStartReference>",
            p_res->NextStartReference);
    }

    while (p_info)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tac:Area token=\"%s\">", 
            p_info->AreaInfo.token);
        offset += build_AreaInfo_xml(p_buf+offset, mlen-offset, &p_info->AreaInfo);
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tac:Area>");
        
        p_info = p_info->next;
    }

	offset += snprintf(p_buf+offset, mlen-offset, "</tac:GetAreaListResponse>");

	return offset;
}

int build_tac_GetAreas_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i = 0;
    int offset = 0;
    tac_GetAreas_REQ * p_res = (tac_GetAreas_REQ *)argv;
    AreaList * p_area;

	offset += snprintf(p_buf+offset, mlen-offset, "<tac:GetAreasResponse>");

    for (i = 0; i < ARRAY_SIZE(p_res->token); i++)
    {
        if (p_res->token[i][0] == '\0')
        {
            break;
        }

        p_area = onvif_find_Area(g_onvif_cfg.areas, p_res->token[i]);
        if (p_area)
        {
            offset += snprintf(p_buf+offset, mlen-offset, 
                "<tac:Area token=\"%s\">", 
                p_area->AreaInfo.token);
            offset += build_AreaInfo_xml(p_buf+offset, mlen-offset, &p_area->AreaInfo);
            offset += snprintf(p_buf+offset, mlen-offset, 
                "</tac:Area>");
        }
    }
	
    offset += snprintf(p_buf+offset, mlen-offset, "</tac:GetAreasResponse>");

	return offset;
}

int build_tac_CreateArea_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tac_CreateArea_RES * p_res = (tac_CreateArea_RES *)argv;
    
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tac:CreateAreaResponse>"
	        "<tac:Token>%s</tac:Token>"
	    "</tac:CreateAreaResponse>",
	    p_res->Token);
	
	return offset;
}

int build_tac_SetArea_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tac:SetAreaResponse />");
	return offset;
}

int build_tac_ModifyArea_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tac:ModifyAreaResponse />");
	return offset;
}

int build_tac_DeleteArea_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tac:DeleteAreaResponse />");
	return offset;
}

int build_tac_GetAreaInfoList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tac_GetAreaInfoList_RES * p_res = (tac_GetAreaInfoList_RES *)argv;
    AreaList * p_info = p_res->AreaInfo;

	offset += snprintf(p_buf+offset, mlen-offset, "<tac:GetAreaInfoListResponse>");

    if (p_res->NextStartReferenceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tac:NextStartReference>%s</tac:NextStartReference>",
            p_res->NextStartReference);
    }

    while (p_info)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tac:AreaInfo token=\"%s\">", 
            p_info->AreaInfo.token);
        offset += build_AreaInfo_xml(p_buf+offset, mlen-offset, &p_info->AreaInfo);
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tac:AreaInfo>");
        
        p_info = p_info->next;
    }

	offset += snprintf(p_buf+offset, mlen-offset, "</tac:GetAreaInfoListResponse>");

	return offset;
}

int build_tac_GetAreaInfo_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i = 0;
    int offset = 0;
    tac_GetAreaInfo_REQ * p_res = (tac_GetAreaInfo_REQ *)argv;
    AreaList * p_info;

	offset += snprintf(p_buf+offset, mlen-offset, "<tac:GetAreaInfoResponse>");

    for (i = 0; i < ARRAY_SIZE(p_res->token); i++)
    {
        if (p_res->token[i][0] == '\0')
        {
            break;
        }

        p_info = onvif_find_Area(g_onvif_cfg.areas, p_res->token[i]);
        if (p_info)
        {
            offset += snprintf(p_buf+offset, mlen-offset, 
                "<tac:AreaInfo token=\"%s\">", 
                p_info->AreaInfo.token);
            offset += build_AreaInfo_xml(p_buf+offset, mlen-offset, &p_info->AreaInfo);
            offset += snprintf(p_buf+offset, mlen-offset, 
                "</tac:AreaInfo>");
        }
    }
	
    offset += snprintf(p_buf+offset, mlen-offset, "</tac:GetAreaInfoResponse>");

	return offset;
}

int build_tac_GetAccessPointState_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tac_GetAccessPointState_REQ * p_res = (tac_GetAccessPointState_REQ *)argv;
    AccessPointList * p_accesspoint = onvif_find_AccessPoint(g_onvif_cfg.access_points, p_res->Token);
    if (NULL == p_accesspoint)
    {
        return ONVIF_ERR_NotFound;
    }

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tac:GetAccessPointStateResponse>"
	        "<tac:AccessPointState>"
                "<tac:Enabled>%s</tac:Enabled>"
            "</tac:AccessPointState>"
        "</tac:GetAccessPointStateResponse>",
        p_accesspoint->Enabled ? "true" : "false");
    
    return offset;
}

int build_tac_EnableAccessPoint_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tac:EnableAccessPointResponse />");
	return offset;
}

int build_tac_DisableAccessPoint_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tac:DisableAccessPointResponse />");
	return offset;
}

int build_tdc_GetDoorList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tdc_GetDoorList_RES * p_res = (tdc_GetDoorList_RES *)argv;
    DoorList * p_door = p_res->Door;

	offset += snprintf(p_buf+offset, mlen-offset, "<tdc:GetDoorListResponse>");

    if (p_res->NextStartReferenceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tdc:NextStartReference>%s</tdc:NextStartReference>",
            p_res->NextStartReference);
    }

    while (p_door)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tdc:Door token=\"%s\">", 
            p_door->Door.DoorInfo.token);
        offset += build_Door_xml(p_buf+offset, mlen-offset, &p_door->Door);
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tdc:Door>");
        
        p_door = p_door->next;
    }

	offset += snprintf(p_buf+offset, mlen-offset, "</tdc:GetDoorListResponse>");

	return offset;
}

int build_tdc_GetDoors_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i = 0;
    int offset = 0;
    tdc_GetDoors_REQ * p_res = (tdc_GetDoors_REQ *)argv;
    DoorList * p_door;

	offset += snprintf(p_buf+offset, mlen-offset, "<tdc:GetDoorsResponse>");

    for (i = 0; i < ARRAY_SIZE(p_res->token); i++)
    {
        if (p_res->token[i][0] == '\0')
        {
            break;
        }

        p_door = onvif_find_Door(g_onvif_cfg.doors, p_res->token[i]);
        if (p_door)
        {
            offset += snprintf(p_buf+offset, mlen-offset, 
                "<tdc:Door token=\"%s\">", 
                p_door->Door.DoorInfo.token);
            offset += build_Door_xml(p_buf+offset, mlen-offset, &p_door->Door);
            offset += snprintf(p_buf+offset, mlen-offset, 
                "</tdc:Door>");
        }
    }
	
    offset += snprintf(p_buf+offset, mlen-offset, "</tdc:GetDoorsResponse>");

	return offset;
}

int build_tdc_CreateDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tdc_CreateDoor_RES * p_res = (tdc_CreateDoor_RES *)argv;
    
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tdc:CreateDoorResponse>"
	        "<tdc:Token>%s</tdc:Token>"
	    "</tdc:CreateDoorResponse>",
	    p_res->Token);
	
	return offset;
}

int build_tdc_SetDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tdc:SetDoorResponse />");
	return offset;
}

int build_tdc_ModifyDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tdc:ModifyDoorResponse />");
	return offset;
}

int build_tdc_DeleteDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tdc:DeleteDoorResponse />");
	return offset;
}

int build_tdc_GetDoorInfoList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tdc_GetDoorInfoList_RES * p_res = (tdc_GetDoorInfoList_RES *)argv;
    DoorInfoList * p_doorinfo = p_res->DoorInfo;

	offset += snprintf(p_buf+offset, mlen-offset, "<tdc:GetDoorInfoListResponse>");

    if (p_res->NextStartReferenceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tdc:NextStartReference>%s</tdc:NextStartReference>",
            p_res->NextStartReference);
    }

    while (p_doorinfo)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tdc:DoorInfo token=\"%s\">", 
            p_doorinfo->DoorInfo.token);
        
        offset += build_DoorInfo_xml(p_buf+offset, mlen-offset, &p_doorinfo->DoorInfo);

        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tdc:DoorInfo>");
        
        p_doorinfo = p_doorinfo->next;
    }

	offset += snprintf(p_buf+offset, mlen-offset, "</tdc:GetDoorInfoListResponse>");

	return offset;
}

int build_tdc_GetDoorInfo_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i = 0;
    int offset = 0;
    tdc_GetDoorInfo_REQ * p_res = (tdc_GetDoorInfo_REQ *)argv;
    DoorList * p_door;

	offset += snprintf(p_buf+offset, mlen-offset, "<tdc:GetDoorInfoResponse>");

    for (i = 0; i < ARRAY_SIZE(p_res->token); i++)
    {
        if (p_res->token[i][0] == '\0')
        {
            break;
        }

        p_door = onvif_find_Door(g_onvif_cfg.doors, p_res->token[i]);
        if (p_door)
        {
            offset += snprintf(p_buf+offset, mlen-offset, 
                "<tdc:DoorInfo token=\"%s\">", 
                p_door->Door.DoorInfo.token);
            
            offset += build_DoorInfo_xml(p_buf+offset, mlen-offset, &p_door->Door.DoorInfo);

            offset += snprintf(p_buf+offset, mlen-offset, 
                "</tdc:DoorInfo>");
        }
    }
	
    offset += snprintf(p_buf+offset, mlen-offset, "</tdc:GetDoorInfoResponse>");

	return offset;
}

int build_tdc_GetDoorState_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tdc_GetDoorState_REQ * p_res = (tdc_GetDoorState_REQ *)argv;
    DoorList * p_info = onvif_find_Door(g_onvif_cfg.doors, p_res->Token);
    if (NULL == p_info)
    {
        return ONVIF_ERR_NotFound;
    }

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tdc:GetDoorStateResponse>"
	    "<tdc:DoorState>");
    offset += build_DoorState_xml(p_buf+offset, mlen-offset, &p_info->DoorState);
    offset += snprintf(p_buf+offset, mlen-offset, 
        "</tdc:DoorState>"
        "</tdc:GetDoorStateResponse>");

    return offset;
}

int build_tdc_AccessDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tdc:AccessDoorResponse />");
	return offset;
}

int build_tdc_LockDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tdc:LockDoorResponse />");
	return offset;
}

int build_tdc_UnlockDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tdc:UnlockDoorResponse />");
	return offset;
}

int build_tdc_DoubleLockDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tdc:DoubleLockDoorResponse />");
	return offset;
}

int build_tdc_BlockDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tdc:BlockDoorResponse />");
	return offset;
}

int build_tdc_LockDownDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tdc:LockDownDoorResponse />");
	return offset;
}

int build_tdc_LockDownReleaseDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tdc:LockDownReleaseDoorResponse />");
	return offset;
}

int build_tdc_LockOpenDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tdc:LockOpenDoorResponse />");
	return offset;
}

int build_tdc_LockOpenReleaseDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tdc:LockOpenReleaseDoorResponse />");
	return offset;
}

#endif // end of PROFILE_C_SUPPORT

#ifdef DEVICEIO_SUPPORT

int build_PaneLayout_xml(char * p_buf, int mlen, onvif_PaneLayout * p_PaneLayout)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:PaneLayout>\r\n"
			"<tt:Pane>%s</tt:Pane>\r\n"
			"<tt:Area bottom=\"%0.1f\" top=\"%0.1f\" right=\"%0.1f\" left=\"%0.1f\">\r\n"
			"</tt:Area>\r\n"
		"</tt:PaneLayout>\r\n",
		p_PaneLayout->Pane,
		p_PaneLayout->Area.bottom,
		p_PaneLayout->Area.top,
		p_PaneLayout->Area.right,
		p_PaneLayout->Area.left);

	return offset;
}

int build_Layout_xml(char * p_buf, int mlen, onvif_Layout * p_Layout)
{
	int offset = 0;
	PaneLayoutList * p_PaneLayout = p_Layout->PaneLayout;

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Layout>\r\n");

	while (p_PaneLayout)
	{
		offset += build_PaneLayout_xml(p_buf+offset, mlen-offset, &p_PaneLayout->PaneLayout);

		p_PaneLayout = p_PaneLayout->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tt:Layout>\r\n");

	return offset;
}

int build_VideoOutput_xml(char * p_buf, int mlen, onvif_VideoOutput * p_VideoOutput)
{
	int offset = 0;

	offset += build_Layout_xml(p_buf+offset, mlen-offset, &p_VideoOutput->Layout);

	if (p_VideoOutput->ResolutionFlag)
	{
		offset += build_VideoResolution_xml(p_buf+offset, mlen-offset, &p_VideoOutput->Resolution);
	}

	if (p_VideoOutput->RefreshRateFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:RefreshRate>%0.2f</tt:RefreshRate>\r\n", p_VideoOutput->RefreshRate);
	}

	if (p_VideoOutput->AspectRatioFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:AspectRatio>%0.2f</tt:AspectRatio>\r\n", p_VideoOutput->AspectRatio);
	}

	return offset;
}

int build_VideoOutputConfiguration_xml(char * p_buf, int mlen, onvif_VideoOutputConfiguration * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Name>%s</tt:Name>\r\n"
		"<tt:UseCount>%d</tt:UseCount>\r\n"
		"<tt:OutputToken>%s</tt:OutputToken>\r\n",
		p_res->Name,
		p_res->UseCount,
		p_res->OutputToken);

	return offset;
}

int build_AudioOutputConfiguration_xml(char * p_buf, int mlen, onvif_AudioOutputConfiguration * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Name>%s</tt:Name>\r\n"
		"<tt:UseCount>%d</tt:UseCount>\r\n"
		"<tt:OutputToken>%s</tt:OutputToken>\r\n",
		p_res->Name,
		p_res->UseCount,
		p_res->OutputToken);

	if (p_res->SendPrimacyFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:SendPrimacy>%s</tt:SendPrimacy>\r\n", p_res->SendPrimacy);
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:OutputLevel>%d</tt:OutputLevel>\r\n", p_res->OutputLevel);

	return offset;
}

int build_AudioOutputConfigurationOptions_xml(char * p_buf, int mlen, onvif_AudioOutputConfigurationOptions * p_res)
{
	uint32 i;
    int offset = 0;
    
    for (i = 0; i < p_res->sizeOutputTokensAvailable; i++)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tt:OutputTokensAvailable>%s</tt:OutputTokensAvailable>\r\n",
            p_res->OutputTokensAvailable[i]);
    }

    for (i = 0; i < p_res->sizeSendPrimacyOptions; i++)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tt:SendPrimacyOptions>%s</tt:SendPrimacyOptions>\r\n",
            p_res->SendPrimacyOptions[i]);
    }

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tt:OutputLevelRange>\r\n"
            "<tt:Min>%d</tt:Min>\r\n"
            "<tt:Max>%d</tt:Max>\r\n"
        "</tt:OutputLevelRange>\r\n",
        p_res->OutputLevelRange.Min,
        p_res->OutputLevelRange.Max); 

    return offset;        
}

int build_RelayOutput_xml(char * p_buf, int mlen, onvif_RelayOutput * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Properties>\r\n"
			"<tt:Mode>%s</tt:Mode>\r\n"
			"<tt:DelayTime>PT%dS</tt:DelayTime>\r\n"
			"<tt:IdleState>%s</tt:IdleState>\r\n"
		"</tt:Properties>\r\n",
		onvif_RelayModeToString(p_res->Properties.Mode),
		p_res->Properties.DelayTime,
		onvif_RelayIdleStateToString(p_res->Properties.IdleState));

	return offset;
}

int build_RelayOutputOptions_xml(char * p_buf, int mlen, onvif_RelayOutputOptions * p_res)
{
	int offset = 0;

	if (p_res->RelayMode_MonostableFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tmd:Mode>Monostable</tmd:Mode>\r\n");
	}

	if (p_res->RelayMode_BistableFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tmd:Mode>Bistable</tmd:Mode>\r\n");
	}

	if (p_res->DelayTimesFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tmd:DelayTimes>%s</tmd:DelayTimes>\r\n", p_res->DelayTimes);
	}

	if (p_res->DiscreteFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tmd:Discrete>%s</tmd:Discrete>\r\n", p_res->Discrete ? "true" : "false");
	}

	return offset;
}

int build_DigitalInputConfigurationOptions_xml(char * p_buf, int mlen, onvif_DigitalInputConfigurationOptions * p_res)
{
	int offset = 0;

	if (p_res->DigitalIdleState_closedFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tmd:IdleState>closed</tmd:IdleState>\r\n");
	}

	if (p_res->DigitalIdleState_openFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tmd:IdleState>open</tmd:IdleState>\r\n");
	}

	return offset;
}

int build_SerialPortConfiguration_xml(char * p_buf, int mlen, onvif_SerialPortConfiguration * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tmd:BaudRate>%d</tmd:BaudRate>\r\n"
        "<tmd:ParityBit>%s</tmd:ParityBit>\r\n"
        "<tmd:CharacterLength>%d</tmd:CharacterLength>\r\n"
        "<tmd:StopBit>%0.1f</tmd:StopBit>\r\n",
        p_res->BaudRate,
        onvif_ParityBitToString(p_res->ParityBit),
        p_res->CharacterLength,
        p_res->StopBit);

    return offset;
}

int build_SerialPortConfigurationOptions_xml(char * p_buf, int mlen, onvif_SerialPortConfigurationOptions * p_res)
{
    int offset = 0;
    
    offset += snprintf(p_buf+offset, mlen-offset, "<tmd:BaudRateList>\r\n");
    offset += build_IntList_xml(p_buf+offset, mlen-offset, &p_res->BaudRateList);
    offset += snprintf(p_buf+offset, mlen-offset, "</tmd:BaudRateList>\r\n");

    offset += snprintf(p_buf+offset, mlen-offset, "<tmd:ParityBitList>\r\n");
    offset += build_ParityBitList_xml(p_buf+offset, mlen-offset, &p_res->ParityBitList);
    offset += snprintf(p_buf+offset, mlen-offset, "</tmd:ParityBitList>\r\n");

    offset += snprintf(p_buf+offset, mlen-offset, "<tmd:CharacterLengthList>\r\n");
    offset += build_IntList_xml(p_buf+offset, mlen-offset, &p_res->CharacterLengthList);
    offset += snprintf(p_buf+offset, mlen-offset, "</tmd:CharacterLengthList>\r\n");

    offset += snprintf(p_buf+offset, mlen-offset, "<tmd:StopBitList>\r\n");
    offset += build_FloatList_xml(p_buf+offset, mlen-offset, &p_res->StopBitList);
    offset += snprintf(p_buf+offset, mlen-offset, "</tmd:StopBitList>\r\n");

    return offset;
}

int build_ParityBitList_xml(char * p_buf, int mlen, onvif_ParityBitList * p_res)
{
    int i, offset = 0;

    for (i = 0; i < p_res->sizeItems; i++)
    {
        offset += snprintf(p_buf+offset, mlen-offset,
            "<tt:Items>%s</tt:Items>\r\n", 
            onvif_ParityBitToString(p_res->Items[i]));
    }       

    return offset;
}

int build_tmd_GetVideoOutputs_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	VideoOutputList * p_output = g_onvif_cfg.v_output;

	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:GetVideoOutputsResponse>");

	while (p_output)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tmd:VideoOutputs token=\"%s\">", p_output->VideoOutput.token);

		offset += build_VideoOutput_xml(p_buf+offset, mlen-offset, &p_output->VideoOutput);

		offset += snprintf(p_buf+offset, mlen-offset, "</tmd:VideoOutputs>");

		p_output = p_output->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tmd:GetVideoOutputsResponse>");            

	return offset;
}

int build_tmd_GetVideoSources_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    VideoSourceList * p_v_src = g_onvif_cfg.v_src;
    
	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:GetVideoSourcesResponse>");

	while (p_v_src)
	{
	    offset += snprintf(p_buf+offset, mlen-offset, 
	    	"<tmd:Token>%s</tmd:Token>", 
	    	p_v_src->VideoSource.token); 
	    
	    p_v_src = p_v_src->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tmd:GetVideoSourcesResponse>");
	
	return offset;
}

int build_tmd_GetVideoOutputConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tmd_GetVideoOutputConfiguration_REQ * p_res = (tmd_GetVideoOutputConfiguration_REQ *)argv;
	VideoOutputConfigurationList * p_cfg = onvif_find_VideoOutputConfiguration_by_OutputToken(g_onvif_cfg.v_output_cfg, p_res->VideoOutputToken);
	if (NULL == p_cfg)
	{
		return ONVIF_ERR_NoVideoOutput;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:GetVideoOutputConfigurationResponse>\r\n");
	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:VideoOutputConfiguration token=\"%s\">\r\n", p_cfg->Configuration.token);
	offset += build_VideoOutputConfiguration_xml(p_buf+offset, mlen-offset, &p_cfg->Configuration);
	offset += snprintf(p_buf+offset, mlen-offset, "</tmd:VideoOutputConfiguration>\r\n");
	offset += snprintf(p_buf+offset, mlen-offset, "</tmd:GetVideoOutputConfigurationResponse>\r\n");

	return offset;
}

int build_tmd_SetVideoOutputConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:SetVideoOutputConfigurationResponse />");
	return offset;
}

int build_tmd_GetVideoOutputConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tmd_GetVideoOutputConfigurationOptions_REQ * p_res = (tmd_GetVideoOutputConfigurationOptions_REQ *)argv;
	VideoOutputList * p_output = onvif_find_VideoOutput(g_onvif_cfg.v_output, p_res->VideoOutputToken);
	if (NULL == p_output)
	{
		return ONVIF_ERR_NoVideoOutput;
	}

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tmd:GetVideoOutputConfigurationOptionsResponse>"
			"<tt:VideoOutputConfigurationOptions />"
		"</tmd:GetVideoOutputConfigurationOptionsResponse>");

	return offset;
}

int build_tmd_GetAudioOutputs_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	AudioOutputList * p_output = g_onvif_cfg.a_output;

	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:GetAudioOutputsResponse>");

	while (p_output)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tmd:Token>%s</tmd:Token>", p_output->AudioOutput.token);

		p_output = p_output->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tmd:GetAudioOutputsResponse>");

	return offset;
}

int build_tmd_GetAudioOutputConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tmd_GetAudioOutputConfiguration_REQ * p_res = (tmd_GetAudioOutputConfiguration_REQ *)argv; 
	AudioOutputConfigurationList * p_cfg = onvif_find_AudioOutputConfiguration_by_OutputToken(g_onvif_cfg.a_output_cfg, p_res->AudioOutputToken);
	if (NULL == p_cfg)
	{
		return ONVIF_ERR_NoAudioOutput;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:GetAudioOutputConfigurationResponse>\r\n");
	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:AudioOutputConfiguration token=\"%s\">\r\n", p_cfg->Configuration.token);
	offset += build_AudioOutputConfiguration_xml(p_buf+offset, mlen-offset, &p_cfg->Configuration);
	offset += snprintf(p_buf+offset, mlen-offset, "</tmd:AudioOutputConfiguration>\r\n");
	offset += snprintf(p_buf+offset, mlen-offset, "</tmd:GetAudioOutputConfigurationResponse>\r\n");

	return offset;
}

int build_tmd_GetAudioOutputConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tmd_GetAudioOutputConfigurationOptions_REQ * p_res = (tmd_GetAudioOutputConfigurationOptions_REQ *)argv; 
    AudioOutputConfigurationList * p_cfg = onvif_find_AudioOutputConfiguration_by_OutputToken(g_onvif_cfg.a_output_cfg, p_res->AudioOutputToken);
    if (NULL == p_cfg)
    {
        return ONVIF_ERR_NoAudioOutput;
    }

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tmd:GetAudioOutputConfigurationOptionsResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:AudioOutputOptions>");
    offset += build_AudioOutputConfigurationOptions_xml(p_buf+offset, mlen-offset, &p_cfg->Options);   
	offset += snprintf(p_buf+offset, mlen-offset, "</tmd:AudioOutputOptions>");

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</tmd:GetAudioOutputConfigurationOptionsResponse>");

	return offset;
}

int build_tmd_GetRelayOutputs_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	RelayOutputList * p_output = g_onvif_cfg.relay_output;

	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:GetRelayOutputsResponse>");

	while (p_output)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tmd:RelayOutputs token=\"%s\">", p_output->RelayOutput.token);

		offset += build_RelayOutput_xml(p_buf+offset, mlen-offset, &p_output->RelayOutput);

		offset += snprintf(p_buf+offset, mlen-offset, "</tmd:RelayOutputs>");

		p_output = p_output->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tmd:GetRelayOutputsResponse>");

	return offset;
}

int build_tmd_GetRelayOutputOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	onvif_RelayOutputOptions * p_option = NULL;
	tmd_GetRelayOutputOptions_REQ * p_res = (tmd_GetRelayOutputOptions_REQ *)argv;
	
	RelayOutputList *relay_output = NULL;
	
	if (p_res->RelayOutputTokenFlag)
	{
		RelayOutputList * p_output = onvif_find_RelayOutput(g_onvif_cfg.relay_output, p_res->RelayOutputToken);
		if (p_output)
		{
			relay_output = p_output;
			p_option = &relay_output->Options;
		}
		else
		{
			return ONVIF_ERR_NoConfig;
		}
	}
	else if (g_onvif_cfg.relay_output)
	{
		relay_output = g_onvif_cfg.relay_output;
		p_option = &relay_output->Options;
	}
	else
	{
		return ONVIF_ERR_NoConfig;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:GetRelayOutputOptionsResponse>");
	while (relay_output)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tmd:RelayOutputOptions token=\"%s\">",p_option->token);
		offset += build_RelayOutputOptions_xml(p_buf+offset, mlen-offset, p_option);
		offset += snprintf(p_buf+offset, mlen-offset, "</tmd:RelayOutputOptions>");
		
		relay_output = relay_output->next;
		p_option = &relay_output->Options;
	}
	offset += snprintf(p_buf+offset, mlen-offset, "</tmd:GetRelayOutputOptionsResponse>");

	return offset;
}

int build_tmd_SetRelayOutputSettings_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:SetRelayOutputSettingsResponse />");
	return offset;
}

int build_tmd_SetRelayOutputState_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:SetRelayOutputStateResponse />");
	return offset;
}

int build_tmd_GetDigitalInputs_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	DigitalInputList * p_input = g_onvif_cfg.digit_input;

	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:GetDigitalInputsResponse>");
	if (!g_bHxVersion)
	{
		while (p_input)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tmd:DigitalInputs token=\"%s\" IdleState=\"%s\">"
				"</tmd:DigitalInputs>",
				p_input->DigitalInput.token,
				onvif_DigitalIdleStateToString(p_input->DigitalInput.IdleState)); 

			p_input = p_input->next;
		}
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tmd:GetDigitalInputsResponse>");

	return offset;
}

int build_tmd_GetDigitalInputConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	onvif_DigitalInputConfigurationOptions * p_option = NULL;
	tmd_GetDigitalInputConfigurationOptions_REQ * p_res = (tmd_GetDigitalInputConfigurationOptions_REQ *)argv;

	if (p_res->TokenFlag)
	{
		DigitalInputList * p_input = onvif_find_DigitalInput(g_onvif_cfg.digit_input, p_res->Token);
		if (p_input)
		{
			p_option = &p_input->Options;
		}
		else
		{
			return ONVIF_ERR_NoConfig;
		}
	}
	else if (g_onvif_cfg.digit_input)
	{
		p_option = &g_onvif_cfg.digit_input->Options;
	}
	else
	{
		return ONVIF_ERR_NoConfig;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:GetDigitalInputConfigurationOptionsResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:DigitalInputOptions>");
	offset += build_DigitalInputConfigurationOptions_xml(p_buf+offset, mlen-offset, p_option);
	offset += snprintf(p_buf+offset, mlen-offset, "</tmd:DigitalInputOptions>");

	offset += snprintf(p_buf+offset, mlen-offset, "</tmd:GetDigitalInputConfigurationOptionsResponse>");

	return offset;
}

int build_tmd_SetDigitalInputConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:SetDigitalInputConfigurationsResponse />");
	return offset;
}

int build_tmd_GetSerialPorts_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    SerialPortList * p_port = g_onvif_cfg.serial_port;
    
	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:GetSerialPortsResponse>");
	
	while (p_port)
	{
	    offset += snprintf(p_buf+offset, mlen-offset, 
	        "<tmd:SerialPort token=\"%s\"></tmd:SerialPort>",
	        p_port->SerialPort.token);

	    p_port = p_port->next;
    }
    
    offset += snprintf(p_buf+offset, mlen-offset, "</tmd:GetSerialPortsResponse>");            

	return offset;
}

int build_tmd_GetSerialPortConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tmd_GetSerialPortConfiguration_REQ * p_res = (tmd_GetSerialPortConfiguration_REQ *)argv;

    SerialPortList * p_port = onvif_find_SerialPort(g_onvif_cfg.serial_port, p_res->SerialPortToken);
    if (NULL == p_port)
    {
        return ONVIF_ERR_InvalidSerialPort;
    }

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tmd:GetSerialPortConfigurationResponse>\r\n");
    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tmd:SerialPortConfiguration token=\"%s\" type=\"%s\">\r\n",
        p_port->Configuration.token,
        onvif_SerialPortTypeToString(p_port->Configuration.type));
    offset += build_SerialPortConfiguration_xml(p_buf+offset, mlen-offset, &p_port->Configuration);
    offset += snprintf(p_buf+offset, mlen-offset, 
        "</tmd:SerialPortConfiguration>\r\n");
    offset += snprintf(p_buf+offset, mlen-offset, 
        "</tmd:GetSerialPortConfigurationResponse>\r\n");

	return offset;    
}

int build_tmd_GetSerialPortConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tmd_GetSerialPortConfigurationOptions_REQ * p_res = (tmd_GetSerialPortConfigurationOptions_REQ *)argv;

    SerialPortList * p_port = onvif_find_SerialPort(g_onvif_cfg.serial_port, p_res->SerialPortToken);
    if (NULL == p_port)
    {
        return ONVIF_ERR_InvalidSerialPort;
    }

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tmd:GetSerialPortConfigurationOptionsResponse>\r\n");
    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tmd:SerialPortOptions token=\"%s\">\r\n",
        p_port->Options.token);
    offset += build_SerialPortConfigurationOptions_xml(p_buf+offset, mlen-offset, &p_port->Options);
    offset += snprintf(p_buf+offset, mlen-offset, 
        "</tmd:SerialPortOptions>\r\n");
    offset += snprintf(p_buf+offset, mlen-offset, 
        "</tmd:GetSerialPortConfigurationOptionsResponse>\r\n");        

	return offset; 
}

int build_tmd_SetSerialPortConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:SetSerialPortConfigurationResponse />");
	return offset;
}

int build_tmd_SendReceiveSerialCommand_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tmd_SendReceiveSerialCommand_RES * p_res = (tmd_SendReceiveSerialCommand_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:SendReceiveSerialCommandResponse>");

	if (p_res->SerialDataFlag)
	{
	    offset += snprintf(p_buf+offset, mlen-offset, "<tmd:SerialData>");

	    if (p_res->SerialData._union_SerialData == 0)
	    {
	        offset += snprintf(p_buf+offset, mlen-offset, 
	            "<tmd:Binary>%s</tmd:Binary>",
	            p_res->SerialData.union_SerialData.Binary);
	    }
	    else
	    {
	        offset += snprintf(p_buf+offset, mlen-offset, 
	            "<tmd:String>%s</tmd:String>",
	            p_res->SerialData.union_SerialData.String);
	    }
	    
	    offset += snprintf(p_buf+offset, mlen-offset, "</tmd:SerialData>");
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tmd:SendReceiveSerialCommandResponse>");

	return offset;
}

int build_tmd_GetAudioSources_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

#ifdef AUDIO_SUPPORT
	AudioSourceList * p_a_src = g_onvif_cfg.a_src;

	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:GetAudioSourcesResponse>");

	while (p_a_src)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tmd:Token>%s</tmd:Token>", p_a_src->AudioSource.token);
		p_a_src = p_a_src->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tmd:GetAudioSourcesResponse>");
#else
	offset += snprintf(p_buf+offset, mlen-offset, "<tmd:GetAudioSourcesResponse />");
#endif

	return offset;
}

#endif // end of DEVICEIO_SUPPORT

#ifdef MEDIA2_SUPPORT

BOOL find_ConfigurationType(tr2_GetProfiles_REQ * p_res, const char * type)
{
    int i;

    for (i = 0; i < p_res->sizeType; i++)
    {
        if (strcasecmp(p_res->Type[i], type) == 0 || 
            strcasecmp(p_res->Type[i], "all") == 0)
        {
            return TRUE;
        }
    }

    return FALSE;
}

int build_ColorspaceRange_xml(char * p_buf, int mlen, onvif_ColorspaceRange * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tt:X>"
            "<tt:Min>%0.2f</tt:Min>"
            "<tt:Max>%0.2f</tt:Max>"
        "</tt:X>"
        "<tt:Y>"
            "<tt:Min>%0.2f</tt:Min>"
            "<tt:Max>%0.2f</tt:Max>"
        "</tt:Y>"
        "<tt:Z>"
            "<tt:Min>%0.2f</tt:Min>"
            "<tt:Max>%0.2f</tt:Max>"
        "</tt:Z>"
        "<tt:Colorspace>%s</tt:Colorspace>",
        p_res->X.Min, p_res->X.Max,
        p_res->Y.Min, p_res->Y.Max,
        p_res->Z.Min, p_res->Z.Max,
        p_res->Colorspace);

    return offset;

}

int build_ColorOptions_xml(char * p_buf, int mlen, onvif_ColorOptions * p_res)
{
	uint32 i;
    int offset = 0;

    for (i = 0; i < p_res->sizeColorList; i++)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tt:ColorList X=\"%0.2f\" Y=\"%0.2f\" Z=\"%0.2f\"", 
            p_res->ColorList[i].X, 
            p_res->ColorList[i].Y, 
            p_res->ColorList[i].Z);

        if (p_res->ColorList[i].ColorspaceFlag)
        {
            offset += snprintf(p_buf+offset, mlen-offset, 
                " Colorspace=\"%s\"", 
                p_res->ColorList[i].Colorspace);
        }

        offset += snprintf(p_buf+offset, mlen-offset, " />");
    }

    for (i = 0; i < p_res->sizeColorspaceRange; i++)
    {
        offset += snprintf(p_buf+offset, mlen-offset, "<tt:ColorspaceRange>");
        offset += build_ColorspaceRange_xml(p_buf+offset, mlen-offset, &p_res->ColorspaceRange[i]);
        offset += snprintf(p_buf+offset, mlen-offset, "</tt:ColorspaceRange>");
    }

    return offset;
}

int build_OSDColorOptions_xml(char * p_buf, int mlen, onvif_OSDColorOptions * p_res)
{
    int offset = 0;

    if (p_res->ColorFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, "<tt:Color>");
        offset += build_ColorOptions_xml(p_buf+offset, mlen-offset, &p_res->Color);
        offset += snprintf(p_buf+offset, mlen-offset, "</tt:Color>");
    }

    if (p_res->TransparentFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:Transparent>"
				"<tt:Min>%d</tt:Min>"				
				"<tt:Max>%d</tt:Max>"				
			"</tt:Transparent>", 
			p_res->Transparent.Min,
			p_res->Transparent.Max);
    }

    return offset;    
}

int build_OSDTextOptions_xml(char * p_buf, int mlen, onvif_OSDTextOptions * p_res)
{
	uint32 i;
    int offset = 0;
    
    offset += snprintf(p_buf+offset, mlen-offset, "<tt:TextOption>");
		
	if (p_res->OSDTextType_Plain)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:Type>%s</tt:Type>", 
			onvif_OSDTextTypeToString(OSDTextType_Plain));
	}
	if (p_res->OSDTextType_Date)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:Type>%s</tt:Type>", 
			onvif_OSDTextTypeToString(OSDTextType_Date));
	}
	if (p_res->OSDTextType_Time)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:Type>%s</tt:Type>", 
			onvif_OSDTextTypeToString(OSDTextType_Time));
	}
	if (p_res->OSDTextType_DateAndTime)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:Type>%s</tt:Type>", 
			onvif_OSDTextTypeToString(OSDTextType_DateAndTime));
	}

	if (p_res->FontSizeRangeFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:FontSizeRange>"
				"<tt:Min>%d</tt:Min>"				
				"<tt:Max>%d</tt:Max>"				
			"</tt:FontSizeRange>", 
			p_res->FontSizeRange.Min,
			p_res->FontSizeRange.Max);
	}

	for (i = 0; i < p_res->DateFormatSize; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:DateFormat>%s</tt:DateFormat>",
			p_res->DateFormat[i]);
	}
	
	for (i = 0; i < p_res->TimeFormatSize; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:TimeFormat>%s</tt:TimeFormat>",
			p_res->TimeFormat[i]);
	}

	if (p_res->FontColorFlag)
	{
	    offset += snprintf(p_buf+offset, mlen-offset, "<tt:FontColor>");
	    offset += build_OSDColorOptions_xml(p_buf+offset, mlen-offset, &p_res->FontColor);
	    offset += snprintf(p_buf+offset, mlen-offset, "</tt:FontColor>");
	}

	if (p_res->BackgroundColorFlag)
	{
	    offset += snprintf(p_buf+offset, mlen-offset, "<tt:BackgroundColor>");
	    offset += build_OSDColorOptions_xml(p_buf+offset, mlen-offset, &p_res->BackgroundColor);
	    offset += snprintf(p_buf+offset, mlen-offset, "</tt:BackgroundColor>");
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tt:TextOption>");

	return offset;
}

int build_OSDImgOptions_xml(char * p_buf, int mlen, onvif_OSDImgOptions * p_res)
{
	uint32 i;
    int offset = 0;
    
    offset += snprintf(p_buf+offset, mlen-offset, "<tt:ImageOption>");

	for (i = 0; i < p_res->ImagePathSize; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:ImagePath>%s</tt:ImagePath>",
			p_res->ImagePath[i]);
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:ImageOption>");

	return offset;
}

int build_Polygon_xml(char * p_buf, int mlen, onvif_Polygon * p_res)
{
	uint32 i;
	int offset = 0;

	for (i = 0; i < p_res->sizePoint; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Point x=\"%0.2f\" y=\"%0.2f\" />", p_res->Point[i].x, p_res->Point[i].y);
	}

	return offset;
}

int build_Color_xml(char * p_buf, int mlen, onvif_Color * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Color X=\"%0.2f\" Y=\"%0.2f\" Z=\"%0.2f\"", p_res->X, p_res->Y, p_res->Z);

	if (p_res->ColorspaceFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, " Colorspace=\"%s\"", p_res->Colorspace);
	}

	if (p_res->LikelihoodFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, " Likelihood=\"%0.2f\"", p_res->Likelihood);
	}

	offset += snprintf(p_buf+offset, mlen-offset, " />\r\n");

	return offset;
}

int build_Mask_xml(char * p_buf, int mlen, onvif_Mask * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:ConfigurationToken>%s</tr2:ConfigurationToken>\r\n", p_res->ConfigurationToken);

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Polygon>");
	offset += build_Polygon_xml(p_buf+offset, mlen-offset, &p_res->Polygon);
	offset += snprintf(p_buf+offset, mlen-offset, "</tr2:Polygon>");

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Type>%s</tr2:Type>\r\n", p_res->Type);

	if (p_res->ColorFlag)
	{
		offset += build_Color_xml(p_buf+offset, mlen-offset, &p_res->Color);
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Enabled>%s</tr2:Enabled>\r\n", p_res->Enabled ? "true" : "false");

	return offset;    
}

int build_VideoEncoder2ConfigurationOptions_xml(char * p_buf, int mlen, onvif_VideoEncoder2ConfigurationOptions * p_res)
{
	uint32 i;
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Encoding>%s</tt:Encoding>"
		"<tt:QualityRange>"
			"<tt:Min>%d</tt:Min>"
			"<tt:Max>%d</tt:Max>"
		"</tt:QualityRange>",
		p_res->Encoding,
		p_res->QualityRange.Min, 
		p_res->QualityRange.Max);

	for (i = 0; i < ARRAY_SIZE(p_res->ResolutionsAvailable); i++)
	{
		if (p_res->ResolutionsAvailable[i].Width == 0 || p_res->ResolutionsAvailable[i].Height == 0)
		{
			continue;
		}

		offset += build_VideoResolution_xml(p_buf+offset, mlen-offset, &p_res->ResolutionsAvailable[i]);
	}

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:BitrateRange>"
			"<tt:Min>%d</tt:Min>"
			"<tt:Max>%d</tt:Max>"
		"</tt:BitrateRange>",
		p_res->BitrateRange.Min, 
		p_res->BitrateRange.Max);

	return offset;
}

int build_AudioEncoder2ConfigurationOptions_xml(char * p_buf, int mlen, onvif_AudioEncoder2ConfigurationOptions * p_res)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Encoding>%s</tt:Encoding>\r\n", p_res->Encoding);

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:BitrateList>\r\n");
	offset += build_IntList_xml(p_buf+offset, mlen-offset, &p_res->BitrateList);
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:BitrateList>\r\n");
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:SampleRateList>\r\n");
	offset += build_IntList_xml(p_buf+offset, mlen-offset, &p_res->SampleRateList);
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:SampleRateList>\r\n");

	return offset;
}

int build_tr2_GetVideoEncoderConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	BOOL loopflag = 0;
	int offset = 0;
	tr2_GetVideoEncoderConfigurations_REQ * p_res = (tr2_GetVideoEncoderConfigurations_REQ *)argv;
	VideoEncoder2ConfigurationList * p_v_enc_cfg = NULL;

	if (p_res->GetConfiguration.ConfigurationTokenFlag)
	{
		p_v_enc_cfg = onvif_find_VideoEncoder2Configuration(g_onvif_cfg.v_enc_cfg, p_res->GetConfiguration.ConfigurationToken);
		if (NULL == p_v_enc_cfg)
		{
			return ONVIF_ERR_NoConfig;
		}
	}

	if (p_res->GetConfiguration.ProfileTokenFlag)
	{
		ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->GetConfiguration.ProfileToken);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NoProfile;
		}
	}

	if (NULL == p_v_enc_cfg)
	{
		loopflag = TRUE;
		p_v_enc_cfg = g_onvif_cfg.v_enc_cfg;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:GetVideoEncoderConfigurationsResponse>");

	while (p_v_enc_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Configurations token=\"%s\"", p_v_enc_cfg->Configuration.token);

		if (p_v_enc_cfg->Configuration.GovLengthFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " GovLength=\"%d\"", p_v_enc_cfg->Configuration.GovLength);
		}

		if (p_v_enc_cfg->Configuration.AnchorFrameDistanceFlag)
		{
			offset += snprintf(p_buf + offset, mlen - offset," AnchorFrameDistance=\"%d\"", p_v_enc_cfg->Configuration.AnchorFrameDistance);
		}

		if (p_v_enc_cfg->Configuration.ProfileFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " Profile=\"%s\"", p_v_enc_cfg->Configuration.Profile);
		}

		offset += snprintf(p_buf+offset, mlen-offset, " GuaranteedFrameRate=\"%s\">", p_v_enc_cfg->Configuration.GuaranteedFrameRate ? "true" : "false");

		offset += build_VideoEncoder2Configuration_xml(p_buf+offset, mlen-offset, &p_v_enc_cfg->Configuration);

		offset += snprintf(p_buf+offset, mlen-offset, "</tr2:Configurations>");

		if (loopflag)
		{
			p_v_enc_cfg = p_v_enc_cfg->next;
		}
		else
		{
			break;
		}
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tr2:GetVideoEncoderConfigurationsResponse>");

	return offset;
}

int build_tr2_GetVideoEncoderConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tr2_GetVideoEncoderConfigurationOptions_RES * p_res = (tr2_GetVideoEncoderConfigurationOptions_RES *)argv;
	VideoEncoder2ConfigurationOptionsList * p_option;

	p_option = p_res->Options;

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:GetVideoEncoderConfigurationOptionsResponse>");

	while (p_option)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Options");

		if (p_option->Options.GovLengthRangeFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " GovLengthRange=\"%s\"", p_option->Options.GovLengthRange);
		}

		if (p_option->Options.MaxAnchorFrameDistanceFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " MaxAnchorFrameDistance=\"%d\"", p_option->Options.MaxAnchorFrameDistance);
		}

		if (p_option->Options.FrameRatesSupportedFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " FrameRatesSupported=\"%s\"", p_option->Options.FrameRatesSupported);
		}

		if (p_option->Options.ProfilesSupportedFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " ProfilesSupported=\"%s\"", p_option->Options.ProfilesSupported);
		}

		offset += snprintf(p_buf+offset, mlen-offset, " ConstantBitRateSupported=\"%s\"", p_option->Options.ConstantBitRateSupported ? "true" : "false");

		offset += snprintf(p_buf+offset, mlen-offset, " GuaranteedFrameRateSupported=\"%s\">", p_option->Options.GuaranteedFrameRateSupported ? "true" : "false");

		offset += build_VideoEncoder2ConfigurationOptions_xml(p_buf + offset, mlen - offset, &p_option->Options);

		offset += snprintf(p_buf+offset, mlen-offset, "</tr2:Options>");

		p_option = p_option->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tr2:GetVideoEncoderConfigurationOptionsResponse>");

	return offset;
}

int build_tr2_SetVideoEncoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:SetVideoEncoderConfigurationResponse />");
	return offset;
}

int build_tr2_CreateProfile_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tr2_CreateProfile_RES * p_res = (tr2_CreateProfile_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tr2:CreateProfileResponse>"
			"<tr2:Token>%s</tr2:Token>"
		"</tr2:CreateProfileResponse>",
		p_res->Token);
	
	return offset;
}

int build_tr2_Profile_xml(char * p_buf, int mlen, tr2_GetProfiles_REQ * p_res, ONVIF_PROFILE * p_profile)
{
	int offset = 0;

	if (p_profile->v_src_cfg)// && find_ConfigurationType(p_res, "VideoSource"))
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tr2:VideoSource token=\"%s\">", p_profile->v_src_cfg->Configuration.token);
		offset += build_VideoSourceConfiguration_xml_by_profile(p_buf+offset, mlen-offset, p_profile);
		offset += snprintf(p_buf+offset, mlen-offset, "</tr2:VideoSource>");
	}

#ifdef AUDIO_SUPPORT
	if (p_profile->a_src_cfg)// && find_ConfigurationType(p_res, "AudioSource"))
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tr2:AudioSource token=\"%s\">", p_profile->a_src_cfg->Configuration.token);
		offset += build_AudioSourceConfiguration_xml(p_buf+offset, mlen-offset, &p_profile->a_src_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tr2:AudioSource>");
	}
#endif

	if (p_profile->v_enc_cfg)// && find_ConfigurationType(p_res, "VideoEncoder"))
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tr2:VideoEncoder token=\"%s\"", p_profile->v_enc_cfg->Configuration.token);

		if (p_profile->v_enc_cfg->Configuration.GovLengthFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " GovLength=\"%d\"", p_profile->v_enc_cfg->Configuration.GovLength);
		}

		if (p_profile->v_enc_cfg->Configuration.AnchorFrameDistanceFlag)
		{
			offset += snprintf(p_buf + offset, mlen - offset," AnchorFrameDistance=\"%d\"", p_profile->v_enc_cfg->Configuration.AnchorFrameDistance);
		}

		if (p_profile->v_enc_cfg->Configuration.ProfileFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, " Profile=\"%s\"", onvif_MediaProfile2Media2Profile(p_profile->v_enc_cfg->Configuration.Profile));
		}

		offset += snprintf(p_buf+offset, mlen-offset, " GuaranteedFrameRate=\"%s\">", p_profile->v_enc_cfg->Configuration.GuaranteedFrameRate ? "true" : "false");

		offset += build_VideoEncoder2Configuration_xml(p_buf+offset, mlen-offset, &p_profile->v_enc_cfg->Configuration);

		offset += snprintf(p_buf+offset, mlen-offset, "</tr2:VideoEncoder>");
	}

#ifdef AUDIO_SUPPORT
	if (p_profile->a_enc_cfg)// && find_ConfigurationType(p_res, "AudioEncoder"))
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tr2:AudioEncoder token=\"%s\">", p_profile->a_enc_cfg->Configuration.token);
		offset += build_AudioEncoder2Configuration_xml(p_buf+offset, mlen-offset, &p_profile->a_enc_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tr2:AudioEncoder>");
	}
#endif

#ifdef VIDEO_ANALYTICS
	if (p_profile->va_cfg)// && find_ConfigurationType(p_res, "Analytics"))
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Analytics token=\"%s\">", p_profile->va_cfg->Configuration.token);
		offset += build_VideoAnalyticsConfiguration_xml(p_buf+offset, mlen-offset, &p_profile->va_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tr2:Analytics>");
	}
#endif

#ifdef PTZ_SUPPORT
	if (p_profile->ptz_cfg)// && find_ConfigurationType(p_res, "PTZ"))
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tr2:PTZ token=\"%s\" "
				"MoveRamp=\"%d\" "
				"PresetRamp=\"%d\" "
				"PresetTourRamp=\"%d\">", 
			p_profile->ptz_cfg->Configuration.token, 
			p_profile->ptz_cfg->Configuration.MoveRamp,
			p_profile->ptz_cfg->Configuration.PresetRamp, 
			p_profile->ptz_cfg->Configuration.PresetTourRamp);
		offset += build_PTZConfiguration_xml(p_buf+offset, mlen-offset, &p_profile->ptz_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tr2:PTZ>");
	}
#endif

	if (p_profile->metadata_cfg)// && find_ConfigurationType(p_res, "Metadata"))
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tr2:Metadata token=\"%s\" "
				"CompressionType=\"%s\" "
				"GeoLocation=\"%s\" "
				"ShapePolygon=\"%s\">", 
			p_profile->metadata_cfg->Configuration.token,
			p_profile->metadata_cfg->Configuration.CompressionType,
			p_profile->metadata_cfg->Configuration.GeoLocation ? "true" : "false",
			p_profile->metadata_cfg->Configuration.ShapePolygon ? "true" : "false");
		offset += build_MetadataConfiguration_xml(p_buf+offset, mlen-offset, &p_profile->metadata_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tr2:Metadata>");
	}

#ifdef DEVICEIO_SUPPORT
	if (p_profile->a_output_cfg)// && find_ConfigurationType(p_res, "AudioOutput"))
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tr2:AudioOutput token=\"%s\">", p_profile->a_output_cfg->Configuration.token);
		offset += build_AudioOutputConfiguration_xml(p_buf+offset, mlen-offset, &p_profile->a_output_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tr2:AudioOutput>");
	}
#endif

#ifdef AUDIO_SUPPORT
	if (p_profile->a_dec_cfg)// && find_ConfigurationType(p_res, "AudioDecoder"))
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tr2:AudioDecoder token=\"%s\">", p_profile->a_dec_cfg->Configuration.token);
		offset += build_AudioDecoderConfiguration_xml(p_buf+offset, mlen-offset, &p_profile->a_dec_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tr2:AudioDecoder>");
	}
#endif

	return offset;
}

int build_tr2_GetProfiles_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	BOOL loopflag = 0;
	int offset = 0;
	tr2_GetProfiles_REQ * p_res = (tr2_GetProfiles_REQ *)argv;
	ONVIF_PROFILE * p_profile = NULL;

	if (p_res->TokenFlag)
	{
		p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->Token);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NoProfile;
		}
	}

	if (NULL == p_profile)
	{
		loopflag = TRUE;
		p_profile = g_onvif_cfg.profiles;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:GetProfilesResponse>\r\n");

	while (p_profile)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tr2:Profiles token=\"%s\" fixed=\"%s\">\r\n"
				"<tr2:Name>%s</tr2:Name>\r\n"
				"<tr2:Configurations>\r\n",
			p_profile->token, p_profile->fixed ? "true" : "false", 
			p_profile->name);

		offset += build_tr2_Profile_xml(p_buf+offset, mlen-offset, p_res, p_profile);
		
		offset += snprintf(p_buf+offset, mlen-offset, 
				"</tr2:Configurations>\r\n"
			"</tr2:Profiles>\r\n");

		if (loopflag)
		{
			p_profile = p_profile->next;
		}
		else
		{
			break;
		}
	}    

	offset += snprintf(p_buf+offset, mlen-offset, "</tr2:GetProfilesResponse>");
	
	return offset;
}

int build_tr2_DeleteProfile_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:DeleteProfileResponse />");		    
	return offset;
}

int build_tr2_AddConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:AddConfigurationResponse />");		    
	return offset;
}

int build_tr2_RemoveConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:RemoveConfigurationResponse />");		    
	return offset;
}

int build_tr2_GetVideoSourceConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	BOOL loopflag = 0;
	int offset = 0;
	tr2_GetVideoSourceConfigurations_REQ * p_res = (tr2_GetVideoSourceConfigurations_REQ *)argv;
	VideoSourceConfigurationList * p_v_src_cfg = NULL;

	if (p_res->GetConfiguration.ConfigurationTokenFlag)
	{
		p_v_src_cfg = onvif_find_VideoSourceConfiguration(g_onvif_cfg.v_src_cfg, p_res->GetConfiguration.ConfigurationToken);
		if (NULL == p_v_src_cfg)
		{
			return ONVIF_ERR_NoConfig;
		}
	}

	if (p_res->GetConfiguration.ProfileTokenFlag)
	{
		ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->GetConfiguration.ProfileToken);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NoProfile;
		}
	}

	if (NULL == p_v_src_cfg)
	{
		loopflag = TRUE;
		p_v_src_cfg = g_onvif_cfg.v_src_cfg;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:GetVideoSourceConfigurationsResponse>");

	while (p_v_src_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Configurations token=\"%s\">", p_v_src_cfg->Configuration.token);
		offset += build_VideoSourceConfiguration_xml(p_buf+offset, mlen-offset, &p_v_src_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tr2:Configurations>");

		if (loopflag)
		{
			p_v_src_cfg = p_v_src_cfg->next;
		}
		else
		{
			break;
		}
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tr2:GetVideoSourceConfigurationsResponse>");

	return offset;
}

int build_tr2_GetMetadataConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	BOOL loopflag = 0;
	int offset = 0;
	tr2_GetMetadataConfigurations_REQ * p_res = (tr2_GetMetadataConfigurations_REQ *)argv;
	MetadataConfigurationList * p_metadata_cfg = NULL;

	if (p_res->GetConfiguration.ConfigurationTokenFlag)
	{
		p_metadata_cfg = onvif_find_MetadataConfiguration(g_onvif_cfg.metadata_cfg, p_res->GetConfiguration.ConfigurationToken);
		if (NULL == p_metadata_cfg)
		{
			return ONVIF_ERR_NoConfig;
		}
	}

	if (p_res->GetConfiguration.ProfileTokenFlag)
	{
		ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->GetConfiguration.ProfileToken);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NoProfile;
		}
	}

	if (NULL == p_metadata_cfg)
	{
		loopflag = TRUE;
		p_metadata_cfg = g_onvif_cfg.metadata_cfg;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:GetMetadataConfigurationsResponse>");

	while (p_metadata_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tr2:Configurations token=\"%s\" "
				"CompressionType=\"%s\" "
				"GeoLocation=\"%s\" "
				"ShapePolygon=\"%s\">", 
			p_metadata_cfg->Configuration.token,
			p_metadata_cfg->Configuration.CompressionType,
			p_metadata_cfg->Configuration.GeoLocation ? "true" : "false",
			p_metadata_cfg->Configuration.ShapePolygon ? "true" : "false");
		offset += build_MetadataConfiguration_xml(p_buf+offset, mlen-offset, &p_metadata_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tr2:Configurations>");

		if (loopflag)
		{
			p_metadata_cfg = p_metadata_cfg->next;
		}
		else
		{
			break;
		}
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tr2:GetMetadataConfigurationsResponse>");

	return offset;
}

int build_tr2_SetVideoSourceConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:SetVideoSourceConfigurationResponse />");
	return offset;
}

int build_tr2_SetMetadataConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:SetMetadataConfigurationResponse />");		    
	return offset;
}

int build_tr2_SetAudioSourceConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:SetAudioSourceConfigurationResponse />");		    
	return offset;
}

int build_tr2_GetVideoSourceConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	VideoSourceConfigurationList * p_v_src_cfg = NULL;
	tr2_GetVideoSourceConfigurationOptions_REQ * p_res = (tr2_GetVideoSourceConfigurationOptions_REQ *)argv;

	if (p_res->GetConfiguration.ProfileTokenFlag && p_res->GetConfiguration.ProfileToken[0] != '\0')
	{
		ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->GetConfiguration.ProfileToken);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NoProfile;
		}

		p_v_src_cfg = p_profile->v_src_cfg;
	}

	if (p_res->GetConfiguration.ConfigurationTokenFlag && p_res->GetConfiguration.ConfigurationToken[0] != '\0')
	{
		p_v_src_cfg = onvif_find_VideoSourceConfiguration(g_onvif_cfg.v_src_cfg, p_res->GetConfiguration.ConfigurationToken);
		if (NULL == p_v_src_cfg)
		{
			return ONVIF_ERR_NoConfig;
		}
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:GetVideoSourceConfigurationOptionsResponse>");

	if (p_v_src_cfg)
	{
		if (p_v_src_cfg->Options.MaximumNumberOfProfilesFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Options MaximumNumberOfProfiles=\"%d\">", p_v_src_cfg->Options.MaximumNumberOfProfiles);
		}
		else
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Options>");
		}

		offset += build_VideoSourceConfigurationOptions_xml(p_buf+offset, mlen-offset, &p_v_src_cfg->Options);

		offset += snprintf(p_buf+offset, mlen-offset, "</tr2:Options>");
	}
	else
	{
		p_v_src_cfg = g_onvif_cfg.v_src_cfg;
		while (p_v_src_cfg)
		{
			if (p_v_src_cfg->Options.MaximumNumberOfProfilesFlag)
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Options MaximumNumberOfProfiles=\"%d\">", p_v_src_cfg->Options.MaximumNumberOfProfiles);
			}
			else
			{
				offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Options>");
			}

			offset += build_VideoSourceConfigurationOptions_xml(p_buf+offset, mlen-offset, &p_v_src_cfg->Options);

			offset += snprintf(p_buf+offset, mlen-offset, "</tr2:Options>");

			p_v_src_cfg = p_v_src_cfg->next;
		}
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tr2:GetVideoSourceConfigurationOptionsResponse>");
	
	return offset;
}

int build_tr2_GetMetadataConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	ONVIF_PROFILE * p_profile = NULL;
	tr2_GetMetadataConfigurationOptions_REQ * p_res = (tr2_GetMetadataConfigurationOptions_REQ *) argv;

	if (p_res->GetConfiguration.ProfileTokenFlag)
	{
		p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->GetConfiguration.ProfileToken);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NoProfile;
		}
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:GetMetadataConfigurationOptionsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Options>");

	offset += build_MetadataConfigurationOptions_xml(p_buf+offset, mlen-offset, &g_onvif_cfg.MetadataConfigurationOptions);
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tr2:Options>");
	offset += snprintf(p_buf+offset, mlen-offset, "</tr2:GetMetadataConfigurationOptionsResponse>");
	
	return offset;
}

int build_tr2_GetVideoEncoderInstances_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	uint32 i;
	int offset = 0;
	tr2_GetVideoEncoderInstances_RES * p_res = (tr2_GetVideoEncoderInstances_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tr2:GetVideoEncoderInstancesResponse>"
		"<tr2:Info>");

	for (i = 0; i < p_res->Info.sizeCodec; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tr2:Codec>"
				"<tr2:Encoding>%s</tr2:Encoding>"
				"<tr2:Number>%d</tr2:Number>"
			"</tr2:Codec>",
			p_res->Info.Codec[i].Encoding,
			p_res->Info.Codec[i].Number);
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Total>%d</tr2:Total>", p_res->Info.Total);

	offset += snprintf(p_buf+offset, mlen-offset, 
		"</tr2:Info>"
		"</tr2:GetVideoEncoderInstancesResponse>");
	
	return offset;
}

int build_tr2_GetStreamUri_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tr2_GetStreamUri_RES * p_res = (tr2_GetStreamUri_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tr2:GetStreamUriResponse>"
			"<tr2:Uri>%s</tr2:Uri>"
		"</tr2:GetStreamUriResponse>", p_res->Uri);

	log_print(HT_LOG_INFO, "rtspuri : %s\n", p_res->Uri);

	return offset;
}

int build_tr2_SetSynchronizationPoint_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:SetSynchronizationPointResponse />");		    
	return offset;
}

int build_tr2_GetVideoSourceModes_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tr2_GetVideoSourceModes_REQ * p_res = (tr2_GetVideoSourceModes_REQ *)argv;

	VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_res->VideoSourceToken);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NoVideoSource;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:GetVideoSourceModesResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:VideoSourceModes token=\"%s\" Enabled=\"%s\">", p_v_src->VideoSourceMode.token, p_v_src->VideoSourceMode.Enabled ? "true" : "false");

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tr2:MaxFramerate>%0.1f</tr2:MaxFramerate>"
		"<tr2:MaxResolution>"
			"<tt:Width>%d</tt:Width>"
			"<tt:Height>%d</tt:Height>"
		"</tr2:MaxResolution>"
		"<tr2:Encodings>%s</tr2:Encodings>"
		"<tr2:Reboot>%s</tr2:Reboot>",
		p_v_src->VideoSourceMode.MaxFramerate,
		p_v_src->VideoSourceMode.MaxResolution.Width,
		p_v_src->VideoSourceMode.MaxResolution.Height,
		p_v_src->VideoSourceMode.Encodings,
		p_v_src->VideoSourceMode.Reboot ? "true" : "false");

	if (p_v_src->VideoSourceMode.DescriptionFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Description>%s</tr2:Description>", p_v_src->VideoSourceMode.Description);
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tr2:VideoSourceModes>");

	offset += snprintf(p_buf+offset, mlen-offset, "</tr2:GetVideoSourceModesResponse>");

	return offset;
}

int build_tr2_SetVideoSourceMode_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tr2_SetVideoSourceMode_RES * p_res = (tr2_SetVideoSourceMode_RES *) argv;
    
    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tr2:SetVideoSourceModeResponse>"
            "<tr2:Reboot>%s</tr2:Reboot>"
        "</tr2:SetVideoSourceModeResponse>",
        p_res->Reboot ? "true" : "false");

	return offset;
}

int build_tr2_GetSnapshotUri_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	tr2_GetSnapshotUri_RES * p_res = (tr2_GetSnapshotUri_RES *) argv;
    
	offset += snprintf(p_buf+offset, mlen-offset, 
        "<tr2:GetSnapshotUriResponse>"
            "<tr2:Uri>%s</tr2:Uri>"
        "</tr2:GetSnapshotUriResponse>",
        p_res->Uri);
	
	return offset;
}

int build_tr2_SetOSD_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:SetOSDResponse />");
	return offset;
}

int build_tr2_GetOSDOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	onvif_OSDConfigurationOptions * p_res = &g_onvif_cfg.OSDConfigurationOptions;

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tr2:GetOSDOptionsResponse>"
	    "<tr2:OSDOptions>");

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tt:MaximumNumberOfOSDs Total=\"%d\"",
		p_res->MaximumNumberOfOSDs.Total);

	if (p_res->MaximumNumberOfOSDs.ImageFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    " Image=\"%d\"", 
			p_res->MaximumNumberOfOSDs.Image);
	}
	
	if (p_res->MaximumNumberOfOSDs.PlainTextFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    " PlainText=\"%d\"", 
			p_res->MaximumNumberOfOSDs.PlainText);
	}
	
	if (p_res->MaximumNumberOfOSDs.DateFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    " Date=\"%d\"", 
			p_res->MaximumNumberOfOSDs.Date);
	}
	
	if (p_res->MaximumNumberOfOSDs.TimeFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    " Time=\"%d\"", 
			p_res->MaximumNumberOfOSDs.Time);
	}
	
	if (p_res->MaximumNumberOfOSDs.DateAndTimeFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    " DateAndTime=\"%d\"", 
			p_res->MaximumNumberOfOSDs.DateAndTime);
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "></tt:MaximumNumberOfOSDs>");

	if (p_res->OSDType_Text)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:Type>%s</tt:Type>", 
			onvif_OSDTypeToString(OSDType_Text));
	}
	
	if (p_res->OSDType_Image)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:Type>%s</tt:Type>", 
			onvif_OSDTypeToString(OSDType_Image));
	}
	
	if (p_res->OSDType_Extended)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:Type>%s</tt:Type>", 
			onvif_OSDTypeToString(OSDType_Extended));
	}

	if (p_res->OSDPosType_LowerLeft)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:PositionOption>%s</tt:PositionOption>", 
			onvif_OSDPosTypeToString(OSDPosType_LowerLeft));
	}
	
	if (p_res->OSDPosType_LowerRight)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:PositionOption>%s</tt:PositionOption>", 
			onvif_OSDPosTypeToString(OSDPosType_LowerRight));
	}
	
	if (p_res->OSDPosType_UpperLeft)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:PositionOption>%s</tt:PositionOption>", 
			onvif_OSDPosTypeToString(OSDPosType_UpperLeft));
	}
	
	if (p_res->OSDPosType_UpperRight)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:PositionOption>%s</tt:PositionOption>", 
			onvif_OSDPosTypeToString(OSDPosType_UpperRight));
	}
	
	if (p_res->OSDPosType_Custom)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		    "<tt:PositionOption>%s</tt:PositionOption>", 
			onvif_OSDPosTypeToString(OSDPosType_Custom));
	}

	if (p_res->TextOptionFlag)
	{
	    offset += build_OSDTextOptions_xml(p_buf+offset, mlen-offset, &p_res->TextOption);		
	}

	if (p_res->ImageOptionFlag)
	{
	    offset += build_OSDImgOptions_xml(p_buf+offset, mlen-offset, &p_res->ImageOption);
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</tr2:OSDOptions>"
	    "</tr2:GetOSDOptionsResponse>");

	return offset;
}

int build_tr2_GetOSDs_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	BOOL loopflag = 0;
	int offset = 0;
	tr2_GetOSDs_REQ * p_res = (tr2_GetOSDs_REQ *)argv;
	OSDConfigurationList * p_osd = NULL;

	if (p_res->OSDTokenFlag)
	{
		p_osd = onvif_find_OSDConfiguration(g_onvif_cfg.OSDs, p_res->OSDToken);
		if (NULL == p_osd)
		{
			return ONVIF_ERR_NoConfig;
		}
	}

	if (p_res->ConfigurationTokenFlag)
	{
		VideoSourceConfigurationList * p_v_src_cfg = onvif_find_VideoSourceConfiguration(g_onvif_cfg.v_src_cfg, p_res->ConfigurationToken);
		if (NULL == p_v_src_cfg)
		{
		}
	}

	if (NULL == p_osd)
	{
		loopflag = TRUE;
		p_osd = g_onvif_cfg.OSDs;
	}	

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:GetOSDsResponse>");

	while (p_osd)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tr2:OSDs token=\"%s\">", p_osd->OSD.token);
		offset += build_OSDConfiguration_xml(p_buf+offset, mlen-offset, &p_osd->OSD);
		offset += snprintf(p_buf+offset, mlen-offset, "</tr2:OSDs>");

		if (loopflag)
		{
			p_osd = p_osd->next;
		}
		else
		{
			break;
		}
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tr2:GetOSDsResponse>");

	return offset;
}

int build_tr2_CreateOSD_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tr2:CreateOSDResponse>"
			"<tr2:OSDToken>%s</tr2:OSDToken>"
		"</tr2:CreateOSDResponse>", 
		argv);

	return offset;
}

int build_tr2_DeleteOSD_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:DeleteOSDResponse />");

	return offset;
}

int build_tr2_CreateMask_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tr2:CreateMaskResponse>"
			"<tr2:Token>%s</tr2:Token>"
		"</tr2:CreateMaskResponse>", 
		argv);

	return offset;
}

int build_tr2_DeleteMask_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:DeleteMaskResponse />");
	return offset;
}

int build_tr2_GetMasks_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	BOOL loopflag = 0;
	int offset = 0;
	tr2_GetMasks_REQ * p_res = (tr2_GetMasks_REQ *)argv;
	MaskList * p_mask = NULL;
	
	onvif_renew_Masks();
	
	if (p_res->TokenFlag)
	{
		p_mask = onvif_find_Mask(g_onvif_cfg.mask, p_res->Token);
		if (NULL == p_mask)
		{
			return ONVIF_ERR_NoConfig;
		}
	}

	if (p_res->ConfigurationTokenFlag)
	{
		VideoSourceConfigurationList * p_v_src_cfg = onvif_find_VideoSourceConfiguration(g_onvif_cfg.v_src_cfg, p_res->ConfigurationToken);
		if (NULL == p_v_src_cfg)
		{
		}
	}

	if (NULL == p_mask)
	{
		loopflag = TRUE;
		p_mask = g_onvif_cfg.mask;
	}	

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:GetMasksResponse>");

	while (p_mask)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Masks token=\"%s\">", p_mask->Mask.token);
		offset += build_Mask_xml(p_buf+offset, mlen-offset, &p_mask->Mask);
		offset += snprintf(p_buf+offset, mlen-offset, "</tr2:Masks>");

		if (loopflag)
		{
			p_mask = p_mask->next;
		}
		else
		{
			break;
		}
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tr2:GetMasksResponse>");

	return offset;
}

int build_tr2_SetMask_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:SetMaskResponse />");
	return offset;
}

int build_tr2_GetMaskOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	uint32 i;
	int offset = 0;
	onvif_MaskOptions * p_res = &g_onvif_cfg.MaskOptions;

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:GetMaskOptionsResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Options RectangleOnly=\"%s\" SingleColorOnly=\"%s\">", p_res->RectangleOnly ? "true" : "false", p_res->SingleColorOnly ? "true" : "false");

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:MaxMasks>%d</tr2:MaxMasks>", p_res->MaxMasks);

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:MaxPoints>%d</tr2:MaxPoints>", p_res->MaxPoints);

	for (i = 0; i < p_res->sizeTypes; i++)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Types>%s</tr2:Types>", p_res->Types[i]);
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Color>");
	offset += build_ColorOptions_xml(p_buf+offset, mlen-offset, &p_res->Color);
	offset += snprintf(p_buf+offset, mlen-offset, "</tr2:Color>");

	offset += snprintf(p_buf+offset, mlen-offset, "</tr2:Options>");

	offset += snprintf(p_buf+offset, mlen-offset, "</tr2:GetMaskOptionsResponse>");

	return offset;        
}

int build_tr2_StartMulticastStreaming_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:StartMulticastStreamingResponse />");
	return offset;
}

int build_tr2_StopMulticastStreaming_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:StopMulticastStreamingResponse />");
	return offset;
}

#ifdef AUDIO_SUPPORT

int build_tr2_GetAudioEncoderConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	BOOL loopflag = 0;
	int offset = 0;
	
	onvif_renew_AudioEncoderConfiguration();
	tr2_GetAudioEncoderConfigurations_REQ * p_res = (tr2_GetAudioEncoderConfigurations_REQ *)argv;
	AudioEncoder2ConfigurationList * p_a_enc_cfg = NULL;

	if (p_res->GetConfiguration.ConfigurationTokenFlag)
	{
		p_a_enc_cfg = onvif_find_AudioEncoder2Configuration(g_onvif_cfg.a_enc_cfg, p_res->GetConfiguration.ConfigurationToken);
		if (NULL == p_a_enc_cfg)
		{
			return ONVIF_ERR_NoConfig;
		}
	}

	if (p_res->GetConfiguration.ProfileTokenFlag)
	{
		ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->GetConfiguration.ProfileToken);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NoProfile;
		}
	}

	if (NULL == p_a_enc_cfg)
	{
		loopflag = TRUE;
		p_a_enc_cfg = g_onvif_cfg.a_enc_cfg;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:GetAudioEncoderConfigurationsResponse>");

	while (p_a_enc_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Configurations token=\"%s\">", p_a_enc_cfg->Configuration.token);
		offset += build_AudioEncoder2Configuration_xml(p_buf+offset, mlen-offset, &p_a_enc_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tr2:Configurations>");

		if (loopflag)
		{
			p_a_enc_cfg = p_a_enc_cfg->next;
		}
		else
		{
			break;
		}
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tr2:GetAudioEncoderConfigurationsResponse>");

	return offset;
}

int build_tr2_GetAudioSourceConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	BOOL loopflag = 0;
	int offset = 0;
	tr2_GetAudioSourceConfigurations_REQ * p_res = (tr2_GetAudioSourceConfigurations_REQ *)argv;
	AudioSourceConfigurationList * p_a_src_cfg = NULL;

	if (p_res->GetConfiguration.ConfigurationTokenFlag)
	{
		p_a_src_cfg = onvif_find_AudioSourceConfiguration(g_onvif_cfg.a_src_cfg, p_res->GetConfiguration.ConfigurationToken);
		if (NULL == p_a_src_cfg)
		{
			return ONVIF_ERR_NoConfig;
		}
	}

	if (p_res->GetConfiguration.ProfileTokenFlag)
	{
		ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->GetConfiguration.ProfileToken);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NoProfile;
		}
	}

	if (NULL == p_a_src_cfg)
	{
		loopflag = TRUE;
		p_a_src_cfg = g_onvif_cfg.a_src_cfg;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:GetAudioSourceConfigurationsResponse>");

	while (p_a_src_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Configurations token=\"%s\">", p_a_src_cfg->Configuration.token);
		offset += build_AudioSourceConfiguration_xml(p_buf+offset, mlen-offset, &p_a_src_cfg->Configuration);
		offset += snprintf(p_buf+offset, mlen-offset, "</tr2:Configurations>");

		if (loopflag)
		{
			p_a_src_cfg = p_a_src_cfg->next;
		}
		else
		{
			break;
		}
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tr2:GetAudioSourceConfigurationsResponse>");

	return offset;
}

int build_tr2_GetAudioSourceConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	AudioSourceConfigurationList * p_a_src_cfg = NULL;
	
	tr2_GetAudioSourceConfigurationOptions_REQ * p_res = (tr2_GetAudioSourceConfigurationOptions_REQ *)argv;
	if (p_res->GetConfiguration.ProfileTokenFlag && p_res->GetConfiguration.ProfileToken[0] != '\0')
	{
		ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->GetConfiguration.ProfileToken);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NoProfile;
		}

		p_a_src_cfg = p_profile->a_src_cfg;
	}

	if (p_res->GetConfiguration.ConfigurationTokenFlag && p_res->GetConfiguration.ConfigurationToken[0] != '\0')
	{
		p_a_src_cfg = onvif_find_AudioSourceConfiguration(g_onvif_cfg.a_src_cfg, p_res->GetConfiguration.ConfigurationToken);
		if (NULL == p_a_src_cfg)
		{
			return ONVIF_ERR_NoConfig;
		}
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tr2:GetAudioSourceConfigurationOptionsResponse>"
		"<tr2:Options>");

	offset += build_AudioSourceConfigurationOptions_xml(p_buf+offset, mlen-offset);

	offset += snprintf(p_buf+offset, mlen-offset, 
		"</tr2:Options>"
		"</tr2:GetAudioSourceConfigurationOptionsResponse>");
	
	return offset;
}

int build_tr2_GetAudioEncoderConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tr2_GetAudioEncoderConfigurationOptions_REQ * p_res = (tr2_GetAudioEncoderConfigurationOptions_REQ *)argv;
	AudioEncoder2ConfigurationList * p_a_enc_cfg = NULL;
	AudioEncoder2ConfigurationOptionsList * p_option;

	if (p_res->GetConfiguration.ConfigurationTokenFlag)
	{
		p_a_enc_cfg = onvif_find_AudioEncoder2Configuration(g_onvif_cfg.a_enc_cfg, p_res->GetConfiguration.ConfigurationToken);
		if (NULL == p_a_enc_cfg)
		{
			return ONVIF_ERR_NoConfig;
		}
	}

	if (p_res->GetConfiguration.ProfileTokenFlag)
	{
		ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->GetConfiguration.ProfileToken);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NoProfile;
		}

		p_a_enc_cfg = p_profile->a_enc_cfg;
	}

	if (NULL == p_a_enc_cfg)
	{
		p_a_enc_cfg = g_onvif_cfg.a_enc_cfg;
	}

	if (NULL == p_a_enc_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	p_option = p_a_enc_cfg->Options;

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:GetAudioEncoderConfigurationOptionsResponse>");

	while (p_option)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Options>");
		offset += build_AudioEncoder2ConfigurationOptions_xml(p_buf+offset, mlen-offset, &p_option->Options);
		offset += snprintf(p_buf+offset, mlen-offset, "</tr2:Options>");

		p_option = p_option->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tr2:GetAudioEncoderConfigurationOptionsResponse>");

	return offset;
}

int build_tr2_SetAudioEncoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:SetAudioEncoderConfigurationResponse />");		    
	return offset;
}

int build_tr2_GetAudioDecoderConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	BOOL loopflag = 0;
	int offset = 0;
	tr2_GetAudioDecoderConfigurations_REQ * p_res = (tr2_GetAudioDecoderConfigurations_REQ *)argv;
	AudioDecoderConfigurationList * p_a_dec_cfg = NULL;

	if (p_res->GetConfiguration.ConfigurationTokenFlag)
	{
		p_a_dec_cfg = onvif_find_AudioDecoderConfiguration(g_onvif_cfg.a_dec_cfg, p_res->GetConfiguration.ConfigurationToken);
		if (NULL == p_a_dec_cfg)
		{
			return ONVIF_ERR_NoConfig;
		}
	}

	if (p_res->GetConfiguration.ProfileTokenFlag)
	{
		ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->GetConfiguration.ProfileToken);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NoProfile;
		}
	}

	if (NULL == p_a_dec_cfg)
	{
		loopflag = TRUE;
		p_a_dec_cfg = g_onvif_cfg.a_dec_cfg;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:GetAudioDecoderConfigurationsResponse>");

	while (p_a_dec_cfg)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Configurations token=\"%s\">", p_a_dec_cfg->Configuration.token);

		offset += build_AudioDecoderConfiguration_xml(p_buf+offset, mlen-offset, &p_a_dec_cfg->Configuration);

		offset += snprintf(p_buf+offset, mlen-offset, "</tr2:Configurations>");

		if (loopflag)
		{
			p_a_dec_cfg = p_a_dec_cfg->next;
		}
		else
		{
			break;
		}
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tr2:GetAudioDecoderConfigurationsResponse>");

	return offset;
}

int build_tr2_SetAudioDecoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:SetAudioDecoderConfigurationResponse />");
	return offset;
}

int build_tr2_GetAudioDecoderConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tr2_GetAudioDecoderConfigurationOptions_REQ * p_res = (tr2_GetAudioDecoderConfigurationOptions_REQ *)argv;
	AudioDecoderConfigurationList * p_a_dec_cfg = NULL;
	AudioEncoder2ConfigurationOptionsList * p_option;
	ONVIF_PROFILE * p_profile = NULL;

	if (p_res->GetConfiguration.ProfileTokenFlag)
	{
		p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->GetConfiguration.ProfileToken);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NoProfile;
		}
	}

	if (p_res->GetConfiguration.ConfigurationTokenFlag)
	{
		p_a_dec_cfg = onvif_find_AudioDecoderConfiguration(g_onvif_cfg.a_dec_cfg, p_res->GetConfiguration.ConfigurationToken);
		if (NULL == p_a_dec_cfg)
		{
			return ONVIF_ERR_NoConfig;
		}
	}
	else if (p_profile && p_profile->a_dec_cfg)
	{
		p_a_dec_cfg = p_profile->a_dec_cfg;
	}
	else if (g_onvif_cfg.a_dec_cfg)
	{
		p_a_dec_cfg = g_onvif_cfg.a_dec_cfg;
	}

	if (NULL == p_a_dec_cfg)
	{
		return ONVIF_ERR_InvalidArgVal;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:GetAudioDecoderConfigurationOptionsResponse>");

	while (p_a_dec_cfg)
	{
		p_option = p_a_dec_cfg->Options2;
		log_print(HT_LOG_INFO, "p_option->Options.Encoding:%s\n", p_option->Options.Encoding);
		offset += snprintf(p_buf+offset, mlen-offset, "<tr2:Options>");
		offset += build_AudioEncoder2ConfigurationOptions_xml(p_buf+offset, mlen-offset, &p_option->Options);
		offset += snprintf(p_buf+offset, mlen-offset, "</tr2:Options>");

		p_a_dec_cfg = p_a_dec_cfg->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tr2:GetAudioDecoderConfigurationOptionsResponse>");

	return offset;
}

#endif // end of AUDIO_SUPPORT

#ifdef DEVICEIO_SUPPORT

int build_tr2_GetAudioOutputConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    AudioOutputConfigurationList * p_cfg = NULL;
    trt_GetAudioOutputConfigurationOptions_REQ * p_res = (trt_GetAudioOutputConfigurationOptions_REQ *)argv; 
    if (p_res->ConfigurationTokenFlag)
    {
        p_cfg = onvif_find_AudioOutputConfiguration(g_onvif_cfg.a_output_cfg, p_res->ConfigurationToken);
        if (NULL == p_cfg)
        {
            return ONVIF_ERR_NoConfig;
        }
    }

    if (p_res->ProfileTokenFlag)
    {
        ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->ProfileToken);
        if (NULL == p_profile)
        {
            return ONVIF_ERR_NoProfile;
        }
    }

    if (NULL == p_cfg)
    {
        p_cfg = g_onvif_cfg.a_output_cfg;
    }

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tr2:GetAudioOutputConfigurationOptionsResponse>"
	    "<tr2:Options>");

    if (p_cfg)
    {
        offset += build_AudioOutputConfigurationOptions_xml(p_buf+offset, mlen-offset, &p_cfg->Options);       
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</tr2:Options>"
	    "</tr2:GetAudioOutputConfigurationOptionsResponse>");

	return offset;
}

int build_tr2_GetAudioOutputConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    BOOL loopflag = 0;
    int offset = 0;
    tr2_GetAudioOutputConfigurations_REQ * p_res = (tr2_GetAudioOutputConfigurations_REQ *)argv;
    AudioOutputConfigurationList * p_a_output_cfg = NULL;

    if (p_res->GetConfiguration.ConfigurationTokenFlag)
    {
        p_a_output_cfg = onvif_find_AudioOutputConfiguration(g_onvif_cfg.a_output_cfg, p_res->GetConfiguration.ConfigurationToken);
        if (NULL == p_a_output_cfg)
        {
            return ONVIF_ERR_NoConfig;
        }
    }

    if (p_res->GetConfiguration.ProfileTokenFlag)
    {
        ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->GetConfiguration.ProfileToken);
        if (NULL == p_profile)
        {
            return ONVIF_ERR_NoProfile;
        }
    }

    if (NULL == p_a_output_cfg)
    {
        loopflag = TRUE;
        p_a_output_cfg = g_onvif_cfg.a_output_cfg;
    }

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tr2:GetAudioOutputConfigurationsResponse>");

    while (p_a_output_cfg)
	{
	    offset += snprintf(p_buf+offset, mlen-offset, 
            "<tr2:Configurations token=\"%s\">", 
            p_a_output_cfg->Configuration.token);
        
        offset += build_AudioOutputConfiguration_xml(p_buf+offset, mlen-offset, &p_a_output_cfg->Configuration);
        
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tr2:Configurations>");            

    	if (loopflag)
    	{
    	    p_a_output_cfg = p_a_output_cfg->next;
    	}
    	else
    	{
    	    break;
    	}
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</tr2:GetAudioOutputConfigurationsResponse>");
    
    return offset;
}

int build_tr2_SetAudioOutputConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tr2:SetAudioOutputConfigurationResponse />");
	return offset;
}

#endif // end of DEVICEIO_SUPPORT

#ifdef VIDEO_ANALYTICS

int build_tr2_GetAnalyticsConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    BOOL loopflag = 0;
    int offset = 0;
    tr2_GetAnalyticsConfigurations_REQ * p_res = (tr2_GetAnalyticsConfigurations_REQ *)argv;
    VideoAnalyticsConfigurationList * p_va_cfg = NULL;

    if (p_res->GetConfiguration.ConfigurationTokenFlag)
    {
        p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_res->GetConfiguration.ConfigurationToken);
        if (NULL == p_va_cfg)
        {
            return ONVIF_ERR_NoConfig;
        }
    }

    if (p_res->GetConfiguration.ProfileTokenFlag)
    {
        ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_res->GetConfiguration.ProfileToken);
        if (NULL == p_profile)
        {
            return ONVIF_ERR_NoProfile;
        }
    }

    if (NULL == p_va_cfg)
    {
        loopflag = TRUE;
        p_va_cfg = g_onvif_cfg.va_cfg;
    }

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tr2:GetAnalyticsConfigurationsResponse>");

    while (p_va_cfg)
	{
	    offset += snprintf(p_buf+offset, mlen-offset, 
	        "<tr2:Configurations token=\"%s\">", 
	        p_va_cfg->Configuration.token);
    	offset += build_VideoAnalyticsConfiguration_xml(p_buf+offset, mlen-offset, &p_va_cfg->Configuration);
    	offset += snprintf(p_buf+offset, mlen-offset, 
    	    "</tr2:Configurations>");

    	if (loopflag)
    	{
    	    p_va_cfg = p_va_cfg->next;
    	}
    	else
    	{
    	    break;
    	}
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</tr2:GetAnalyticsConfigurationsResponse>");
    
    return offset;
}

#endif // end of VIDEO_ANALYTICS

#endif // end of MEDIA2_SUPPORT

#ifdef THERMAL_SUPPORT

int build_ColorPalette_xml(char * p_buf, int mlen, onvif_ColorPalette * p_res)
{
    int offset = 0;
    
    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tth:ColorPalette token=\"%s\" Type=\"%s\">\r\n"
        "<tth:Name>%s</tth:Name>\r\n"
        "</tth:ColorPalette>\r\n",
        p_res->token, 
        p_res->Type,
        p_res->Name);

    return offset;        
}

int build_NUCTable_xml(char * p_buf, int mlen, onvif_NUCTable * p_res)
{
    int offset = 0;
    
    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tth:NUCTable token=\"%s\"", 
        p_res->token);

    if (p_res->LowTemperatureFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            " LowTemperature=\"%f\"", 
            p_res->LowTemperature);
    }
    
    if (p_res->HighTemperatureFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
        " HighTemperature=\"%f\"", 
        p_res->HighTemperature);
    }
    
    offset += snprintf(p_buf+offset, mlen-offset, ">\r\n");

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tth:Name>%s</tth:Name>\r\n", 
        p_res->Name);

    offset += snprintf(p_buf+offset, mlen-offset, 
        "</tth:NUCTable>\r\n"); 

    return offset;        
}

int build_ThermalConfiguration_xml(char * p_buf, int mlen, onvif_ThermalConfiguration * p_res)
{
	int offset = 0;

	offset += build_ColorPalette_xml(p_buf+offset, mlen-offset, &p_res->ColorPalette);

	offset += snprintf(p_buf+offset, mlen-offset, "<tth:Polarity>%s</tth:Polarity>\r\n", onvif_PolarityToString(p_res->Polarity));

	if (p_res->NUCTableFlag)
	{
		offset += build_NUCTable_xml(p_buf+offset, mlen-offset, &p_res->NUCTable);     
	}

	if (p_res->CoolerFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tth:Cooler>\r\n");
		offset += snprintf(p_buf+offset, mlen-offset, "<tth:Enabled>%s</tth:Enabled>\r\n", p_res->Cooler.Enabled ? "true" : "false");

		if (p_res->Cooler.RunTimeFlag)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tth:RunTime>%f</tth:RunTime>\r\n", p_res->Cooler.RunTime);
		}

		offset += snprintf(p_buf+offset, mlen-offset, "</tth:Cooler>\r\n");
	}

	return offset;
}

int build_RadiometryGlobalParameters_xml(char * p_buf, int mlen, onvif_RadiometryGlobalParameters * p_res)
{
    int offset = 0;
    
    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tth:ReflectedAmbientTemperature>%f</tth:ReflectedAmbientTemperature>\r\n"
        "<tth:Emissivity>%f</tth:Emissivity>\r\n"
        "<tth:DistanceToObject>%f</tth:DistanceToObject>\r\n",
        p_res->ReflectedAmbientTemperature, 
        p_res->Emissivity,
        p_res->DistanceToObject);

    if (p_res->RelativeHumidityFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tth:RelativeHumidity>%f</tth:RelativeHumidity>\r\n",
            p_res->RelativeHumidity);
    }

    if (p_res->AtmosphericTemperatureFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tth:AtmosphericTemperature>%f</tth:AtmosphericTemperature>\r\n",
            p_res->AtmosphericTemperature);
    }

    if (p_res->AtmosphericTransmittanceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tth:AtmosphericTransmittance>%f</tth:AtmosphericTransmittance>\r\n",
            p_res->AtmosphericTransmittance);
    }

    if (p_res->ExtOpticsTemperatureFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tth:ExtOpticsTemperature>%f</tth:ExtOpticsTemperature>\r\n",
            p_res->ExtOpticsTemperature);
    }

    if (p_res->ExtOpticsTransmittanceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tth:ExtOpticsTransmittance>%f</tth:ExtOpticsTransmittance>\r\n",
            p_res->ExtOpticsTransmittance);
    }
    
    return offset;
}

int build_RadiometryConfiguration_xml(char * p_buf, int mlen, onvif_RadiometryConfiguration * p_res)
{
    int offset = 0;

    if (p_res->RadiometryGlobalParametersFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, "<tth:RadiometryGlobalParameters>\r\n");
        offset += build_RadiometryGlobalParameters_xml(p_buf+offset, mlen-offset, &p_res->RadiometryGlobalParameters);
        offset += snprintf(p_buf+offset, mlen-offset, "</tth:RadiometryGlobalParameters>\r\n");
    }
    
    return offset;
}

int build_RadiometryGlobalParameterOptions_xml(char * p_buf, int mlen, onvif_RadiometryGlobalParameterOptions * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, "<tth:ReflectedAmbientTemperature>");
    offset += build_FloatRange_xml(p_buf+offset, mlen-offset, &p_res->ReflectedAmbientTemperature);
    offset += snprintf(p_buf+offset, mlen-offset, "</tth:ReflectedAmbientTemperature>");

    offset += snprintf(p_buf+offset, mlen-offset, "<tth:Emissivity>");
    offset += build_FloatRange_xml(p_buf+offset, mlen-offset, &p_res->Emissivity);
    offset += snprintf(p_buf+offset, mlen-offset, "</tth:Emissivity>");

    offset += snprintf(p_buf+offset, mlen-offset, "<tth:DistanceToObject>");
    offset += build_FloatRange_xml(p_buf+offset, mlen-offset, &p_res->DistanceToObject);
    offset += snprintf(p_buf+offset, mlen-offset, "</tth:DistanceToObject>");

    if (p_res->RelativeHumidityFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, "<tth:RelativeHumidity>");
        offset += build_FloatRange_xml(p_buf+offset, mlen-offset, &p_res->RelativeHumidity);
        offset += snprintf(p_buf+offset, mlen-offset, "</tth:RelativeHumidity>");
    }

    if (p_res->AtmosphericTemperatureFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, "<tth:AtmosphericTemperature>");
        offset += build_FloatRange_xml(p_buf+offset, mlen-offset, &p_res->AtmosphericTemperature);
        offset += snprintf(p_buf+offset, mlen-offset, "</tth:AtmosphericTemperature>");
    }

    if (p_res->AtmosphericTransmittanceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, "<tth:AtmosphericTransmittance>");
        offset += build_FloatRange_xml(p_buf+offset, mlen-offset, &p_res->AtmosphericTransmittance);
        offset += snprintf(p_buf+offset, mlen-offset, "</tth:AtmosphericTransmittance>");
    }

    if (p_res->ExtOpticsTemperatureFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, "<tth:ExtOpticsTemperature>");
        offset += build_FloatRange_xml(p_buf+offset, mlen-offset, &p_res->ExtOpticsTemperature);
        offset += snprintf(p_buf+offset, mlen-offset, "</tth:ExtOpticsTemperature>");
    }

    if (p_res->ExtOpticsTransmittanceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, "<tth:ExtOpticsTransmittance>");
        offset += build_FloatRange_xml(p_buf+offset, mlen-offset, &p_res->ExtOpticsTransmittance);
        offset += snprintf(p_buf+offset, mlen-offset, "</tth:ExtOpticsTransmittance>");
    }
    
    return offset;
}

int build_tth_GetConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	VideoSourceList * p_v_src = g_onvif_cfg.v_src;

	offset += snprintf(p_buf+offset, mlen-offset, "<tth:GetConfigurationsResponse>");

	while (p_v_src)
	{
		if (p_v_src->ThermalSupport)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tth:Configurations token=\"%s\">"
				"<tth:Configuration>", 
				p_v_src->VideoSource.token);
			offset += build_ThermalConfiguration_xml(p_buf+offset, mlen-offset, &p_v_src->ThermalConfiguration);
			offset += snprintf(p_buf+offset, mlen-offset, 
				"</tth:Configuration>"
				"</tth:Configurations>");
		}

		p_v_src = p_v_src->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tth:GetConfigurationsResponse>");

	return offset;
}

int build_tth_GetConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tth_GetConfiguration_REQ * p_res = (tth_GetConfiguration_REQ *)argv;
	VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_res->VideoSourceToken);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NoScope;
	}

	if (FALSE == p_v_src->ThermalSupport)
	{
		return ONVIF_ERR_NoThermalForSource;
	}

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tth:GetConfigurationResponse>"
		"<tth:Configuration>");
	offset += build_ThermalConfiguration_xml(p_buf+offset, mlen-offset, &p_v_src->ThermalConfiguration);
	offset += snprintf(p_buf+offset, mlen-offset, 
		"</tth:Configuration>"
		"</tth:GetConfigurationResponse>");

	return offset;
}

int build_tth_SetConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tth:SetConfigurationResponse />");
	return offset;
}

int build_tth_GetConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	ColorPaletteList * p_ColorPalette;
	NUCTableList * p_NUCTable;
	tth_GetConfigurationOptions_REQ * p_res = (tth_GetConfigurationOptions_REQ *) argv;
	VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_res->VideoSourceToken);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NoSource;
	}

	if (FALSE == p_v_src->ThermalSupport)
	{
		return ONVIF_ERR_NoThermalForSource;
	}

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tth:GetConfigurationOptionsResponse>"
		"<tth:ConfigurationOptions>");

	p_ColorPalette = p_v_src->ThermalConfigurationOptions.ColorPalette;
	while (p_ColorPalette)
	{
		offset += build_ColorPalette_xml(p_buf+offset, mlen-offset, &p_ColorPalette->ColorPalette);

		p_ColorPalette = p_ColorPalette->next;
	}

	p_NUCTable = p_v_src->ThermalConfigurationOptions.NUCTable;
	while (p_NUCTable)
	{
		offset += build_NUCTable_xml(p_buf+offset, mlen-offset, &p_NUCTable->NUCTable);

		p_NUCTable = p_NUCTable->next;
	}

	if (p_v_src->ThermalConfigurationOptions.CoolerOptionsFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tth:CoolerOptions>"
				"<tth:Enabled>%s</tth:Enabled>"
			"</tth:CoolerOptions>",
			p_v_src->ThermalConfigurationOptions.CoolerOptions.Enabled ? "true" : "false");
	}

	offset += snprintf(p_buf+offset, mlen-offset, 
		"</tth:ConfigurationOptions>"
		"</tth:GetConfigurationOptionsResponse>");

	return offset;
}

int build_tth_GetRadiometryConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tth_GetRadiometryConfiguration_REQ * p_res = (tth_GetRadiometryConfiguration_REQ *)argv;
	VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_res->VideoSourceToken);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NoSource;
	}

	if (FALSE == p_v_src->ThermalSupport)
	{
		return ONVIF_ERR_NoRadiometryForSource;
	}

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tth:GetRadiometryConfigurationResponse>"
		"<tth:Configuration>");
	offset += build_RadiometryConfiguration_xml(p_buf+offset, mlen-offset, &p_v_src->RadiometryConfiguration);
	offset += snprintf(p_buf+offset, mlen-offset, 
		"</tth:Configuration>"
		"</tth:GetRadiometryConfigurationResponse>");

	return offset;
}

int build_tth_SetRadiometryConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tth:SetRadiometryConfigurationResponse />");
	return offset;
}

int build_tth_GetRadiometryConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	tth_GetRadiometryConfigurationOptions_REQ * p_res = (tth_GetRadiometryConfigurationOptions_REQ *) argv;
	VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_res->VideoSourceToken);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NoSource;
	}

	if (FALSE == p_v_src->ThermalSupport)
	{
		return ONVIF_ERR_NoRadiometryForSource;
	}

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tth:GetRadiometryConfigurationOptionsResponse>"
		"<tth:ConfigurationOptions>");

	if (p_v_src->RadiometryConfigurationOptions.RadiometryGlobalParameterOptionsFlag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tth:RadiometryGlobalParameterOptions>");
		offset += build_RadiometryGlobalParameterOptions_xml(p_buf+offset, mlen-offset, 
			&p_v_src->RadiometryConfigurationOptions.RadiometryGlobalParameterOptions);
		offset += snprintf(p_buf+offset, mlen-offset, "</tth:RadiometryGlobalParameterOptions>");
	}

	offset += snprintf(p_buf+offset, mlen-offset, 
		"</tth:ConfigurationOptions>"
		"</tth:GetRadiometryConfigurationOptionsResponse>");

	return offset;
}

#endif // end of THERMAL_SUPPORT

#ifdef CREDENTIAL_SUPPORT

int build_CredentialInfo_xml(char * p_buf, int mlen, onvif_CredentialInfo * p_res)
{
    int offset = 0;

    if (p_res->DescriptionFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tcr:Description>%s</tcr:Description>", 
            p_res->Description);
    }

    offset += snprintf(p_buf+offset, mlen-offset, 
            "<tcr:CredentialHolderReference>%s</tcr:CredentialHolderReference>", 
            p_res->CredentialHolderReference);

    if (p_res->ValidFromFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tcr:ValidFrom>%s</tcr:ValidFrom>", 
            p_res->ValidFrom);
    }

    if (p_res->ValidToFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tcr:ValidTo>%s</tcr:ValidTo>", 
            p_res->ValidTo);
    }
    
    return offset;
}

int build_CredentialIdentifierType_xml(char * p_buf, int mlen, onvif_CredentialIdentifierType * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tcr:Name>%s</tcr:Name>\r\n"
        "<tcr:FormatType>%s</tcr:FormatType>\r\n",
        p_res->Name,
        p_res->FormatType);

    return offset;
}

int build_CredentialIdentifier_xml(char * p_buf, int mlen, onvif_CredentialIdentifier * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, "<tcr:Type>\r\n");
    offset += build_CredentialIdentifierType_xml(p_buf+offset, mlen-offset, &p_res->Type);   
    offset += snprintf(p_buf+offset, mlen-offset, "</tcr:Type>\r\n");

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tcr:ExemptedFromAuthentication>%s</tcr:ExemptedFromAuthentication>\r\n"
        "<tcr:Value>%s</tcr:Value>\r\n",
        p_res->ExemptedFromAuthentication ? "true" : "false",
        p_res->Value);

    return offset;
}

int build_CredentialIdentifierItem_xml(char * p_buf, int mlen, onvif_CredentialIdentifierItem * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, "<tcr:Type>\r\n");
    offset += build_CredentialIdentifierType_xml(p_buf+offset, mlen-offset, &p_res->Type);   
    offset += snprintf(p_buf+offset, mlen-offset, "</tcr:Type>\r\n");

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tcr:Value>%s</tcr:Value>\r\n", 
        p_res->Value);

    return offset;
}

int build_CredentialAccessProfile_xml(char * p_buf, int mlen, onvif_CredentialAccessProfile * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tcr:AccessProfileToken>%s</tcr:AccessProfileToken>\r\n",
        p_res->AccessProfileToken);

    if (p_res->ValidFromFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tcr:ValidFrom>%s</tcr:ValidFrom>\r\n", 
            p_res->ValidFrom);
    }

    if (p_res->ValidToFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tcr:ValidTo>%s</tcr:ValidTo>\r\n", 
            p_res->ValidTo);
    }
    
    return offset;        
}

int build_Credential_xml(char * p_buf, int mlen, onvif_Credential * p_res)
{
	uint32 i;
    int offset = 0;

    if (p_res->DescriptionFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tcr:Description>%s</tcr:Description>\r\n", 
            p_res->Description);
    }

    offset += snprintf(p_buf+offset, mlen-offset, 
            "<tcr:CredentialHolderReference>%s</tcr:CredentialHolderReference>\r\n", 
            p_res->CredentialHolderReference);

    if (p_res->ValidFromFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tcr:ValidFrom>%s</tcr:ValidFrom>\r\n", 
            p_res->ValidFrom);
    }

    if (p_res->ValidToFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tcr:ValidTo>%s</tcr:ValidTo>\r\n", 
            p_res->ValidTo);
    }

    for (i = 0; i < p_res->sizeCredentialIdentifier; i++)
    {
        offset += snprintf(p_buf+offset, mlen-offset, "<tcr:CredentialIdentifier>\r\n");
        offset += build_CredentialIdentifier_xml(p_buf+offset, mlen-offset, &p_res->CredentialIdentifier[i]);
        offset += snprintf(p_buf+offset, mlen-offset, "</tcr:CredentialIdentifier>\r\n");
    }

    for (i = 0; i < p_res->sizeCredentialAccessProfile; i++)
    {
        offset += snprintf(p_buf+offset, mlen-offset, "<tcr:CredentialAccessProfile>\r\n");
        offset += build_CredentialAccessProfile_xml(p_buf+offset, mlen-offset, &p_res->CredentialAccessProfile[i]);
        offset += snprintf(p_buf+offset, mlen-offset, "</tcr:CredentialAccessProfile>\r\n");
    }

    offset += snprintf(p_buf+offset, mlen-offset, 
            "<tcr:ExtendedGrantTime>%s</tcr:ExtendedGrantTime>\r\n",
            p_res->ExtendedGrantTime ? "true" : "false");
    
    for (i = 0; i < p_res->sizeAttribute; i++)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tcr:Attribute Name=\"%s\"",
            p_res->Attribute[i].Name);

        if (p_res->Attribute[i].ValueFlag)
        {
            offset += snprintf(p_buf+offset, mlen-offset, 
                " Value=\"%s\"",
                p_res->Attribute[i].Value);
        }

        offset += snprintf(p_buf+offset, mlen-offset, "/>\r\n");
    }
    
    return offset;
}

int build_CredentialIdentifierFormatTypeInfo_xml(char * p_buf, int mlen, onvif_CredentialIdentifierFormatTypeInfo * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tcr:FormatType>%s</tcr:FormatType>"
        "<tcr:Description>%s</tcr:Description>",
        p_res->FormatType,
        p_res->Description);

    return offset;
}

int build_CredentialState_xml(char * p_buf, int mlen, onvif_CredentialState * p_res)
{
    int offset = 0;
    
    offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tcr:Enabled>%s</tcr:Enabled>\r\n", 
	    p_res->Enabled ? "true" : "false");

	if (p_res->ReasonFlag)
	{
	    offset += snprintf(p_buf+offset, mlen-offset, 
	        "<tcr:Reason>%s</tcr:Reason>\r\n", 
	        p_res->Reason);
	}
	
	if (p_res->AntipassbackStateFlag)
	{
	    offset += snprintf(p_buf+offset, mlen-offset, 
	        "<tcr:AntipassbackState>\r\n"
	            "<tcr:AntipassbackViolated>%s</tcr:AntipassbackViolated>\r\n"
	        "</tcr:AntipassbackState>\r\n", 
	        p_res->AntipassbackState.AntipassbackViolated ? "true" : "false");
	}

	return offset;
}

int build_tcr_GetCredentialInfo_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i;
    int offset = 0;
    tcr_GetCredentialInfo_RES * p_res = (tcr_GetCredentialInfo_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:GetCredentialInfoResponse>");

	for (i = 0; i < p_res->sizeCredentialInfo; i++)
	{
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tcr:CredentialInfo token=\"%s\">", 
            p_res->CredentialInfo[i].token);
        offset += build_CredentialInfo_xml(p_buf+offset, mlen-offset, &p_res->CredentialInfo[i]);
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tcr:CredentialInfo>");
    }
    
    offset += snprintf(p_buf+offset, mlen-offset, "</tcr:GetCredentialInfoResponse>");
    
    return offset;
}

int build_tcr_GetCredentialInfoList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int i;
    int offset = 0;
    tcr_GetCredentialInfoList_RES * p_res = (tcr_GetCredentialInfoList_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:GetCredentialInfoListResponse>");

    if (p_res->NextStartReferenceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tcr:NextStartReference>%s</tcr:NextStartReference>",
            p_res->NextStartReference);
    }
    
	for (i = 0; i < p_res->sizeCredentialInfo; i++)
	{
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tcr:CredentialInfo token=\"%s\">", 
            p_res->CredentialInfo[i].token);
        offset += build_CredentialInfo_xml(p_buf+offset, mlen-offset, &p_res->CredentialInfo[i]);
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tcr:CredentialInfo>");
    }
    
    offset += snprintf(p_buf+offset, mlen-offset, "</tcr:GetCredentialInfoListResponse>");
    
    return offset;
}

int build_tcr_GetCredentials_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i;
    int offset = 0;
    tcr_GetCredentials_RES * p_res = (tcr_GetCredentials_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:GetCredentialsResponse>");
    
	for (i = 0; i < p_res->sizeCredential; i++)
	{
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tcr:Credential token=\"%s\">", 
            p_res->Credential[i].token);
        offset += build_Credential_xml(p_buf+offset, mlen-offset, &p_res->Credential[i]);
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tcr:Credential>");
    }
    
    offset += snprintf(p_buf+offset, mlen-offset, "</tcr:GetCredentialsResponse>");
    
    return offset;
}

int build_tcr_GetCredentialList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i;
    int offset = 0;
    tcr_GetCredentialList_RES * p_res = (tcr_GetCredentialList_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:GetCredentialListResponse>");

    if (p_res->NextStartReferenceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tcr:NextStartReference>%s</tcr:NextStartReference>",
            p_res->NextStartReference);
    }
    
	for (i = 0; i < p_res->sizeCredential; i++)
	{
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tcr:Credential token=\"%s\">", 
            p_res->Credential[i].token);
        offset += build_Credential_xml(p_buf+offset, mlen-offset, &p_res->Credential[i]);
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tcr:Credential>");
    }
    
    offset += snprintf(p_buf+offset, mlen-offset, "</tcr:GetCredentialListResponse>");
    
    return offset;
}

int build_tcr_CreateCredential_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tcr_CreateCredential_RES * p_res = (tcr_CreateCredential_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tcr:CreateCredentialResponse>"
	        "<tcr:Token>%s</tcr:Token>"
	    "</tcr:CreateCredentialResponse>",
	    p_res->Token);
	    
    return offset;
}

int build_tcr_ModifyCredential_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:ModifyCredentialResponse />");
    return offset;
}

int build_tcr_DeleteCredential_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:DeleteCredentialResponse />");
    return offset;
}

int build_tcr_GetCredentialState_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tcr_GetCredentialState_RES * p_res = (tcr_GetCredentialState_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:GetCredentialStateResponse>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:State>");
    offset += build_CredentialState_xml(p_buf+offset, mlen-offset, &p_res->State);	
	offset += snprintf(p_buf+offset, mlen-offset, "</tcr:State>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tcr:GetCredentialStateResponse>");
    
    return offset;
}

int build_tcr_EnableCredential_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:EnableCredentialResponse />");

    return offset;
}

int build_tcr_DisableCredential_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:DisableCredentialResponse />");
    return offset;
}

int build_tcr_SetCredential_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:SetCredentialResponse />");
    return offset;
}

int build_tcr_ResetAntipassbackViolation_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:ResetAntipassbackViolationResponse />");
    return offset;
}

int build_tcr_GetSupportedFormatTypes_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i;
    int offset = 0;
    tcr_GetSupportedFormatTypes_RES * p_res = (tcr_GetSupportedFormatTypes_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:GetSupportedFormatTypesResponse>");

	for (i = 0; i < p_res->sizeFormatTypeInfo; i++)
	{
	    offset += snprintf(p_buf+offset, mlen-offset, "<tcr:FormatTypeInfo>");
	    offset += build_CredentialIdentifierFormatTypeInfo_xml(p_buf+offset, mlen-offset, &p_res->FormatTypeInfo[i]);
	    offset += snprintf(p_buf+offset, mlen-offset, "</tcr:FormatTypeInfo>");
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tcr:GetSupportedFormatTypesResponse>");
	    
    return offset;
}

int build_tcr_GetCredentialIdentifiers_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i;
    int offset = 0;
    tcr_GetCredentialIdentifiers_RES * p_res = (tcr_GetCredentialIdentifiers_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:GetCredentialIdentifiersResponse>");

	for (i = 0; i < p_res->sizeCredentialIdentifier; i++)
	{
	    offset += snprintf(p_buf+offset, mlen-offset, "<tcr:CredentialIdentifier>");
	    offset += build_CredentialIdentifier_xml(p_buf+offset, mlen-offset, &p_res->CredentialIdentifier[i]);
	    offset += snprintf(p_buf+offset, mlen-offset, "</tcr:CredentialIdentifier>");
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tcr:GetCredentialIdentifiersResponse>");
    
    return offset;
}

int build_tcr_SetCredentialIdentifier_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:SetCredentialIdentifierResponse />");
    return offset;
}

int build_tcr_DeleteCredentialIdentifier_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:DeleteCredentialIdentifierResponse />");
    return offset;
}

int build_tcr_GetCredentialAccessProfiles_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i;
    int offset = 0;
    tcr_GetCredentialAccessProfiles_RES * p_res = (tcr_GetCredentialAccessProfiles_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:GetCredentialAccessProfilesResponse>");

	for (i = 0; i < p_res->sizeCredentialAccessProfile; i++)
	{
	    offset += snprintf(p_buf+offset, mlen-offset, "<tcr:CredentialAccessProfile>");
	    offset += build_CredentialAccessProfile_xml(p_buf+offset, mlen-offset, &p_res->CredentialAccessProfile[i]);
	    offset += snprintf(p_buf+offset, mlen-offset, "</tcr:CredentialAccessProfile>");
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tcr:GetCredentialAccessProfilesResponse>");
	    
    return offset;
}

int build_tcr_SetCredentialAccessProfiles_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:SetCredentialAccessProfilesResponse />");
    return offset;
}

int build_tcr_DeleteCredentialAccessProfiles_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:DeleteCredentialAccessProfilesResponse />");
    return offset;
}

int build_tcr_GetWhitelist_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tcr_GetWhitelist_RES * p_res = (tcr_GetWhitelist_RES *)argv;
    CredentialIdentifierItemList * p_item = p_res->Identifier;

	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:GetWhitelistResponse>");

    if (p_res->NextStartReferenceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tcr:NextStartReference>%s</tcr:NextStartReference>",
            p_res->NextStartReference);
    }
    
	while (p_item)
	{
	    offset += snprintf(p_buf+offset, mlen-offset, "<tcr:Identifier>");
	    offset += build_CredentialIdentifierItem_xml(p_buf+offset, mlen-offset, &p_item->Item);
	    offset += snprintf(p_buf+offset, mlen-offset, "</tcr:Identifier>");

	    p_item = p_item->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tcr:GetWhitelistResponse>");
	    
    return offset;
}

int build_tcr_AddToWhitelist_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:AddToWhitelistResponse />");
    return offset;
}

int build_tcr_RemoveFromWhitelist_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:RemoveFromWhitelistResponse />");
    return offset;
}

int build_tcr_DeleteWhitelist_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:DeleteWhitelistResponse />");
    return offset;
}

int build_tcr_GetBlacklist_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tcr_GetBlacklist_RES * p_res = (tcr_GetBlacklist_RES *)argv;
    CredentialIdentifierItemList * p_item = p_res->Identifier;

	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:GetBlacklistResponse>");

    if (p_res->NextStartReferenceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tcr:NextStartReference>%s</tcr:NextStartReference>",
            p_res->NextStartReference);
    }
    
	while (p_item)
	{
	    offset += snprintf(p_buf+offset, mlen-offset, "<tcr:Identifier>");
	    offset += build_CredentialIdentifierItem_xml(p_buf+offset, mlen-offset, &p_item->Item);
	    offset += snprintf(p_buf+offset, mlen-offset, "</tcr:Identifier>");

	    p_item = p_item->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tcr:GetBlacklistResponse>");
	    
    return offset;
}

int build_tcr_AddToBlacklist_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:AddToBlacklistResponse />");
    return offset;
}

int build_tcr_RemoveFromBlacklist_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:RemoveFromBlacklistResponse />");
    return offset;
}

int build_tcr_DeleteBlacklist_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tcr:DeleteBlacklistResponse />");
    return offset;
}

#endif // end of CREDENTIAL_SUPPORT

#ifdef ACCESS_RULES

int build_AccessProfileInfo_xml(char * p_buf, int mlen, onvif_AccessProfileInfo * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tar:Name>%s</tar:Name>", 
        p_res->Name);

    if (p_res->DescriptionFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tar:Description>%s</tar:Description>", 
            p_res->Description);
    }

    return offset;
}

int build_AccessPolicy_xml(char * p_buf, int mlen, onvif_AccessPolicy * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tar:ScheduleToken>%s</tar:ScheduleToken>\r\n"
        "<tar:Entity>%s</tar:Entity>\r\n", 
        p_res->ScheduleToken,
        p_res->Entity);

    if (p_res->EntityTypeFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tar:EntityType>%s</tar:EntityType>\r\n",
            p_res->EntityType);
    }

    return offset;
}

int build_AccessProfile_xml(char * p_buf, int mlen, onvif_AccessProfile * p_res)
{
	uint32 i;
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tar:Name>%s</tar:Name>\r\n", 
        p_res->Name);

    if (p_res->DescriptionFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tar:Description>%s</tar:Description>\r\n", 
            p_res->Description);
    }

    for (i = 0; i < p_res->sizeAccessPolicy; i++)
    {
        offset += snprintf(p_buf+offset, mlen-offset, "<tar:AccessPolicy>\r\n");
        offset += build_AccessPolicy_xml(p_buf+offset, mlen-offset, &p_res->AccessPolicy[i]);
        offset += snprintf(p_buf+offset, mlen-offset, "</tar:AccessPolicy>\r\n");
    }

    return offset;
}

int build_tar_GetAccessProfileInfo_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i;
    int offset = 0;
    tar_GetAccessProfileInfo_RES * p_res = (tar_GetAccessProfileInfo_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tar:GetAccessProfileInfoResponse>");

	for (i = 0; i < p_res->sizeAccessProfileInfo; i++)
	{
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tar:AccessProfileInfo token=\"%s\">", 
            p_res->AccessProfileInfo[i].token);
        offset += build_AccessProfileInfo_xml(p_buf+offset, mlen-offset, &p_res->AccessProfileInfo[i]);
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tar:AccessProfileInfo>");
    }
    
    offset += snprintf(p_buf+offset, mlen-offset, "</tar:GetAccessProfileInfoResponse>");
    
    return offset;
}

int build_tar_GetAccessProfileInfoList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i;
    int offset = 0;
    tar_GetAccessProfileInfoList_RES * p_res = (tar_GetAccessProfileInfoList_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tar:GetAccessProfileInfoListResponse>");

    if (p_res->NextStartReferenceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tar:NextStartReference>%s</tar:NextStartReference>",
            p_res->NextStartReference);
    }
    
	for (i = 0; i < p_res->sizeAccessProfileInfo; i++)
	{
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tar:AccessProfileInfo token=\"%s\">", 
            p_res->AccessProfileInfo[i].token);
        offset += build_AccessProfileInfo_xml(p_buf+offset, mlen-offset, &p_res->AccessProfileInfo[i]);
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tar:AccessProfileInfo>");
    }
    
    offset += snprintf(p_buf+offset, mlen-offset, "</tar:GetAccessProfileInfoListResponse>");
    
    return offset;
}

int build_tar_GetAccessProfiles_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i;
    int offset = 0;
    tar_GetAccessProfiles_RES * p_res = (tar_GetAccessProfiles_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tar:GetAccessProfilesResponse>");
    
	for (i = 0; i < p_res->sizeAccessProfile; i++)
	{
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tar:AccessProfile token=\"%s\">", 
            p_res->AccessProfile[i].token);
        offset += build_AccessProfile_xml(p_buf+offset, mlen-offset, &p_res->AccessProfile[i]);
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tar:AccessProfile>");
    }
    
    offset += snprintf(p_buf+offset, mlen-offset, "</tar:GetAccessProfilesResponse>");
    
    return offset;
}

int build_tar_GetAccessProfileList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i;
    int offset = 0;
    tar_GetAccessProfileList_RES * p_res = (tar_GetAccessProfileList_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tar:GetAccessProfileListResponse>");

    if (p_res->NextStartReferenceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tar:NextStartReference>%s</tar:NextStartReference>",
            p_res->NextStartReference);
    }
    
	for (i = 0; i < p_res->sizeAccessProfile; i++)
	{
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tar:AccessProfile token=\"%s\">", 
            p_res->AccessProfile[i].token);
        offset += build_AccessProfile_xml(p_buf+offset, mlen-offset, &p_res->AccessProfile[i]);
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tar:AccessProfile>");
    }
    
    offset += snprintf(p_buf+offset, mlen-offset, "</tar:GetAccessProfileListResponse>");
    
    return offset;
}

int build_tar_CreateAccessProfile_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tar_CreateAccessProfile_RES * p_res = (tar_CreateAccessProfile_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tar:CreateAccessProfileResponse>"
	        "<tar:Token>%s</tar:Token>"
	    "</tar:CreateAccessProfileResponse>",
	    p_res->Token);
	    
    return offset;
}

int build_tar_ModifyAccessProfile_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tar:ModifyAccessProfileResponse />");
    return offset;
}

int build_tar_DeleteAccessProfile_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tar:DeleteAccessProfileResponse />");
    return offset;
}

int build_tar_SetAccessProfile_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tar:SetAccessProfileResponse />");
    return offset;
}

#endif // end of ACCESS_RULES

#ifdef SCHEDULE_SUPPORT

int build_ScheduleInfo_xml(char * p_buf, int mlen, onvif_ScheduleInfo * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tsc:Name>%s</tsc:Name>", 
        p_res->Name);

    if (p_res->DescriptionFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tsc:Description>%s</tsc:Description>", 
            p_res->Description);
    }

    return offset;
}

int build_TimePeriod_xml(char * p_buf, int mlen, onvif_TimePeriod * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tsc:From>%s</tsc:From>\r\n", 
        p_res->From);

    if (p_res->UntilFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tsc:Until>%s</tsc:Until>\r\n", 
            p_res->Until);
    }
    
    return offset;
}

int build_SpecialDaysSchedule_xml(char * p_buf, int mlen, onvif_SpecialDaysSchedule * p_res)
{
	uint32 i;
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tsc:GroupToken>%s</tsc:GroupToken>\r\n", 
        p_res->GroupToken);

    for (i = 0; i < p_res->sizeTimeRange; i++)
    {
        offset += snprintf(p_buf+offset, mlen-offset, "<tsc:TimeRange>\r\n");
        offset += build_TimePeriod_xml(p_buf+offset, mlen-offset, &p_res->TimeRange[i]);
        offset += snprintf(p_buf+offset, mlen-offset, "</tsc:TimeRange>\r\n");
    }
    
    return offset;
}

int build_Schedule_xml(char * p_buf, int mlen, onvif_Schedule * p_res)
{
	uint32 i;
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tsc:Name>%s</tsc:Name>\r\n", 
        p_res->Name);

    if (p_res->DescriptionFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tsc:Description>%s</tsc:Description>\r\n", 
            p_res->Description);
    }

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tsc:Standard>%s</tsc:Standard>\r\n", 
        p_res->Standard);

    for (i = 0; i < p_res->sizeSpecialDays; i++)
    {
        offset += snprintf(p_buf+offset, mlen-offset, "<tsc:SpecialDays>\r\n");
        offset += build_SpecialDaysSchedule_xml(p_buf+offset, mlen-offset, &p_res->SpecialDays[i]);
        offset += snprintf(p_buf+offset, mlen-offset, "</tsc:SpecialDays>\r\n");
    }
    
    return offset;
}

int build_SpecialDayGroupInfo_xml(char * p_buf, int mlen, onvif_SpecialDayGroupInfo * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tsc:Name>%s</tsc:Name>", 
        p_res->Name);

    if (p_res->DescriptionFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tsc:Description>%s</tsc:Description>", 
            p_res->Description);
    }

    return offset;
}

int build_SpecialDayGroup_xml(char * p_buf, int mlen, onvif_SpecialDayGroup * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tsc:Name>%s</tsc:Name>", 
        p_res->Name);

    if (p_res->DescriptionFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tsc:Description>%s</tsc:Description>", 
            p_res->Description);
    }

    if (p_res->DaysFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tsc:Days>%s</tsc:Days>", 
            p_res->Days);
    }

    return offset;
}

int build_tsc_GetScheduleInfo_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i;
    int offset = 0;
    tsc_GetScheduleInfo_RES * p_res = (tsc_GetScheduleInfo_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tsc:GetScheduleInfoResponse>");

	for (i = 0; i < p_res->sizeScheduleInfo; i++)
	{
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tsc:ScheduleInfo token=\"%s\">", 
            p_res->ScheduleInfo[i].token);
        offset += build_ScheduleInfo_xml(p_buf+offset, mlen-offset, &p_res->ScheduleInfo[i]);
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tsc:ScheduleInfo>");
    }
    
    offset += snprintf(p_buf+offset, mlen-offset, "</tsc:GetScheduleInfoResponse>");
    
    return offset;
}

int build_tsc_GetScheduleInfoList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i;
    int offset = 0;
    tsc_GetScheduleInfoList_RES * p_res = (tsc_GetScheduleInfoList_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tsc:GetScheduleInfoListResponse>");

    if (p_res->NextStartReferenceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tsc:NextStartReference>%s</tsc:NextStartReference>",
            p_res->NextStartReference);
    }
    
	for (i = 0; i < p_res->sizeScheduleInfo; i++)
	{
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tsc:ScheduleInfo token=\"%s\">", 
            p_res->ScheduleInfo[i].token);
        offset += build_ScheduleInfo_xml(p_buf+offset, mlen-offset, &p_res->ScheduleInfo[i]);
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tsc:ScheduleInfo>");
    }
    
    offset += snprintf(p_buf+offset, mlen-offset, "</tsc:GetScheduleInfoListResponse>");
    
    return offset;
}

int build_tsc_GetSchedules_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i;
    int offset = 0;
    tsc_GetSchedules_RES * p_res = (tsc_GetSchedules_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tsc:GetSchedulesResponse>");

	for (i = 0; i < p_res->sizeSchedule; i++)
	{
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tsc:Schedule token=\"%s\">", 
            p_res->Schedule[i].token);
        offset += build_Schedule_xml(p_buf+offset, mlen-offset, &p_res->Schedule[i]);
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tsc:Schedule>");
    }
    
    offset += snprintf(p_buf+offset, mlen-offset, "</tsc:GetSchedulesResponse>");
    
    return offset;
}

int build_tsc_GetScheduleList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i;
    int offset = 0;
    tsc_GetScheduleList_RES * p_res = (tsc_GetScheduleList_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tsc:GetScheduleListResponse>");

    if (p_res->NextStartReferenceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tsc:NextStartReference>%s</tsc:NextStartReference>",
            p_res->NextStartReference);
    }
    
	for (i = 0; i < p_res->sizeSchedule; i++)
	{
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tsc:Schedule token=\"%s\">", 
            p_res->Schedule[i].token);
        offset += build_Schedule_xml(p_buf+offset, mlen-offset, &p_res->Schedule[i]);
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tsc:Schedule>");
    }
    
    offset += snprintf(p_buf+offset, mlen-offset, "</tsc:GetScheduleListResponse>");
    
    return offset;
}

int build_tsc_CreateSchedule_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tsc_CreateSchedule_RES * p_res = (tsc_CreateSchedule_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tsc:CreateScheduleResponse>"
	        "<tsc:Token>%s</tsc:Token>"
	    "</tsc:CreateScheduleResponse>",
	    p_res->Token);
    
    return offset;
}

int build_tsc_ModifySchedule_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tsc:ModifyScheduleResponse />");
    return offset;
}

int build_tsc_DeleteSchedule_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tsc:DeleteScheduleResponse />");
    return offset;
}

int build_tsc_GetSpecialDayGroupInfo_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i;
    int offset = 0;
    tsc_GetSpecialDayGroupInfo_RES * p_res = (tsc_GetSpecialDayGroupInfo_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tsc:GetSpecialDayGroupInfoResponse>");

	for (i = 0; i < p_res->sizeSpecialDayGroupInfo; i++)
	{
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tsc:SpecialDayGroupInfo token=\"%s\">", 
            p_res->SpecialDayGroupInfo[i].token);
        offset += build_SpecialDayGroupInfo_xml(p_buf+offset, mlen-offset, &p_res->SpecialDayGroupInfo[i]);
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tsc:SpecialDayGroupInfo>");
    }
    
    offset += snprintf(p_buf+offset, mlen-offset, "</tsc:GetSpecialDayGroupInfoResponse>");
    
    return offset;
}

int build_tsc_GetSpecialDayGroupInfoList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i;
    int offset = 0;
    tsc_GetSpecialDayGroupInfoList_RES * p_res = (tsc_GetSpecialDayGroupInfoList_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tsc:GetSpecialDayGroupInfoListResponse>");

    if (p_res->NextStartReferenceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tsc:NextStartReference>%s</tsc:NextStartReference>",
            p_res->NextStartReference);
    }
    
	for (i = 0; i < p_res->sizeSpecialDayGroupInfo; i++)
	{
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tsc:SpecialDayGroupInfo token=\"%s\">", 
            p_res->SpecialDayGroupInfo[i].token);
        offset += build_SpecialDayGroupInfo_xml(p_buf+offset, mlen-offset, &p_res->SpecialDayGroupInfo[i]);
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tsc:SpecialDayGroupInfo>");
    }
    
    offset += snprintf(p_buf+offset, mlen-offset, "</tsc:GetSpecialDayGroupInfoListResponse>");
    
    return offset;
}

int build_tsc_GetSpecialDayGroups_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i;
    int offset = 0;
    tsc_GetSpecialDayGroups_RES * p_res = (tsc_GetSpecialDayGroups_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tsc:GetSpecialDayGroupsResponse>");

	for (i = 0; i < p_res->sizeSpecialDayGroup; i++)
	{
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tsc:SpecialDayGroup token=\"%s\">", 
            p_res->SpecialDayGroup[i].token);
        offset += build_SpecialDayGroup_xml(p_buf+offset, mlen-offset, &p_res->SpecialDayGroup[i]);
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tsc:SpecialDayGroup>");
    }
    
    offset += snprintf(p_buf+offset, mlen-offset, "</tsc:GetSpecialDayGroupsResponse>");
    
    return offset;
}

int build_tsc_GetSpecialDayGroupList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int i;
    int offset = 0;
    tsc_GetSpecialDayGroupList_RES * p_res = (tsc_GetSpecialDayGroupList_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, "<tsc:GetSpecialDayGroupListResponse>");

    if (p_res->NextStartReferenceFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tsc:NextStartReference>%s</tsc:NextStartReference>",
            p_res->NextStartReference);
    }
    
	for (i = 0; i < p_res->sizeSpecialDayGroup; i++)
	{
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tsc:SpecialDayGroup token=\"%s\">", 
            p_res->SpecialDayGroup[i].token);
        offset += build_SpecialDayGroup_xml(p_buf+offset, mlen-offset, &p_res->SpecialDayGroup[i]);
        offset += snprintf(p_buf+offset, mlen-offset, 
            "</tsc:SpecialDayGroup>");
    }
    
    offset += snprintf(p_buf+offset, mlen-offset, "</tsc:GetSpecialDayGroupListResponse>");
    
    return offset;
}

int build_tsc_CreateSpecialDayGroup_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tsc_CreateSpecialDayGroup_RES * p_res = (tsc_CreateSpecialDayGroup_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tsc:CreateSpecialDayGroupResponse>"
	        "<tsc:Token>%s</tsc:Token>"
	    "</tsc:CreateSpecialDayGroupResponse>",
	    p_res->Token);
    
    return offset;
}

int build_tsc_ModifySpecialDayGroup_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tsc:ModifySpecialDayGroupResponse />");
    return offset;
}

int build_tsc_DeleteSpecialDayGroup_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tsc:DeleteSpecialDayGroupResponse />");
    return offset;
}

int build_tsc_GetScheduleState_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tsc_GetScheduleState_RES * p_res = (tsc_GetScheduleState_RES *)argv;

	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tsc:GetScheduleStateResponse>"
	    "<tsc:ScheduleState>");

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tsc:Active>%s</tsc:Active>", 
        p_res->ScheduleState.Active ? "true" : "false");
    
    if (p_res->ScheduleState.SpecialDayFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tsc:SpecialDay>%s</tsc:SpecialDay>", 
            p_res->ScheduleState.SpecialDay ? "true" : "false");
    }
    
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "</tsc:ScheduleState>"
	    "</tsc:GetScheduleStateResponse>");
	    
    return offset;
}

int build_tsc_SetSchedule_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tsc:SetScheduleResponse />");
    return offset;
}

int build_tsc_SetSpecialDayGroup_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tsc:SetSpecialDayGroupResponse />");
    return offset;
}

#endif // end of SCHEDULE_SUPPORT

#ifdef RECEIVER_SUPPORT

int build_StreamSetup_xml(char * p_buf, int mlen, onvif_StreamSetup * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Stream>%s</tt:Stream>\r\n"
		"<tt:Transport>\r\n"
			"<tt:Protocol>%s</tt:Protocol>\r\n"
		"</tt:Transport>\r\n",
		onvif_StreamTypeToString(p_res->Stream),
		onvif_TransportProtocolToString(p_res->Transport.Protocol));
		
    return offset;
}

int build_ReceiverConfiguration_xml(char * p_buf, int mlen, onvif_ReceiverConfiguration * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tt:Mode>%s</tt:Mode>\r\n"
        "<tt:MediaUri>%s</tt:MediaUri>\r\n", 
        onvif_ReceiverModeToString(p_res->Mode), 
        p_res->MediaUri);

    offset += snprintf(p_buf+offset, mlen-offset, "<tt:StreamSetup>\r\n");
    offset += build_StreamSetup_xml(p_buf+offset, mlen-offset, &p_res->StreamSetup);
    offset += snprintf(p_buf+offset, mlen-offset, "</tt:StreamSetup>\r\n");
    
    return offset;        
}

int build_Receiver_xml(char * p_buf, int mlen, onvif_Receiver * p_res)
{
    int offset = 0;

    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tt:Token>%s</tt:Token>\r\n", 
        p_res->Token);

    offset += snprintf(p_buf+offset, mlen-offset, "<tt:Configuration>\r\n");
    offset += build_ReceiverConfiguration_xml(p_buf+offset, mlen-offset, &p_res->Configuration);
    offset += snprintf(p_buf+offset, mlen-offset, "</tt:Configuration>\r\n");

    return offset;
}

int build_trv_GetReceivers_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    trv_GetReceivers_RES * p_res = (trv_GetReceivers_RES *)argv;
    ReceiverList * p_Receiver;

    p_Receiver = p_res->Receivers;
    
	offset += snprintf(p_buf+offset, mlen-offset, "<trv:GetReceiversResponse>");

    while (p_Receiver)
    {
        offset += snprintf(p_buf+offset, mlen-offset, "<trv:Receivers>");
        offset += build_Receiver_xml(p_buf+offset, mlen-offset, &p_Receiver->Receiver);
        offset += snprintf(p_buf+offset, mlen-offset, "</trv:Receivers>");
        
        p_Receiver = p_Receiver->next;
    }
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trv:GetReceiversResponse>");
	    
    return offset;
}

int build_trv_GetReceiver_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    trv_GetReceiver_RES * p_res = (trv_GetReceiver_RES *)argv;

    offset += snprintf(p_buf+offset, mlen-offset, "<trv:GetReceiverResponse><trv:Receiver>");
    offset += build_Receiver_xml(p_buf+offset, mlen-offset, &p_res->Receiver);
    offset += snprintf(p_buf+offset, mlen-offset, "</trv:Receiver></trv:GetReceiverResponse>");

    return offset;
}

int build_trv_CreateReceiver_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    trv_CreateReceiver_RES * p_res = (trv_CreateReceiver_RES *)argv;

    offset += snprintf(p_buf+offset, mlen-offset, "<trv:CreateReceiverResponse><trv:Receiver>");
    offset += build_Receiver_xml(p_buf+offset, mlen-offset, &p_res->Receiver);
    offset += snprintf(p_buf+offset, mlen-offset, "</trv:Receiver></trv:CreateReceiverResponse>");

    return offset;
}

int build_trv_DeleteReceiver_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    offset += snprintf(p_buf+offset, mlen-offset, "<trv:DeleteReceiverResponse />");
    return offset;
}

int build_trv_ConfigureReceiver_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    offset += snprintf(p_buf+offset, mlen-offset, "<trv:ConfigureReceiverResponse />");
    return offset;
}

int build_trv_SetReceiverMode_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    offset += snprintf(p_buf+offset, mlen-offset, "<trv:SetReceiverModeResponse />");
    return offset;
}

int build_trv_GetReceiverState_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    trv_GetReceiverState_RES * p_res = (trv_GetReceiverState_RES *)argv;
    
    offset += snprintf(p_buf+offset, mlen-offset, 
        "<trv:GetReceiverStateResponse>"
            "<trv:ReceiverState>"
                "<tt:State>%s</tt:State>"
                "<tt:AutoCreated>%s</tt:AutoCreated>"
            "</trv:ReceiverState>"
        "</trv:GetReceiverStateResponse>",
        onvif_ReceiverStateToString(p_res->ReceiverState.State),
        p_res->ReceiverState.AutoCreated ? "true" : "false");

    return offset;                
}

#endif // end of RECEIVER_SUPPORT

#ifdef PROVISIONING_SUPPORT

int build_tpv_PanMove_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    offset += snprintf(p_buf+offset, mlen-offset, "<tpv:PanMoveResponse />");
    return offset;
}

int build_tpv_TiltMove_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    offset += snprintf(p_buf+offset, mlen-offset, "<tpv:TiltMoveResponse />");
    return offset;
}

int build_tpv_ZoomMove_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    offset += snprintf(p_buf+offset, mlen-offset, "<tpv:ZoomMoveResponse />");
    return offset;
}

int build_tpv_RollMove_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    offset += snprintf(p_buf+offset, mlen-offset, "<tpv:RollMoveResponse />");
    return offset;
}

int build_tpv_FocusMove_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    offset += snprintf(p_buf+offset, mlen-offset, "<tpv:FocusMoveResponse />");
    return offset;
}

int build_tpv_Stop_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    offset += snprintf(p_buf+offset, mlen-offset, "<tpv:StopResponse />");
    return offset;
}

int build_tpv_GetUsage_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
    int offset = 0;
    tpv_GetUsage_RES * p_res = (tpv_GetUsage_RES *)argv;
    
    offset += snprintf(p_buf+offset, mlen-offset, 
        "<tpv:GetUsageResponse>"
        "<tpv:Usage>");

    if (p_res->Usage.PanFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tpv:Pan>%d</tpv:Pan>", 
            p_res->Usage.Pan);
    }

    if (p_res->Usage.TiltFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tpv:Tilt>%d</tpv:Tilt>", 
            p_res->Usage.Tilt);
    }

    if (p_res->Usage.ZoomFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tpv:Zoom>%d</tpv:Zoom>", 
            p_res->Usage.Zoom);
    }

    if (p_res->Usage.RollFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tpv:Roll>%d</tpv:Roll>", 
            p_res->Usage.Roll);
    }

    if (p_res->Usage.FocusFlag)
    {
        offset += snprintf(p_buf+offset, mlen-offset, 
            "<tpv:Focus>%d</tpv:Focus>", 
            p_res->Usage.Focus);
    }
    
    offset += snprintf(p_buf+offset, mlen-offset, 
        "</tpv:Usage>"
        "</tpv:GetUsageResponse>");
    
    return offset;
}

#endif // end of PROVISIONING_SUPPORT

#if 1

int build_tpl_GetPrivacyMasks_rly_xml_old(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	log_print(HT_LOG_INFO, "build_tpl_GetPrivacyMasks_rly_xml\n");
	int offset = 0;
	MaskList * p_mask = g_onvif_cfg.mask;

	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:GetPrivacyMasksResponse>");
	if (p_mask != NULL)
	{
		if (p_mask->Mask.Enabled == TRUE)
			offset += snprintf(p_buf+offset, mlen-offset, "<tpl:PrivacyMaskEnable>true</tpl:PrivacyMaskEnable>");
		else
			offset += snprintf(p_buf+offset, mlen-offset, "<tpl:PrivacyMaskEnable>flase</tpl:PrivacyMaskEnable>");
	}
	else
		offset += snprintf(p_buf+offset, mlen-offset, "<tpl:PrivacyMaskEnable>flase</tpl:PrivacyMaskEnable>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tpl:GetPrivacyMasksResponse>");

	return offset;
}

#endif

#if 1

int build_hbgk_ext_GetPrivacyMaskOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	onvif_MaskOptions * p_maskoptions = &g_onvif_cfg.MaskOptions;

	offset += snprintf(p_buf+offset, mlen-offset, "<ewsd:GetPrivacyMaskOptionsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, "<ewsd:PrivacyMaskOptions>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "<exsd:MaximumNumberOfPrivacyMasks>%d</exsd:MaximumNumberOfPrivacyMasks>", p_maskoptions->MaxMasks);
	//offset += snprintf(p_buf+offset, mlen-offset, "<tplt:PolygonType>%s</tplt:PolygonType>", p_maskoptions->Types[0]);

	offset += snprintf(p_buf+offset, mlen-offset, "<exsd:Position>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "<exsd:XRange>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Min>%d</tt:Min>", -1);
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Max>%d</tt:Max>", 1);
	
	offset += snprintf(p_buf+offset, mlen-offset, "</exsd:XRange>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "<exsd:YRange>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Min>%d</tt:Min>", -1);
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Max>%d</tt:Max>", 1);
	
	offset += snprintf(p_buf+offset, mlen-offset, "</exsd:YRange>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "</exsd:Position>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "</ewsd:PrivacyMaskOptions>");
	offset += snprintf(p_buf+offset, mlen-offset, "</ewsd:GetPrivacyMaskOptionsResponse>");

	return offset;
}

int build_hbgk_ext_CreatePrivacyMask_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<ewsd:CreatePrivacyMaskResponse>"
			"<ewsd:PrivacyMaskToken>%s</ewsd:PrivacyMaskToken>"
		"</ewsd:CreatePrivacyMaskResponse>", 
		argv);

	return offset;
}

int build_hbgk_ext_DeletePrivacyMask_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<ewsd:DeletePrivacyMaskResponse />");
	return offset;
}

int build_hbgk_ext_PrivacyMask_xml(char * p_buf, int mlen, onvif_Mask * p_res)
{
	log_print(HT_LOG_INFO, "%s\r\n", __FUNCTION__);
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<exsd:VideoSourceToken>%s</exsd:VideoSourceToken>\r\n", p_res->ConfigurationToken);
	
	offset += snprintf(p_buf+offset, mlen-offset, "<exsd:MaskArea>");
	offset += build_Polygon_xml(p_buf+offset, mlen-offset, &p_res->Polygon);
	offset += snprintf(p_buf+offset, mlen-offset, "</exsd:MaskArea>");

	return offset;
}

int build_hbgk_ext_GetPrivacyMasks_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	log_print(HT_LOG_INFO, "%s\r\n", __FUNCTION__);
	BOOL loopflag = 0;
	int offset = 0;
	ewsd_GetMasks_REQ * p_res = (ewsd_GetMasks_REQ *)argv;
	MaskList * p_mask = NULL;
	
	onvif_renew_Masks();

	if (p_res->TokenFlag)
	{
		p_mask = onvif_find_Mask(g_onvif_cfg.mask, p_res->Token);
		if (NULL == p_mask)
		{
			return ONVIF_ERR_NoConfig;
		}
	}

	if (NULL == p_mask)
	{
		loopflag = TRUE;
		p_mask = g_onvif_cfg.mask;
	}	

	offset += snprintf(p_buf+offset, mlen-offset, "<ewsd:GetPrivacyMasksResponse>");
	
	if (g_mask_flag[0] == 0 && g_mask_flag[1] == 0 && g_mask_flag[2] == 0 && g_mask_flag[3] == 0)
		offset += snprintf(p_buf+offset, mlen-offset, "<ewsd:PrivacyMaskEnable>false</ewsd:PrivacyMaskEnable>\r\n");
	else
		offset += snprintf(p_buf+offset, mlen-offset, "<ewsd:PrivacyMaskEnable>true</ewsd:PrivacyMaskEnable>\r\n");
	
	while (p_mask)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<ewsd:PrivacyMask token=\"%s\">", p_mask->Mask.token);
		offset += build_hbgk_ext_PrivacyMask_xml(p_buf+offset, mlen-offset, &p_mask->Mask);
		offset += snprintf(p_buf+offset, mlen-offset, "</ewsd:PrivacyMask>");

		if (loopflag)
		{
			p_mask = p_mask->next;
		}
		else
		{
			break;
		}
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</ewsd:GetPrivacyMasksResponse>");

	return offset;
}

int build_hbgk_ext_SetPrivacyMask_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<ewsd:SetPrivacyMaskResponse />");
	return offset;
}

#endif

#if 1

int build_tpl_GetPrivacyMaskOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	onvif_MaskOptions * p_maskoptions = &g_onvif_cfg.MaskOptions;

	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:GetPrivacyMaskOptionsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:Options>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tplt:MaximumNumberOfPrivacyMasks>%d</tplt:MaximumNumberOfPrivacyMasks>", p_maskoptions->MaxMasks);
	offset += snprintf(p_buf+offset, mlen-offset, "<tplt:PolygonType>%s</tplt:PolygonType>", p_maskoptions->Types[0]);

	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:Position>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:XRange>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Min>%d</tt:Min>", -1);
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Max>%d</tt:Max>", 1);
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tpl:XRange>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:YRange>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Min>%d</tt:Min>", -1);
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Max>%d</tt:Max>", 1);
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tpl:YRange>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tpl:Position>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tpl:Options>");
	offset += snprintf(p_buf+offset, mlen-offset, "</tpl:GetPrivacyMaskOptionsResponse>");

	return offset;
}


int build_tpl_CreatePrivacyMask_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tpl:CreatePrivacyMaskResponse>"
			"<tpl:PrivacyMaskToken>%s</tpl:PrivacyMaskToken>"
		"</tpl:CreatePrivacyMaskResponse>", 
		argv);

	return offset;
}

int build_tpl_DeletePrivacyMask_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:DeletePrivacyMaskResponse />");
	return offset;
}

int build_tpl_PrivacyMask_xml(char * p_buf, int mlen, onvif_Mask * p_res)
{
	log_print(HT_LOG_INFO, "%s\r\n", __FUNCTION__);
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tplt:ConfigurationToken>%s</tplt:ConfigurationToken>\r\n", p_res->ConfigurationToken);
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tplt:Type>%s</tplt:Type>\r\n", p_res->Type);

	offset += snprintf(p_buf+offset, mlen-offset, "<tplt:MaskArea>");
	offset += build_Polygon_xml(p_buf+offset, mlen-offset, &p_res->Polygon);
	offset += snprintf(p_buf+offset, mlen-offset, "</tplt:MaskArea>");

	return offset;
}

int build_tpl_GetPrivacyMasks_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	log_print(HT_LOG_INFO, "%s\r\n", __FUNCTION__);
	BOOL loopflag = 0;
	int offset = 0;
	tr2_GetMasks_REQ * p_res = (tr2_GetMasks_REQ *)argv;
	MaskList * p_mask = NULL;
	
	onvif_renew_Masks();
	
	if (p_res->TokenFlag)
	{
		p_mask = onvif_find_Mask(g_onvif_cfg.mask, p_res->Token);
		if (NULL == p_mask)
		{
			return ONVIF_ERR_NoConfig;
		}
	}

	if (p_res->ConfigurationTokenFlag)
	{
		VideoSourceConfigurationList * p_v_src_cfg = onvif_find_VideoSourceConfiguration(g_onvif_cfg.v_src_cfg, p_res->ConfigurationToken);
		if (NULL == p_v_src_cfg)
		{
		}
	}

	if (NULL == p_mask)
	{
		loopflag = TRUE;
		p_mask = g_onvif_cfg.mask;
	}	

	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:GetPrivacyMasksResponse>");
	
	if (g_mask_flag[0] == 0 && g_mask_flag[1] == 0 && g_mask_flag[2] == 0 && g_mask_flag[3] == 0)
		offset += snprintf(p_buf+offset, mlen-offset, "<tpl:PrivacyMaskEnable>false</tpl:PrivacyMaskEnable>\r\n");
	else
		offset += snprintf(p_buf+offset, mlen-offset, "<tpl:PrivacyMaskEnable>true</tpl:PrivacyMaskEnable>\r\n");
	
	while (p_mask)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tpl:PrivacyMask token=\"%s\">", p_mask->Mask.token);
		offset += build_tpl_PrivacyMask_xml(p_buf+offset, mlen-offset, &p_mask->Mask);
		offset += snprintf(p_buf+offset, mlen-offset, "</tpl:PrivacyMask>");

		if (loopflag)
		{
			p_mask = p_mask->next;
		}
		else
		{
			break;
		}
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tpl:GetPrivacyMasksResponse>");

	return offset;
}

int build_tpl_SetPrivacyMask_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:SetPrivacyMaskResponse />");
	return offset;
}

#endif

#if 1
int build_tpl_SetImagingSettingDefault_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:SetImagingSettingDefaultResponse></tpl:SetImagingSettingDefaultResponse>");
	return offset;
}

int build_tpl_SendCommand_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:SendCommandResponse></tpl:SendCommandResponse>");
	return offset;
}

int build_tpl_ServicesCapabilities_xml(char * p_buf, int mlen)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:Capabilities PTZGuard=\"false\" H265=\"true\" PrivacyMask=\"true\" PTZTrack=\"false\">");

	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:Network xsi:nil=\"true\"/>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:Security xsi:nil=\"true\"/>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:System xsi:nil=\"true\"/>");

	offset += snprintf(p_buf+offset, mlen-offset, "</tpl:Capabilities>");

	return offset;
}

int build_tpl_GetImagingOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:GetImagingOptionsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:ImagingOptions>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:ExposureOption>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:Mode>Custom</tpl:Mode>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:Mode>Auto</tpl:Mode>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:Mode>Manual</tpl:Mode>");
	offset += snprintf(p_buf+offset, mlen-offset, "</tpl:ExposureOption>");
	offset += snprintf(p_buf+offset, mlen-offset, "</tpl:ImagingOptions>");
	offset += snprintf(p_buf+offset, mlen-offset, "</tpl:GetImagingOptionsResponse>");

	return offset;
}

int build_tpl_GetDigitalInputs_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;

	offset += snprintf(p_buf+offset, mlen-offset, " <tpl:GetDigitalInputsResponse></tpl:GetDigitalInputsResponse>");

	return offset;
}

int build_tpl_GetVideoEncoderConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	int EncodeCap = 2065500;
	if (1)
	{
		int w = 0, h = 0, max_frame = 0, EncodeCap_value = 0;
		/* Get max resolution: replaces GetMaxVideoResEntryAll(0, ...) */
		anj_sysmng_max_res_get(0, &w, &h);
		max_frame = 25;
		EncodeCap_value += w*h*max_frame;
			
		w = 0; h = 0; max_frame = 0;
		/* Get max resolution: replaces GetMaxVideoResEntryAll(1, ...) */
		anj_sysmng_max_res_get(0, &w, &h);
		max_frame = 25;
		EncodeCap_value += w*h*max_frame;
		
		w = 0; h = 0; max_frame = 0;
		/* Get max resolution: replaces GetMaxVideoResEntryAll(2, ...) */
		anj_sysmng_max_res_get(0, &w, &h);
		max_frame = 25;
		EncodeCap_value += w*h*max_frame;

		EncodeCap_value = EncodeCap_value/256;
		EncodeCap = EncodeCap_value;
	}
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tpl:GetVideoEncoderConfigurationOptionsResponse>"
		"<tpl:Options>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tptl:Encoding>H265</tptl:Encoding>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tptl:Smoothy>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Min>1</tt:Min>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Max>9</tt:Max>");
	offset += snprintf(p_buf+offset, mlen-offset, "</tptl:Smoothy>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tptl:EncodeCap>%d</tptl:EncodeCap>", EncodeCap);
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tptl:H264ExtendEncodeModes>"
		"<tptl:ExtendEncodeModeType>NORMAL</tptl:ExtendEncodeModeType>"
		"</tptl:H264ExtendEncodeModes>");
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tptl:H264ExtendEncodeModes>"
		"<tptl:ExtendEncodeModeType>BASIC_UCODE</tptl:ExtendEncodeModeType>"
		"</tptl:H264ExtendEncodeModes>");
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tptl:H264ExtendEncodeModes>"
		"<tptl:ExtendEncodeModeType>ADVANCED_UCODE</tptl:ExtendEncodeModeType>"
		"</tptl:H264ExtendEncodeModes>");
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tptl:H265ExtendEncodeModes>"
		"<tptl:ExtendEncodeModeType>NORMAL</tptl:ExtendEncodeModeType>"
		"</tptl:H265ExtendEncodeModes>");
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tptl:H265ExtendEncodeModes>"
		"<tptl:ExtendEncodeModeType>BASIC_UCODE</tptl:ExtendEncodeModeType>"
		"</tptl:H265ExtendEncodeModes>");
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tptl:H265ExtendEncodeModes>"
		"<tptl:ExtendEncodeModeType>ADVANCED_UCODE</tptl:ExtendEncodeModeType>"
		"</tptl:H265ExtendEncodeModes>");
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"</tpl:Options>"
		"</tpl:GetVideoEncoderConfigurationOptionsResponse>");
	
	return offset;
}

int build_tpl_SetVideoEncoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:SetVideoEncoderConfigurationResponse></tpl:SetVideoEncoderConfigurationResponse>");
	return offset;
}

int build_tpl_SetImagingSettings_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:SetImagingSettingsResponse></tpl:SetImagingSettingsResponse>");
	return offset;
}

int build_tpl_SetDigitalInputConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:SetDigitalInputConfigurationResponse></tpl:SetDigitalInputConfigurationResponse>");
	return offset;
}

int build_tpl_GetImagingSettings_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv)
{
	int offset = 0;
	
	VideoCaptureCfg videoCfg;
	memcpy(&videoCfg, &((MediaConfig *)getMediaConfig())->videoConfig[0].videoCapture, sizeof(videoCfg));
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:GetImagingSettingsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tpl:ImagingSettings>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tplt:Exposure>");
	if (videoCfg.shutterSetting.shutter_mode_day == 0)
		offset += snprintf(p_buf+offset, mlen-offset, "<tplt:Mode>Auto</tplt:Mode>");
	else
		offset += snprintf(p_buf+offset, mlen-offset, "<tplt:Mode>Manual</tplt:Mode>");
	offset += snprintf(p_buf+offset, mlen-offset, "</tplt:Exposure>");
	offset += snprintf(p_buf+offset, mlen-offset, "</tpl:ImagingSettings>");
	offset += snprintf(p_buf+offset, mlen-offset, "</tpl:GetImagingSettingsResponse>");
	return offset;
}


#endif

