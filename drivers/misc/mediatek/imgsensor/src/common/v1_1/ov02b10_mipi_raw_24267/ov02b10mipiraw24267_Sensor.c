/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/types.h>
#include "ov02b10mipiraw24267_Sensor.h"

#define PFX "OV02B10_camera_sensor"
#define LOG_INF(format, args...)    pr_info(PFX "[%s] " format, __func__, ##args)

#define LEFT_SIZE (0x4000)
#define READ_EEPROM_1K (1024)
#define OV02B10_EEPROM_I2C_ADDR 0xA4
static kal_uint8 otp_data[LEFT_SIZE] = {0};
static BYTE ov02b10_common_data[CAMERA_EEPPROM_COMDATA_LENGTH] = { 0 };
static kal_uint8 deviceInfo_register_value = 0x00;

extern enum IMGSENSOR_RETURN Eeprom_DataInit(
            enum IMGSENSOR_SENSOR_IDX sensor_idx,
            kal_uint32 sensorID);

static kal_uint32 streaming_control(kal_bool enable);

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static imgsensor_info_struct imgsensor_info = {
    .sensor_id = OV02B10_SENSOR_ID_24267,
    .checksum_value = 0xb1893b4f, /*checksum value for Camera Auto Test*/
    .pre = {
        .pclk = 16500000,
        .linelength  = 448,
        .framelength = 1221,
        .startx = 0,
        .starty = 0,
        .grabwindow_width  = 1600,
        .grabwindow_height = 1200,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
        .mipi_pixel_rate = 66000000,
    },
    .cap = {
        .pclk = 16500000,
        .linelength  = 448,
        .framelength = 1221,
        .startx = 0,
        .starty = 0,
        .grabwindow_width  = 1600,
        .grabwindow_height = 1200,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
        .mipi_pixel_rate = 66000000,
    },
    .cap1 = { /*capture for 15fps*/
        .pclk = 16500000,
        .linelength  = 448,
        .framelength = 1221,
        .startx = 0,
        .starty = 0,
        .grabwindow_width  = 1600,
        .grabwindow_height = 1200,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
        .mipi_pixel_rate = 66000000,
    },
    .normal_video = { /* cap*/
        .pclk = 16500000,
        .linelength  = 448,
        .framelength = 1221,
        .startx = 0,
        .starty = 0,
        .grabwindow_width  = 1600,
        .grabwindow_height = 1200,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
        .mipi_pixel_rate = 66000000,
    },
    .hs_video = {
        .pclk = 16500000,
        .linelength  = 448,
        .framelength = 1221,
        .startx = 0,
        .starty = 0,
        .grabwindow_width  = 1600,
        .grabwindow_height = 1200,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
        .mipi_pixel_rate = 66000000,
    },
    .slim_video = {/*pre*/
        .pclk = 16500000,
        .linelength  = 448,
        .framelength = 1221,
        .startx = 0,
        .starty = 0,
        .grabwindow_width  = 1600,
        .grabwindow_height = 1200,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
        .mipi_pixel_rate = 66000000,
    },
    .custom1 = {
        .pclk = 16500000,
        .linelength  = 448,
        .framelength = 1536,
        .startx = 0,
        .starty = 0,
        .grabwindow_width  = 1600,
        .grabwindow_height = 1200,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 240,
        .mipi_pixel_rate = 66000000,
    },

    .margin = 7,  /*sensor framelength & shutter margin*/
    .min_shutter = 4,  /*min shutter*/
    .min_gain = 64, /*1x gain*/
    .max_gain = 960, /*64x gain*/
    .min_gain_iso = 100,
    .gain_step = 1,
    .gain_type = 1,/*to be modify,no gain table for sony*/

    /*max framelength by sensor register's limitation*/
    .max_frame_length = 0x7fff,
    .ae_shut_delay_frame = 0,
    .ae_sensor_gain_delay_frame = 0,
    .ae_ispGain_delay_frame = 2, /*isp gain delay frame for AE cycle*/
    .frame_time_delay_frame = 2,
    .ihdr_support = 0,      /*1, support; 0,not support*/
    .ihdr_le_firstline = 0,  /*1,le first ; 0, se first*/

    /*support sensor mode num ,don't support Slow motion*/
    .sensor_mode_num = 6,
    .cap_delay_frame = 3,        /*enter capture delay frame num*/
    .pre_delay_frame = 3,        /*enter preview delay frame num*/
    .video_delay_frame = 3,        /*enter video delay frame num*/
    .hs_video_delay_frame = 3, /*enter high speed video  delay frame num*/
    .slim_video_delay_frame = 3,/*enter slim video delay frame num*/
    .custom1_delay_frame = 3,
    .isp_driving_current = ISP_DRIVING_6MA, /*mclk driving current*/

    /*Sensor_interface_type*/
    .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,

    /*0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2*/
    .mipi_sensor_type = MIPI_OPHY_NCSI2,

    /*0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL*/
    .mipi_settle_delay_mode = 0, //0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL

    /*sensor output first pixel color*/
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,

    .mclk = 24,/*mclk value, suggest 24 or 26 for 24Mhz or 26Mhz*/
    .mipi_lane_num = SENSOR_MIPI_1_LANE,/*mipi lane num*/
	.i2c_addr_table = {0x7A, 0xff},
    .i2c_speed = 400,
};


static struct imgsensor_struct imgsensor = {
    .mirror = IMAGE_HV_MIRROR,
    .sensor_mode = IMGSENSOR_MODE_INIT,
    .shutter = 0x0410,            /*current shutter*/
    .gain = 0x40,                /*current gain*/
    .dummy_pixel = 0,            /*current dummypixel*/
    .dummy_line = 0,            /*current dummyline*/

    /*full size current fps : 24fps for PIP, 30fps for Normal or ZSD*/
    .current_fps = 300,
    .autoflicker_en = KAL_FALSE,
    .test_pattern = KAL_FALSE,

    /*current scenario id*/
    .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
    .ihdr_mode = 0, //sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x7A, /*record current sensor's i2c write id*/
};


/* Sensor output window information*/
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[10] = {
    {1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 0, 0, 1600, 1200}, // Preview
    {1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 0, 0, 1600, 1200}, // capture
    {1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 0, 0, 1600, 1200}, // video
    {1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 0, 0, 1600, 1200}, // hight video 120
    {1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 0, 0, 1600, 1200}, // slim video
    {1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 0, 0, 1600, 1200}, // custom1
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
    kal_uint16 get_byte = 0;
    char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

    iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
    return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
    char pu_send_cmd[2] = {(char)(addr & 0xFF), (char)(para & 0xFF)};

    iWriteRegI2C(pu_send_cmd, 2, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
    if (imgsensor.frame_length%2 != 0) {
        imgsensor.frame_length = imgsensor.frame_length - imgsensor.frame_length % 2;
    }

    LOG_INF("imgsensor.frame_length = %d\n", imgsensor.frame_length);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0x14, ((imgsensor.frame_length-1220) & 0x7F00) >> 8);
    write_cmos_sensor(0x15, (imgsensor.frame_length-1220) & 0xFF);
    write_cmos_sensor(0x25, imgsensor.line_length >> 8);
    write_cmos_sensor(0x26, imgsensor.line_length & 0xFF);
    write_cmos_sensor(0xfe, 0x02);
}    /*    set_dummy  */


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
    kal_uint32 frame_length = imgsensor.frame_length;

    LOG_INF("framerate = %d, min framelength should enable %d\n", framerate,
        min_framelength_en);

    frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
    spin_lock(&imgsensor_drv_lock);
    if (frame_length >= imgsensor.min_frame_length) {
        imgsensor.frame_length = frame_length;
    } else {
        imgsensor.frame_length = imgsensor.min_frame_length;
    }
    imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;

    if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
        imgsensor.frame_length = imgsensor_info.max_frame_length;
        imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
    }
    if (min_framelength_en) {
        imgsensor.min_frame_length = imgsensor.frame_length;
    }
    spin_unlock(&imgsensor_drv_lock);
    set_dummy();
}    /*    set_max_framerate  */

static void set_shutter_frame_length(
                kal_uint16 shutter, kal_uint16 frame_length)
{
    unsigned long flags;
    kal_uint16 realtime_fps = 0;
    kal_int32 dummy_line = 0;

    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
    spin_lock(&imgsensor_drv_lock);
    /*Change frame time*/
    if (frame_length > 1) {
        dummy_line = frame_length - imgsensor.frame_length;
    }
    imgsensor.frame_length = imgsensor.frame_length + dummy_line;

    if (shutter > imgsensor.frame_length - imgsensor_info.margin) {
        imgsensor.frame_length = shutter + imgsensor_info.margin;
    }
    if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    }
    spin_unlock(&imgsensor_drv_lock);

    shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;

    if (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) {
        shutter = (imgsensor_info.max_frame_length - imgsensor_info.margin);
    }

    //imgsensor.frame_length = imgsensor.frame_length - imgsensor.frame_length % 2;

    if (imgsensor.autoflicker_en) {
        realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;

        if (realtime_fps >= 297 && realtime_fps <= 305) {
            set_max_framerate(296, 0);
        } else if (realtime_fps >= 147 && realtime_fps <= 150) {
            set_max_framerate(146, 0);
        } else {
            /* Extend frame length*/
            write_cmos_sensor(0xfd, 0x01);
            write_cmos_sensor(0x14, ((imgsensor.frame_length-1220) & 0x7F00) >> 8);
            write_cmos_sensor(0x15, (imgsensor.frame_length-1220) & 0xFF);
            write_cmos_sensor(0xfe, 0x02);
        }
    } else {
        /* Extend frame length*/
        write_cmos_sensor(0xfd, 0x01);
        write_cmos_sensor(0x14, ((imgsensor.frame_length-1220) & 0x7F00) >> 8);
        write_cmos_sensor(0x15, (imgsensor.frame_length-1220) & 0xFF);
        write_cmos_sensor(0xfe, 0x02);
    }

    /* Update Shutter*/
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0x0f, (shutter) & 0xFF);
    write_cmos_sensor(0x0e, (shutter >> 8) & 0xFF);
    write_cmos_sensor(0xfe, 0x02);
    LOG_INF("Add for N3D! shutterlzl =%d, framelength =%d\n", shutter, imgsensor.frame_length);

}

/*************************************************************************
 * FUNCTION
 *    set_shutter
 *
 * DESCRIPTION
 *    This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *    iShutter : exposured lines
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
    unsigned long flags;
    kal_uint16 realtime_fps = 0;

    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

    spin_lock(&imgsensor_drv_lock);
    if (shutter > imgsensor.min_frame_length - imgsensor_info.margin) {
        imgsensor.frame_length = shutter + imgsensor_info.margin;
    } else {
        imgsensor.frame_length = imgsensor.min_frame_length;
    }
    if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    }
    spin_unlock(&imgsensor_drv_lock);

    shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;

    if (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) {
        shutter = (imgsensor_info.max_frame_length - imgsensor_info.margin);
    }
    //imgsensor.frame_length = imgsensor.frame_length - imgsensor.frame_length % 2;

    if (imgsensor.autoflicker_en) {
        realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;

        if (realtime_fps >= 297 && realtime_fps <= 305) {
            set_max_framerate(296, 0);
        } else if (realtime_fps >= 147 && realtime_fps <= 150) {
            set_max_framerate(146, 0);
        } else {
            /* Extend frame length*/
            write_cmos_sensor(0xfd, 0x01);
            write_cmos_sensor(0x14, (imgsensor.frame_length-1220) >> 8);
            write_cmos_sensor(0x15, (imgsensor.frame_length-1220) & 0xFF);
            write_cmos_sensor(0xfe, 0x02);
        }
    } else {
        /* Extend frame length*/
        write_cmos_sensor(0xfd, 0x01);
        write_cmos_sensor(0x14, (imgsensor.frame_length-1220) >> 8);
        write_cmos_sensor(0x15, (imgsensor.frame_length-1220) & 0xFF);
        write_cmos_sensor(0xfe, 0x02);
    }

    /* Update Shutter*/
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0x0f, (shutter) & 0xFF);
    write_cmos_sensor(0x0e, (shutter >> 8) & 0xFF);
    write_cmos_sensor(0xfe, 0x02);

    LOG_INF("shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);
}

/*************************************************************************
 * FUNCTION
 *    set_gain
 *
 * DESCRIPTION
 *    This function is to set global gain to sensor.
 *
 * PARAMETERS
 *    iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *    the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
    kal_uint16 reg_gain;
    //for gain64
    if (gain < 64) {
        gain = 64;
    } else if (gain > (15 * 64)) {
        gain = 15 * 64;
    }
    reg_gain = gain / 4;
    spin_lock(&imgsensor_drv_lock);
    imgsensor.gain = reg_gain;
    spin_unlock(&imgsensor_drv_lock);
    LOG_INF("gain = %d, reg_gain = 0x%x\n ", gain, reg_gain);

    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0x22, (reg_gain & 0xFF));
    write_cmos_sensor(0xfe, 0x02);
    return gain;
}    /*    set_gain  */

static void ihdr_write_shutter_gain(
            kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
    LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
    if (imgsensor.ihdr_mode) {

        spin_lock(&imgsensor_drv_lock);
        if (le > imgsensor.min_frame_length - imgsensor_info.margin) {
            imgsensor.frame_length = le + imgsensor_info.margin;
        } else {
            imgsensor.frame_length = imgsensor.min_frame_length;
        }
        if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
            imgsensor.frame_length = imgsensor_info.max_frame_length;
        }
        spin_unlock(&imgsensor_drv_lock);

        if (le < imgsensor_info.min_shutter) {
            le = imgsensor_info.min_shutter;
        }
        if (se < imgsensor_info.min_shutter) {
            se = imgsensor_info.min_shutter;
        }

        /* Extend frame length first*/
        write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
        write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
        write_cmos_sensor(0x3502, (le << 4) & 0xFF);
        write_cmos_sensor(0x3501, (le >> 4) & 0xFF);
        write_cmos_sensor(0x3500, (le >> 12) & 0x0F);
        write_cmos_sensor(0x3512, (se << 4) & 0xFF);
        write_cmos_sensor(0x3511, (se >> 4) & 0xFF);
        write_cmos_sensor(0x3510, (se >> 12) & 0x0F);
        set_gain(gain);
    }
}

static void set_mirror_flip(kal_uint8 image_mirror)
{
    LOG_INF("image_mirror = %d\n", image_mirror);

    switch(image_mirror) {
    case IMAGE_NORMAL:
        //.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
        write_cmos_sensor(0xfd, 0x01);
        write_cmos_sensor(0x12, 0x00);
        write_cmos_sensor(0x01, 0x01);
        break;

    case IMAGE_H_MIRROR:
        //.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_GB,
        write_cmos_sensor(0xfd, 0x01);
        write_cmos_sensor(0x12, 0x01);
        write_cmos_sensor(0x01, 0x01);
        break;

    case IMAGE_V_MIRROR:
        //.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_GR,
        write_cmos_sensor(0xfd, 0x01);
        write_cmos_sensor(0x12, 0x02);
        write_cmos_sensor(0x01, 0x01);
        break;

    case IMAGE_HV_MIRROR:
        //.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
        write_cmos_sensor(0xfd, 0x01);
        write_cmos_sensor(0x12, 0x03);
        write_cmos_sensor(0x01, 0x01);
        break;

    default:
        LOG_INF("Error image_mirror setting\n");
    }
}
#define I2C_BUFFER_LEN 225    /* trans# max is 255, each 3 bytes */
extern int iBurstWriteReg_multi(u8 *pData, u32 bytes, u16 i2cId, u16 transfer_length, u16 timing);

static kal_uint16 table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
    char puSendCmd[I2C_BUFFER_LEN];
    kal_uint32 tosend, IDX;
    kal_uint16 addr = 0, addr_last = 0, data;

    tosend = 0;
    IDX = 0;
    while (len > IDX) {
        addr = para[IDX];
        {
            puSendCmd[tosend++] = (char)addr;
            data = para[IDX + 1];
            puSendCmd[tosend++] = (char)data;
            IDX += 2;
            addr_last = addr;
        }

        /* Write when remain buffer size is less than 4 bytes or reach end of data */
        if ((I2C_BUFFER_LEN - tosend) < 2 || IDX == len || addr != addr_last) {
            iBurstWriteReg_multi(puSendCmd, tosend, imgsensor.i2c_write_id,
                                2, imgsensor_info.i2c_speed);
            tosend = 0;
        }
    }
    return 0;
}

static kal_uint16 addr_data_pair_global[] = {
    0xfc, 0x01,
    0xfd, 0x00,
    0xfd, 0x00,
    0x24, 0x02,
    0x25, 0x06,
    0x29, 0x03,
    0x2a, 0x34,
    0x1e, 0x17,
    0x33, 0x07,
    0x35, 0x07,
    0x4a, 0x0c,
    0x3a, 0x05,
    0x3b, 0x02,
    0x3e, 0x00,
    0x46, 0x01,
    0x6d, 0x03,
    0xfd, 0x01,
    0x0e, 0x02,
    0x0f, 0x1a,
    0x18, 0x00,
    0x22, 0xff,
    0x23, 0x02,
    0x17, 0x2c,
    0x19, 0x20,
    0x1b, 0x06,
    0x1c, 0x04,
    0x20, 0x03,
    0x30, 0x01,
    0x33, 0x01,
    0x31, 0x0a,
    0x32, 0x09,
    0x38, 0x01,
    0x39, 0x01,
    0x3a, 0x01,
    0x3b, 0x01,
    0x4f, 0x04,
    0x4e, 0x05,
    0x50, 0x01,
    0x35, 0x0c,
    0x45, 0x2a,
    0x46, 0x2a,
    0x47, 0x2a,
    0x48, 0x2a,
    0x4a, 0x2c,
    0x4b, 0x2c,
    0x4c, 0x2c,
    0x4d, 0x2c,
    0x56, 0x3a,
    0x57, 0x0a,
    0x58, 0x24,
    0x59, 0x20,
    0x5a, 0x0a,
    0x5b, 0xff,
    0x37, 0x0a,
    0x42, 0x0e,
    0x68, 0x90,
    0x69, 0xcd,
    0x6a, 0x8f,
    0x7c, 0x0a,
    0x7d, 0x0a,
    0x7e, 0x0a,
    0x7f, 0x08,
    0x83, 0x14,
    0x84, 0x14,
    0x86, 0x14,
    0x87, 0x07,
    0x88, 0x0f,
    0x94, 0x02,
    0x98, 0xd1,
    0xfe, 0x02,
    0xfd, 0x03,
    0x97, 0x6c,
    0x98, 0x60,
    0x99, 0x60,
    0x9a, 0x6c,
    0xa1, 0x40,
    0xaf, 0x04,
    0xb1, 0x40,
    0xae, 0x0d,
    0x88, 0x5b,
    0x89, 0x7c,
    0xb4, 0x05,
    0x8c, 0x40,
    0x8e, 0x40,
    0x90, 0x40,
    0x92, 0x40,
    0x9b, 0x46,
    0xac, 0x40,
    0xfd, 0x00,
    0x5a, 0x15,
    0x74, 0x01,
    0xfd, 0x00,
    0x50, 0x40,
    0x52, 0xb0,
    0xfd, 0x01,
    0x03, 0x70,
    0x05, 0x10,
    0x07, 0x20,
    0x09, 0xb0,
    0xfd, 0x03,
    0xc2, 0x01,
    0xfb, 0x01,
    0xfd, 0x03,
    0xc2, 0x00,
};
/*************************************************************************
 * FUNCTION
 *    sensor_init
 *
 * DESCRIPTION
 *    Sensor init
 *
 * PARAMETERS
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void sensor_init(void)
{
    LOG_INF("sensor_init Start\n");

    table_write_cmos_sensor(addr_data_pair_global,
        sizeof(addr_data_pair_global) / sizeof(kal_uint16));
    LOG_INF("sensor_init End\n");
}    /*    sensor_init  */

/*************************************************************************
 * FUNCTION
 *    preview_setting
 *
 * DESCRIPTION
 *    Sensor preview
 *
 * PARAMETERS
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void preview_setting(void)
{
    LOG_INF("preview_setting E!\n");
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0x0e, 0x02);
    write_cmos_sensor(0x0f, 0x1a);
    write_cmos_sensor(0x14, 0x00);
    write_cmos_sensor(0x15, 0x01);
    write_cmos_sensor(0xfe, 0x02);
    LOG_INF("preview_setting END!\n");
}    /*    preview_setting  */

static void custom1_setting(void)
{
    LOG_INF("custom1_setting E!\n");
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0x0e, 0x02);
    write_cmos_sensor(0x0f, 0x1a);
    write_cmos_sensor(0x14, 0x01);
    write_cmos_sensor(0x15, 0x3b);
    write_cmos_sensor(0xfe, 0x02);
}

static kal_uint32 return_sensor_id(void)
{
    write_cmos_sensor(0xfd, 0x00);
    return (read_cmos_sensor(0x0200) << 8) | read_cmos_sensor(0x0300);
}

static void read_cmos_eeprom_table(kal_uint16 addr, kal_uint8 *table, kal_uint32 size)
{
    char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
    iReadRegI2C(pusendcmd, 2, (u8 *)table, size, OV02B10_EEPROM_I2C_ADDR);
}

static kal_uint16 read_otp_info(kal_uint8 *data)
{
    kal_uint16 addr = 0x00;
    kal_uint16 left_size = 0;

    left_size = LEFT_SIZE;
    while (left_size > 0) {
        if (left_size >= READ_EEPROM_1K) {
            read_cmos_eeprom_table(addr,data, READ_EEPROM_1K);
            left_size = left_size - READ_EEPROM_1K;
        } else {
            read_cmos_eeprom_table(addr,data, left_size);
            break;
        }
        addr = addr + READ_EEPROM_1K;
        data = data + READ_EEPROM_1K;
    }
    return 0;
}

static kal_uint16 read_ov02b10_eeprom_module(kal_uint32 addr)
{
    kal_uint16 get_byte = 0;
    char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

    iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 1, OV02B10_EEPROM_I2C_ADDR);
    return get_byte;
}

static void read_ov02b10_module_data()
{
    // length   : 64
    // QR       : [8 ~ 26]
    // vcm      : [40 ~ 41]
    // lensid   : [44 ~ 45]
    // marcro   : [48 ~ 49]
    // inf      : [52 ~ 53]

    kal_uint16 idx = 0;
    kal_uint16 sn_length = 23;
    kal_uint32 sn_starAddr = 0xB0;
    kal_uint32 vcmAddr = 0x0A;
    kal_uint32 lensAddr = 0x08;

    memset(ov02b10_common_data, 0, sizeof(ov02b10_common_data));
    // QR
    for(idx = 0; idx < sn_length; idx++)
    {
       ov02b10_common_data[8 + idx] = read_ov02b10_eeprom_module(sn_starAddr + idx);
    }
    // Lens ID
    ov02b10_common_data[44] = read_ov02b10_eeprom_module(lensAddr);
    // Vcm ID
    ov02b10_common_data[40] = read_ov02b10_eeprom_module(vcmAddr);

    for (idx = 0; idx < CAMERA_EEPPROM_COMDATA_LENGTH; idx = idx + 4)
    {
        pr_info("ov02b10 common cam data : %02x %02x %02x %02x\n",
           ov02b10_common_data[idx],
           ov02b10_common_data[idx + 1],
           ov02b10_common_data[idx + 2],
           ov02b10_common_data[idx + 3]);
    }
}

/*************************************************************************
 * FUNCTION
 *    get_imgsensor_id
 *
 * DESCRIPTION
 *    This function get the sensor ID
 *
 * PARAMETERS
 *    *sensorID : return the sensor ID
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;

    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            *sensor_id = return_sensor_id();
            if (*sensor_id == OV02B10_SENSOR_ID) {
                *sensor_id = imgsensor_info.sensor_id;
                pr_info("[get_imgsensor_id] Ov02b10 Read sensor id Success, write id:0x%x, id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
                if (deviceInfo_register_value == 0x00) {
                    read_ov02b10_module_data();
                    deviceInfo_register_value = 0x01;
                }
                if (otp_data[0] == 0) {
                    read_otp_info(otp_data);
                } else {
                    pr_info("otp data has already read");
                }
                return ERROR_NONE;
            }
            pr_info("[get_imgsensor_id] Ov02b10 Read sensor id fail, write id:0x%x, id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
            retry--;
        } while (retry > 0);
        i++;
        retry = 2;
    }
    if (*sensor_id != imgsensor_info.sensor_id) {
        *sensor_id = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
    LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
    if (enable) {
        write_cmos_sensor(0xfd, 0x03);
        write_cmos_sensor(0xc2, 0x01);
    } else {
        write_cmos_sensor(0xfd, 0x03);
        write_cmos_sensor(0xc2, 0x00);
        mdelay(70);
    }
    return ERROR_NONE;
}
/*************************************************************************
 * FUNCTION
 *    open
 *
 * DESCRIPTION
 *    This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *    None
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 open(void)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
    kal_uint32 sensor_id = 0;

    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            sensor_id = return_sensor_id();
            if (sensor_id == OV02B10_SENSOR_ID) {
                sensor_id = imgsensor_info.sensor_id;
                pr_info("[Open] Ov02b10 Read sensor id Success, write id:0x%x, id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
                break;
            }
            pr_info("[Open] Ov02b10 Read sensor id fail, write id:0x%x, id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
            retry--;
        } while (retry > 0);
        i++;
        if (sensor_id == imgsensor_info.sensor_id)
            break;
        retry = 2;
    }
    if (imgsensor_info.sensor_id != sensor_id) {
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    /* initail sequence write in  */
    sensor_init();
    mdelay(10);
    spin_lock(&imgsensor_drv_lock);

    imgsensor.autoflicker_en = KAL_FALSE;
    imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
    imgsensor.shutter = 0x0400;
    imgsensor.gain = 0x100;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.dummy_pixel = 0;
    imgsensor.dummy_line = 0;
    imgsensor.ihdr_mode = 0;
    imgsensor.test_pattern = KAL_FALSE;
    imgsensor.current_fps = imgsensor_info.pre.max_framerate;
    spin_unlock(&imgsensor_drv_lock);

    return ERROR_NONE;
}    /*    open  */

/*************************************************************************
 * FUNCTION
 *    close
 *
 * DESCRIPTION
 *
 * PARAMETERS
 *    None
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 close(void)
{
    LOG_INF("E\n");
    streaming_control(KAL_FALSE);
    /*No Need to implement this function*/
    return ERROR_NONE;
}    /*    close  */


/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *    This function start the sensor preview.
 *
 * PARAMETERS
 *    *image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    /*imgsensor.video_mode = KAL_FALSE;*/
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    preview_setting();
    set_mirror_flip(imgsensor.mirror);
    mdelay(10);
    return ERROR_NONE;
}    /*    preview   */

/*************************************************************************
 * FUNCTION
 *    capture
 *
 * DESCRIPTION
 *    This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                 MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("capture E\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

    /*15fps*/
    if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
        imgsensor.pclk = imgsensor_info.cap1.pclk;
        imgsensor.line_length = imgsensor_info.cap1.linelength;
        imgsensor.frame_length = imgsensor_info.cap1.framelength;
        imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
    } else {

        if (imgsensor.current_fps != imgsensor_info.cap.max_framerate) {
            LOG_INF("Warning: current_fps %d fps is not support, so use cap1's setting: %d fps!\n", imgsensor.current_fps, imgsensor_info.cap1.max_framerate / 10);
        }
        imgsensor.pclk = imgsensor_info.cap.pclk;
        imgsensor.line_length = imgsensor_info.cap.linelength;
        imgsensor.frame_length = imgsensor_info.cap.framelength;
        imgsensor.min_frame_length = imgsensor_info.cap.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
    }
    spin_unlock(&imgsensor_drv_lock);

    preview_setting();

    mdelay(10);

    if (imgsensor.test_pattern == KAL_TRUE) {
        /*write_cmos_sensor(0x5002,0x00);*/
        write_cmos_sensor(0x5000, ((read_cmos_sensor(0x5000) & 0xBF) | 0x00));
    }
    set_mirror_flip(imgsensor.mirror);
    return ERROR_NONE;
}    /* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("normal_video E\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
    imgsensor.pclk = imgsensor_info.normal_video.pclk;
    imgsensor.line_length = imgsensor_info.normal_video.linelength;
    imgsensor.frame_length = imgsensor_info.normal_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
    /*imgsensor.current_fps = 300;*/
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    preview_setting();
    set_mirror_flip(imgsensor.mirror);
    mdelay(10);
    return ERROR_NONE;
}    /*    normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("hs_video E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
    imgsensor.pclk = imgsensor_info.hs_video.pclk;
    /*imgsensor.video_mode = KAL_TRUE;*/
    imgsensor.line_length = imgsensor_info.hs_video.linelength;
    imgsensor.frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    /*imgsensor.current_fps = 300;*/
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    preview_setting();
    set_mirror_flip(imgsensor.mirror);
    mdelay(10);

    return ERROR_NONE;
}    /*    hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                 MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("slim_video E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
    imgsensor.pclk = imgsensor_info.slim_video.pclk;
    /*imgsensor.video_mode = KAL_TRUE;*/
    imgsensor.line_length = imgsensor_info.slim_video.linelength;
    imgsensor.frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    /*imgsensor.current_fps = 300;*/
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    preview_setting();
    set_mirror_flip(imgsensor.mirror);
    mdelay(10);

    return ERROR_NONE;
}    /*    slim_video     */

static kal_uint32 Custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("Custom1 E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
    imgsensor.pclk = imgsensor_info.custom1.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.custom1.linelength;
    imgsensor.frame_length = imgsensor_info.custom1.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom1_setting();
    set_mirror_flip(imgsensor.mirror);
    mdelay(10);
    return ERROR_NONE;
}   /*  Custom1   */


static kal_uint32 get_resolution(
            MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
{
    LOG_INF("get_resolution E\n");
    sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
    sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

    sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
    sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

    sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
    sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;

    sensor_resolution->SensorHighSpeedVideoWidth = imgsensor_info.hs_video.grabwindow_width;
    sensor_resolution->SensorHighSpeedVideoHeight = imgsensor_info.hs_video.grabwindow_height;

    sensor_resolution->SensorSlimVideoWidth = imgsensor_info.slim_video.grabwindow_width;
    sensor_resolution->SensorSlimVideoHeight = imgsensor_info.slim_video.grabwindow_height;

    sensor_resolution->SensorCustom1Width  = imgsensor_info.custom1.grabwindow_width;
    sensor_resolution->SensorCustom1Height     = imgsensor_info.custom1.grabwindow_height;

    return ERROR_NONE;
}    /*    get_resolution    */


static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                MSDK_SENSOR_INFO_STRUCT *sensor_info,
                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("scenario_id = %d\n", scenario_id);

    sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;

    /* not use */
    sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;

    /* inverse with datasheet*/
    sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

    sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorInterruptDelayLines = 4; /* not use */
    sensor_info->SensorResetActiveHigh = FALSE; /* not use */
    sensor_info->SensorResetDelayCount = 5; /* not use */
    sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
    sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
    sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;

    sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

    sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
    sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
    sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
    sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
    sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;

    sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;

    sensor_info->SensorMasterClockSwitch = 0; /* not use */
    sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
    sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;

    sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
    sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
    sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame; /* The delay frame of setting frame length  */

    sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
    sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
    sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
    sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
    sensor_info->SensorClockFreq = imgsensor_info.mclk;
    sensor_info->SensorClockDividCount = 3; /* not use */
    sensor_info->SensorClockRisingCount = 0;
    sensor_info->SensorClockFallingCount = 2; /* not use */
    sensor_info->SensorPixelClockCount = 3; /* not use */
    sensor_info->SensorDataLatchCount = 2; /* not use */
    sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
    sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
    sensor_info->SensorWidthSampling = 0;  /* 0 is default 1x*/
    sensor_info->SensorHightSampling = 0;    /* 0 is default 1x */
    sensor_info->SensorPacketECCOrder = 1;

    switch (scenario_id) {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
        break;

    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.cap.mipi_data_lp2hs_settle_dc;
        break;

    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;
        break;

    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;
        break;

    case MSDK_SCENARIO_ID_SLIM_VIDEO:
        sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;
        break;

    case MSDK_SCENARIO_ID_CUSTOM1:
        sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
        break;

    default:
        sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
        break;
    }
    return ERROR_NONE;
}    /*    get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
            MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
            MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("scenario_id = %d\n", scenario_id);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.current_scenario_id = scenario_id;
    spin_unlock(&imgsensor_drv_lock);
    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            preview(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            capture(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            normal_video(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            hs_video(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            slim_video(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            Custom1(image_window, sensor_config_data);  // Custom1
            break;
        default:
            LOG_INF("Error ScenarioId setting");
            preview(image_window, sensor_config_data);
            return ERROR_INVALID_SCENARIO_ID;
    }
    return ERROR_NONE;
}    /* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
    LOG_INF("framerate = %d\n ", framerate);
    /* SetVideoMode Function should fix framerate*/
    if (framerate == 0) {
        /* Dynamic frame rate*/
        return ERROR_NONE;
    }
    spin_lock(&imgsensor_drv_lock);
    if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE)) {
        imgsensor.current_fps = 296;
    } else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE)) {
        imgsensor.current_fps = 146;
    } else {
        imgsensor.current_fps = framerate;
    }
    spin_unlock(&imgsensor_drv_lock);
    set_max_framerate(imgsensor.current_fps, 1);

    return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
    LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
    spin_lock(&imgsensor_drv_lock);
    if (enable) {/*enable auto flicker      */
        imgsensor.autoflicker_en = KAL_TRUE;
    } else {/*Cancel Auto flick*/
        imgsensor.autoflicker_en = KAL_FALSE;
    }
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
        enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
    kal_uint32 frame_length;

    LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);
    switch (scenario_id) {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;

        spin_lock(&imgsensor_drv_lock);
        if (frame_length > imgsensor_info.pre.framelength) {
            imgsensor.dummy_line = (frame_length - imgsensor_info.pre.framelength);
        } else {
            imgsensor.dummy_line = 0;
        }
        imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;

        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        break;

    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        if (framerate == 0) {
            return ERROR_NONE;
        }

        frame_length = imgsensor_info.normal_video.pclk / framerate * 10;

        frame_length /= imgsensor_info.normal_video.linelength;
        spin_lock(&imgsensor_drv_lock);

        if (frame_length > imgsensor_info.normal_video.framelength) {
            imgsensor.dummy_line = frame_length - imgsensor_info.normal_video.framelength;
        } else {
            imgsensor.dummy_line = 0;
        }

        imgsensor.frame_length =
        imgsensor_info.normal_video.framelength + imgsensor.dummy_line;

        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        break;

    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;

        spin_lock(&imgsensor_drv_lock);
        if (frame_length > imgsensor_info.cap.framelength) {
            imgsensor.dummy_line = (frame_length - imgsensor_info.cap.framelength);
        } else {
            imgsensor.dummy_line = 0;
        }
        imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;

        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        break;

    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        frame_length = imgsensor_info.hs_video.pclk / framerate * 10;

        frame_length /= imgsensor_info.hs_video.linelength;

        spin_lock(&imgsensor_drv_lock);
        if (frame_length > imgsensor_info.hs_video.framelength) {
            imgsensor.dummy_line =(frame_length - imgsensor_info.hs_video.framelength);
        } else {
            imgsensor.dummy_line = 0;
        }
        imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;

        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        break;

    case MSDK_SCENARIO_ID_SLIM_VIDEO:
        frame_length = imgsensor_info.slim_video.pclk / framerate * 10;

        frame_length /= imgsensor_info.slim_video.linelength;

        spin_lock(&imgsensor_drv_lock);

        if (frame_length > imgsensor_info.slim_video.framelength) {
            imgsensor.dummy_line = (frame_length - imgsensor_info.slim_video.framelength);
        } else {
            imgsensor.dummy_line = 0;
        }
        imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;

        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);

        break;
    case MSDK_SCENARIO_ID_CUSTOM1:
        frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength) ? (frame_length - imgsensor_info.custom1.framelength) : 0;
        if (imgsensor.dummy_line < 0) {
            imgsensor.dummy_line = 0;
        }
        imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter) {
            set_dummy();
        }
        break;

    default:  /*coding with  preview scenario by default*/
        frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;

        spin_lock(&imgsensor_drv_lock);
        if (frame_length > imgsensor_info.pre.framelength) {
            imgsensor.dummy_line = (frame_length - imgsensor_info.pre.framelength);
        } else {
            imgsensor.dummy_line = 0;
        }
        imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;

        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);

        LOG_INF("error scenario_id = %d, we use preview scenario\n", scenario_id);
        break;
    }
    return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
            enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
    LOG_INF("scenario_id = %d\n", scenario_id);
    switch (scenario_id) {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        *framerate = imgsensor_info.pre.max_framerate;
        break;
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        *framerate = imgsensor_info.normal_video.max_framerate;
        break;
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        *framerate = imgsensor_info.cap.max_framerate;
        break;
    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        *framerate = imgsensor_info.hs_video.max_framerate;
        break;
    case MSDK_SCENARIO_ID_SLIM_VIDEO:
        *framerate = imgsensor_info.slim_video.max_framerate;
        break;
    case MSDK_SCENARIO_ID_CUSTOM1:
        *framerate = imgsensor_info.custom1.max_framerate;
        break;
    default:
        break;
    }
    return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
    LOG_INF("enable: %d\n", enable);

    if (enable) {
        write_cmos_sensor(0xfd, 0x03);
        write_cmos_sensor(0x8c, 0x00);
        write_cmos_sensor(0x8e, 0x00);
        write_cmos_sensor(0x90, 0x00);
        write_cmos_sensor(0x92, 0x00);
        write_cmos_sensor(0x9b, 0x00);
        write_cmos_sensor(0xfe, 0x02);
    } else {
        write_cmos_sensor(0xfd, 0x03);
        write_cmos_sensor(0x8c, 0x40);
        write_cmos_sensor(0x8e, 0x40);
        write_cmos_sensor(0x90, 0x40);
        write_cmos_sensor(0x92, 0x40);
        write_cmos_sensor(0x9b, 0x46);
        write_cmos_sensor(0xfe, 0x02);
    }
    spin_lock(&imgsensor_drv_lock);
    imgsensor.test_pattern = enable;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
    UINT8 *feature_para, UINT32 *feature_para_len)
{
    UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
    UINT16 *feature_data_16 = (UINT16 *) feature_para;
    UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
    UINT32 *feature_data_32 = (UINT32 *) feature_para;
    unsigned long long *feature_data = (unsigned long long *) feature_para;
    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

    MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
                (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

    LOG_INF("feature_id = %d\n", feature_id);
    switch (feature_id) {
    case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
        *(feature_data + 1) = imgsensor_info.min_gain;
        *(feature_data + 2) = imgsensor_info.max_gain;
        break;
    case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
        *(feature_data + 0) = imgsensor_info.min_gain_iso;
        *(feature_data + 1) = imgsensor_info.gain_step;
        *(feature_data + 2) = imgsensor_info.gain_type;
        break;
    case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
    *(feature_data + 1) = imgsensor_info.min_shutter;
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        case MSDK_SCENARIO_ID_CUSTOM1:
            *(feature_data + 2) = 2;
            break;
        default:
            *(feature_data + 2) = 1;
            break;
        }
        break;
    case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
        *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
        break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.cap.pclk;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.normal_video.pclk;
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.hs_video.pclk;
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.slim_video.pclk;
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom1.pclk;
            break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.pre.pclk;
            break;
        }
        break;
    case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.cap.framelength << 16)
                + imgsensor_info.cap.linelength;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.normal_video.framelength << 16)
                + imgsensor_info.normal_video.linelength;
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.hs_video.framelength << 16)
                + imgsensor_info.hs_video.linelength;
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.slim_video.framelength << 16)
                + imgsensor_info.slim_video.linelength;
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.custom1.framelength << 16)
                + imgsensor_info.custom1.linelength;
            break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.pre.framelength << 16)
                + imgsensor_info.pre.linelength;
            break;
        }
        break;
    case SENSOR_FEATURE_GET_EEPROM_COMDATA:
        memcpy(feature_return_para_32, ov02b10_common_data, CAMERA_EEPPROM_COMDATA_LENGTH);
        *feature_para_len = CAMERA_EEPPROM_COMDATA_LENGTH;
        break;
    case SENSOR_FEATURE_GET_PERIOD:
        *feature_return_para_16++ = imgsensor.line_length;
        *feature_return_para_16 = imgsensor.frame_length;
        *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
        *feature_return_para_32 = imgsensor.pclk;
        *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_SET_ESHUTTER:
        set_shutter(*feature_data);
        break;
    case SENSOR_FEATURE_SET_NIGHTMODE:
        break;
    case SENSOR_FEATURE_SET_GAIN:
        set_gain((UINT16) *feature_data);
        break;
    case SENSOR_FEATURE_SET_FLASHLIGHT:
        break;
    case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
        break;

    case SENSOR_FEATURE_SET_REGISTER:
        write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
        break;

    case SENSOR_FEATURE_GET_REGISTER:
        sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
        break;

    case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
        *feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
        *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_SET_VIDEO_MODE:
        set_video_mode(*feature_data);
        break;
    case SENSOR_FEATURE_CHECK_SENSOR_ID:
        get_imgsensor_id(feature_return_para_32);
        break;
    case SENSOR_FEATURE_GET_SENSOR_OTP_ALL:
        memcpy(feature_return_para_32, (UINT32 *)otp_data, sizeof(otp_data));
        break;
    case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
        set_auto_flicker_mode((BOOL)*feature_data_16, *(feature_data_16 + 1));
        break;

    case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
        set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data + 1));
        break;

    case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
        get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data),(MUINT32 *)(uintptr_t)(*(feature_data + 1)));
        break;
    case SENSOR_FEATURE_SET_TEST_PATTERN:
        set_test_pattern_mode((BOOL)*feature_data);
        break;
    case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
        /*
         * 1, if driver support new sw frame sync
         * set_shutter_frame_length() support third para auto_extend_en
         */
        *(feature_data + 1) = 1; /* margin info by scenario */
        *(feature_data + 2) = imgsensor_info.margin;
        break;
    /*for factory mode auto testing*/
    case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
        *feature_return_para_32 = imgsensor_info.checksum_value;
        *feature_para_len = 4;
        break;

    case SENSOR_FEATURE_SET_FRAMERATE:
        LOG_INF("current fps :%d\n", *feature_data_32);
        spin_lock(&imgsensor_drv_lock);
        imgsensor.current_fps = (UINT16)*feature_data_32;
        spin_unlock(&imgsensor_drv_lock);
        break;
    case SENSOR_FEATURE_SET_HDR:
        LOG_INF("ihdr enable :%d\n", *feature_data_32);
        spin_lock(&imgsensor_drv_lock);
        imgsensor.ihdr_mode = (UINT8)*feature_data_32;
        spin_unlock(&imgsensor_drv_lock);
        break;
    case SENSOR_FEATURE_GET_CROP_INFO:
        LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32)*feature_data);

        wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data + 1));

        switch (*feature_data_32) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            LOG_INF("lch MSDK_SCENARIO_ID_CUSTOM1 \n");
            memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[5], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        }
        break;

    case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
        LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n", (UINT16)*feature_data, (UINT16)*(feature_data + 1), (UINT16)*(feature_data + 2));

        ihdr_write_shutter_gain((UINT16)*feature_data, (UINT16)*(feature_data+1), (UINT16)*(feature_data + 2));
        break;

    case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
        set_shutter_frame_length((UINT16) (*feature_data), (UINT16) (*(feature_data + 1)));
        break;

    case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
        LOG_INF("This sensor can get temperature 20\n");
        *feature_return_para_32 = 20;
        *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
        LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
        streaming_control(KAL_FALSE);
        break;
    case SENSOR_FEATURE_SET_STREAMING_RESUME:
        LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n", *feature_data);

        if (*feature_data != 0) {
            set_shutter(*feature_data);
        }
        streaming_control(KAL_TRUE);
        break;
    case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
        {
            kal_uint32 rate;
            switch (*feature_data) {
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                    rate = imgsensor_info.cap.mipi_pixel_rate;
                    break;
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                    rate = imgsensor_info.normal_video.mipi_pixel_rate;
                    break;
                case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                    rate = imgsensor_info.hs_video.mipi_pixel_rate;
                    break;
                case MSDK_SCENARIO_ID_CUSTOM1:
                    rate = imgsensor_info.custom1.mipi_pixel_rate;
                    break;
                case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                default:
                    rate = imgsensor_info.pre.mipi_pixel_rate;
                    break;
            }
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
        }
        break;
    default:
        break;
    }

    return ERROR_NONE;
}    /*    feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
    open,
    get_info,
    get_resolution,
    feature_control,
    control,
    close
};

UINT32 OV02B10_MIPI_RAW_24267_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc != NULL) {
        *pfFunc = &sensor_func;
    }
    return ERROR_NONE;
}    /*    OV02B10_MIPI_RAW_SensorInit    */
