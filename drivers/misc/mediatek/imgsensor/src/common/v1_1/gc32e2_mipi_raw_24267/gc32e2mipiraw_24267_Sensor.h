/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *     gc32e2mipiraw_24267_Sensor.h
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     CMOS sensor header file
 *
 ****************************************************************************/
#ifndef _GC32E2MIPI_SENSOR_H
#define _GC32E2MIPI_SENSOR_H

#define MULTI_WRITE             1
#if MULTI_WRITE
#define I2C_BUFFER_LEN  765 /* Max is 255, each 3 bytes */
#else
#define I2C_BUFFER_LEN  3
#endif



#define GC32E2_MIRROR_NORMAL    1
#define GC32E2_MIRROR_HORIZON   0
#define GC32E2_MIRROR_VERTICAL  0
#define GC32E2_MIRROR_HV        0

#if GC32E2_MIRROR_NORMAL
#define GC32E2_MIRROR       0x00

#elif GC32E2_MIRROR_HORIZON
#define GC32E2_MIRROR       0x01

#elif GC32E2_MIRROR_VERTICAL
#define GC32E2_MIRROR       0x02

#elif GC32E2_MIRROR_HV
#define GC32E2_MIRROR       0x03

#else
#define GC32E2_MIRROR       0x00

#endif

enum IMGSENSOR_MODE {
    IMGSENSOR_MODE_INIT,
    IMGSENSOR_MODE_PREVIEW,
    IMGSENSOR_MODE_CAPTURE,
    IMGSENSOR_MODE_VIDEO,
    IMGSENSOR_MODE_HIGH_SPEED_VIDEO,
    IMGSENSOR_MODE_SLIM_VIDEO,
    IMGSENSOR_MODE_CUSTOM1,
    IMGSENSOR_MODE_CUSTOM2,
    IMGSENSOR_MODE_CUSTOM3,
    IMGSENSOR_MODE_CUSTOM4,
    IMGSENSOR_MODE_CUSTOM5,
};

struct imgsensor_mode_struct {
    kal_uint32 pclk; /* record different mode's pclk */
    kal_uint32 linelength; /* record different mode's linelength */
    kal_uint32 framelength; /* record different mode's framelength */

    kal_uint8 startx; /* record different mode's startx of grabwindow */
    kal_uint8 starty; /* record different mode's startx of grabwindow */

    kal_uint16 grabwindow_width;
    kal_uint16 grabwindow_height;

/* for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario*/
    kal_uint8 mipi_data_lp2hs_settle_dc;

 /*     following for GetDefaultFramerateByScenario()    */
    kal_uint16 max_framerate;
    kal_uint32 mipi_pixel_rate;
};

/* SENSOR PRIVATE STRUCT FOR VARIABLES*/
struct imgsensor_struct {
    kal_uint8 mirror; /* mirrorflip information */

    kal_uint8 sensor_mode; /* record IMGSENSOR_MODE enum value */

    kal_uint32 shutter; /* current shutter */
    kal_uint16 gain; /* current gain */

    kal_uint32 pclk; /* current pclk */

    kal_uint32 frame_length; /* current framelength */
    kal_uint32 line_length; /* current linelength */

    kal_uint32 min_frame_length;
    kal_uint16 dummy_pixel; /* current dummypixel */
    kal_uint16 dummy_line; /* current dummline */

    kal_uint16 current_fps; /* current max fps */
    kal_bool autoflicker_en; /* record autoflicker enable or disable */
    kal_bool test_pattern; /* record test pattern mode or not */
    enum MSDK_SCENARIO_ID_ENUM current_scenario_id;
    /*kal_bool ihdr_en;  ihdr enable or disable */
    kal_uint8 ihdr_mode; /* ihdr enable or disable */
    kal_uint8 pdaf_mode; /* pdaf enable or disable */
    kal_uint8 i2c_write_id; /* record current sensor's i2c write id */
};

/* SENSOR PRIVATE STRUCT FOR CONSTANT*/
struct imgsensor_info_struct {
    kal_uint32 sensor_id; /* record sensor id defined in Kd_imgsensor.h */
    kal_uint16 module_id;
    kal_uint32 checksum_value; /* checksum value for Camera Auto Test */
    struct imgsensor_mode_struct pre;
    struct imgsensor_mode_struct cap;
    struct imgsensor_mode_struct normal_video;
    struct imgsensor_mode_struct hs_video;
    struct imgsensor_mode_struct slim_video;
    struct imgsensor_mode_struct custom1;
    struct imgsensor_mode_struct custom2;
    struct imgsensor_mode_struct custom3;
    struct imgsensor_mode_struct custom4;
    struct imgsensor_mode_struct custom5;

    kal_uint8 ae_shut_delay_frame; /* shutter delay frame for AE cycle */
    kal_uint8 ae_sensor_gain_delay_frame;
    kal_uint8 ae_ispGain_delay_frame;
    kal_uint8 ihdr_support; /* 1, support; 0,not support */
    kal_uint8 ihdr_le_firstline; /* 1,le first ; 0, se first */
    kal_uint8 temperature_support;  /* 1, support; 0,not support */
    kal_uint8 sensor_mode_num; /* support sensor mode num */

    kal_uint8 cap_delay_frame; /* enter capture delay frame num */
    kal_uint8 pre_delay_frame; /* enter preview delay frame num */
    kal_uint8 video_delay_frame; /* enter video delay frame num */
    kal_uint8 hs_video_delay_frame;
    kal_uint8 slim_video_delay_frame; /* enter slim video delay frame num */
    kal_uint8 custom1_delay_frame;
    kal_uint8 custom2_delay_frame;
    kal_uint8 custom3_delay_frame;
    kal_uint8 custom4_delay_frame;
    kal_uint8 custom5_delay_frame;
    kal_uint8 frame_time_delay_frame;
    kal_uint8 margin; /* sensor framelength & shutter margin */
    kal_uint32 min_shutter; /* min shutter */
    kal_uint32 max_frame_length;
    kal_uint32 min_gain;
    kal_uint32 max_gain;
    kal_uint32 min_gain_iso;
    kal_uint32 gain_step;
    kal_uint32 exp_step;
    kal_uint32 gain_type;
    kal_uint8 isp_driving_current; /* mclk driving current */
    kal_uint8 sensor_interface_type; /* sensor_interface_type */
    kal_uint8 mipi_sensor_type;
    /* 0,MIPI_OPHY_NCSI2; 1,MIPI_OPHY_CSI2, default is NCSI2,
     * don't modify this para
     */
    kal_uint8 mipi_settle_delay_mode;
    /* 0, high speed signal auto detect;
     * 1, use settle delay,unit is ns,
     * default is auto detect, don't modify this para
     */
    kal_uint8 sensor_output_dataformat;
    kal_uint8 mclk;  /* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
    kal_uint32 i2c_speed; /* i2c speed */
    kal_uint8 mipi_lane_num; /* mipi lane num */
    kal_uint8 i2c_addr_table[5];
};

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData,
    u8 *a_pRecvData, u16 a_sizeRecvData,
               u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);

/*extern int iReadReg(u16 a_u2Addr, u8 *a_puBuff, u16 i2cId);*/
/*extern int iWriteReg(u16 a_u2Addr, u32 a_u4Data, u32 a_u4Bytes, u16 i2cId);*/
extern void kdSetI2CSpeed(u16 i2cSpeed);
extern int iBurstWriteReg_multi(u8 *pData, u32 bytes, u16 i2cId,
                u16 transfer_length, u16 timing);
extern int iWriteRegI2CTiming(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId, u16 timing);

extern void gc32e2_copy_crosstalk_data(kal_uint8 *ct_data);
extern void gc32e2_otp_identify(void);

#endif
