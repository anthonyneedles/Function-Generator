/*
 * Wave.c
 *  Source File for the Wave generator module.
 *
 *
 *  Created on: Feb 9, 2018
 *      Author: Mesecar
 */


#include "MCUType.h"
#include "app_cfg.h"
#include "os.h"
#include "Wave.h"
#include "DMA.h"
#include "K65TWR_GPIO.h"

#define CONVERTION_FACTOR 2426U


static OS_TCB ProcessTaskTCB;
static CPU_STK ProcessTaskStk[APP_CFG_PROCESS_TASK_STK_SIZE];
// uCOS stuff
static OS_MUTEX WaveMutexKey;
static WAVE_W CurrentSignal; // The Current Signal to be Produced


static void ProcessTask(void *p_arg);
static q31_t FreqToQ31(INT16U freq, INT16U step);

static void ProcessTask(void *p_arg);
/*
 * WaveInit()
 * Public Function
 *
 * Initializes the Wave module:
 *
 * ~Rod Mesecar, 2/9/18
 */
void WaveInit(void){
    OS_ERR os_err;

    OSMutexCreate(&WaveMutexKey, "Wave Mutex Key", &os_err);

    OSTaskCreate(&ProcessTaskTCB,                    //Create UITask
                 "Process Task",
                 ProcessTask,
                 (void *) 0,
                 APP_CFG_PROCESS_TASK_PRIO,
                 &ProcessTaskStk[0],
                 (APP_CFG_PROCESS_TASK_STK_SIZE / 10u),
                 APP_CFG_PROCESS_TASK_STK_SIZE,
                 0,
                 0,
                 (void *) 0,
                 (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 &os_err);
        while(os_err != OS_ERR_NONE){}

    // Default Values
    CurrentSignal.amp = 20;
    CurrentSignal.freq= 100;
    CurrentSignal.waveshape = SIN;

}

/*
 * WaveGet()
 * Public Function
 *
 * When Called, pends on WaveMutexKey and copies the contents of
 * CurrentSignal to the address of the passed pointer.
 *
 * ~Rod Mesecar, 2/9/18
 */
void WaveGet(WAVE_W *passWave){
    OS_ERR os_err;
    OSMutexPend(&WaveMutexKey, 0, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, &os_err);
    *passWave = CurrentSignal;
    OSMutexPost(&WaveMutexKey, OS_OPT_POST_NONE, &os_err);
}


/*
 * WaveSet()
 * Public Function
 *
 * When Called, pends on WaveMutexKey and copies the contents at
 * the address of the passed pointer to CurrentSignal
 *
 * ~Rod Mesecar, 2/9/18
 */
void WaveSet(WAVE_W *passWave){
    OS_ERR os_err;
    OSMutexPend(&WaveMutexKey, 0, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, &os_err);
    CurrentSignal = *passWave;
    OSMutexPost(&WaveMutexKey, OS_OPT_POST_NONE, &os_err);
}

static void ProcessTask(void *p_arg){
    (void)p_arg;
    OS_ERR os_err;
    INT16U sample_index;
    INT8U block_index;
    INT16U ramp_max;
    INT16U ramp_min;
    INT32U ramp_slope_q15;
    INT32U ramp_sample_period_q15;
    INT32U sin_sample_period_q15;
    INT16U wave_amp = 0;
    INT16U wave_freq = 0;
    INT32U sample_counter = 0;
    INT8U wave_shape;
    q31_t sin_input_q31;
    q31_t sin_sample;

    while(1){
        DB0_TURN_OFF();
        block_index = DMABlockDonePend(&os_err);
        DB0_TURN_ON();

        OSMutexPend(&WaveMutexKey, 0, OS_OPT_PEND_BLOCKING, (CPU_TS *)0, &os_err);
        while(os_err != OS_ERR_NONE){}
        wave_amp = CurrentSignal.amp;
        wave_freq = CurrentSignal.freq;
        wave_shape = CurrentSignal.waveshape;
        OSMutexPost(&WaveMutexKey, OS_OPT_POST_NONE, &os_err);
        while(os_err != OS_ERR_NONE){}
        sample_index = 0;
        switch(wave_shape){
            case TRI:
                ramp_max = 2048 + ((1707*wave_amp)/20);
                ramp_min = 2048 - ((1707*wave_amp)/20);
                ramp_sample_period_q15 = ((48000<<15)/wave_freq);
                ramp_slope_q15 = ((3414*wave_amp)/20);
                ramp_slope_q15 = (ramp_slope_q15<<15)/((ramp_sample_period_q15>>15)/2);

                 while(sample_index < 64){
                    if((sample_counter<<15) >= ramp_sample_period_q15){
                        sample_counter = 1;
                    }else{}

                    if((sample_counter<<15) < (ramp_sample_period_q15/2)){
                        wavCurSamples[block_index][sample_index] = ramp_min + ((sample_counter*ramp_slope_q15>>15));
                    }else if((sample_counter<<15) < (ramp_sample_period_q15)){
                        wavCurSamples[block_index][sample_index] = ramp_max - (((sample_counter-((ramp_sample_period_q15/2)>>15))*(ramp_slope_q15)>>15));
                    }else{}

                    sample_index++;
                    sample_counter++;
                }
                break;
            case SIN:
                while(sample_index < 64){
                    sin_sample_period_q15 = ((48000<<15)/wave_freq);

                    if((sample_counter<<15) >= sin_sample_period_q15){
                        sample_counter = 0;
                    }else{}

                    sin_input_q31 = FreqToQ31(wave_freq, sample_counter);
                    sin_sample = arm_sin_q31(sin_input_q31);
                    sin_sample = 2048 + (((sin_sample>>10)*1707*wave_amp)/20);
                    wavCurSamples[block_index][sample_index] = sin_sample;
                    sample_index++;
                    sample_counter++;
                }
                break;

            default:
                break;
        }
    }
}
/*
 * FreqtoQ31()
 *
 * Converts the passsed frequency and step into a q31_t
 * and returns. Should be used with arm_sin_q31
 *
 *~Rod Mesecar 2/16/18
 */
static q31_t FreqToQ31(INT16U freq, INT16U step){
	q31_t result = (q31_t)(CONVERTION_FACTOR * freq* step);
	return result;
}
