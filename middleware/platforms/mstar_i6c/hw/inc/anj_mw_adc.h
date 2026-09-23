#ifndef __ANJ_MW_ADC_H__
#define __ANJ_MW_ADC_H__


#define ANJ_BAT_ADC_DEV "/dev/sar"
#define SARADC_IOC_MAGIC 'a'
#define MS_SAR_SET_CHANNEL_READ_VALUE   _IO(SARADC_IOC_MAGIC,1)
#define MS_SAR_INIT                     _IO(SARADC_IOC_MAGIC,0)


int anj_mw_adc_battery_init();
int anj_mw_adc_battery_uninit();
int anj_mw_adc_battery_get();



#endif