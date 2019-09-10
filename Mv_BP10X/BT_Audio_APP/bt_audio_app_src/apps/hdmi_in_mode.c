/**
 **************************************************************************************
 * @file    hdmi_in_mode.c
 * @brief   
 *
 * @author  Cecilia Wang
 * @version V1.0.0
 *
 * $Created: 2018-03-23 13:06:47$
 *
 * @Copyright (C) 2016, Shanghai Mountain View Silicon Co.,Ltd. All rights reserved.
 **************************************************************************************
 */
#include <string.h>
#include "type.h"
#include "irqn.h"
#include "gpio.h"
#include "dma.h"
#include "rtos_api.h"
#include "app_message.h"
#include "app_config.h"
#include "debug.h"
#include "delay.h"
#include "audio_adc.h"
#include "dac.h"
#include "adc_interface.h"
#include "dac_interface.h"
#include "audio_core_api.h"
#include "audio_core_service.h"
#include "hdmi_in_api.h"
#include "timeout.h"
#include "main_task.h"
#include "audio_effect.h"
#include "mcu_circular_buf.h"
#include "resampler.h"
#include "decoder_service.h"
#include "remind_sound_service.h"
#include "hdmi_in_Mode.h"
#include "powercontroller.h"
#include "deepsleep.h"
#include "backup_interface.h"
#include "breakpoint.h"
#include "otg_device_hcd.h"
#include "otg_device_standard_request.h"
#include "ctrlvars.h"
#include "recorder_service.h"
#include "encoder_service.h"
#include "audio_vol.h"
#include "clk.h"
#include "ctrlvars.h"
#include "reset.h"
extern CECInitTypeDef 	*gCecInitDef;
#ifdef  CFG_APP_HDMIIN_MODE_EN

#define HDMI_IN_PLAY_TASK_STACK_SIZE		512//1024
#define HDMI_IN_PLAY_TASK_PRIO				3
#define HDMI_IN_NUM_MESSAGE_QUEUE			10

#define HDMI_IN_SOURCE_NUM					APP_SOURCE_NUM

//#define HDMI_ARC_FIFO_LEN					(CFG_PARA_SAMPLES_PER_FRAME * 2 * 2 * 2 * 2) //recv, dma buf len
//由于采样值从32bit转换为16bit，可以使用同一个buf，否则要独立申请
#define HDMI_ARC_CARRY_LEN					(MAX_FRAME_SAMPLES * 2 * 2 * 2)// buf len for get data form dma fifo, deal
 //转采样输出buf,如果spdif转采样提升大于四倍需要加大此SPDIF_CARRY_LEN，比如输入8000以下转48000,需要缩小单次carry帧大小或调大HDMI_SRC_RESAMPLER_OUT_LEN、HDMI_ARC_PCM_FIFO_LEN
#define HDMI_SRC_RESAMPLER_OUT_LEN			(MAX_FRAME_SAMPLES * 2 * 2 * 4)
//audiocore混音前缓冲fifo 大小
//#define HDMI_ARC_PCM_FIFO_LEN				(CFG_PARA_SAMPLES_PER_FRAME * 2 * 2 * 2 * 2)
//#define HDMI_ARC_SOURCE_LEN					(CFG_PARA_SAMPLES_PER_FRAME * 2 * 2) //one frame for audiocore source1 carry



typedef struct _HdmiInPlayContext
{
	xTaskHandle 		taskHandle;
	MessageHandle		msgHandle;
	MessageHandle		parentMsgHandle;

	TaskState			state;

	uint32_t			*hdmiARCFIFO;	    //ARC的DMA循环fifo
	uint32_t            *sourceBuf_ARC;		//取ARC数据
	uint32_t            *hdmiARCCarry;
	uint32_t			*hdmiARCPcmFifo;
	MCU_CIRCULAR_CONTEXT hdmiPcmCircularBuf;

	AudioCoreContext 	*AudioCoreHdmiIn;
#if	defined(CFG_FUNC_REMIND_SOUND_EN)
	uint16_t*			Source2Decoder;
	TaskState			DecoderSync;
	bool				IsSoundRemindDone;
#endif

#ifdef CFG_FUNC_RECORDER_EN
	TaskState			RecorderSync;
	TaskState			EncoderSync;
#endif

	//play
	uint32_t 			SampleRate;

#ifdef CFG_FUNC_MIXER_SRC_EN
	ResamplerContext*	hdmiResamplerCt;
	uint32_t*			rSampleRateBuf;
#endif
	uint32_t			hdmiSampleRate;

	uint32_t 			hdmiDmaWritePtr;
	uint32_t 			hdmiDmaReadPtr;
	uint32_t 			hdmiPreSample;
	uint8_t  			hdmiSampleRateCheckFlg;
	uint32_t 			hdmiSampleRateFromSW;
}HdmiInPlayContext;


/**根据appconfig缺省配置:DMA 8个通道配置**/
/*1、cec需PERIPHERAL_ID_TIMER3*/
/*2、SD卡录音需PERIPHERAL_ID_SDIO1 RX/TX*/
/*3、SPDIF需PERIPHERAL_ID_SDPIF_RX*/
/*4、在线串口调音需PERIPHERAL_ID_UART1 RX/TX*/ 
/*5、IR按键需PERIPHERAL_ID_TIMER4，mode之间通道必须一致*/
/*6、Mic开启需PERIPHERAL_ID_AUDIO_ADC1_RX，mode之间通道必须一致*/
/*7、Dac0开启需PERIPHERAL_ID_AUDIO_DAC0_TX mode之间通道必须一致*/
/*8、DacX需开启PERIPHERAL_ID_AUDIO_DAC1_TX mode之间通道必须一致*/

static const uint8_t DmaChannelMap[29] = {
	255,//PERIPHERAL_ID_SPIS_RX = 0,	//0
	255,//PERIPHERAL_ID_SPIS_TX,		//1
	5,//PERIPHERAL_ID_TIMER3,			//2
	6,//PERIPHERAL_ID_SDIO_RX,		//3
	6,//PERIPHERAL_ID_SDIO_TX,		//4
	255,//PERIPHERAL_ID_UART0_RX,		//5
	255,//PERIPHERAL_ID_TIMER1,			//6
	255,//PERIPHERAL_ID_TIMER2,			//7
	4,//PERIPHERAL_ID_SDPIF_RX,			//8 SPDIF_RX /TX same chanell
	4,//PERIPHERAL_ID_SDPIF_TX,			//8 SPDIF_RX /TX same chanell
	255,//PERIPHERAL_ID_SPIM_RX,		//9
	255,//PERIPHERAL_ID_SPIM_TX,		//10
	255,//PERIPHERAL_ID_UART0_TX,		//11
	
#ifdef CFG_COMMUNICATION_BY_UART	
	7,//PERIPHERAL_ID_UART1_RX,			//12
	6,//PERIPHERAL_ID_UART1_TX,			//13
#else
	255,//PERIPHERAL_ID_UART1_RX,		//12
	255,//PERIPHERAL_ID_UART1_TX,		//13
#endif

	255,//PERIPHERAL_ID_TIMER4,			//14
	255,//PERIPHERAL_ID_TIMER5,			//15
	255,//PERIPHERAL_ID_TIMER6,			//16
	0,//PERIPHERAL_ID_AUDIO_ADC0_RX,	//17
	1,//PERIPHERAL_ID_AUDIO_ADC1_RX,	//18
	2,//PERIPHERAL_ID_AUDIO_DAC0_TX,	//19
	3,//PERIPHERAL_ID_AUDIO_DAC1_TX,	//20
	255,//PERIPHERAL_ID_I2S0_RX,		//21
#if	(defined(CFG_RES_AUDIO_I2SOUT_EN )&&(CFG_RES_I2S_PORT==0))
	7,//PERIPHERAL_ID_I2S0_TX,			//22
#else	
	255,//PERIPHERAL_ID_I2S0_TX,		//22
#endif	
	255,//PERIPHERAL_ID_I2S1_RX,		//23
#if	(defined(CFG_RES_AUDIO_I2SOUT_EN )&&(CFG_RES_I2S_PORT==1))
	7,	//PERIPHERAL_ID_I2S1_TX,		//24
#else
	255,//PERIPHERAL_ID_I2S1_TX,		//24
#endif
	255,//PERIPHERAL_ID_PPWM,			//25
	255,//PERIPHERAL_ID_ADC,     		//26
	255,//PERIPHERAL_ID_SOFTWARE,		//27
};

static HdmiInPlayContext*		hdmiInPlayCt;

osMutexId			hdmiPcmFifoMutex;


static void HdmiInPlayModeCreating(uint16_t msgId);
static void HdmiInPlayModeStarting(uint16_t msgId);
static void HdmiInPlayModeStopping(uint16_t msgId);
static void HdmiARCScan(void);
static void HdmiInPlayRunning(uint16_t msgId);

void HdmiInPlayResRelease(void)
{
	//DMA_ChannelDisable(PERIPHERAL_ID_AUDIO_ADC0_RX);
	DMA_ChannelDisable(PERIPHERAL_ID_SPDIF_RX);
	//Reset_FunctionReset(DMAC_FUNC_SEPA);
	
	DMA_InterruptFlagClear(PERIPHERAL_ID_SPDIF_RX, DMA_DONE_INT);
	DMA_InterruptFlagClear(PERIPHERAL_ID_SPDIF_RX, DMA_THRESHOLD_INT);
	DMA_InterruptFlagClear(PERIPHERAL_ID_SPDIF_RX, DMA_ERROR_INT);

	if(hdmiInPlayCt->hdmiARCFIFO != NULL)
	{
		DBG("hdmiARCFIFO\n");
		osPortFree(hdmiInPlayCt->hdmiARCFIFO);
		hdmiInPlayCt->hdmiARCFIFO = NULL;
	}
	if(hdmiInPlayCt->sourceBuf_ARC != NULL)
	{
		DBG("sourceBuf_ARC\n");
		osPortFree(hdmiInPlayCt->sourceBuf_ARC);
		hdmiInPlayCt->sourceBuf_ARC = NULL;
	}

	if(hdmiInPlayCt->hdmiARCPcmFifo != NULL)
	{
		DBG("hdmiARCPcmFifo\n");
		osPortFree(hdmiInPlayCt->hdmiARCPcmFifo);
		hdmiInPlayCt->hdmiARCPcmFifo = NULL;
	}
#if	defined(CFG_FUNC_REMIND_SOUND_EN)
	if(hdmiInPlayCt->Source2Decoder != NULL)
	{
		DBG("Source2Decoder\n");
		osPortFree(hdmiInPlayCt->Source2Decoder);
		hdmiInPlayCt->Source2Decoder = NULL;
	}
#endif

#ifdef CFG_FUNC_FREQ_ADJUST
	AudioCoreSourceFreqAdjustDisable();
#endif
}

bool HdmiInPlayResMalloc(uint16_t SampleLen)
{
	hdmiInPlayCt->hdmiARCFIFO = (uint32_t*)osPortMallocFromEnd(SampleLen * 2 * 2 * 2 * 2);
	if(hdmiInPlayCt->hdmiARCFIFO == NULL)
	{
		return FALSE;
	}
	memset(hdmiInPlayCt->hdmiARCFIFO, 0, SampleLen * 2 * 2 * 2 * 2);

	hdmiInPlayCt->sourceBuf_ARC = (uint32_t *)osPortMallocFromEnd(SampleLen * 2 * 2);
	if(hdmiInPlayCt->sourceBuf_ARC == NULL)
	{
		return FALSE;
	}
	memset(hdmiInPlayCt->sourceBuf_ARC, 0, SampleLen * 2 * 2);

	hdmiInPlayCt->hdmiARCPcmFifo = (uint32_t *)osPortMallocFromEnd(SampleLen * 2 * 2 * 2 * 2);
	if(hdmiInPlayCt->hdmiARCPcmFifo == NULL)
	{
		return FALSE;
	}
	MCUCircular_Config(&hdmiInPlayCt->hdmiPcmCircularBuf, hdmiInPlayCt->hdmiARCPcmFifo, SampleLen * 2 * 2 * 2 * 2);
	memset(hdmiInPlayCt->hdmiARCPcmFifo, 0, SampleLen * 2 * 2 * 2 * 2);

#if	defined(CFG_FUNC_REMIND_SOUND_EN)
	hdmiInPlayCt->Source2Decoder = (uint16_t*)osPortMalloc(SampleLen * 2 * 2);//One Frame
	if(hdmiInPlayCt->Source2Decoder == NULL)
	{
		return FALSE;
	}
	memset(hdmiInPlayCt->Source2Decoder, 0, SampleLen * 2 * 2);//2K 
#endif
	return TRUE;
}

void HdmiInPlayResInit(void)
{
	if(hdmiInPlayCt->sourceBuf_ARC != NULL)
	{
		hdmiInPlayCt->AudioCoreHdmiIn->AudioSource[HDMI_IN_SOURCE_NUM].PcmInBuf = (int16_t *)hdmiInPlayCt->sourceBuf_ARC;
	}
#if	defined(CFG_FUNC_REMIND_SOUND_EN)
	if(hdmiInPlayCt->Source2Decoder != NULL)
	{
		hdmiInPlayCt->AudioCoreHdmiIn->AudioSource[REMIND_SOURCE_NUM].PcmInBuf = (int16_t *)hdmiInPlayCt->Source2Decoder;
	}	
#endif
	DMA_ChannelDisable(PERIPHERAL_ID_SPDIF_RX);
	DMA_CircularConfig(PERIPHERAL_ID_SPDIF_RX, mainAppCt.SamplesPreFrame * 2 * 2 * 2 * 2, (uint16_t *)hdmiInPlayCt->hdmiARCFIFO, mainAppCt.SamplesPreFrame * 2 * 2 * 2 * 2);
    DMA_ChannelEnable(PERIPHERAL_ID_SPDIF_RX);
#ifdef CFG_FUNC_FREQ_ADJUST
	AudioCoreSourceFreqAdjustEnable(HDMI_IN_SOURCE_NUM, (mainAppCt.SamplesPreFrame * 2 * 2 * 2 * 2 / 4 - mainAppCt.SamplesPreFrame) / 4, (mainAppCt.SamplesPreFrame * 2 * 2 * 2 * 2 / 4 - mainAppCt.SamplesPreFrame) * 3 / 4);
#endif
}

/**
 * @func        HdmiInPaly_Init
 * @brief       HdmiIn模式参数配置，资源初始化
 * @param       MessageHandle parentMsgHandle
 * @Output      None
 * @return      bool
 * @Others      任务块、Adc、Dac、AudioCore配置
 * @Others      数据流从Adc到audiocore配有函数指针，audioCore到Dac同理，由audiocoreService任务按需驱动
 * Record
 */
static bool HdmiInPlay_Init(MessageHandle parentMsgHandle)
{
	//将SPDIF时钟切换到AUPLL
	Clock_SpdifClkSelect(APLL_CLK_MODE);

	DMA_ChannelAllocTableSet((uint8_t *)DmaChannelMap);//HdmiIn

	hdmiInPlayCt = osPortMalloc(sizeof(HdmiInPlayContext));
	if(hdmiInPlayCt == NULL)
	{
		return FALSE;
	}
	memset(hdmiInPlayCt, 0, sizeof(HdmiInPlayContext));
#if	defined(CFG_FUNC_REMIND_SOUND_EN)
	hdmiInPlayCt->IsSoundRemindDone = FALSE;
#endif
	hdmiInPlayCt->msgHandle = MessageRegister(HDMI_IN_NUM_MESSAGE_QUEUE);
	if(hdmiInPlayCt->msgHandle == NULL)
	{
		return FALSE;
	}
	hdmiInPlayCt->parentMsgHandle = parentMsgHandle;
	hdmiInPlayCt->state = TaskStateCreating;

	// Audio core config
	hdmiInPlayCt->SampleRate = CFG_PARA_SAMPLE_RATE;

//	hdmiInPlayCt->hdmiARCFIFO = (uint32_t*)osPortMalloc(HDMI_ARC_FIFO_LEN);
//	if(hdmiInPlayCt->hdmiARCFIFO == NULL)
//	{
//		return FALSE;
//	}
//	memset(hdmiInPlayCt->hdmiARCFIFO, 0, HDMI_ARC_FIFO_LEN);

//	hdmiInPlayCt->sourceBuf_ARC = (uint32_t *)osPortMalloc(HDMI_ARC_SOURCE_LEN);
//	if(hdmiInPlayCt->sourceBuf_ARC == NULL)
//	{
//		return FALSE;
//	}
//	memset(hdmiInPlayCt->sourceBuf_ARC, 0, HDMI_ARC_SOURCE_LEN);
	if(!HdmiInPlayResMalloc(mainAppCt.SamplesPreFrame))
	{
		DBG("HdmiInPlayResMalloc error\n");
		return FALSE;
	}
	
	hdmiInPlayCt->hdmiARCCarry = (uint32_t *)osPortMalloc(HDMI_ARC_CARRY_LEN);
	if(hdmiInPlayCt->hdmiARCCarry == NULL)
	{
		return FALSE;
	}
	memset(hdmiInPlayCt->hdmiARCCarry, 0, HDMI_ARC_CARRY_LEN);

#ifdef CFG_FUNC_MIXER_SRC_EN
	hdmiInPlayCt->hdmiResamplerCt = (ResamplerContext*)osPortMalloc(sizeof(ResamplerContext));
	if(hdmiInPlayCt->hdmiResamplerCt == NULL)
	{
		return FALSE;
	}
	hdmiInPlayCt->rSampleRateBuf = (uint32_t *)osPortMalloc(HDMI_SRC_RESAMPLER_OUT_LEN);
	if(hdmiInPlayCt->rSampleRateBuf == NULL)
	{
		return FALSE;
	}
#endif
	hdmiInPlayCt->hdmiDmaWritePtr 		 = 0;
	hdmiInPlayCt->hdmiDmaReadPtr  		 = 0;
	hdmiInPlayCt->hdmiSampleRateCheckFlg = 0;
	hdmiInPlayCt->hdmiSampleRateFromSW 	 = 0;
	hdmiInPlayCt->hdmiPreSample			 = 0;

//	hdmiInPlayCt->hdmiARCPcmFifo = (uint32_t *)osPortMalloc(HDMI_ARC_PCM_FIFO_LEN);
//	if(hdmiInPlayCt->hdmiARCPcmFifo == NULL)
//	{
//		return FALSE;
//	}
//	MCUCircular_Config(&hdmiInPlayCt->hdmiPcmCircularBuf, hdmiInPlayCt->hdmiARCPcmFifo, HDMI_ARC_PCM_FIFO_LEN);
//	memset(hdmiInPlayCt->hdmiARCPcmFifo, 0, HDMI_ARC_PCM_FIFO_LEN);

	hdmiInPlayCt->AudioCoreHdmiIn = (AudioCoreContext *)&AudioCore;

	//HDMI_ARC_Init((uint16_t *)hdmiInPlayCt->hdmiARCFIFO, HDMI_ARC_FIFO_LEN, &hdmiInPlayCt->hdmiPcmCircularBuf);
	HDMI_ARC_Init((uint16_t *)hdmiInPlayCt->hdmiARCFIFO, mainAppCt.SamplesPreFrame * 2 * 2 * 2 * 2, &hdmiInPlayCt->hdmiPcmCircularBuf);
	HDMI_CEC_DDC_Init();

	//Audio init
//	//note Soure0.和sink0已经在main app中配置，不要随意配置

	//Soure1.
	hdmiInPlayCt->AudioCoreHdmiIn->AudioSource[HDMI_IN_SOURCE_NUM].Enable = 0;
	hdmiInPlayCt->AudioCoreHdmiIn->AudioSource[HDMI_IN_SOURCE_NUM].FuncDataGet	= HDMI_ARC_DataGet;
	hdmiInPlayCt->AudioCoreHdmiIn->AudioSource[HDMI_IN_SOURCE_NUM].FuncDataGetLen = HDMI_ARC_DataLenGet;
	hdmiInPlayCt->AudioCoreHdmiIn->AudioSource[HDMI_IN_SOURCE_NUM].IsSreamData    = TRUE;
	hdmiInPlayCt->AudioCoreHdmiIn->AudioSource[HDMI_IN_SOURCE_NUM].PcmFormat      = 2;//stereo
	hdmiInPlayCt->AudioCoreHdmiIn->AudioSource[HDMI_IN_SOURCE_NUM].PcmInBuf       = (int16_t *)hdmiInPlayCt->sourceBuf_ARC;

	//InCore2 buf
#if	defined(CFG_FUNC_REMIND_SOUND_EN)
	//hdmiInPlayCt->Source2Decoder = (uint16_t*)osPortMalloc(CFG_PARA_SAMPLES_PER_FRAME * 2 * 2);//One Frame
	//if(hdmiInPlayCt->Source2Decoder == NULL)
	//{
	//	return FALSE;
	//}
	//memset(hdmiInPlayCt->Source2Decoder, 0, CFG_PARA_SAMPLES_PER_FRAME * 2 * 2);//2K 

	//Core Soure2 Para
	DecoderSourceNumSet(REMIND_SOURCE_NUM);
	hdmiInPlayCt->AudioCoreHdmiIn->AudioSource[REMIND_SOURCE_NUM].Enable = 0;
	hdmiInPlayCt->AudioCoreHdmiIn->AudioSource[REMIND_SOURCE_NUM].FuncDataGet = DecodedPcmDataGet;
	hdmiInPlayCt->AudioCoreHdmiIn->AudioSource[REMIND_SOURCE_NUM].FuncDataGetLen = NULL;
	hdmiInPlayCt->AudioCoreHdmiIn->AudioSource[REMIND_SOURCE_NUM].IsSreamData = FALSE;//Decoder
	hdmiInPlayCt->AudioCoreHdmiIn->AudioSource[REMIND_SOURCE_NUM].PcmFormat = 2; //stereo
	hdmiInPlayCt->AudioCoreHdmiIn->AudioSource[REMIND_SOURCE_NUM].PcmInBuf = (int16_t *)hdmiInPlayCt->Source2Decoder;
#endif
	
#ifdef CFG_FUNC_AUDIO_EFFECT_EN
	#ifdef CFG_FUNC_MIC_KARAOKE_EN
	hdmiInPlayCt->AudioCoreHdmiIn->AudioEffectProcess = (AudioCoreProcessFunc)AudioEffectProcess;
	#else
	hdmiInPlayCt->AudioCoreHdmiIn->AudioEffectProcess = (AudioCoreProcessFunc)AudioMusicProcess;
	#endif
#else
	hdmiInPlayCt->AudioCoreHdmiIn->AudioEffectProcess = (AudioCoreProcessFunc)AudioBypassProcess;
#endif
	//HDMI_CEC_InitiateARC();

#ifdef CFG_FUNC_RECORDER_EN
	hdmiInPlayCt->RecorderSync = TaskStateNone;
	hdmiInPlayCt->EncoderSync = TaskStateNone;
#endif

	return TRUE;
}

//创建从属services
static void HdmiInPlayModeCreate(void)
{
	bool NoService = TRUE;
	// Create service task
#if defined(CFG_FUNC_REMIND_SBC)
	DecoderServiceCreate(hdmiInPlayCt->msgHandle, DECODER_BUF_SIZE_SBC);//提示音格式决定解码器内存消耗
	NoService = FALSE;
#elif defined(CFG_FUNC_REMIND_SOUND_EN)
	DecoderServiceCreate(hdmiInPlayCt->msgHandle, DECODER_BUF_SIZE_MP3);
	NoService = FALSE;
#endif
	if(NoService)
	{
		HdmiInPlayModeCreating(MSG_NONE);
	}
}

//All of services is created
//Send CREATED message to parent
static void HdmiInPlayModeCreating(uint16_t msgId)
{
	MessageContext		msgSend;
#ifdef CFG_FUNC_REMIND_SOUND_EN
	if(msgId == MSG_DECODER_SERVICE_CREATED)
	{
		hdmiInPlayCt->DecoderSync = TaskStateReady;
	}
#endif

#ifdef CFG_FUNC_REMIND_SOUND_EN
	if(hdmiInPlayCt->DecoderSync == TaskStateReady)
#endif
	{
		msgSend.msgId		= MSG_HDMI_AUDIO_MODE_CREATED;
		MessageSend(hdmiInPlayCt->parentMsgHandle, &msgSend);
		hdmiInPlayCt->state = TaskStateReady;
	}
}


static void HdmiInPlayModeStart(void)
{
	bool NoService = TRUE;
#if	defined(CFG_FUNC_REMIND_SOUND_EN)
	DecoderServiceStart();
	hdmiInPlayCt->DecoderSync = TaskStateStarting;
	NoService = FALSE;
#endif
	if(NoService)
	{
		HdmiInPlayModeStarting(MSG_NONE);
	}
	else
	{
		hdmiInPlayCt->state = TaskStateStarting;
	}
}

static void HdmiInPlayModeStarting(uint16_t msgId)
{
	MessageContext		msgSend;
#ifdef CFG_FUNC_REMIND_SOUND_EN
	if(msgId == MSG_DECODER_SERVICE_STARTED)
	{
		hdmiInPlayCt->DecoderSync = TaskStateRunning;
	}
#endif

#ifdef CFG_FUNC_REMIND_SOUND_EN
	if(hdmiInPlayCt->DecoderSync == TaskStateRunning)
#endif
	{
		msgSend.msgId		= MSG_HDMI_AUDIO_MODE_STARTED;
		MessageSend(hdmiInPlayCt->parentMsgHandle, &msgSend);
		hdmiInPlayCt->state = TaskStateRunning;
		
#ifdef CFG_FUNC_REMIND_SOUND_EN
		if(!RemindSoundServiceItemRequest(SOUND_REMIND_HDMIMODE)) //插播提示音
		{
			hdmiInPlayCt->IsSoundRemindDone = TRUE;//无提示音，标志位位1
		}
#endif
	}
}

static void HdmiInPlayModeStop(void)
{
	bool NoService = TRUE;
#if	defined(CFG_FUNC_REMIND_SOUND_EN)
	DecoderServiceStop();
	NoService = FALSE;
#endif
#ifdef CFG_FUNC_RECORDER_EN
	if(hdmiInPlayCt->RecorderSync != TaskStateNone)
	{//此service 随用随Kill
		MediaRecorderServiceStop();
		hdmiInPlayCt->RecorderSync = TaskStateStopping;
		NoService = FALSE;
	}
	if(hdmiInPlayCt->EncoderSync != TaskStateNone)
	{//此service 随用随Kill
		EncoderServiceStop();
		hdmiInPlayCt->EncoderSync = TaskStateStopping;
		NoService = FALSE;
	}
#endif

	hdmiInPlayCt->state = TaskStateStopping;
	if(NoService)
	{
		HdmiInPlayModeStopping(MSG_NONE);
	}
}

static void HdmiInPlayModeStopping(uint16_t msgId)
{
	MessageContext		msgSend;
	//Set para
#ifdef CFG_FUNC_RECORDER_EN
	if(msgId == MSG_MEDIA_RECORDER_SERVICE_STOPPED)
	{
		DBG("Hdmi:RecorderKill");
		MediaRecorderServiceKill();
		hdmiInPlayCt->RecorderSync = TaskStateNone;
	}
	if(msgId == MSG_ENCODER_SERVICE_STOPPED)
	{
		DBG("Hdmi:EncoderKill");
		EncoderServiceKill();
		hdmiInPlayCt->EncoderSync = TaskStateNone;
	}
#endif	
#if	defined(CFG_FUNC_REMIND_SOUND_EN)
	if(msgId == MSG_DECODER_SERVICE_STOPPED)
	{
		hdmiInPlayCt->DecoderSync = TaskStateNone;
	}
#endif

	if((hdmiInPlayCt->state == TaskStateStopping)
#ifdef CFG_FUNC_RECORDER_EN
		&& (hdmiInPlayCt->RecorderSync == TaskStateNone)
		&& (hdmiInPlayCt->EncoderSync == TaskStateNone)
#endif
#ifdef CFG_FUNC_REMIND_SOUND_EN
		&& (hdmiInPlayCt->DecoderSync == TaskStateNone)
#endif
		)
	{
		if(gCecInitDef)
		{
			while(HDMI_CEC_IsWorking() == CEC_IS_WORKING)
			{
				WDG_Feed();
			}
			HDMI_CEC_TerminationARC();
			while(HDMI_CEC_IsWorking() == CEC_IS_WORKING)
			{
				WDG_Feed();
			}
			HDMI_CEC_SetSystemAudioModeoff();
			while(HDMI_CEC_IsWorking() == CEC_IS_WORKING)
			{
				WDG_Feed();
			}
		}

		//clear msg
		MessageClear(hdmiInPlayCt->msgHandle);
		
		//Set state
		hdmiInPlayCt->state = TaskStateStopped;

		//reply
		msgSend.msgId		= MSG_HDMI_AUDIO_MODE_STOPPED;
		MessageSend(hdmiInPlayCt->parentMsgHandle, &msgSend);
	}
}

static void HdmiInSampleRateChange(void)
{
#ifdef CFG_FUNC_MIXER_SRC_EN
	resampler_init(hdmiInPlayCt->hdmiResamplerCt, 2, hdmiInPlayCt->hdmiSampleRate, CFG_PARA_SAMPLE_RATE, 0, 0);
#else
	if(hdmiInPlayCt->hdmiSampleRate != hdmiInPlayCt->SampleRate)
	{
		hdmiInPlayCt->SampleRate = hdmiInPlayCt->hdmiSampleRate;//注意此处调整会造成提示音和mic数据dac不正常。
		DBG("Dac Sample:%d\n",(int)hdmiInPlayCt->SampleRate);
#ifdef CFG_RES_AUDIO_DACX_EN
		AudioDAC_SampleRateChange(ALL, hdmiInPlayCt->SampleRate);
#else
		AudioDAC_SampleRateChange(DAC0, hdmiInPlayCt->SampleRate);
#endif
	}
#endif	
}

static void HdmiInPlayEntrance(void * param)
{
	MessageContext		msgRecv;
	uint32_t hdmiCurSampleRate = 0;
	bool SpdifLockFlag = FALSE;
	// Create services
	HdmiInPlayModeCreate();
#ifdef CFG_FUNC_AUDIO_EFFECT_EN	
	AudioEffectModeSel(mainAppCt.EffectMode, 2);//0=init hw,1=effect,2=hw+effect
	AudioEffectParamSync();
#ifdef CFG_COMMUNICATION_BY_UART
	UART1_Communication_Init((void *)(&UartRxBuf[0]), 1024, (void *)(&UartTxBuf[0]), 1024);
#endif
#endif

	DBG("HdmiIn:App\n");

#ifdef CFG_FUNC_BREAKPOINT_EN
	//HdmiInPlayBPUpdata();//进入模式的时候先做一次断点记忆
	BackupInfoUpdata(BACKUP_SYS_INFO);
#endif

//	//如果进入模式AudioCore为静音状态，则unmute
//	if(IsAudioPlayerMute() == TRUE)
//	{
//		AudioPlayerMute();
//	}
#if (CFG_RES_MIC_SELECT) && defined(CFG_FUNC_AUDIO_EFFECT_EN)
	AudioCoreSourceUnmute(MIC_SOURCE_NUM, TRUE, TRUE);
#endif
	while(1)
	{
		WDG_Feed();
		MessageRecv(hdmiInPlayCt->msgHandle, &msgRecv, 1);

		switch(msgRecv.msgId)
		{
			case MSG_TASK_CREATE://API, not msg, only happy
				break;
				
			case MSG_DECODER_SERVICE_CREATED:
				HdmiInPlayModeCreating(msgRecv.msgId);
				break;

			case MSG_TASK_START:
				HdmiInPlayModeStart();
				break;
			
			case MSG_DECODER_SERVICE_STARTED:
				HdmiInPlayModeStarting(msgRecv.msgId);
				break;

			case MSG_TASK_STOP:
#ifdef CFG_FUNC_REMIND_SOUND_EN
			RemindSoundServiceReset();
#endif
#if 0//CFG_COMMUNICATION_BY_USB
				NVIC_DisableIRQ(Usb_IRQn);
				OTG_DeviceDisConnect();
#endif
				HdmiInPlayModeStop();
				break;

			case MSG_MEDIA_RECORDER_SERVICE_STOPPED:
			case MSG_DECODER_SERVICE_STOPPED:
			case MSG_ENCODER_SERVICE_STOPPED:
				HdmiInPlayModeStopping(msgRecv.msgId);
				break;
			
			case MSG_APP_RES_RELEASE:
				HdmiInPlayResRelease();
				{
					MessageContext		msgSend;
					msgSend.msgId		= MSG_APP_RES_RELEASE_SUC;
					MessageSend(GetMainMessageHandle(), &msgSend);
				}
				break;
			case MSG_APP_RES_MALLOC:
				HdmiInPlayResMalloc(mainAppCt.SamplesPreFrame);
				{
					MessageContext		msgSend;
					msgSend.msgId		= MSG_APP_RES_MALLOC_SUC;
					MessageSend(GetMainMessageHandle(), &msgSend);
				}
				break;
			case MSG_APP_RES_INIT:
				HdmiInPlayResInit();
				{
					MessageContext		msgSend;
					msgSend.msgId		= MSG_APP_RES_INIT_SUC;
					MessageSend(GetMainMessageHandle(), &msgSend);
				}
				break;

			case MSG_REMIND_SOUND_PLAY_START:
				#if	defined(CFG_FUNC_REMIND_SOUND_EN)
				hdmiInPlayCt->IsSoundRemindDone = FALSE;
				#endif
				break;
					
			case MSG_REMIND_SOUND_PLAY_DONE://提示音播放结束
			case MSG_REMIND_SOUND_PLAY_REQUEST_FAIL:
				AudioCoreSourceUnmute(APP_SOURCE_NUM, TRUE, TRUE);
				#if	defined(CFG_FUNC_REMIND_SOUND_EN)
				hdmiInPlayCt->IsSoundRemindDone = TRUE;
				#endif
				if(SpdifLockFlag)
				{
					AudioCoreSourceUnmute(APP_SOURCE_NUM, TRUE, TRUE);
				}
				break;
			
			default:
				if(hdmiInPlayCt->state == TaskStateRunning)
				{
					HdmiInPlayRunning(msgRecv.msgId);
					if(SpdifLockFlag && !SPDIF_FlagStatusGet(LOCK_FLAG_STATUS))
					{
						DBG("SPDIF RX UNLOCK!\n");
						SpdifLockFlag = FALSE;
						AudioCoreSourceDisable(HDMI_IN_SOURCE_NUM);
				#ifdef CFG_FUNC_FREQ_ADJUST
						AudioCoreSourceFreqAdjustDisable();
				#endif
					}
					if(!SpdifLockFlag && SPDIF_FlagStatusGet(LOCK_FLAG_STATUS) && HDMI_ARC_IsReady()
						#if	defined(CFG_FUNC_REMIND_SOUND_EN)
						&& hdmiInPlayCt->IsSoundRemindDone
						#endif
					)
					{
						DBG("SPDIF RX LOCK!\n");
						SpdifLockFlag = TRUE;
						AudioCoreSourceMute(APP_SOURCE_NUM, TRUE, TRUE);
						vTaskDelay(20);
						AudioCoreSourceEnable(HDMI_IN_SOURCE_NUM);
						AudioCoreSourceUnmute(APP_SOURCE_NUM, TRUE, TRUE);
						
				#ifdef CFG_FUNC_FREQ_ADJUST
						AudioCoreSourceFreqAdjustEnable(HDMI_IN_SOURCE_NUM, (mainAppCt.SamplesPreFrame * 2 * 2 * 2 * 2 / 4 - mainAppCt.SamplesPreFrame) / 4, (mainAppCt.SamplesPreFrame * 2 * 2 * 2 * 2 / 4 - mainAppCt.SamplesPreFrame) * 3 / 4);
				#endif
					}
					if(SpdifLockFlag == TRUE)
					{
						//硬件采样率获取，设定。
						if(hdmiCurSampleRate != SPDIF_SampleRateGet())
						{
							hdmiCurSampleRate = SPDIF_SampleRateGet();

							DBG("Get samplerate: %d\n", (int)hdmiCurSampleRate);
							hdmiInPlayCt->hdmiSampleRate = hdmiCurSampleRate;
							HdmiInSampleRateChange();
						}

						HdmiARCScan();
					}
					HDMI_CEC_Scan();

					//任务优先级设置为4,通过发送该命令，可以提高AudioCore service有效利用率
					{
						MessageContext		msgSend;
						msgSend.msgId		= MSG_NONE;
						MessageSend(GetAudioCoreServiceMsgHandle(), &msgSend);
					}
				}
				break;
		}
	}
}


#include "hdmi_in_api.h"
extern CECInitTypeDef  *gCecInitCt;

static void HdmiInPlayRunning(uint16_t msgId)
{
	switch(msgId)
	{
#ifdef	CFG_FUNC_POWERKEY_EN
		case MSG_TASK_POWERDOWN:
			DBG("MSG receive PowerDown, Please breakpoint\n");
			SystemPowerDown();
			break;
#endif	

		case MSG_DECODER_STOPPED:
#if defined(CFG_FUNC_REMIND_SOUND_EN)
			{
				MessageContext		msgSend;
				msgSend.msgId = msgId;
				MessageSend(GetRemindSoundServiceMessageHandle(), &msgSend);//提示音期间转发解码器消息。
			}
#endif
			break;			

#ifdef CFG_FUNC_RECORDER_EN	
		case MSG_DEVICE_SERVICE_RECORDER:
			if(ResourceValue(AppResourceCard) || ResourceValue(AppResourceUDisk))
			{
				if(hdmiInPlayCt->RecorderSync == TaskStateNone)
				{
					MediaRecorderServiceCreate(hdmiInPlayCt->msgHandle);
					EncoderServiceCreate(hdmiInPlayCt->msgHandle);
					hdmiInPlayCt->RecorderSync = TaskStateCreating;
					hdmiInPlayCt->EncoderSync = TaskStateCreating;
				}
				else if(hdmiInPlayCt->RecorderSync == TaskStateRunning)//再按录音键 停止
				{
					MediaRecorderStop();
					MediaRecorderServiceStop();
					EncoderServiceStop();
					hdmiInPlayCt->RecorderSync = TaskStateStopping;
					hdmiInPlayCt->EncoderSync = TaskStateStopping;
				}
			}
			else
			{//flashfs录音 待处理
				DBG("Hdmi:error, no disk!!!\n");
			}
			break;
			
		case MSG_MEDIA_RECORDER_SERVICE_CREATED:
			hdmiInPlayCt->RecorderSync = TaskStateStarting;
			MediaRecorderServiceStart();
			break;
			
		case MSG_ENCODER_SERVICE_CREATED:
			hdmiInPlayCt->EncoderSync = TaskStateStarting;
			EncoderServiceStart();
			break;

		case MSG_MEDIA_RECORDER_SERVICE_STARTED:
			MediaRecorderRun();
			hdmiInPlayCt->RecorderSync = TaskStateRunning;
			break;
			
		case MSG_ENCODER_SERVICE_STARTED:
			hdmiInPlayCt->EncoderSync = TaskStateRunning;
			break;

		case MSG_MEDIA_RECORDER_STOPPED:
			MediaRecorderServiceStop();
			hdmiInPlayCt->RecorderSync = TaskStateStopping;
			break;

		case MSG_MEDIA_RECORDER_ERROR:
		case MSG_DEVICE_SERVICE_STOP:
			if(hdmiInPlayCt->RecorderSync == TaskStateRunning)
			{
				MediaRecorderStop();
				MediaRecorderServiceStop();
				hdmiInPlayCt->RecorderSync = TaskStateStopping;
				EncoderServiceStop();
				hdmiInPlayCt->EncoderSync = TaskStateStopping;

			}
			break;
#endif //录音

		case MSG_HDMI_AUDIO_ARC_ONOFF:
			mainAppCt.hdmiArcOnFlg = !mainAppCt.hdmiArcOnFlg;
			if(HDMI_CEC_IsWorking() == CEC_IS_IDLE)
			{
				HDMI_ARC_OnOff(mainAppCt.hdmiArcOnFlg);
			}
			break;
		case MSG_DEVICE_SERVICE_VOLUP:
			SystemVolUp();
			if(HDMI_CEC_IsWorking() == CEC_IS_IDLE)
			{
				 HDMI_CEC_SystemAudioModeStatus((uint8_t)(mainAppCt.gSysVol.AudioSinkVol[0] * 100 / 32) & 0x7f);
			}
			break;
			
		case MSG_DEVICE_SERVICE_VOLDOWN:
			SystemVolDown();
			if(HDMI_CEC_IsWorking() == CEC_IS_IDLE)
			{
			    HDMI_CEC_SystemAudioModeStatus((uint8_t)(mainAppCt.gSysVol.AudioSinkVol[0] * 100 / 32) & 0x7f);
			}
			break;

		default:
			CommonMsgProccess(msgId);
			break;
	}
}

static void HdmiARCScan(void)
{
	uint32_t pcm_space;
	uint16_t spdif_len;
	int16_t pcm_len;
	int16_t *pcmBuf  = (int16_t *)hdmiInPlayCt->hdmiARCCarry;
	uint16_t cnt;

	spdif_len = DMA_CircularDataLenGet(PERIPHERAL_ID_SPDIF_RX);
	pcm_space = MCUCircular_GetSpaceLen(&hdmiInPlayCt->hdmiPcmCircularBuf) - 16;

#ifdef CFG_FUNC_MIXER_SRC_EN
	pcm_space = (pcm_space * hdmiInPlayCt->hdmiSampleRate) / CFG_PARA_SAMPLE_RATE - 16;
#endif

	if((spdif_len >> 1) > pcm_space)
	{
		spdif_len = pcm_space * 2;
	}

	spdif_len = spdif_len /(MAX_FRAME_SAMPLES * 2 * 4) * (MAX_FRAME_SAMPLES * 2 * 4);
	spdif_len = spdif_len & 0xFFF8;
	if(!spdif_len)
	{
		return ;
	}

	cnt = (spdif_len / 8) / MAX_FRAME_SAMPLES;
	while(cnt--)
	{
		DMA_CircularDataGet(PERIPHERAL_ID_SPDIF_RX, pcmBuf, MAX_FRAME_SAMPLES * 8);
		int32_t *temp = (int32_t *)pcmBuf;
		uint16_t Count = 0;

		if(temp[0] & (1 << 28)) //左声道数据
	    {
	    	//DBG("%x\n",temp[0]);
	    }
	    else//第一个是右声道则判断为错误帧，丢弃，读一个右声道数据，保证下一次读出来第一个是左声道
	    {
			while(DMA_CircularDataLenGet(PERIPHERAL_ID_SPDIF_RX) < 4)
			{
				Count++;
				if(Count > 1000)
					break;
			}
			DMA_CircularDataGet(PERIPHERAL_ID_SPDIF_RX, pcmBuf, 4);
			//DBG("drop error frame\n");
			return;
	    }

		//由于从32bit转换为16bit，buf可以使用同一个，否则要独立申请。
        pcm_len = SPDIF_SPDIFDataToPCMData((int32_t *)pcmBuf, MAX_FRAME_SAMPLES * 8, (int32_t *)pcmBuf, SPDIF_WORDLTH_16BIT);

		if(pcm_len < 0)
		{
			return;
		}
		if(!mainAppCt.hdmiArcOnFlg)
		{
			memset(pcmBuf, 0, pcm_len);
		}
		if((pcm_len / 4) > MAX_FRAME_SAMPLES)//错误处理，每次MAX_FRAME_SAMPLES，
		{
			memset(pcmBuf, 0, MAX_FRAME_SAMPLES * 4);
			pcm_len = MAX_FRAME_SAMPLES * 4;
		}
#ifdef CFG_FUNC_MIXER_SRC_EN
		pcm_len = resampler_apply(hdmiInPlayCt->hdmiResamplerCt, (int16_t*)pcmBuf, (int16_t*)hdmiInPlayCt->rSampleRateBuf, pcm_len/4);
		if(pcm_len < 0)
		{
			return;
		}
		MCUCircular_PutData(&hdmiInPlayCt->hdmiPcmCircularBuf, hdmiInPlayCt->rSampleRateBuf, pcm_len*4);
#else
		MCUCircular_PutData(&hdmiInPlayCt->hdmiPcmCircularBuf, pcmBuf, pcm_len);//注意格式转换返回值是byte
#endif
	}
}


/***************************************************************************************
 *
 * APIs
 *
 */
bool HdmiInPlayCreate(MessageHandle parentMsgHandle)
{
	bool		ret = TRUE;

	ret = HdmiInPlay_Init(parentMsgHandle);
	if(ret)
	{
		hdmiInPlayCt->taskHandle = NULL;
		xTaskCreate(HdmiInPlayEntrance,
					"hdmiInPlay",
					HDMI_IN_PLAY_TASK_STACK_SIZE,
					NULL, HDMI_IN_PLAY_TASK_PRIO,
					&hdmiInPlayCt->taskHandle);
		if(hdmiInPlayCt->taskHandle == NULL)
		{
			ret = FALSE;
		}
	}
	if(!ret)
	{
		DBG("hdmi:app create fail!\n");
	}
	return ret;
}


bool HdmiInPlayStart(void)
{
	MessageContext		msgSend;
	if(hdmiInPlayCt == NULL)
	{
		return FALSE;
	}
	msgSend.msgId		= MSG_TASK_START;
	return MessageSend(hdmiInPlayCt->msgHandle, &msgSend);
}

bool HdmiInPlayPause(void)
{
	MessageContext		msgSend;
	if(hdmiInPlayCt == NULL)
	{
		return FALSE;
	}
	msgSend.msgId		= MSG_TASK_PAUSE;
	return MessageSend(hdmiInPlayCt->msgHandle, &msgSend);
}

bool HdmiInPlayResume(void)
{
	MessageContext		msgSend;
	if(hdmiInPlayCt == NULL)
	{
		return FALSE;
	}
	msgSend.msgId		= MSG_TASK_RESUME;
	return MessageSend(hdmiInPlayCt->msgHandle, &msgSend);
}

bool HdmiInPlayStop(void)
{
	MessageContext		msgSend;
	if(hdmiInPlayCt == NULL)
	{
		return FALSE;
	}
	AudioCoreSourceMute(APP_SOURCE_NUM, TRUE, TRUE);
#if (CFG_RES_MIC_SELECT) && defined(CFG_FUNC_AUDIO_EFFECT_EN)
	AudioCoreSourceMute(MIC_SOURCE_NUM, TRUE, TRUE);
#endif
	vTaskDelay(30);

	msgSend.msgId		= MSG_TASK_STOP;
	return MessageSend(hdmiInPlayCt->msgHandle, &msgSend);
}

bool HdmiInPlayKill(void)
{
	if(hdmiInPlayCt == NULL)
	{
		return FALSE;
	}

//	//Kill used services
#if	defined(CFG_FUNC_REMIND_SOUND_EN)
	AudioCoreSourceDisable(REMIND_SOURCE_NUM);
	DecoderServiceKill();
#endif

//	//注意：AudioCore父任务调整到mainApp下，此处只关闭AudioCore通道，不关闭任务
	AudioCoreProcessConfig(NULL);
	AudioCoreSourceDisable(HDMI_IN_SOURCE_NUM);
#ifdef CFG_FUNC_FREQ_ADJUST	
	AudioCoreSourceFreqAdjustDisable();
#endif
#ifndef CFG_FUNC_MIXER_SRC_EN
#ifdef CFG_RES_AUDIO_DACX_EN
	AudioDAC_SampleRateChange(ALL, CFG_PARA_SAMPLE_RATE);//恢复
#else
	AudioDAC_SampleRateChange(DAC0, CFG_PARA_SAMPLE_RATE);//恢复
#endif
#endif

	//task
	if(hdmiInPlayCt->taskHandle != NULL)
	{
		vTaskDelete(hdmiInPlayCt->taskHandle);
		hdmiInPlayCt->taskHandle = NULL;
	}

	//Msgbox
	if(hdmiInPlayCt->msgHandle != NULL)
	{
		MessageDeregister(hdmiInPlayCt->msgHandle);
		hdmiInPlayCt->msgHandle = NULL;
	}

	HDMI_CEC_DDC_DeInit();
	HDMI_ARC_DeInit();

	//PortFree
	//osPortFree(hdmiInPlayCt.AudioCoreHdmiIn);
	hdmiInPlayCt->AudioCoreHdmiIn = NULL;
	if(hdmiInPlayCt->hdmiARCFIFO != NULL)
	{
		osPortFree(hdmiInPlayCt->hdmiARCFIFO);
		hdmiInPlayCt->hdmiARCFIFO = NULL;
	}

	if(hdmiInPlayCt->sourceBuf_ARC != NULL)
	{
		osPortFree(hdmiInPlayCt->sourceBuf_ARC);
		hdmiInPlayCt->sourceBuf_ARC = NULL;
	}

	if(hdmiInPlayCt->hdmiARCCarry != NULL)
	{
		osPortFree(hdmiInPlayCt->hdmiARCCarry);
		hdmiInPlayCt->hdmiARCCarry = NULL;
	}

	if(hdmiInPlayCt->hdmiARCPcmFifo != NULL)
	{
		osPortFree(hdmiInPlayCt->hdmiARCPcmFifo);
		hdmiInPlayCt->hdmiARCPcmFifo = NULL;
	}

	//osPortFree(hdmiInPlayCt.sourceBuf_CECRx);
	//hdmiInPlayCt.sourceBuf_CECRx = NULL;

#if	defined(CFG_FUNC_REMIND_SOUND_EN)
	if(hdmiInPlayCt->Source2Decoder != NULL)
	{
		osPortFree(hdmiInPlayCt->Source2Decoder);
		hdmiInPlayCt->Source2Decoder = NULL;
	}
#endif
	
#ifdef CFG_FUNC_MIXER_SRC_EN
	if(hdmiInPlayCt->hdmiResamplerCt != NULL)
	{
		osPortFree(hdmiInPlayCt->hdmiResamplerCt);
		hdmiInPlayCt->hdmiResamplerCt = NULL;
	}
	if(hdmiInPlayCt->rSampleRateBuf != NULL)
	{
		osPortFree(hdmiInPlayCt->rSampleRateBuf);
		hdmiInPlayCt->rSampleRateBuf = NULL;
	}
#endif

	osPortFree(hdmiInPlayCt);
	hdmiInPlayCt = NULL;

#ifdef CFG_FUNC_AUDIO_EFFECT_EN
	AudioEffectsDeInit();
#endif
	return TRUE;
}

MessageHandle GetHdmiInMessageHandle(void)
{
	if(hdmiInPlayCt == NULL)
	{
		return NULL;
	}
	return hdmiInPlayCt->msgHandle;
}

#endif  //CFG_APP_HDMIIN_MODE_EN
