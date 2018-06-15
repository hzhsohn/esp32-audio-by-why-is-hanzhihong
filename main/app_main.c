#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "tcpip_adapter.h"

//-------------------------
#include "duerOSService.h"
#include "wifi.h"
#include "event.h"
#include "h_gpio.h"
#include "aplay.h"
#include "encoding.h"
#include "zhHttp.h"
#include "spiram_fifo.h"
#include "webplay.h"
//-------------------------
//播放音乐的路径
char g_payloadurl[128]={0};
char *g_myVopBuf=NULL;
int g_myVopBufLen=0;

void httpCallBack_tsn(EzhHttpOperat operat,
						   char*host,
						   int port,
						   char*file,
						   char*parameter,
						   const char*body,
						   int body_len,
						   char*szBuf,
						   int nLen)
{
	switch(operat)
	{
	case ezhHttpOperatConnected:
		{
		printf("http---tsn----------ezhHttpOperatConnected\n");
			g_spiRamMP3Pos=0;
		}
		break;
	case ezhHttpOperatGetData:
		{
			printf("http---tsn----------data len=%d\r\n",nLen);
			memcpy(&g_spiRamMP3Buf[g_spiRamMP3Pos],szBuf,nLen);
			g_spiRamMP3Pos+=nLen;
		}
		break;
	case ezhHttpOperatFinish:
		{
			printf("http---tsn----------Finish\r\n");
			_aplay_spiram_mp3();
		}
		break;
	case ezhHttpOperatGetSize:
		break;
	case ezhHttpOperatRecviceFail:
		{
			printf("httpCallBack_tsn-------------ezhHttpOperatRecviceFail!\r\n");
		}
		break;
	case ezhHttpOperatConnectFail:
		{
			printf("httpCallBack_tsn-------------connect server fail!\r\n");
		}
		break;
	case ezhHttpOperatPostFail:
		{
			printf("httpCallBack_tsn-------------ezhHttpOperatPostFail!!\r\n");
		}
		break;
	case ezhHttpOperatPageJump:
		{
			printf("httpCallBack_tsn-------------ezhHttpOperatPageJump!!  szBuf=%s\r\n",szBuf);
		}
		break;
	}
}

void httpCallBack_music(EzhWebPlayHttpOperat operat,
						   char*host,
						   int port,
						   char*file,
						   char*parameter,
						   const char*body,
						   int body_len,
						   char*szBuf,
						   int nLen)
{
	switch(operat)
	{
	case ezhWebPlayConnected:
		{
			printf("httpCallBack_music----------ezhWebPlayHttpConnected\n");
		}
		break;
	case ezhWebPlayGetData:
		{
			printf("httpCallBack_music----------ezhWebPlayHttpGetData=%d\n",nLen);
		}
		break;
	case ezhWebPlayFinish:
		{
			printf("httpCallBack_music----------ezhWebPlayHttpFinish\n");
		}
		break;
	case ezhWebPlayRecviceFail:
		{
			printf("httpCallBack_music-------------ezhWebPlayHttpRecviceFail!\r\n");
		}
		break;
	case ezhWebPlayConnectFail:
		{
			printf("httpCallBack_music-------------connect server fail!\r\n");
		}
		break;
	case ezhWebPlayInterrupt:
		{
			printf("httpCallBack_music-------------ezhWebPlayPlayInterrupt\r\n");
		}
		break;
	}
}

//
void btn_press(int gpio_num, int is_press)
{
		if(GPIO_NUM_34==gpio_num)
		{
				static int cur_press_cacal=0;
				static int is_long_press_ok=0;
				if(0==is_press)
				{
					cur_press_cacal=0;
					is_long_press_ok=0;
				}
				else
				{
					if(0==is_long_press_ok)
					{
						cur_press_cacal++;
						if(cur_press_cacal>50)
						{
							printf("BTN1 long press action \n");
							is_long_press_ok=1;
							//-------------------
							//
							//-------------------
							is_long_press_ok=0;
							cur_press_cacal=0;
						}
					}
				}
		}
		else if(GPIO_NUM_35==gpio_num)
		{
		    static int snd_doudong=0; 
		    if(1==is_press)
		    {				
				if(false==webplay_stop_mp3())
				{
				    aplay_end();
				    speaker_begin();
				    snd_doudong=0;
				}
		    }
		    else
		    {
		      snd_doudong++;
		      if(snd_doudong>1)
		      {
		        speaker_end();
		        snd_doudong=0;
		      }
		    }
		}
}


void AUDIO_PLAY_CALLBACK (bool isplay)
{
		printf("AUDIO_PLAY_CALLBACK     isPlay=%d\r\n",isplay);
}

void AUDIO_SPEAK_CALLBACK (APlaySpeakStatus status , const char *buf, int len)
{
		switch (status)
		{
		case ezhAPSpeakStatus_Begin:
				ESP_LOGI("AUDIO_SPEAK_CALLBACK","ezhAPSpeakStatus_Begin\n");
				g_myVopBufLen=0;
				
			break;
		case ezhAPSpeakStatus_Ing:
				ESP_LOGI("AUDIO_SPEAK_CALLBACK","ezhAPSpeakStatus_Ing len=%d\n" ,len);
				if(g_myVopBufLen+len>300*1024)
				{g_myVopBufLen=0;}
				memcpy(&g_myVopBuf[g_myVopBufLen],buf,len);
				g_myVopBufLen+=len;
			break;
		case ezhAPSpeakStatus_End:
				ESP_LOGI("AUDIO_SPEAK_CALLBACK","ezhAPSpeakStatus_End ");
				//上传语音
				duerOSEvtSend(UploadEvt_Start, NULL, 0, 0);
				duerOSEvtSend(UploadEvt_Data, g_myVopBuf, g_myVopBufLen, 0);
				duerOSEvtSend(UploadEvt_Stop, NULL, 0, 0);
			break;
		}
}

void task_url_music(char * url)
{
	webplay_begin_mp3(url,httpCallBack_music);
	vTaskDelete(NULL);
}

void task_url_sound(char * url)
{
	zhHttpGet(url,0,httpCallBack_tsn);
	//有音乐就播放音乐
	if(strlen(g_payloadurl)>0)
	{
		xTaskCreate(task_url_music, "task_url_music", 7*1024, g_payloadurl, 6, NULL);
	}
	vTaskDelete(NULL);
}

void DUEROS_PLAY_CALLBACK (char* url,char*text,char *payload)
{
  if(url)
  {
      strcpy(g_payloadurl,"");
      printf("\r\n播放声音-->%s \r\n",url);
      xTaskCreate(task_url_sound, "task_url_sound", 7*1024, url, 5, NULL);
  }
  if(text)
  {
      char gb2312Buf[128]={0};
      Utf8ToGb2312(text,strlen(text),gb2312Buf);
      printf("\r\n播放内容-->%s \r\n",gb2312Buf);
  }
  if(payload)
  {
	  printf("\r\n播放音乐-->%s \r\n",payload);
	  strcpy(g_payloadurl,payload);
  }
}

void app_main()
{

	printf("app_main \r\n");

    nvs_flash_init();
    tcpip_adapter_init();
    event_engine_init();

	printf("h_gpio_init \r\n");
    h_gpio_init(btn_press);	

	printf("aplay_init \r\n");
    aplay_init(AUDIO_PLAY_CALLBACK , AUDIO_SPEAK_CALLBACK);
	xTaskCreate(webplay_task_mp3, "webplay_task_mp3", 4*1024, NULL, 5, NULL);
	
	//语音缓冲区
	g_myVopBuf=pvPortMallocCaps(600*1024, MALLOC_CAP_SPIRAM);

    app_wifi_sta("hx-kong.com","89918000");
    
	vTaskDelay(3000 / portTICK_PERIOD_MS);

    xTaskCreate(task_url_music, "task_url_music", 7*1024, "http://res.iot.baidu.com/api/v1/voice/0f6c000000000a/tts/2be183c35d9b68c3ac48971019a67eb3.mp3", 6, NULL);

	vTaskDelay(3000 / portTICK_PERIOD_MS);
    /* Duer OS voice recognition*/
    initDuerOS(DUEROS_PLAY_CALLBACK);
	
	vTaskSuspend(NULL);
}


