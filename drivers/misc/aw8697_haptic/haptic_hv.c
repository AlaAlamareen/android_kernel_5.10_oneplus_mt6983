/*
 * File: haptic_hv.c
 *
 * Author: Ethan <chelvming@awinic.com>
 *
 * Copyright (c) 2021 AWINIC Technology CO., LTD
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/power_supply.h>
#include <linux/vmalloc.h>
#include <linux/pm_qos.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <linux/mman.h>
#include <linux/proc_fs.h>
#ifdef OPLUS_FEATURE_CHG_BASIC
#include <soc/oplus/system/oplus_project.h>
#endif

#include "haptic_hv.h"
#include "haptic_hv_reg.h"
#include "haptic_hv_rtp_key_data.h"

#ifdef CONFIG_HAPTIC_FEEDBACK_MODULE
#include "haptic_feedback.h"
#endif

#define HAPTIC_HV_DRIVER_VERSION	"v0.0.0.13"
static uint8_t AW86927_HAPTIC_HIGH_LEVEL_REG_VAL = 0x5E; /* max boost 9.408V */

#define CPU_LATENCY_QOC_VALUE (0)
static struct pm_qos_request pm_qos_req;

struct aw_haptic_container *aw_rtp;
struct aw_haptic *g_aw_haptic;
static int rtp_osc_cali(struct aw_haptic *);
static void rtp_trim_lra_cali(struct aw_haptic *);
int aw_container_size = AW_CONTAINER_DEFAULT_SIZE;

static int rtp_regroup_work(struct aw_haptic *aw_haptic);

static struct aw_vmax_map vmax_map[] = {
	{800,  0x28, 0x40},//6.0V
	{900,  0x28, 0x49},
	{1000, 0x28, 0x51},
	{1100, 0x28, 0x5A},
	{1200, 0x28, 0x62},
	{1300, 0x28, 0x6B},
	{1400, 0x28, 0x73},
	{1500, 0x28, 0x7C},
	{1600, 0x2A, 0x80},//6.142
	{1700, 0x31, 0x80},//6.568
	{1800, 0x38, 0x80},//6.994
	{1900, 0x3F, 0x80},//7.42
	{2000, 0x46, 0x80},//7.846
	{2100, 0x4C, 0x80},//8.272
	{2200, 0x51, 0x80},//8.556
	{2300, 0x58, 0x80},//8.982
	{2400, 0x5E, 0x80},//9.408
};

static char aw_ram_name[5][30] = {
	{"aw8697_haptic_170.bin"},
	{"aw8697_haptic_170.bin"},
	{"aw8697_haptic_170.bin"},
	{"aw8697_haptic_170.bin"},
	{"aw8697_haptic_170.bin"},
};

static char aw_ram_name_170_soft[5][30] ={
	{"aw8697_haptic_170_soft.bin"},
	{"aw8697_haptic_170_soft.bin"},
	{"aw8697_haptic_170_soft.bin"},
	{"aw8697_haptic_170_soft.bin"},
	{"aw8697_haptic_170_soft.bin"},
};


static char aw_ram_name_150[5][30] = {
	{"aw8697_haptic_150.bin"},
	{"aw8697_haptic_150.bin"},
	{"aw8697_haptic_150.bin"},
	{"aw8697_haptic_150.bin"},
	{"aw8697_haptic_150.bin"},
};

static char aw_ram_name_150_soft[5][30] = {
	{"aw8697_haptic_150_soft.bin"},
	{"aw8697_haptic_150_soft.bin"},
	{"aw8697_haptic_150_soft.bin"},
	{"aw8697_haptic_150_soft.bin"},
	{"aw8697_haptic_150_soft.bin"},
};


static char aw_long_sound_rtp_name[5][30] = {
	{"aw8697_long_sound_168.bin"},
	{"aw8697_long_sound_170.bin"},
	{"aw8697_long_sound_173.bin"},
	{"aw8697_long_sound_175.bin"},
};

static char aw_old_steady_test_rtp_name_0815[11][60] = {
	{"aw8697_old_steady_test_RTP_52_160Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_162Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_164Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_166Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_168Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_170Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_172Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_174Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_176Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_178Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_180Hz.bin"},
};

static char aw_old_steady_test_rtp_name_081538[11][60] = {
	{"aw8697_old_steady_test_RTP_52_140Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_142Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_144Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_146Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_148Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_150Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_152Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_154Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_156Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_158Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_160Hz.bin"},
};

static char aw_high_temp_high_humidity_0815[11][60] = {
	{"aw8697_high_temp_high_humidity_channel_RTP_51_160Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_162Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_164Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_166Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_168Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_170Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_172Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_174Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_176Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_178Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_180Hz.bin"},
};

static char aw_high_temp_high_humidity_081538[11][60] = {
	{"aw8697_high_temp_high_humidity_channel_RTP_51_140Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_142Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_144Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_146Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_148Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_150Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_152Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_154Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_156Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_158Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_160Hz.bin"},
};

static char aw_old_steady_test_rtp_name_0832[11][60] = {
	{"aw8697_old_steady_test_RTP_52_225Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_226Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_227Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_228Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_229Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_230Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_231Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_232Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_233Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_234Hz.bin"},
	{"aw8697_old_steady_test_RTP_52_235Hz.bin"},
};

static char aw_high_temp_high_humidity_0832[11][60] = {
	{"aw8697_high_temp_high_humidity_channel_RTP_51_225Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_226Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_227Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_228Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_229Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_230Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_231Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_232Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_233Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_234Hz.bin"},
	{"aw8697_high_temp_high_humidity_channel_RTP_51_235Hz.bin"},
};

static char aw_ringtone_rtp_f0_170_name[][AW_RTP_NAME_MAX] = {
	{"aw8697_rtp.bin"},
	{"aw8697_Hearty_channel_RTP_1_170.bin"},
	{"aw8697_Instant_channel_RTP_2_170.bin"},
	{"aw8697_Music_channel_RTP_3_170.bin"},
	{"aw8697_Percussion_channel_RTP_4_170.bin"},
	{"aw8697_Ripple_channel_RTP_5_170.bin"},
	{"aw8697_Bright_channel_RTP_6_170.bin"},
	{"aw8697_Fun_channel_RTP_7_170.bin"},
	{"aw8697_Glittering_channel_RTP_8_170.bin"},
	{"aw8697_Granules_channel_RTP_9_170.bin"},
	{"aw8697_Harp_channel_RTP_10_170.bin"},
	{"aw8697_Impression_channel_RTP_11_170.bin"},
	{"aw8697_Ingenious_channel_RTP_12_170.bin"},
	{"aw8697_Joy_channel_RTP_13_170.bin"},
	{"aw8697_Overtone_channel_RTP_14_170.bin"},
	{"aw8697_Receive_channel_RTP_15_170.bin"},
	{"aw8697_Splash_channel_RTP_16_170.bin"},

	{"aw8697_About_School_RTP_17_170.bin"},
	{"aw8697_Bliss_RTP_18_170.bin"},
	{"aw8697_Childhood_RTP_19_170.bin"},
	{"aw8697_Commuting_RTP_20_170.bin"},
	{"aw8697_Dream_RTP_21_170.bin"},
	{"aw8697_Firefly_RTP_22_170.bin"},
	{"aw8697_Gathering_RTP_23_170.bin"},
	{"aw8697_Gaze_RTP_24_170.bin"},
	{"aw8697_Lakeside_RTP_25_170.bin"},
	{"aw8697_Lifestyle_RTP_26_170.bin"},
	{"aw8697_Memories_RTP_27_170.bin"},
	{"aw8697_Messy_RTP_28_170.bin"},
	{"aw8697_Night_RTP_29_170.bin"},
	{"aw8697_Passionate_Dance_RTP_30_170.bin"},
	{"aw8697_Playground_RTP_31_170.bin"},
	{"aw8697_Relax_RTP_32_170.bin"},
	{"aw8697_Reminiscence_RTP_33_170.bin"},
	{"aw8697_Silence_From_Afar_RTP_34_170.bin"},
	{"aw8697_Silence_RTP_35_170.bin"},
	{"aw8697_Stars_RTP_36_170.bin"},
	{"aw8697_Summer_RTP_37_170.bin"},
	{"aw8697_Toys_RTP_38_170.bin"},
	{"aw8697_Travel_RTP_39_170.bin"},
	{"aw8697_Vision_RTP_40_170.bin"},

	{"aw8697_reserved.bin"},
	{"aw8697_reserved.bin"},
	{"aw8697_reserved.bin"},
	{"aw8697_reserved.bin"},
	{"aw8697_reserved.bin"},
	{"aw8697_reserved.bin"},

	{"aw8697_reserved.bin"},
	{"aw8697_Simple_channel_RTP_48_170.bin"},
	{"aw8697_Pure_RTP_49_170.bin"},
	{"barca_alarm_ring_RTP_120_170.bin"},
	{"barca_incoming_ring_RTP_121_170.bin"},
	{"barca_notice_ring_RTP_122_170.bin"},
};

#ifdef OPLUS_FEATURE_CHG_BASIC
static char aw_rtp_name_150Hz[][AW_RTP_NAME_MAX] = {
	{"aw8697_rtp.bin"},
	{"aw8697_Hearty_channel_RTP_1.bin"},
	{"aw8697_Instant_channel_RTP_2_150Hz.bin"},
	{"aw8697_Music_channel_RTP_3.bin"},
	{"aw8697_Percussion_channel_RTP_4.bin"},
	{"aw8697_Ripple_channel_RTP_5.bin"},
	{"aw8697_Bright_channel_RTP_6.bin"},
	{"aw8697_Fun_channel_RTP_7.bin"},
	{"aw8697_Glittering_channel_RTP_8.bin"},
	{"aw8697_Granules_channel_RTP_9_150Hz.bin"},
	{"aw8697_Harp_channel_RTP_10.bin"},
	{"aw8697_Impression_channel_RTP_11.bin"},
	{"aw8697_Ingenious_channel_RTP_12_150Hz.bin"},
	{"aw8697_Joy_channel_RTP_13_150Hz.bin"},
	{"aw8697_Overtone_channel_RTP_14.bin"},
	{"aw8697_Receive_channel_RTP_15_150Hz.bin"},
	{"aw8697_Splash_channel_RTP_16_150Hz.bin"},

	{"aw8697_About_School_RTP_17_150Hz.bin"},
	{"aw8697_Bliss_RTP_18.bin"},
	{"aw8697_Childhood_RTP_19_150Hz.bin"},
	{"aw8697_Commuting_RTP_20_150Hz.bin"},
	{"aw8697_Dream_RTP_21.bin"},
	{"aw8697_Firefly_RTP_22_150Hz.bin"},
	{"aw8697_Gathering_RTP_23.bin"},
	{"aw8697_Gaze_RTP_24_150Hz.bin"},
	{"aw8697_Lakeside_RTP_25_150Hz.bin"},
	{"aw8697_Lifestyle_RTP_26.bin"},
	{"aw8697_Memories_RTP_27_150Hz.bin"},
	{"aw8697_Messy_RTP_28_150Hz.bin"},
	{"aw8697_Night_RTP_29_150Hz.bin"},
	{"aw8697_Passionate_Dance_RTP_30_150Hz.bin"},
	{"aw8697_Playground_RTP_31_150Hz.bin"},
	{"aw8697_Relax_RTP_32_150Hz.bin"},
	{"aw8697_Reminiscence_RTP_33.bin"},
	{"aw8697_Silence_From_Afar_RTP_34_150Hz.bin"},
	{"aw8697_Silence_RTP_35_150Hz.bin"},
	{"aw8697_Stars_RTP_36_150Hz.bin"},
	{"aw8697_Summer_RTP_37_150Hz.bin"},
	{"aw8697_Toys_RTP_38_150Hz.bin"},
	{"aw8697_Travel_RTP_39.bin"},
	{"aw8697_Vision_RTP_40.bin"},

	{"aw8697_waltz_channel_RTP_41_150Hz.bin"},
	{"aw8697_cut_channel_RTP_42_150Hz.bin"},
	{"aw8697_clock_channel_RTP_43_150Hz.bin"},
	{"aw8697_long_sound_channel_RTP_44_150Hz.bin"},
	{"aw8697_short_channel_RTP_45_150Hz.bin"},
	{"aw8697_two_error_remaind_RTP_46_150Hz.bin"},

	{"aw8697_kill_program_RTP_47_150Hz.bin"},
	{"aw8697_Simple_channel_RTP_48.bin"},
	{"aw8697_Pure_RTP_49_150Hz.bin"},
	{"aw8697_reserved_sound_channel_RTP_50.bin"},

	{"aw8697_high_temp_high_humidity_channel_RTP_51.bin"},

	{"aw8697_old_steady_test_RTP_52.bin"},
	{"aw8697_listen_pop_53.bin"},
	{"aw8697_desk_7_RTP_54_150Hz.bin"},
	{"aw8697_nfc_10_RTP_55_150Hz.bin"},
	{"aw8697_vibrator_remain_12_RTP_56_150Hz.bin"},
	{"aw8697_notice_13_RTP_57.bin"},
	{"aw8697_third_ring_14_RTP_58.bin"},
	{"aw8697_reserved_59.bin"},

	{"aw8697_honor_fisrt_kill_RTP_60_150Hz.bin"},
	{"aw8697_honor_two_kill_RTP_61_150Hz.bin"},
	{"aw8697_honor_three_kill_RTP_62_150Hz.bin"},
	{"aw8697_honor_four_kill_RTP_63_150Hz.bin"},
	{"aw8697_honor_five_kill_RTP_64_150Hz.bin"},
	{"aw8697_honor_three_continu_kill_RTP_65_150Hz.bin"},
	{"aw8697_honor_four_continu_kill_RTP_66_150Hz.bin"},
	{"aw8697_honor_unstoppable_RTP_67_150Hz.bin"},
	{"aw8697_honor_thousands_kill_RTP_68_150Hz.bin"},
	{"aw8697_honor_lengendary_RTP_69_150Hz.bin"},


	{"aw8697_Freshmorning_RTP_70_150Hz.bin"},
	{"aw8697_Peaceful_RTP_71_150Hz.bin"},
	{"aw8697_Cicada_RTP_72_150Hz.bin"},
	{"aw8697_Electronica_RTP_73_150Hz.bin"},
	{"aw8697_Holiday_RTP_74_150Hz.bin"},
	{"aw8697_Funk_RTP_75_150Hz.bin"},
	{"aw8697_House_RTP_76_150Hz.bin"},
	{"aw8697_Temple_RTP_77_150Hz.bin"},
	{"aw8697_Dreamyjazz_RTP_78_150Hz.bin"},
	{"aw8697_Modern_RTP_79_150Hz.bin"},

	{"aw8697_Round_RTP_80_150Hz.bin"},
	{"aw8697_Rising_RTP_81_150Hz.bin"},
	{"aw8697_Wood_RTP_82_150Hz.bin"},
	{"aw8697_Heys_RTP_83_150Hz.bin"},
	{"aw8697_Mbira_RTP_84_150Hz.bin"},
	{"aw8697_News_RTP_85_150Hz.bin"},
	{"aw8697_Peak_RTP_86_150Hz.bin"},
	{"aw8697_Crisp_RTP_87_150Hz.bin"},
	{"aw8697_Singingbowls_RTP_88_150Hz.bin"},
	{"aw8697_Bounce_RTP_89_150Hz.bin"},

	{"aw8697_reserved_90.bin"},
	{"aw8697_reserved_91.bin"},
	{"aw8697_reserved_92.bin"},
	{"aw8697_reserved_93.bin"},
	{"aw8697_reserved_94.bin"},
	{"aw8697_reserved_95.bin"},
	{"aw8697_reserved_96.bin"},
	{"aw8697_reserved_97.bin"},
	{"aw8697_reserved_98.bin"},
	{"aw8697_reserved_99.bin"},

	{"aw8697_soldier_first_kill_RTP_100_150Hz.bin"},
	{"aw8697_soldier_second_kill_RTP_101_150Hz.bin"},
	{"aw8697_soldier_third_kill_RTP_102_150Hz.bin"},
	{"aw8697_soldier_fourth_kill_RTP_103_150Hz.bin"},
	{"aw8697_soldier_fifth_kill_RTP_104_150Hz.bin"},
	{"aw8697_stepable_regulate_RTP_105_150Hz.bin"},
	{"aw8697_voice_level_bar_edge_RTP_106_150Hz.bin"},
	{"aw8697_strength_level_bar_edge_RTP_107_150Hz.bin"},
	{"aw8697_charging_simulation_RTP_108_150Hz.bin"},
	{"aw8697_fingerprint_success_RTP_109.bin"},

	{"aw8697_fingerprint_effect1_RTP_110.bin"},
	{"aw8697_fingerprint_effect2_RTP_111.bin"},
	{"aw8697_fingerprint_effect3_RTP_112.bin"},
	{"aw8697_fingerprint_effect4_RTP_113.bin"},
	{"aw8697_fingerprint_effect5_RTP_114.bin"},
	{"aw8697_fingerprint_effect6_RTP_115.bin"},
	{"aw8697_fingerprint_effect7_RTP_116.bin"},
	{"aw8697_fingerprint_effect8_RTP_117.bin"},
	{"aw8697_breath_simulation_RTP_118.bin"},
	{"aw8697_reserved_119.bin"},

	{"aw8697_Miss_RTP_120.bin"},
	{"aw8697_Scenic_RTP_121_150Hz.bin"},
	{"aw8697_voice_assistant_RTP_122.bin"},
/* used for 7 */
	{"aw8697_Appear_channel_RTP_123_150Hz.bin"},
	{"aw8697_Miss_RTP_124_150Hz.bin"},
	{"aw8697_Music_channel_RTP_125_150Hz.bin"},
	{"aw8697_Percussion_channel_RTP_126_150Hz.bin"},
	{"aw8697_Ripple_channel_RTP_127_150Hz.bin"},
	{"aw8697_Bright_channel_RTP_128_150Hz.bin"},
	{"aw8697_Fun_channel_RTP_129_150Hz.bin"},
	{"aw8697_Glittering_channel_RTP_130_150Hz.bin"},
	{"aw8697_Harp_channel_RTP_131_150Hz.bin"},
	{"aw8697_Overtone_channel_RTP_132_150Hz.bin"},
	{"aw8697_Simple_channel_RTP_133_150Hz.bin"},

	{"aw8697_Seine_past_RTP_134_150Hz.bin"},
	{"aw8697_Classical_ring_RTP_135_150Hz.bin"},
	{"aw8697_Long_for_RTP_136_150Hz.bin"},
	{"aw8697_Romantic_RTP_137_150Hz.bin"},
	{"aw8697_Bliss_RTP_138_150Hz.bin"},
	{"aw8697_Dream_RTP_139_150Hz.bin"},
	{"aw8697_Relax_RTP_140_150Hz.bin"},
	{"aw8697_Joy_channel_RTP_141_150Hz.bin"},
	{"aw8697_weather_wind_RTP_142_150Hz.bin"},
	{"aw8697_weather_cloudy_RTP_143_150Hz.bin"},
	{"aw8697_weather_thunderstorm_RTP_144_150Hz.bin"},
	{"aw8697_weather_default_RTP_145_150Hz.bin"},
	{"aw8697_weather_sunny_RTP_146_150Hz.bin"},
	{"aw8697_weather_smog_RTP_147_150Hz.bin"},
	{"aw8697_weather_snow_RTP_148_150Hz.bin"},
	{"aw8697_weather_rain_RTP_149_150Hz.bin"},

/* used for 7 end*/
	{"aw8697_rtp_lighthouse.bin"},
	{"aw8697_rtp_silk.bin"},
	{"aw8697_reserved_152.bin"},
	{"aw8697_reserved_153.bin"},
	{"aw8697_reserved_154.bin"},
	{"aw8697_reserved_155.bin"},
	{"aw8697_reserved_156.bin"},
	{"aw8697_reserved_157.bin"},
	{"aw8697_reserved_158.bin"},
	{"aw8697_reserved_159.bin"},
	{"aw8697_reserved_160.bin"},

	{"aw8697_reserved_161.bin"},
	{"aw8697_reserved_162.bin"},
	{"aw8697_reserved_163.bin"},
	{"aw8697_reserved_164.bin"},
	{"aw8697_reserved_165.bin"},
	{"aw8697_reserved_166.bin"},
	{"aw8697_reserved_167.bin"},
	{"aw8697_reserved_168.bin"},
	{"aw8697_reserved_169.bin"},
	{"aw8697_reserved_170.bin"},
	{"aw8697_Threefingers_Long_RTP_171_150Hz.bin"},
	{"aw8697_Threefingers_Up_RTP_172_150Hz.bin"},
	{"aw8697_Threefingers_Screenshot_RTP_173_150Hz.bin"},
	{"aw8697_Unfold_RTP_174_150Hz.bin"},
	{"aw8697_Close_RTP_175_150Hz.bin"},
	{"aw8697_HalfLap_RTP_176_150Hz.bin"},
	{"aw8697_Twofingers_Down_RTP_177_150Hz.bin"},
	{"aw8697_Twofingers_Long_RTP_178_150Hz.bin"},
	{"aw8697_Compatible_1_RTP_179_150Hz.bin"},
	{"aw8697_Compatible_2_RTP_180_150Hz.bin"},
	{"aw8697_Styleswitch_RTP_181_150Hz.bin"},
	{"aw8697_Waterripple_RTP_182_150Hz.bin"},
	{"aw8697_Suspendbutton_Bottomout_RTP_183_150Hz.bin"},
	{"aw8697_Suspendbutton_Menu_RTP_184_150Hz.bin"},
	{"aw8697_Complete_RTP_185_150Hz.bin"},
	{"aw8697_Bulb_RTP_186_150Hz.bin"},
	{"aw8697_Elasticity_RTP_187_150Hz.bin"},
	{"aw8697_reserved_188.bin"},
	{"aw8697_reserved_189.bin"},
	{"aw8697_reserved_190.bin"},
	{"aw8697_reserved_191.bin"},
	{"aw8697_reserved_192.bin"},
	{"aw8697_reserved_193.bin"},
	{"aw8697_reserved_194.bin"},
	{"aw8697_reserved_195.bin"},
	{"aw8697_reserved_196.bin"},
	{"aw8697_reserved_197.bin"},
	{"alarm_Pacman_RTP_198.bin"},
	{"notif_Pacman_RTP_199.bin"},
	{"ringtone_Pacman_RTP_200.bin"},
	{"ringtone_Alacrity_RTP_201.bin"},
	{"ring_Amenity_RTP_202.bin"},
	{"ringtone_Blues_RTP_203.bin"},
	{"ring_Bounce_RTP_204.bin"},
	{"ring_Calm_RTP_205.bin"},
	{"ringtone_Cloud_RTP_206.bin"},
	{"ringtone_Cyclotron_RTP_207.bin"},
	{"ringtone_Distinct_RTP_208.bin"},
	{"ringtone_Dynamic_RTP_209.bin"},
	{"ringtone_Echo_RTP_210.bin"},
	{"ringtone_Expect_RTP_211.bin"},
	{"ringtone_Fanatical_RTP_212.bin"},
	{"ringtone_Funky_RTP_213.bin"},
	{"ringtone_Guitar_RTP_214.bin"},
	{"ringtone_Harping_RTP_215.bin"},
	{"ringtone_Highlight_RTP_216.bin"},
	{"ringtone_Idyl_RTP_217.bin"},
	{"ringtone_Innocence_RTP_218.bin"},
	{"ringtone_Journey_RTP_219.bin"},
	{"ringtone_Joyous_RTP_220.bin"},
	{"ring_Lazy_RTP_221.bin"},
	{"ringtone_Marimba_RTP_222.bin"},
	{"ring_Mystical_RTP_223.bin"},
	{"ringtone_Old_telephone_RTP_224.bin"},
	{"ringtone_Oneplus_tune_RTP_225.bin"},
	{"ringtone_Rhythm_RTP_226.bin"},
	{"ringtone_Optimistic_RTP_227.bin"},
	{"ringtone_Piano_RTP_228.bin"},
	{"ring_Whirl_RTP_229.bin"},
	{"VZW_Alrwave_RTP_230.bin"},
	{"t-jingle_RTP_231.bin"},
	{"ringtone_Eager_232.bin"},
	{"ringtone_Ebullition_233.bin"},
	{"ringtone_Friendship_234.bin"},
	{"ringtone_Jazz_life_RTP_235.bin"},
	{"ringtone_Sun_glittering_RTP_236.bin"},
	{"notif_Allay_RTP_237.bin"},
	{"notif_Allusion_RTP_238.bin"},
	{"notif_Amiable_RTP_239.bin"},
	{"notif_Blare_RTP_240.bin"},
	{"notif_Blissful_RTP_241.bin"},
	{"notif_Brisk_RTP_242.bin"},
	{"notif_Bubble_RTP_243.bin"},
	{"notif_Cheerful_RTP_244.bin"},
	{"notif_Clear_RTP_245.bin"},
	{"notif_Comely_RTP_246.bin"},
	{"notif_Cozy_RTP_247.bin"},
	{"notif_Ding_RTP_248.bin"},
	{"notif_Effervesce_RTP_249.bin"},
	{"notif_Elegant_RTP_250.bin"},
	{"notif_Free_RTP_251.bin"},
	{"notif_Hallucination_RTP_252.bin"},
	{"notif_Inbound_RTP_253.bin"},
	{"notif_Light_RTP_254.bin"},
	{"notif_Meet_RTP_255.bin"},
	{"notif_Naivety_RTP_256.bin"},
	{"notif_Quickly_RTP_257.bin"},
	{"notif_Rhythm_RTP_258.bin"},
	{"notif_Surprise_RTP_259.bin"},
	{"notif_Twinkle_RTP_260.bin"},
	{"Version_Alert_RTP_261.bin"},
	{"alarm_Alarm_clock_RTP_262.bin"},
	{"alarm_Beep_RTP_263.bin"},
	{"alarm_Breeze_RTP_264.bin"},
	{"alarm_Dawn_RTP_265.bin"},
	{"alarm_Dream_RTP_266.bin"},
	{"alarm_Fluttering_RTP_267.bin"},
	{"alarm_Flyer_RTP_268.bin"},
	{"alarm_Interesting_RTP_269.bin"},
	{"alarm_Leisurely_RTP_270.bin"},
	{"alarm_Memory_RTP_271.bin"},
	{"alarm_Relieved_RTP_272.bin"},
	{"alarm_Ripple_RTP_273.bin"},
	{"alarm_Slowly_RTP_274.bin"},
	{"alarm_spring_RTP_275.bin"},
	{"alarm_Stars_RTP_276.bin"},
	{"alarm_Surging_RTP_277.bin"},
	{"alarm_tactfully_RTP_278.bin"},
	{"alarm_The_wind_RTP_279.bin"},
	{"alarm_Walking_in_the_rain_RTP_280.bin"},
	{"BoHaoPanAnJian_281.bin"},
	{"BoHaoPanAnNiu_282.bin"},
	{"BoHaoPanKuaiJie_283.bin"},
	{"DianHuaGuaDuan_284.bin"},
	{"DianJinMoShiQieHuan_285.bin"},
	{"HuaDongTiaoZhenDong_286.bin"},
	{"LeiShen_287.bin"},
	{"XuanZhongYouXi_288.bin"},
	{"YeJianMoShiDaZi_289.bin"},
	{"YouXiSheZhiKuang_290.bin"},
	{"ZhuanYeMoShi_291.bin"},
	{"Climber_RTP_292.bin"},
	{"Chase_RTP_293.bin"},
	{"shuntai24k_rtp_294.bin"},
	{"wentai24k_rtp_295.bin"},
	{"20ms_RTP_296.bin"},
	{"40ms_RTP_297.bin"},
	{"60ms_RTP_298.bin"},
	{"80ms_RTP_299.bin"},
	{"100ms_RTP_300.bin"},
	{"120ms_RTP_301.bin"},
	{"140ms_RTP_302.bin"},
	{"160ms_RTP_303.bin"},
	{"180ms_RTP_304.bin"},
	{"200ms_RTP_305.bin"},
	{"220ms_RTP_306.bin"},
	{"240ms_RTP_307.bin"},
	{"260ms_RTP_308.bin"},
	{"280ms_RTP_309.bin"},
	{"300ms_RTP_310.bin"},
	{"320ms_RTP_311.bin"},
	{"340ms_RTP_312.bin"},
	{"360ms_RTP_313.bin"},
	{"380ms_RTP_314.bin"},
	{"400ms_RTP_315.bin"},
	{"420ms_RTP_316.bin"},
	{"440ms_RTP_317.bin"},
	{"460ms_RTP_318.bin"},
	{"480ms_RTP_319.bin"},
	{"500ms_RTP_320.bin"},
	{"AT500ms_RTP_321.bin"},
};
#endif /* OPLUS_FEATURE_CHG_BASIC */

#ifdef KERNEL_VERSION_510
static char aw_rtp_name_162Hz[][AW_RTP_NAME_MAX] = {
	{"aw8697_rtp.bin"},
#ifdef OPLUS_FEATURE_CHG_BASIC
	{"aw8697_Hearty_channel_RTP_1.bin"},
	{"aw8697_Instant_channel_RTP_2_162Hz.bin"},
	{"aw8697_Music_channel_RTP_3.bin"},
	{"aw8697_Percussion_channel_RTP_4.bin"},
	{"aw8697_Ripple_channel_RTP_5.bin"},
	{"aw8697_Bright_channel_RTP_6.bin"},
	{"aw8697_Fun_channel_RTP_7.bin"},
	{"aw8697_Glittering_channel_RTP_8.bin"},
	{"aw8697_Granules_channel_RTP_9_162Hz.bin"},
	{"aw8697_Harp_channel_RTP_10.bin"},
	{"aw8697_Impression_channel_RTP_11.bin"},
	{"aw8697_Ingenious_channel_RTP_12_162Hz.bin"},
	{"aw8697_Joy_channel_RTP_13_162Hz.bin"},
	{"aw8697_Overtone_channel_RTP_14.bin"},
	{"aw8697_Receive_channel_RTP_15_162Hz.bin"},
	{"aw8697_Splash_channel_RTP_16_162Hz.bin"},
	{"aw8697_About_School_RTP_17_162Hz.bin"},
	{"aw8697_Bliss_RTP_18.bin"},
	{"aw8697_Childhood_RTP_19_162Hz.bin"},
	{"aw8697_Commuting_RTP_20_162Hz.bin"},
	{"aw8697_Dream_RTP_21.bin"},
	{"aw8697_Firefly_RTP_22_162Hz.bin"},
	{"aw8697_Gathering_RTP_23.bin"},
	{"aw8697_Gaze_RTP_24_162Hz.bin"},
	{"aw8697_Lakeside_RTP_25_162Hz.bin"},
	{"aw8697_Lifestyle_RTP_26.bin"},
	{"aw8697_Memories_RTP_27_162Hz.bin"},
	{"aw8697_Messy_RTP_28_162Hz.bin"},
	{"aw8697_Night_RTP_29_162Hz.bin"},
	{"aw8697_Passionate_Dance_RTP_30_162Hz.bin"},
	{"aw8697_Playground_RTP_31_162Hz.bin"},
	{"aw8697_Relax_RTP_32_162Hz.bin"},
	{"aw8697_Reminiscence_RTP_33.bin"},
	{"aw8697_Silence_From_Afar_RTP_34_162Hz.bin"},
	{"aw8697_Silence_RTP_35_162Hz.bin"},
	{"aw8697_Stars_RTP_36_162Hz.bin"},
	{"aw8697_Summer_RTP_37_162Hz.bin"},
	{"aw8697_Toys_RTP_38_162Hz.bin"},
	{"aw8697_Travel_RTP_39.bin"},
	{"aw8697_Vision_RTP_40.bin"},

	{"aw8697_waltz_channel_RTP_41_162Hz.bin"},
	{"aw8697_cut_channel_RTP_42_162Hz.bin"},
	{"aw8697_clock_channel_RTP_43_162Hz.bin"},
	{"aw8697_long_sound_channel_RTP_44_162Hz.bin"},
	{"aw8697_short_channel_RTP_45_162Hz.bin"},
	{"aw8697_two_error_remaind_RTP_46_162Hz.bin"},

	{"aw8697_kill_program_RTP_47_162Hz.bin"},
	{"aw8697_Simple_channel_RTP_48.bin"},
	{"aw8697_Pure_RTP_49_162Hz.bin"},
	{"aw8697_reserved_sound_channel_RTP_50.bin"},

	{"aw8697_high_temp_high_humidity_channel_RTP_51.bin"},
	{"aw8697_old_steady_test_RTP_52.bin"},
	{"aw8697_listen_pop_53.bin"},
	{"aw8697_desk_7_RTP_54_162Hz.bin"},
	{"aw8697_nfc_10_RTP_55_162Hz.bin"},
	{"aw8697_vibrator_remain_12_RTP_56.bin"},
	{"aw8697_notice_13_RTP_57.bin"},
	{"aw8697_third_ring_14_RTP_58.bin"},
	{"aw8697_reserved_59.bin"},

	{"aw8697_honor_fisrt_kill_RTP_60_162Hz.bin"},
	{"aw8697_honor_two_kill_RTP_61_162Hz.bin"},
	{"aw8697_honor_three_kill_RTP_62_162Hz.bin"},
	{"aw8697_honor_four_kill_RTP_63_162Hz.bin"},
	{"aw8697_honor_five_kill_RTP_64_162Hz.bin"},
	{"aw8697_honor_three_continu_kill_RTP_65_162Hz.bin"},
	{"aw8697_honor_four_continu_kill_RTP_66_162Hz.bin"},
	{"aw8697_honor_unstoppable_RTP_67_162Hz.bin"},
	{"aw8697_honor_thousands_kill_RTP_68_162Hz.bin"},
	{"aw8697_honor_lengendary_RTP_69_162Hz.bin"},

	{"aw8697_Freshmorning_RTP_70_162Hz.bin"},
	{"aw8697_Peaceful_RTP_71_162Hz.bin"},
	{"aw8697_Cicada_RTP_72_162Hz.bin"},
	{"aw8697_Electronica_RTP_73_162Hz.bin"},
	{"aw8697_Holiday_RTP_74_162Hz.bin"},
	{"aw8697_Funk_RTP_75_162Hz.bin"},
	{"aw8697_House_RTP_76_162Hz.bin"},
	{"aw8697_Temple_RTP_77_162Hz.bin"},
	{"aw8697_Dreamyjazz_RTP_78_162Hz.bin"},
	{"aw8697_Modern_RTP_79_162Hz.bin"},

	{"aw8697_Round_RTP_80_162Hz.bin"},
	{"aw8697_Rising_RTP_81_162Hz.bin"},
	{"aw8697_Wood_RTP_82_162Hz.bin"},
	{"aw8697_Heys_RTP_83_162Hz.bin"},
	{"aw8697_Mbira_RTP_84_162Hz.bin"},
	{"aw8697_News_RTP_85_162Hz.bin"},
	{"aw8697_Peak_RTP_86_162Hz.bin"},
	{"aw8697_Crisp_RTP_87_162Hz.bin"},
	{"aw8697_Singingbowls_RTP_88_162Hz.bin"},
	{"aw8697_Bounce_RTP_89_162Hz.bin"},

	{"aw8697_reserved_90.bin"},
	{"aw8697_reserved_91.bin"},
	{"aw8697_reserved_92.bin"},
	{"aw8697_reserved_93.bin"},
	{"aw8697_ALCloudscape_94_162HZ.bin"},
	{"aw8697_ALGoodenergy_95_162HZ.bin"},
	{"aw8697_NTblink_96_162HZ.bin"},
	{"aw8697_NTwhoop_97_162HZ.bin"},
	{"aw8697_Newfeeling_98_162HZ.bin"},
	{"aw8697_nature_99_162HZ.bin"},

	{"aw8697_soldier_first_kill_RTP_100.bin"},
	{"aw8697_soldier_second_kill_RTP_101.bin"},
	{"aw8697_soldier_third_kill_RTP_102.bin"},
	{"aw8697_soldier_fourth_kill_RTP_103.bin"},
	{"aw8697_soldier_fifth_kill_RTP_104.bin"},
	{"aw8697_stepable_regulate_RTP_105_162Hz.bin"},
	{"aw8697_voice_level_bar_edge_RTP_106_162Hz.bin"},
	{"aw8697_strength_level_bar_edge_RTP_107_162Hz.bin"},
	{"aw8697_charging_simulation_RTP_108_162Hz.bin"},
	{"aw8697_fingerprint_success_RTP_109_162Hz.bin"},

	{"aw8697_fingerprint_effect1_RTP_110.bin"},
	{"aw8697_fingerprint_effect2_RTP_111.bin"},
	{"aw8697_fingerprint_effect3_RTP_112.bin"},
	{"aw8697_fingerprint_effect4_RTP_113.bin"},
	{"aw8697_fingerprint_effect5_RTP_114.bin"},
	{"aw8697_fingerprint_effect6_RTP_115.bin"},
	{"aw8697_fingerprint_effect7_RTP_116.bin"},
	{"aw8697_fingerprint_effect8_RTP_117.bin"},
	{"aw8697_breath_simulation_RTP_118_162Hz.bin"},
	{"aw8697_Telcel_Torreblanca_RTP_119_162Hz.bin"},

	{"aw8697_Miss_RTP_120.bin"},
	{"aw8697_Scenic_RTP_121_162Hz.bin"},
	{"aw8697_voice_assistant_RTP_122_162Hz.bin"},
	{"aw8697_Appear_channel_RTP_123_162Hz.bin"},
	{"aw8697_Miss_RTP_124_162Hz.bin"},
	{"aw8697_Music_channel_RTP_125_162Hz.bin"},
	{"aw8697_Percussion_channel_RTP_126_162Hz.bin"},
	{"aw8697_Ripple_channel_RTP_127_162Hz.bin"},
	{"aw8697_Bright_channel_RTP_128_162Hz.bin"},
	{"aw8697_Fun_channel_RTP_129_162Hz.bin"},
	{"aw8697_Glittering_channel_RTP_130_162Hz.bin"},
	{"aw8697_Harp_channel_RTP_131_162Hz.bin"},
	{"aw8697_Overtone_channel_RTP_132_162Hz.bin"},
	{"aw8697_Simple_channel_RTP_133_162Hz.bin"},
	{"aw8697_Seine_past_RTP_134_162Hz.bin"},
	{"aw8697_Classical_ring_RTP_135_162Hz.bin"},
	{"aw8697_Long_for_RTP_136_162Hz.bin"},
	{"aw8697_Romantic_RTP_137_162Hz.bin"},
	{"aw8697_Bliss_RTP_138_162Hz.bin"},
	{"aw8697_Dream_RTP_139_162Hz.bin"},
	{"aw8697_Relax_RTP_140_162Hz.bin"},
	{"aw8697_Joy_channel_RTP_141_162Hz.bin"},
	{"aw8697_weather_wind_RTP_142.bin"},
	{"aw8697_weather_cloudy_RTP_143.bin"},
	{"aw8697_weather_thunderstorm_RTP_144.bin"},
	{"aw8697_weather_default_RTP_145.bin"},
	{"aw8697_weather_sunny_RTP_146.bin"},
	{"aw8697_weather_smog_RTP_147.bin"},
	{"aw8697_weather_snow_RTP_148.bin"},
	{"aw8697_weather_rain_RTP_149.bin"},
#endif

	{"aw8697_Master_Notification_RTP_150_162Hz.bin"},
	{"aw8697_Master_Artist_Ringtong_RTP_151_162Hz.bin"},
	{"aw8697_Master_Text_RTP_152_162Hz.bin"},
	{"aw8697_Master_Artist_Alarm_RTP_153_162Hz.bin"},
	{"aw8697_reserved_154.bin"},
	{"aw8697_reserved_155.bin"},
	{"aw8697_reserved_156.bin"},
	{"aw8697_reserved_157.bin"},
	{"aw8697_reserved_158.bin"},
	{"aw8697_reserved_159.bin"},
	{"aw8697_reserved_160.bin"},

	{"aw8697_oplus_its_oplus_RTP_161_162Hz.bin"},
	{"aw8697_oplus_tune_RTP_162_162Hz.bin"},
	{"aw8697_oplus_jingle_RTP_163_162Hz.bin"},
	{"aw8697_reserved_164.bin"},
	{"aw8697_reserved_165.bin"},
	{"aw8697_reserved_166.bin"},
	{"aw8697_reserved_167.bin"},
	{"aw8697_reserved_168.bin"},
	{"aw8697_reserved_169.bin"},
	{"aw8697_oplus_gt_RTP_170_162Hz.bin"},

	{"aw8697_Threefingers_Long_RTP_171.bin"},
	{"aw8697_Threefingers_Up_RTP_172.bin"},
	{"aw8697_Threefingers_Screenshot_RTP_173.bin"},
	{"aw8697_Unfold_RTP_174.bin"},
	{"aw8697_Close_RTP_175.bin"},
	{"aw8697_HalfLap_RTP_176.bin"},
	{"aw8697_Twofingers_Down_RTP_177.bin"},
	{"aw8697_Twofingers_Long_RTP_178.bin"},
	{"aw8697_Compatible_1_RTP_179.bin"},
	{"aw8697_Compatible_2_RTP_180.bin"},
	{"aw8697_Styleswitch_RTP_181.bin"},
	{"aw8697_Waterripple_RTP_182.bin"},
	{"aw8697_Suspendbutton_Bottomout_RTP_183.bin"},
	{"aw8697_Suspendbutton_Menu_RTP_184.bin"},
	{"aw8697_Complete_RTP_185.bin"},
	{"aw8697_Bulb_RTP_186.bin"},
	{"aw8697_Elasticity_RTP_187.bin"},
	{"aw8697_reserved_188.bin"},
	{"aw8697_reserved_189.bin"},
	{"aw8697_reserved_190.bin"},
	{"aw8697_reserved_191.bin"},
	{"aw8697_reserved_192.bin"},
	{"aw8697_reserved_193.bin"},
	{"aw8697_reserved_194.bin"},
	{"aw8697_reserved_195.bin"},
	{"aw8697_reserved_196.bin"},
	{"aw8697_reserved_197.bin"},
	{"alarm_Pacman_RTP_198.bin"},
	{"notif_Pacman_RTP_199.bin"},
	{"ringtone_Pacman_RTP_200.bin"},
	{"ringtone_Alacrity_RTP_201_162Hz.bin"},
	{"ring_Amenity_RTP_202_162Hz.bin"},
	{"ringtone_Blues_RTP_203_162Hz.bin"},
	{"ring_Bounce_RTP_204_162Hz.bin"},
	{"ring_Calm_RTP_205_162Hz.bin"},
	{"ringtone_Cloud_RTP_206_162Hz.bin"},
	{"ringtone_Cyclotron_RTP_207_162Hz.bin"},
	{"ringtone_Distinct_RTP_208_162Hz.bin"},
	{"ringtone_Dynamic_RTP_209_162Hz.bin"},
	{"ringtone_Echo_RTP_210_162Hz.bin"},
	{"ringtone_Expect_RTP_211_162Hz.bin"},
	{"ringtone_Fanatical_RTP_212_162Hz.bin"},
	{"ringtone_Funky_RTP_213_162Hz.bin"},
	{"ringtone_Guitar_RTP_214_162Hz.bin"},
	{"ringtone_Harping_RTP_215_162Hz.bin"},
	{"ringtone_Highlight_RTP_216_162Hz.bin"},
	{"ringtone_Idyl_RTP_217_162Hz.bin"},
	{"ringtone_Innocence_RTP_218_162Hz.bin"},
	{"ringtone_Journey_RTP_219_162Hz.bin"},
	{"ringtone_Joyous_RTP_220_162Hz.bin"},
	{"ring_Lazy_RTP_221_162Hz.bin"},
	{"ringtone_Marimba_RTP_222_162Hz.bin"},
	{"ring_Mystical_RTP_223_162Hz.bin"},
	{"ringtone_Old_telephone_RTP_224_162Hz.bin"},
	{"ringtone_Oneplus_tune_RTP_225_162Hz.bin"},
	{"ringtone_Rhythm_RTP_226_162Hz.bin"},
	{"ringtone_Optimistic_RTP_227_162Hz.bin"},
	{"ringtone_Piano_RTP_228_162Hz.bin"},
	{"ring_Whirl_RTP_229_162Hz.bin"},
	{"VZW_Alrwave_RTP_230.bin"},
	{"t-jingle_RTP_231.bin"},
	{"ringtone_Eager_232_162Hz.bin"},
	{"ringtone_Ebullition_233_162Hz.bin"},
	{"ringtone_Friendship_234_162Hz.bin"},
	{"ringtone_Jazz_life_RTP_235_162Hz.bin"},
	{"ringtone_Sun_glittering_RTP_236_162Hz.bin"},
	{"notif_Allay_RTP_237_162Hz.bin"},
	{"notif_Allusion_RTP_238_162Hz.bin"},
	{"notif_Amiable_RTP_239_162Hz.bin"},
	{"notif_Blare_RTP_240_162Hz.bin"},
	{"notif_Blissful_RTP_241_162Hz.bin"},
	{"notif_Brisk_RTP_242_162Hz.bin"},
	{"notif_Bubble_RTP_243_162Hz.bin"},
	{"notif_Cheerful_RTP_244_162Hz.bin"},
	{"notif_Clear_RTP_245_162Hz.bin"},
	{"notif_Comely_RTP_246_162Hz.bin"},
	{"notif_Cozy_RTP_247_162Hz.bin"},
	{"notif_Ding_RTP_248_162Hz.bin"},
	{"notif_Effervesce_RTP_249_162Hz.bin"},
	{"notif_Elegant_RTP_250_162Hz.bin"},
	{"notif_Free_RTP_251_162Hz.bin"},
	{"notif_Hallucination_RTP_252_162Hz.bin"},
	{"notif_Inbound_RTP_253_162Hz.bin"},
	{"notif_Light_RTP_254_162Hz.bin"},
	{"notif_Meet_RTP_255_162Hz.bin"},
	{"notif_Naivety_RTP_256_162Hz.bin"},
	{"notif_Quickly_RTP_257_162Hz.bin"},
	{"notif_Rhythm_RTP_258_162Hz.bin"},
	{"notif_Surprise_RTP_259_162Hz.bin"},
	{"notif_Twinkle_RTP_260_162Hz.bin"},
	{"Version_Alert_RTP_261_162Hz.bin"},
	{"alarm_Alarm_clock_RTP_262_162Hz.bin"},
	{"alarm_Beep_RTP_263_162Hz.bin"},
	{"alarm_Breeze_RTP_264_162Hz.bin"},
	{"alarm_Dawn_RTP_265_162Hz.bin"},
	{"alarm_Dream_RTP_266_162Hz.bin"},
	{"alarm_Fluttering_RTP_267_162Hz.bin"},
	{"alarm_Flyer_RTP_268_162Hz.bin"},
	{"alarm_Interesting_RTP_269_162Hz.bin"},
	{"alarm_Leisurely_RTP_270_162Hz.bin"},
	{"alarm_Memory_RTP_271_162Hz.bin"},
	{"alarm_Relieved_RTP_272_162Hz.bin"},
	{"alarm_Ripple_RTP_273_162Hz.bin"},
	{"alarm_Slowly_RTP_274_162Hz.bin"},
	{"alarm_spring_RTP_275_162Hz.bin"},
	{"alarm_Stars_RTP_276_162Hz.bin"},
	{"alarm_Surging_RTP_277_162Hz.bin"},
	{"alarm_tactfully_RTP_278_162Hz.bin"},
	{"alarm_The_wind_RTP_279_162Hz.bin"},
	{"alarm_Walking_in_the_rain_RTP_280_162Hz.bin"},
	{"BoHaoPanAnJian_281.bin"},
	{"BoHaoPanAnNiu_282.bin"},
	{"BoHaoPanKuaiJie_283.bin"},
	{"DianHuaGuaDuan_284.bin"},
	{"DianJinMoShiQieHuan_285.bin"},
	{"HuaDongTiaoZhenDong_286.bin"},
	{"LeiShen_287.bin"},
	{"XuanZhongYouXi_288.bin"},
	{"YeJianMoShiDaZi_289.bin"},
	{"YouXiSheZhiKuang_290.bin"},
	{"ZhuanYeMoShi_291.bin"},
	{"Climber_RTP_292_162Hz.bin"},
	{"Chase_RTP_293_162Hz.bin"},
	{"shuntai24k_rtp_294.bin"},
	{"wentai24k_rtp_295.bin"},
	{"20ms_RTP_296.bin"},
	{"40ms_RTP_297.bin"},
	{"60ms_RTP_298.bin"},
	{"80ms_RTP_299.bin"},
	{"100ms_RTP_300.bin"},
	{"120ms_RTP_301.bin"},
	{"140ms_RTP_302.bin"},
	{"160ms_RTP_303.bin"},
	{"180ms_RTP_304.bin"},
	{"200ms_RTP_305.bin"},
	{"220ms_RTP_306.bin"},
	{"240ms_RTP_307.bin"},
	{"260ms_RTP_308.bin"},
	{"280ms_RTP_309.bin"},
	{"300ms_RTP_310.bin"},
	{"320ms_RTP_311.bin"},
	{"340ms_RTP_312.bin"},
	{"360ms_RTP_313.bin"},
	{"380ms_RTP_314.bin"},
	{"400ms_RTP_315.bin"},
	{"420ms_RTP_316.bin"},
	{"440ms_RTP_317.bin"},
	{"460ms_RTP_318.bin"},
	{"480ms_RTP_319.bin"},
	{"500ms_RTP_320.bin"},
	{"AT500ms_RTP_321.bin"},

	{"aw8697_reserved_322.bin"},
	{"aw8697_reserved_323.bin"},
	{"aw8697_reserved_324.bin"},
	{"aw8697_reserved_325.bin"},
	{"aw8697_reserved_326.bin"},
	{"aw8697_reserved_327.bin"},
	{"aw8697_reserved_328.bin"},
	{"aw8697_reserved_329.bin"},
	{"aw8697_reserved_330.bin"},
	{"aw8697_reserved_331.bin"},
	{"aw8697_reserved_332.bin"},
	{"aw8697_reserved_333.bin"},
	{"aw8697_reserved_334.bin"},
	{"aw8697_reserved_335.bin"},
	{"aw8697_reserved_336.bin"},
	{"aw8697_reserved_337.bin"},
	{"aw8697_reserved_338.bin"},
	{"aw8697_reserved_339.bin"},
	{"aw8697_reserved_340.bin"},
	{"aw8697_reserved_341.bin"},
	{"aw8697_reserved_342.bin"},
	{"aw8697_reserved_343.bin"},
	{"aw8697_reserved_344.bin"},
	{"aw8697_reserved_345.bin"},
	{"aw8697_reserved_346.bin"},
	{"aw8697_reserved_347.bin"},
	{"aw8697_reserved_348.bin"},
	{"aw8697_reserved_349.bin"},
	{"aw8697_reserved_350.bin"},
	{"aw8697_reserved_351.bin"},
	{"aw8697_reserved_352.bin"},
	{"aw8697_reserved_353.bin"},
	{"aw8697_reserved_354.bin"},
	{"aw8697_reserved_355.bin"},
	{"aw8697_reserved_356.bin"},
	{"aw8697_reserved_357.bin"},
	{"aw8697_reserved_358.bin"},
	{"aw8697_reserved_359.bin"},
	{"aw8697_reserved_360.bin"},
	{"aw8697_reserved_361.bin"},
	{"aw8697_reserved_362.bin"},
	{"aw8697_programs_clean_RTP_363_162Hz.bin"},
	{"aw8697_reserved_364.bin"},
	{"aw8697_reserved_365.bin"},
	{"aw8697_reserved_366.bin"},
	{"aw8697_reserved_367.bin"},
	{"aw8697_reserved_368.bin"},
	{"aw8697_reserved_369.bin"},
	{"aw8697_reserved_370.bin"},

	/* Add for OS14 Start */
	{"aw8697_Nightsky_RTP_371_162Hz.bin"},
	{"aw8697_TheStars_RTP_372_162Hz.bin"},
	{"aw8697_TheSunrise_RTP_373_162Hz.bin"},
	{"aw8697_TheSunset_RTP_374_162Hz.bin"},
	{"aw8697_Meditate_RTP_375_162Hz.bin"},
	{"aw8697_Distant_RTP_376_162Hz.bin"},
	{"aw8697_Pond_RTP_377_162Hz.bin"},
	{"aw8697_Moonlotus_RTP_378_162Hz.bin"},
	{"aw8697_Ripplingwater_RTP_379_162Hz.bin"},
	{"aw8697_Shimmer_RTP_380_162Hz.bin"},
	{"aw8697_Batheearth_RTP_381_162Hz.bin"},
	{"aw8697_Junglemorning_RTP_382_162Hz.bin"},
	{"aw8697_Silver_RTP_383_162Hz.bin"},
	{"aw8697_Elegantquiet_RTP_384_162Hz.bin"},
	{"aw8697_Summerbeach_RTP_385_162Hz.bin"},
	{"aw8697_Summernight_RTP_386_162Hz.bin"},
	{"aw8697_Icesnow_RTP_387_162Hz.bin"},
	{"aw8697_Wintersnow_RTP_388_162Hz.bin"},
	{"aw8697_Rainforest_RTP_389_162Hz.bin"},
	{"aw8697_Raineverything_RTP_390_162Hz.bin"},
	{"aw8697_Staracross_RTP_391_162Hz.bin"},
	{"aw8697_Fullmoon_RTP_392_162Hz.bin"},
	{"aw8697_Clouds_RTP_393_162Hz.bin"},
	{"aw8697_Wonderland_RTP_394_162Hz.bin"},
	{"aw8697_Still_RTP_395_162Hz.bin"},
	{"aw8697_Haunting_RTP_396_162Hz.bin"},
	{"aw8697_Dragonfly_RTP_397_162Hz.bin"},
	{"aw8697_Dropwater_RTP_398_162Hz.bin"},
	{"aw8697_Fluctuation_RTP_399_162Hz.bin"},
	{"aw8697_Blow_RTP_400_162Hz.bin"},
	{"aw8697_Leaveslight_RTP_401_162Hz.bin"},
	{"aw8697_Warmsun_RTP_402_162Hz.bin"},
	{"aw8697_Snowflake_RTP_403_162Hz.bin"},
	{"aw8697_Crystalclear_RTP_404_162Hz.bin"},
	{"aw8697_Insects_RTP_405_162Hz.bin"},
	{"aw8697_Dew_RTP_406_162Hz.bin"},
	{"aw8697_Shine_RTP_407_162Hz.bin"},
	{"aw8697_Frost_RTP_408_162Hz.bin"},
	{"aw8697_Rainsplash_RTP_409_162Hz.bin"},
	{"aw8697_Raindrop_RTP_410_162Hz.bin"},
	/* Add for OS14 End */
};

static char aw_rtp_name_166Hz[][AW_RTP_NAME_MAX] = {
	{"aw8697_rtp.bin"},
#ifdef OPLUS_FEATURE_CHG_BASIC
	{"aw8697_Hearty_channel_RTP_1.bin"},
	{"aw8697_Instant_channel_RTP_2_166Hz.bin"},
	{"aw8697_Music_channel_RTP_3.bin"},
	{"aw8697_Percussion_channel_RTP_4.bin"},
	{"aw8697_Ripple_channel_RTP_5.bin"},
	{"aw8697_Bright_channel_RTP_6.bin"},
	{"aw8697_Fun_channel_RTP_7.bin"},
	{"aw8697_Glittering_channel_RTP_8.bin"},
	{"aw8697_Granules_channel_RTP_9_166Hz.bin"},
	{"aw8697_Harp_channel_RTP_10.bin"},
	{"aw8697_Impression_channel_RTP_11.bin"},
	{"aw8697_Ingenious_channel_RTP_12_166Hz.bin"},
	{"aw8697_Joy_channel_RTP_13_166Hz.bin"},
	{"aw8697_Overtone_channel_RTP_14.bin"},
	{"aw8697_Receive_channel_RTP_15_166Hz.bin"},
	{"aw8697_Splash_channel_RTP_16_166Hz.bin"},
	{"aw8697_About_School_RTP_17_166Hz.bin"},
	{"aw8697_Bliss_RTP_18.bin"},
	{"aw8697_Childhood_RTP_19_166Hz.bin"},
	{"aw8697_Commuting_RTP_20_166Hz.bin"},
	{"aw8697_Dream_RTP_21.bin"},
	{"aw8697_Firefly_RTP_22_166Hz.bin"},
	{"aw8697_Gathering_RTP_23.bin"},
	{"aw8697_Gaze_RTP_24_166Hz.bin"},
	{"aw8697_Lakeside_RTP_25_166Hz.bin"},
	{"aw8697_Lifestyle_RTP_26.bin"},
	{"aw8697_Memories_RTP_27_166Hz.bin"},
	{"aw8697_Messy_RTP_28_166Hz.bin"},
	{"aw8697_Night_RTP_29_166Hz.bin"},
	{"aw8697_Passionate_Dance_RTP_30_166Hz.bin"},
	{"aw8697_Playground_RTP_31_166Hz.bin"},
	{"aw8697_Relax_RTP_32_166Hz.bin"},
	{"aw8697_Reminiscence_RTP_33.bin"},
	{"aw8697_Silence_From_Afar_RTP_34_166Hz.bin"},
	{"aw8697_Silence_RTP_35_166Hz.bin"},
	{"aw8697_Stars_RTP_36_166Hz.bin"},
	{"aw8697_Summer_RTP_37_166Hz.bin"},
	{"aw8697_Toys_RTP_38_166Hz.bin"},
	{"aw8697_Travel_RTP_39.bin"},
	{"aw8697_Vision_RTP_40.bin"},

	{"aw8697_waltz_channel_RTP_41_166Hz.bin"},
	{"aw8697_cut_channel_RTP_42_166Hz.bin"},
	{"aw8697_clock_channel_RTP_43_166Hz.bin"},
	{"aw8697_long_sound_channel_RTP_44_166Hz.bin"},
	{"aw8697_short_channel_RTP_45_166Hz.bin"},
	{"aw8697_two_error_remaind_RTP_46_166Hz.bin"},

	{"aw8697_kill_program_RTP_47_166Hz.bin"},
	{"aw8697_Simple_channel_RTP_48.bin"},
	{"aw8697_Pure_RTP_49_166Hz.bin"},
	{"aw8697_reserved_sound_channel_RTP_50.bin"},

	{"aw8697_high_temp_high_humidity_channel_RTP_51.bin"},
	{"aw8697_old_steady_test_RTP_52.bin"},
	{"aw8697_listen_pop_53.bin"},
	{"aw8697_desk_7_RTP_54_166Hz.bin"},
	{"aw8697_nfc_10_RTP_55_166Hz.bin"},
	{"aw8697_vibrator_remain_12_RTP_56.bin"},
	{"aw8697_notice_13_RTP_57.bin"},
	{"aw8697_third_ring_14_RTP_58.bin"},
	{"aw8697_reserved_59.bin"},

	{"aw8697_honor_fisrt_kill_RTP_60_166Hz.bin"},
	{"aw8697_honor_two_kill_RTP_61_166Hz.bin"},
	{"aw8697_honor_three_kill_RTP_62_166Hz.bin"},
	{"aw8697_honor_four_kill_RTP_63_166Hz.bin"},
	{"aw8697_honor_five_kill_RTP_64_166Hz.bin"},
	{"aw8697_honor_three_continu_kill_RTP_65_166Hz.bin"},
	{"aw8697_honor_four_continu_kill_RTP_66_166Hz.bin"},
	{"aw8697_honor_unstoppable_RTP_67_166Hz.bin"},
	{"aw8697_honor_thousands_kill_RTP_68_166Hz.bin"},
	{"aw8697_honor_lengendary_RTP_69_166Hz.bin"},

	{"aw8697_Freshmorning_RTP_70_166Hz.bin"},
	{"aw8697_Peaceful_RTP_71_166Hz.bin"},
	{"aw8697_Cicada_RTP_72_166Hz.bin"},
	{"aw8697_Electronica_RTP_73_166Hz.bin"},
	{"aw8697_Holiday_RTP_74_166Hz.bin"},
	{"aw8697_Funk_RTP_75_166Hz.bin"},
	{"aw8697_House_RTP_76_166Hz.bin"},
	{"aw8697_Temple_RTP_77_166Hz.bin"},
	{"aw8697_Dreamyjazz_RTP_78_166Hz.bin"},
	{"aw8697_Modern_RTP_79_166Hz.bin"},

	{"aw8697_Round_RTP_80_166Hz.bin"},
	{"aw8697_Rising_RTP_81_166Hz.bin"},
	{"aw8697_Wood_RTP_82_166Hz.bin"},
	{"aw8697_Heys_RTP_83_166Hz.bin"},
	{"aw8697_Mbira_RTP_84_166Hz.bin"},
	{"aw8697_News_RTP_85_166Hz.bin"},
	{"aw8697_Peak_RTP_86_166Hz.bin"},
	{"aw8697_Crisp_RTP_87_166Hz.bin"},
	{"aw8697_Singingbowls_RTP_88_166Hz.bin"},
	{"aw8697_Bounce_RTP_89_166Hz.bin"},

	{"aw8697_reserved_90.bin"},
	{"aw8697_reserved_91.bin"},
	{"aw8697_reserved_92.bin"},
	{"aw8697_reserved_93.bin"},
	{"aw8697_ALCloudscape_94_166HZ.bin"},
	{"aw8697_ALGoodenergy_95_166HZ.bin"},
	{"aw8697_NTblink_96_166HZ.bin"},
	{"aw8697_NTwhoop_97_166HZ.bin"},
	{"aw8697_Newfeeling_98_166HZ.bin"},
	{"aw8697_nature_99_166HZ.bin"},

	{"aw8697_soldier_first_kill_RTP_100.bin"},
	{"aw8697_soldier_second_kill_RTP_101.bin"},
	{"aw8697_soldier_third_kill_RTP_102.bin"},
	{"aw8697_soldier_fourth_kill_RTP_103.bin"},
	{"aw8697_soldier_fifth_kill_RTP_104.bin"},
	{"aw8697_stepable_regulate_RTP_105_166Hz.bin"},
	{"aw8697_voice_level_bar_edge_RTP_106_166Hz.bin"},
	{"aw8697_strength_level_bar_edge_RTP_107_166Hz.bin"},
	{"aw8697_charging_simulation_RTP_108_166Hz.bin"},
	{"aw8697_fingerprint_success_RTP_109_166Hz.bin"},

	{"aw8697_fingerprint_effect1_RTP_110.bin"},
	{"aw8697_fingerprint_effect2_RTP_111.bin"},
	{"aw8697_fingerprint_effect3_RTP_112.bin"},
	{"aw8697_fingerprint_effect4_RTP_113.bin"},
	{"aw8697_fingerprint_effect5_RTP_114.bin"},
	{"aw8697_fingerprint_effect6_RTP_115.bin"},
	{"aw8697_fingerprint_effect7_RTP_116.bin"},
	{"aw8697_fingerprint_effect8_RTP_117.bin"},
	{"aw8697_breath_simulation_RTP_118_166Hz.bin"},
	{"aw8697_Telcel_Torreblanca_RTP_119_166Hz.bin"},

	{"aw8697_Miss_RTP_120.bin"},
	{"aw8697_Scenic_RTP_121_166Hz.bin"},
	{"aw8697_voice_assistant_RTP_122_166Hz.bin"},
	{"aw8697_Appear_channel_RTP_123_166Hz.bin"},
	{"aw8697_Miss_RTP_124_166Hz.bin"},
	{"aw8697_Music_channel_RTP_125_166Hz.bin"},
	{"aw8697_Percussion_channel_RTP_126_166Hz.bin"},
	{"aw8697_Ripple_channel_RTP_127_166Hz.bin"},
	{"aw8697_Bright_channel_RTP_128_166Hz.bin"},
	{"aw8697_Fun_channel_RTP_129_166Hz.bin"},
	{"aw8697_Glittering_channel_RTP_130_166Hz.bin"},
	{"aw8697_Harp_channel_RTP_131_166Hz.bin"},
	{"aw8697_Overtone_channel_RTP_132_166Hz.bin"},
	{"aw8697_Simple_channel_RTP_133_166Hz.bin"},
	{"aw8697_Seine_past_RTP_134_166Hz.bin"},
	{"aw8697_Classical_ring_RTP_135_166Hz.bin"},
	{"aw8697_Long_for_RTP_136_166Hz.bin"},
	{"aw8697_Romantic_RTP_137_166Hz.bin"},
	{"aw8697_Bliss_RTP_138_166Hz.bin"},
	{"aw8697_Dream_RTP_139_166Hz.bin"},
	{"aw8697_Relax_RTP_140_166Hz.bin"},
	{"aw8697_Joy_channel_RTP_141_166Hz.bin"},
	{"aw8697_weather_wind_RTP_142.bin"},
	{"aw8697_weather_cloudy_RTP_143.bin"},
	{"aw8697_weather_thunderstorm_RTP_144.bin"},
	{"aw8697_weather_default_RTP_145.bin"},
	{"aw8697_weather_sunny_RTP_146.bin"},
	{"aw8697_weather_smog_RTP_147.bin"},
	{"aw8697_weather_snow_RTP_148.bin"},
	{"aw8697_weather_rain_RTP_149.bin"},
#endif

	{"aw8697_Master_Notification_RTP_150_166Hz.bin"},
	{"aw8697_Master_Artist_Ringtong_RTP_151_166Hz.bin"},
	{"aw8697_Master_Text_RTP_152_166Hz.bin"},
	{"aw8697_Master_Artist_Alarm_RTP_153_166Hz.bin"},
	{"aw8697_reserved_154.bin"},
	{"aw8697_reserved_155.bin"},
	{"aw8697_reserved_156.bin"},
	{"aw8697_reserved_157.bin"},
	{"aw8697_reserved_158.bin"},
	{"aw8697_reserved_159.bin"},
	{"aw8697_reserved_160.bin"},

	{"aw8697_oplus_its_oplus_RTP_161_166Hz.bin"},
	{"aw8697_oplus_tune_RTP_162_166Hz.bin"},
	{"aw8697_oplus_jingle_RTP_163_166Hz.bin"},
	{"aw8697_reserved_164.bin"},
	{"aw8697_reserved_165.bin"},
	{"aw8697_reserved_166.bin"},
	{"aw8697_reserved_167.bin"},
	{"aw8697_reserved_168.bin"},
	{"aw8697_reserved_169.bin"},
	{"aw8697_oplus_gt_RTP_170_166Hz.bin"},

	{"aw8697_Threefingers_Long_RTP_171.bin"},
	{"aw8697_Threefingers_Up_RTP_172.bin"},
	{"aw8697_Threefingers_Screenshot_RTP_173.bin"},
	{"aw8697_Unfold_RTP_174.bin"},
	{"aw8697_Close_RTP_175.bin"},
	{"aw8697_HalfLap_RTP_176.bin"},
	{"aw8697_Twofingers_Down_RTP_177.bin"},
	{"aw8697_Twofingers_Long_RTP_178.bin"},
	{"aw8697_Compatible_1_RTP_179.bin"},
	{"aw8697_Compatible_2_RTP_180.bin"},
	{"aw8697_Styleswitch_RTP_181.bin"},
	{"aw8697_Waterripple_RTP_182.bin"},
	{"aw8697_Suspendbutton_Bottomout_RTP_183.bin"},
	{"aw8697_Suspendbutton_Menu_RTP_184.bin"},
	{"aw8697_Complete_RTP_185.bin"},
	{"aw8697_Bulb_RTP_186.bin"},
	{"aw8697_Elasticity_RTP_187.bin"},
	{"aw8697_reserved_188.bin"},
	{"aw8697_reserved_189.bin"},
	{"aw8697_reserved_190.bin"},
	{"aw8697_reserved_191.bin"},
	{"aw8697_reserved_192.bin"},
	{"aw8697_reserved_193.bin"},
	{"aw8697_reserved_194.bin"},
	{"aw8697_reserved_195.bin"},
	{"aw8697_reserved_196.bin"},
	{"aw8697_reserved_197.bin"},
	{"alarm_Pacman_RTP_198.bin"},
	{"notif_Pacman_RTP_199.bin"},
	{"ringtone_Pacman_RTP_200.bin"},
	{"ringtone_Alacrity_RTP_201_166Hz.bin"},
	{"ring_Amenity_RTP_202_166Hz.bin"},
	{"ringtone_Blues_RTP_203_166Hz.bin"},
	{"ring_Bounce_RTP_204_166Hz.bin"},
	{"ring_Calm_RTP_205_166Hz.bin"},
	{"ringtone_Cloud_RTP_206_166Hz.bin"},
	{"ringtone_Cyclotron_RTP_207_166Hz.bin"},
	{"ringtone_Distinct_RTP_208_166Hz.bin"},
	{"ringtone_Dynamic_RTP_209_166Hz.bin"},
	{"ringtone_Echo_RTP_210_166Hz.bin"},
	{"ringtone_Expect_RTP_211_166Hz.bin"},
	{"ringtone_Fanatical_RTP_212_166Hz.bin"},
	{"ringtone_Funky_RTP_213_166Hz.bin"},
	{"ringtone_Guitar_RTP_214_166Hz.bin"},
	{"ringtone_Harping_RTP_215_166Hz.bin"},
	{"ringtone_Highlight_RTP_216_166Hz.bin"},
	{"ringtone_Idyl_RTP_217_166Hz.bin"},
	{"ringtone_Innocence_RTP_218_166Hz.bin"},
	{"ringtone_Journey_RTP_219_166Hz.bin"},
	{"ringtone_Joyous_RTP_220_166Hz.bin"},
	{"ring_Lazy_RTP_221_166Hz.bin"},
	{"ringtone_Marimba_RTP_222_166Hz.bin"},
	{"ring_Mystical_RTP_223_166Hz.bin"},
	{"ringtone_Old_telephone_RTP_224_166Hz.bin"},
	{"ringtone_Oneplus_tune_RTP_225_166Hz.bin"},
	{"ringtone_Rhythm_RTP_226_166Hz.bin"},
	{"ringtone_Optimistic_RTP_227_166Hz.bin"},
	{"ringtone_Piano_RTP_228_166Hz.bin"},
	{"ring_Whirl_RTP_229_166Hz.bin"},
	{"VZW_Alrwave_RTP_230.bin"},
	{"t-jingle_RTP_231.bin"},
	{"ringtone_Eager_232_166Hz.bin"},
	{"ringtone_Ebullition_233_166Hz.bin"},
	{"ringtone_Friendship_234_166Hz.bin"},
	{"ringtone_Jazz_life_RTP_235_166Hz.bin"},
	{"ringtone_Sun_glittering_RTP_236_166Hz.bin"},
	{"notif_Allay_RTP_237_166Hz.bin"},
	{"notif_Allusion_RTP_238_166Hz.bin"},
	{"notif_Amiable_RTP_239_166Hz.bin"},
	{"notif_Blare_RTP_240_166Hz.bin"},
	{"notif_Blissful_RTP_241_166Hz.bin"},
	{"notif_Brisk_RTP_242_166Hz.bin"},
	{"notif_Bubble_RTP_243_166Hz.bin"},
	{"notif_Cheerful_RTP_244_166Hz.bin"},
	{"notif_Clear_RTP_245_166Hz.bin"},
	{"notif_Comely_RTP_246_166Hz.bin"},
	{"notif_Cozy_RTP_247_166Hz.bin"},
	{"notif_Ding_RTP_248_166Hz.bin"},
	{"notif_Effervesce_RTP_249_166Hz.bin"},
	{"notif_Elegant_RTP_250_166Hz.bin"},
	{"notif_Free_RTP_251_166Hz.bin"},
	{"notif_Hallucination_RTP_252_166Hz.bin"},
	{"notif_Inbound_RTP_253_166Hz.bin"},
	{"notif_Light_RTP_254_166Hz.bin"},
	{"notif_Meet_RTP_255_166Hz.bin"},
	{"notif_Naivety_RTP_256_166Hz.bin"},
	{"notif_Quickly_RTP_257_166Hz.bin"},
	{"notif_Rhythm_RTP_258_166Hz.bin"},
	{"notif_Surprise_RTP_259_166Hz.bin"},
	{"notif_Twinkle_RTP_260_166Hz.bin"},
	{"Version_Alert_RTP_261.bin"},
	{"alarm_Alarm_clock_RTP_262_166Hz.bin"},
	{"alarm_Beep_RTP_263_166Hz.bin"},
	{"alarm_Breeze_RTP_264_166Hz.bin"},
	{"alarm_Dawn_RTP_265_166Hz.bin"},
	{"alarm_Dream_RTP_266_166Hz.bin"},
	{"alarm_Fluttering_RTP_267_166Hz.bin"},
	{"alarm_Flyer_RTP_268_166Hz.bin"},
	{"alarm_Interesting_RTP_269_166Hz.bin"},
	{"alarm_Leisurely_RTP_270_166Hz.bin"},
	{"alarm_Memory_RTP_271_166Hz.bin"},
	{"alarm_Relieved_RTP_272_166Hz.bin"},
	{"alarm_Ripple_RTP_273_166Hz.bin"},
	{"alarm_Slowly_RTP_274_166Hz.bin"},
	{"alarm_spring_RTP_275_166Hz.bin"},
	{"alarm_Stars_RTP_276_166Hz.bin"},
	{"alarm_Surging_RTP_277_166Hz.bin"},
	{"alarm_tactfully_RTP_278_166Hz.bin"},
	{"alarm_The_wind_RTP_279_166Hz.bin"},
	{"alarm_Walking_in_the_rain_RTP_280_166Hz.bin"},
	{"BoHaoPanAnJian_281.bin"},
	{"BoHaoPanAnNiu_282.bin"},
	{"BoHaoPanKuaiJie_283.bin"},
	{"DianHuaGuaDuan_284.bin"},
	{"DianJinMoShiQieHuan_285.bin"},
	{"HuaDongTiaoZhenDong_286.bin"},
	{"LeiShen_287.bin"},
	{"XuanZhongYouXi_288.bin"},
	{"YeJianMoShiDaZi_289.bin"},
	{"YouXiSheZhiKuang_290.bin"},
	{"ZhuanYeMoShi_291.bin"},
	{"Climber_RTP_292_166Hz.bin"},
	{"Chase_RTP_293_166Hz.bin"},
	{"shuntai24k_rtp_294.bin"},
	{"wentai24k_rtp_295.bin"},
	{"20ms_RTP_296.bin"},
	{"40ms_RTP_297.bin"},
	{"60ms_RTP_298.bin"},
	{"80ms_RTP_299.bin"},
	{"100ms_RTP_300.bin"},
	{"120ms_RTP_301.bin"},
	{"140ms_RTP_302.bin"},
	{"160ms_RTP_303.bin"},
	{"180ms_RTP_304.bin"},
	{"200ms_RTP_305.bin"},
	{"220ms_RTP_306.bin"},
	{"240ms_RTP_307.bin"},
	{"260ms_RTP_308.bin"},
	{"280ms_RTP_309.bin"},
	{"300ms_RTP_310.bin"},
	{"320ms_RTP_311.bin"},
	{"340ms_RTP_312.bin"},
	{"360ms_RTP_313.bin"},
	{"380ms_RTP_314.bin"},
	{"400ms_RTP_315.bin"},
	{"420ms_RTP_316.bin"},
	{"440ms_RTP_317.bin"},
	{"460ms_RTP_318.bin"},
	{"480ms_RTP_319.bin"},
	{"500ms_RTP_320.bin"},
	{"AT500ms_RTP_321.bin"},

	{"aw8697_reserved_322.bin"},
	{"aw8697_reserved_323.bin"},
	{"aw8697_reserved_324.bin"},
	{"aw8697_reserved_325.bin"},
	{"aw8697_reserved_326.bin"},
	{"aw8697_reserved_327.bin"},
	{"aw8697_reserved_328.bin"},
	{"aw8697_reserved_329.bin"},
	{"aw8697_reserved_330.bin"},
	{"aw8697_reserved_331.bin"},
	{"aw8697_reserved_332.bin"},
	{"aw8697_reserved_333.bin"},
	{"aw8697_reserved_334.bin"},
	{"aw8697_reserved_335.bin"},
	{"aw8697_reserved_336.bin"},
	{"aw8697_reserved_337.bin"},
	{"aw8697_reserved_338.bin"},
	{"aw8697_reserved_339.bin"},
	{"aw8697_reserved_340.bin"},
	{"aw8697_reserved_341.bin"},
	{"aw8697_reserved_342.bin"},
	{"aw8697_reserved_343.bin"},
	{"aw8697_reserved_344.bin"},
	{"aw8697_reserved_345.bin"},
	{"aw8697_reserved_346.bin"},
	{"aw8697_reserved_347.bin"},
	{"aw8697_reserved_348.bin"},
	{"aw8697_reserved_349.bin"},
	{"aw8697_reserved_350.bin"},
	{"aw8697_reserved_351.bin"},
	{"aw8697_reserved_352.bin"},
	{"aw8697_reserved_353.bin"},
	{"aw8697_reserved_354.bin"},
	{"aw8697_reserved_355.bin"},
	{"aw8697_reserved_356.bin"},
	{"aw8697_reserved_357.bin"},
	{"aw8697_reserved_358.bin"},
	{"aw8697_reserved_359.bin"},
	{"aw8697_reserved_360.bin"},
	{"aw8697_reserved_361.bin"},
	{"aw8697_reserved_362.bin"},
	{"aw8697_programs_clean_RTP_363_166Hz.bin"},
	{"aw8697_reserved_364.bin"},
	{"aw8697_reserved_365.bin"},
	{"aw8697_reserved_366.bin"},
	{"aw8697_reserved_367.bin"},
	{"aw8697_reserved_368.bin"},
	{"aw8697_reserved_369.bin"},
	{"aw8697_reserved_370.bin"},

	/* Add for OS14 Start */
	{"aw8697_Nightsky_RTP_371_166Hz.bin"},
	{"aw8697_TheStars_RTP_372_166Hz.bin"},
	{"aw8697_TheSunrise_RTP_373_166Hz.bin"},
	{"aw8697_TheSunset_RTP_374_166Hz.bin"},
	{"aw8697_Meditate_RTP_375_166Hz.bin"},
	{"aw8697_Distant_RTP_376_166Hz.bin"},
	{"aw8697_Pond_RTP_377_166Hz.bin"},
	{"aw8697_Moonlotus_RTP_378_166Hz.bin"},
	{"aw8697_Ripplingwater_RTP_379_166Hz.bin"},
	{"aw8697_Shimmer_RTP_380_166Hz.bin"},
	{"aw8697_Batheearth_RTP_381_166Hz.bin"},
	{"aw8697_Junglemorning_RTP_382_166Hz.bin"},
	{"aw8697_Silver_RTP_383_166Hz.bin"},
	{"aw8697_Elegantquiet_RTP_384_166Hz.bin"},
	{"aw8697_Summerbeach_RTP_385_166Hz.bin"},
	{"aw8697_Summernight_RTP_386_166Hz.bin"},
	{"aw8697_Icesnow_RTP_387_166Hz.bin"},
	{"aw8697_Wintersnow_RTP_388_166Hz.bin"},
	{"aw8697_Rainforest_RTP_389_166Hz.bin"},
	{"aw8697_Raineverything_RTP_390_166Hz.bin"},
	{"aw8697_Staracross_RTP_391_166Hz.bin"},
	{"aw8697_Fullmoon_RTP_392_166Hz.bin"},
	{"aw8697_Clouds_RTP_393_166Hz.bin"},
	{"aw8697_Wonderland_RTP_394_166Hz.bin"},
	{"aw8697_Still_RTP_395_166Hz.bin"},
	{"aw8697_Haunting_RTP_396_166Hz.bin"},
	{"aw8697_Dragonfly_RTP_397_166Hz.bin"},
	{"aw8697_Dropwater_RTP_398_166Hz.bin"},
	{"aw8697_Fluctuation_RTP_399_166Hz.bin"},
	{"aw8697_Blow_RTP_400_166Hz.bin"},
	{"aw8697_Leaveslight_RTP_401_166Hz.bin"},
	{"aw8697_Warmsun_RTP_402_166Hz.bin"},
	{"aw8697_Snowflake_RTP_403_166Hz.bin"},
	{"aw8697_Crystalclear_RTP_404_166Hz.bin"},
	{"aw8697_Insects_RTP_405_166Hz.bin"},
	{"aw8697_Dew_RTP_406_166Hz.bin"},
	{"aw8697_Shine_RTP_407_166Hz.bin"},
	{"aw8697_Frost_RTP_408_166Hz.bin"},
	{"aw8697_Rainsplash_RTP_409_166Hz.bin"},
	{"aw8697_Raindrop_RTP_410_166Hz.bin"},
	/* Add for OS14 End */
};

static char aw_rtp_name_174Hz[][AW_RTP_NAME_MAX] = {
	{"aw8697_rtp.bin"},
#ifdef OPLUS_FEATURE_CHG_BASIC
	{"aw8697_Hearty_channel_RTP_1.bin"},
	{"aw8697_Instant_channel_RTP_2_174Hz.bin"},
	{"aw8697_Music_channel_RTP_3.bin"},
	{"aw8697_Percussion_channel_RTP_4.bin"},
	{"aw8697_Ripple_channel_RTP_5.bin"},
	{"aw8697_Bright_channel_RTP_6.bin"},
	{"aw8697_Fun_channel_RTP_7.bin"},
	{"aw8697_Glittering_channel_RTP_8.bin"},
	{"aw8697_Granules_channel_RTP_9_174Hz.bin"},
	{"aw8697_Harp_channel_RTP_10.bin"},
	{"aw8697_Impression_channel_RTP_11.bin"},
	{"aw8697_Ingenious_channel_RTP_12_174Hz.bin"},
	{"aw8697_Joy_channel_RTP_13_174Hz.bin"},
	{"aw8697_Overtone_channel_RTP_14.bin"},
	{"aw8697_Receive_channel_RTP_15_174Hz.bin"},
	{"aw8697_Splash_channel_RTP_16_174Hz.bin"},
	{"aw8697_About_School_RTP_17_174Hz.bin"},
	{"aw8697_Bliss_RTP_18.bin"},
	{"aw8697_Childhood_RTP_19_174Hz.bin"},
	{"aw8697_Commuting_RTP_20_174Hz.bin"},
	{"aw8697_Dream_RTP_21.bin"},
	{"aw8697_Firefly_RTP_22_174Hz.bin"},
	{"aw8697_Gathering_RTP_23.bin"},
	{"aw8697_Gaze_RTP_24_174Hz.bin"},
	{"aw8697_Lakeside_RTP_25_174Hz.bin"},
	{"aw8697_Lifestyle_RTP_26.bin"},
	{"aw8697_Memories_RTP_27_174Hz.bin"},
	{"aw8697_Messy_RTP_28_174Hz.bin"},
	{"aw8697_Night_RTP_29_174Hz.bin"},
	{"aw8697_Passionate_Dance_RTP_30_174Hz.bin"},
	{"aw8697_Playground_RTP_31_174Hz.bin"},
	{"aw8697_Relax_RTP_32_174Hz.bin"},
	{"aw8697_Reminiscence_RTP_33.bin"},
	{"aw8697_Silence_From_Afar_RTP_34_174Hz.bin"},
	{"aw8697_Silence_RTP_35_174Hz.bin"},
	{"aw8697_Stars_RTP_36_174Hz.bin"},
	{"aw8697_Summer_RTP_37_174Hz.bin"},
	{"aw8697_Toys_RTP_38_174Hz.bin"},
	{"aw8697_Travel_RTP_39.bin"},
	{"aw8697_Vision_RTP_40.bin"},

	{"aw8697_waltz_channel_RTP_41_174Hz.bin"},
	{"aw8697_cut_channel_RTP_42_174Hz.bin"},
	{"aw8697_clock_channel_RTP_43_174Hz.bin"},
	{"aw8697_long_sound_channel_RTP_44_174Hz.bin"},
	{"aw8697_short_channel_RTP_45_174Hz.bin"},
	{"aw8697_two_error_remaind_RTP_46_174Hz.bin"},
	{"aw8697_kill_program_RTP_47_174Hz.bin"},
	{"aw8697_Simple_channel_RTP_48.bin"},
	{"aw8697_Pure_RTP_49_174Hz.bin"},
	{"aw8697_reserved_sound_channel_RTP_50.bin"},

	{"aw8697_high_temp_high_humidity_channel_RTP_51.bin"},
	{"aw8697_old_steady_test_RTP_52.bin"},
	{"aw8697_listen_pop_53.bin"},
	{"aw8697_desk_7_RTP_54_174Hz.bin"},
	{"aw8697_nfc_10_RTP_55_174Hz.bin"},
	{"aw8697_vibrator_remain_12_RTP_56.bin"},
	{"aw8697_notice_13_RTP_57.bin"},
	{"aw8697_third_ring_14_RTP_58.bin"},
	{"aw8697_reserved_59.bin"},

	{"aw8697_honor_fisrt_kill_RTP_60_174Hz.bin"},
	{"aw8697_honor_two_kill_RTP_61_174Hz.bin"},
	{"aw8697_honor_three_kill_RTP_62_174Hz.bin"},
	{"aw8697_honor_four_kill_RTP_63_174Hz.bin"},
	{"aw8697_honor_five_kill_RTP_64_174Hz.bin"},
	{"aw8697_honor_three_continu_kill_RTP_65_174Hz.bin"},
	{"aw8697_honor_four_continu_kill_RTP_66_174Hz.bin"},
	{"aw8697_honor_unstoppable_RTP_67_174Hz.bin"},
	{"aw8697_honor_thousands_kill_RTP_68_174Hz.bin"},
	{"aw8697_honor_lengendary_RTP_69_174Hz.bin"},

	{"aw8697_Freshmorning_RTP_70_174Hz.bin"},
	{"aw8697_Peaceful_RTP_71_174Hz.bin"},
	{"aw8697_Cicada_RTP_72_174Hz.bin"},
	{"aw8697_Electronica_RTP_73_174Hz.bin"},
	{"aw8697_Holiday_RTP_74_174Hz.bin"},
	{"aw8697_Funk_RTP_75_174Hz.bin"},
	{"aw8697_House_RTP_76_174Hz.bin"},
	{"aw8697_Temple_RTP_77_174Hz.bin"},
	{"aw8697_Dreamyjazz_RTP_78_174Hz.bin"},
	{"aw8697_Modern_RTP_79_174Hz.bin"},
	{"aw8697_Round_RTP_80_174Hz.bin"},
	{"aw8697_Rising_RTP_81_174Hz.bin"},
	{"aw8697_Wood_RTP_82_174Hz.bin"},
	{"aw8697_Heys_RTP_83_174Hz.bin"},
	{"aw8697_Mbira_RTP_84_174Hz.bin"},
	{"aw8697_News_RTP_85_174Hz.bin"},
	{"aw8697_Peak_RTP_86_174Hz.bin"},
	{"aw8697_Crisp_RTP_87_174Hz.bin"},
	{"aw8697_Singingbowls_RTP_88_174Hz.bin"},
	{"aw8697_Bounce_RTP_89_174Hz.bin"},

	{"aw8697_reserved_90.bin"},
	{"aw8697_reserved_91.bin"},
	{"aw8697_reserved_92.bin"},
	{"aw8697_reserved_93.bin"},
	{"aw8697_ALCloudscape_94_174HZ.bin"},
	{"aw8697_ALGoodenergy_95_174HZ.bin"},
	{"aw8697_NTblink_96_174HZ.bin"},
	{"aw8697_NTwhoop_97_174HZ.bin"},
	{"aw8697_Newfeeling_98_174HZ.bin"},
	{"aw8697_nature_99_174HZ.bin"},

	{"aw8697_soldier_first_kill_RTP_100.bin"},
	{"aw8697_soldier_second_kill_RTP_101.bin"},
	{"aw8697_soldier_third_kill_RTP_102.bin"},
	{"aw8697_soldier_fourth_kill_RTP_103.bin"},
	{"aw8697_soldier_fifth_kill_RTP_104.bin"},
	{"aw8697_stepable_regulate_RTP_105_174Hz.bin"},
	{"aw8697_voice_level_bar_edge_RTP_106_174Hz.bin"},
	{"aw8697_strength_level_bar_edge_RTP_107_174Hz.bin"},
	{"aw8697_charging_simulation_RTP_108_174Hz.bin"},
	{"aw8697_fingerprint_success_RTP_109_174Hz.bin"},

	{"aw8697_fingerprint_effect1_RTP_110.bin"},
	{"aw8697_fingerprint_effect2_RTP_111.bin"},
	{"aw8697_fingerprint_effect3_RTP_112.bin"},
	{"aw8697_fingerprint_effect4_RTP_113.bin"},
	{"aw8697_fingerprint_effect5_RTP_114.bin"},
	{"aw8697_fingerprint_effect6_RTP_115.bin"},
	{"aw8697_fingerprint_effect7_RTP_116.bin"},
	{"aw8697_fingerprint_effect8_RTP_117.bin"},
	{"aw8697_breath_simulation_RTP_118_174Hz.bin"},
	{"aw8697_Telcel_Torreblanca_RTP_119_174Hz.bin"},

	{"aw8697_Miss_RTP_120.bin"},
	{"aw8697_Scenic_RTP_121_174Hz.bin"},
	{"aw8697_voice_assistant_RTP_122_174Hz.bin"},
	{"aw8697_Appear_channel_RTP_123_174Hz.bin"},
	{"aw8697_Miss_RTP_124_174Hz.bin"},
	{"aw8697_Music_channel_RTP_125_174Hz.bin"},
	{"aw8697_Percussion_channel_RTP_126_174Hz.bin"},
	{"aw8697_Ripple_channel_RTP_127_174Hz.bin"},
	{"aw8697_Bright_channel_RTP_128_174Hz.bin"},
	{"aw8697_Fun_channel_RTP_129_174Hz.bin"},
	{"aw8697_Glittering_channel_RTP_130_174Hz.bin"},
	{"aw8697_Harp_channel_RTP_131_174Hz.bin"},
	{"aw8697_Overtone_channel_RTP_132_174Hz.bin"},
	{"aw8697_Simple_channel_RTP_133_174Hz.bin"},
	{"aw8697_Seine_past_RTP_134_174Hz.bin"},
	{"aw8697_Classical_ring_RTP_135_174Hz.bin"},
	{"aw8697_Long_for_RTP_136_174Hz.bin"},
	{"aw8697_Romantic_RTP_137_174Hz.bin"},
	{"aw8697_Bliss_RTP_138_174Hz.bin"},
	{"aw8697_Dream_RTP_139_174Hz.bin"},
	{"aw8697_Relax_RTP_140_174Hz.bin"},
	{"aw8697_Joy_channel_RTP_141_174Hz.bin"},
	{"aw8697_weather_wind_RTP_142.bin"},
	{"aw8697_weather_cloudy_RTP_143.bin"},
	{"aw8697_weather_thunderstorm_RTP_144.bin"},
	{"aw8697_weather_default_RTP_145.bin"},
	{"aw8697_weather_sunny_RTP_146.bin"},
	{"aw8697_weather_smog_RTP_147.bin"},
	{"aw8697_weather_snow_RTP_148.bin"},
	{"aw8697_weather_rain_RTP_149.bin"},
#endif

	{"aw8697_Master_Notification_RTP_150_174Hz.bin"},
	{"aw8697_Master_Artist_Ringtong_RTP_151_174Hz.bin"},
	{"aw8697_Master_Text_RTP_152_174Hz.bin"},
	{"aw8697_Master_Artist_Alarm_RTP_153_174Hz.bin"},
	{"aw8697_reserved_154.bin"},
	{"aw8697_reserved_155.bin"},
	{"aw8697_reserved_156.bin"},
	{"aw8697_reserved_157.bin"},
	{"aw8697_reserved_158.bin"},
	{"aw8697_reserved_159.bin"},
	{"aw8697_reserved_160.bin"},

	{"aw8697_oplus_its_oplus_RTP_161_174Hz.bin"},
	{"aw8697_oplus_tune_RTP_162_174Hz.bin"},
	{"aw8697_oplus_jingle_RTP_163_174Hz.bin"},
	{"aw8697_reserved_164.bin"},
	{"aw8697_reserved_165.bin"},
	{"aw8697_reserved_166.bin"},
	{"aw8697_reserved_167.bin"},
	{"aw8697_reserved_168.bin"},
	{"aw8697_reserved_169.bin"},
	{"aw8697_oplus_gt_RTP_170_174Hz.bin"},

	{"aw8697_Threefingers_Long_RTP_171.bin"},
	{"aw8697_Threefingers_Up_RTP_172.bin"},
	{"aw8697_Threefingers_Screenshot_RTP_173.bin"},
	{"aw8697_Unfold_RTP_174.bin"},
	{"aw8697_Close_RTP_175.bin"},
	{"aw8697_HalfLap_RTP_176.bin"},
	{"aw8697_Twofingers_Down_RTP_177.bin"},
	{"aw8697_Twofingers_Long_RTP_178.bin"},
	{"aw8697_Compatible_1_RTP_179.bin"},
	{"aw8697_Compatible_2_RTP_180.bin"},
	{"aw8697_Styleswitch_RTP_181.bin"},
	{"aw8697_Waterripple_RTP_182.bin"},
	{"aw8697_Suspendbutton_Bottomout_RTP_183.bin"},
	{"aw8697_Suspendbutton_Menu_RTP_184.bin"},
	{"aw8697_Complete_RTP_185.bin"},
	{"aw8697_Bulb_RTP_186.bin"},
	{"aw8697_Elasticity_RTP_187.bin"},
	{"aw8697_reserved_188.bin"},
	{"aw8697_reserved_189.bin"},
	{"aw8697_reserved_190.bin"},
	{"aw8697_reserved_191.bin"},
	{"aw8697_reserved_192.bin"},
	{"aw8697_reserved_193.bin"},
	{"aw8697_reserved_194.bin"},
	{"aw8697_reserved_195.bin"},
	{"aw8697_reserved_196.bin"},
	{"aw8697_reserved_197.bin"},
	{"alarm_Pacman_RTP_198.bin"},
	{"notif_Pacman_RTP_199.bin"},
	{"ringtone_Pacman_RTP_200.bin"},
	{"ringtone_Alacrity_RTP_201_174Hz.bin"},
	{"ring_Amenity_RTP_202_174Hz.bin"},
	{"ringtone_Blues_RTP_203_174Hz.bin"},
	{"ring_Bounce_RTP_204_174Hz.bin"},
	{"ring_Calm_RTP_205_174Hz.bin"},
	{"ringtone_Cloud_RTP_206_174Hz.bin"},
	{"ringtone_Cyclotron_RTP_207_174Hz.bin"},
	{"ringtone_Distinct_RTP_208_174Hz.bin"},
	{"ringtone_Dynamic_RTP_209_174Hz.bin"},
	{"ringtone_Echo_RTP_210_174Hz.bin"},
	{"ringtone_Expect_RTP_211_174Hz.bin"},
	{"ringtone_Fanatical_RTP_212_174Hz.bin"},
	{"ringtone_Funky_RTP_213_174Hz.bin"},
	{"ringtone_Guitar_RTP_214_174Hz.bin"},
	{"ringtone_Harping_RTP_215_174Hz.bin"},
	{"ringtone_Highlight_RTP_216_174Hz.bin"},
	{"ringtone_Idyl_RTP_217_174Hz.bin"},
	{"ringtone_Innocence_RTP_218_174Hz.bin"},
	{"ringtone_Journey_RTP_219_174Hz.bin"},
	{"ringtone_Joyous_RTP_220_174Hz.bin"},
	{"ring_Lazy_RTP_221_174Hz.bin"},
	{"ringtone_Marimba_RTP_222_174Hz.bin"},
	{"ring_Mystical_RTP_223_174Hz.bin"},
	{"ringtone_Old_telephone_RTP_224_174Hz.bin"},
	{"ringtone_Oneplus_tune_RTP_225_174Hz.bin"},
	{"ringtone_Rhythm_RTP_226_174Hz.bin"},
	{"ringtone_Optimistic_RTP_227_174Hz.bin"},
	{"ringtone_Piano_RTP_228_174Hz.bin"},
	{"ring_Whirl_RTP_229_174Hz.bin"},
	{"VZW_Alrwave_RTP_230.bin"},
	{"t-jingle_RTP_231.bin"},
	{"ringtone_Eager_232_174Hz.bin"},
	{"ringtone_Ebullition_233_174Hz.bin"},
	{"ringtone_Friendship_234_174Hz.bin"},
	{"ringtone_Jazz_life_RTP_235_174Hz.bin"},
	{"ringtone_Sun_glittering_RTP_236_174Hz.bin"},
	{"notif_Allay_RTP_237_174Hz.bin"},
	{"notif_Allusion_RTP_238_174Hz.bin"},
	{"notif_Amiable_RTP_239_174Hz.bin"},
	{"notif_Blare_RTP_240_174Hz.bin"},
	{"notif_Blissful_RTP_241_174Hz.bin"},
	{"notif_Brisk_RTP_242_174Hz.bin"},
	{"notif_Bubble_RTP_243_174Hz.bin"},
	{"notif_Cheerful_RTP_244_174Hz.bin"},
	{"notif_Clear_RTP_245_174Hz.bin"},
	{"notif_Comely_RTP_246_174Hz.bin"},
	{"notif_Cozy_RTP_247_174Hz.bin"},
	{"notif_Ding_RTP_248_174Hz.bin"},
	{"notif_Effervesce_RTP_249_174Hz.bin"},
	{"notif_Elegant_RTP_250_174Hz.bin"},
	{"notif_Free_RTP_251_174Hz.bin"},
	{"notif_Hallucination_RTP_252_174Hz.bin"},
	{"notif_Inbound_RTP_253_174Hz.bin"},
	{"notif_Light_RTP_254_174Hz.bin"},
	{"notif_Meet_RTP_255_174Hz.bin"},
	{"notif_Naivety_RTP_256_174Hz.bin"},
	{"notif_Quickly_RTP_257_174Hz.bin"},
	{"notif_Rhythm_RTP_258_174Hz.bin"},
	{"notif_Surprise_RTP_259_174Hz.bin"},
	{"notif_Twinkle_RTP_260_174Hz.bin"},
	{"Version_Alert_RTP_261.bin"},
	{"alarm_Alarm_clock_RTP_262_174Hz.bin"},
	{"alarm_Beep_RTP_263_174Hz.bin"},
	{"alarm_Breeze_RTP_264_174Hz.bin"},
	{"alarm_Dawn_RTP_265_174Hz.bin"},
	{"alarm_Dream_RTP_266_174Hz.bin"},
	{"alarm_Fluttering_RTP_267_174Hz.bin"},
	{"alarm_Flyer_RTP_268_174Hz.bin"},
	{"alarm_Interesting_RTP_269_174Hz.bin"},
	{"alarm_Leisurely_RTP_270_174Hz.bin"},
	{"alarm_Memory_RTP_271_174Hz.bin"},
	{"alarm_Relieved_RTP_272_174Hz.bin"},
	{"alarm_Ripple_RTP_273_174Hz.bin"},
	{"alarm_Slowly_RTP_274_174Hz.bin"},
	{"alarm_spring_RTP_275_174Hz.bin"},
	{"alarm_Stars_RTP_276_174Hz.bin"},
	{"alarm_Surging_RTP_277_174Hz.bin"},
	{"alarm_tactfully_RTP_278_174Hz.bin"},
	{"alarm_The_wind_RTP_279_174Hz.bin"},
	{"alarm_Walking_in_the_rain_RTP_280_174Hz.bin"},
	{"BoHaoPanAnJian_281.bin"},
	{"BoHaoPanAnNiu_282.bin"},
	{"BoHaoPanKuaiJie_283.bin"},
	{"DianHuaGuaDuan_284.bin"},
	{"DianJinMoShiQieHuan_285.bin"},
	{"HuaDongTiaoZhenDong_286.bin"},
	{"LeiShen_287.bin"},
	{"XuanZhongYouXi_288.bin"},
	{"YeJianMoShiDaZi_289.bin"},
	{"YouXiSheZhiKuang_290.bin"},
	{"ZhuanYeMoShi_291.bin"},
	{"Climber_RTP_292_174Hz.bin"},
	{"Chase_RTP_293_174Hz.bin"},
	{"shuntai24k_rtp_294.bin"},
	{"wentai24k_rtp_295.bin"},
	{"20ms_RTP_296.bin"},
	{"40ms_RTP_297.bin"},
	{"60ms_RTP_298.bin"},
	{"80ms_RTP_299.bin"},
	{"100ms_RTP_300.bin"},
	{"120ms_RTP_301.bin"},
	{"140ms_RTP_302.bin"},
	{"160ms_RTP_303.bin"},
	{"180ms_RTP_304.bin"},
	{"200ms_RTP_305.bin"},
	{"220ms_RTP_306.bin"},
	{"240ms_RTP_307.bin"},
	{"260ms_RTP_308.bin"},
	{"280ms_RTP_309.bin"},
	{"300ms_RTP_310.bin"},
	{"320ms_RTP_311.bin"},
	{"340ms_RTP_312.bin"},
	{"360ms_RTP_313.bin"},
	{"380ms_RTP_314.bin"},
	{"400ms_RTP_315.bin"},
	{"420ms_RTP_316.bin"},
	{"440ms_RTP_317.bin"},
	{"460ms_RTP_318.bin"},
	{"480ms_RTP_319.bin"},
	{"500ms_RTP_320.bin"},
	{"AT500ms_RTP_321.bin"},

	{"aw8697_reserved_322.bin"},
	{"aw8697_reserved_323.bin"},
	{"aw8697_reserved_324.bin"},
	{"aw8697_reserved_325.bin"},
	{"aw8697_reserved_326.bin"},
	{"aw8697_reserved_327.bin"},
	{"aw8697_reserved_328.bin"},
	{"aw8697_reserved_329.bin"},
	{"aw8697_reserved_330.bin"},
	{"aw8697_reserved_331.bin"},
	{"aw8697_reserved_332.bin"},
	{"aw8697_reserved_333.bin"},
	{"aw8697_reserved_334.bin"},
	{"aw8697_reserved_335.bin"},
	{"aw8697_reserved_336.bin"},
	{"aw8697_reserved_337.bin"},
	{"aw8697_reserved_338.bin"},
	{"aw8697_reserved_339.bin"},
	{"aw8697_reserved_340.bin"},
	{"aw8697_reserved_341.bin"},
	{"aw8697_reserved_342.bin"},
	{"aw8697_reserved_343.bin"},
	{"aw8697_reserved_344.bin"},
	{"aw8697_reserved_345.bin"},
	{"aw8697_reserved_346.bin"},
	{"aw8697_reserved_347.bin"},
	{"aw8697_reserved_348.bin"},
	{"aw8697_reserved_349.bin"},
	{"aw8697_reserved_350.bin"},
	{"aw8697_reserved_351.bin"},
	{"aw8697_reserved_352.bin"},
	{"aw8697_reserved_353.bin"},
	{"aw8697_reserved_354.bin"},
	{"aw8697_reserved_355.bin"},
	{"aw8697_reserved_356.bin"},
	{"aw8697_reserved_357.bin"},
	{"aw8697_reserved_358.bin"},
	{"aw8697_reserved_359.bin"},
	{"aw8697_reserved_360.bin"},
	{"aw8697_reserved_361.bin"},
	{"aw8697_reserved_362.bin"},
	{"aw8697_programs_clean_RTP_363_174Hz.bin"},
	{"aw8697_reserved_364.bin"},
	{"aw8697_reserved_365.bin"},
	{"aw8697_reserved_366.bin"},
	{"aw8697_reserved_367.bin"},
	{"aw8697_reserved_368.bin"},
	{"aw8697_reserved_369.bin"},
	{"aw8697_reserved_370.bin"},

	/* Add for OS14 Start */
	{"aw8697_Nightsky_RTP_371_174Hz.bin"},
	{"aw8697_TheStars_RTP_372_174Hz.bin"},
	{"aw8697_TheSunrise_RTP_373_174Hz.bin"},
	{"aw8697_TheSunset_RTP_374_174Hz.bin"},
	{"aw8697_Meditate_RTP_375_174Hz.bin"},
	{"aw8697_Distant_RTP_376_174Hz.bin"},
	{"aw8697_Pond_RTP_377_174Hz.bin"},
	{"aw8697_Moonlotus_RTP_378_174Hz.bin"},
	{"aw8697_Ripplingwater_RTP_379_174Hz.bin"},
	{"aw8697_Shimmer_RTP_380_174Hz.bin"},
	{"aw8697_Batheearth_RTP_381_174Hz.bin"},
	{"aw8697_Junglemorning_RTP_382_174Hz.bin"},
	{"aw8697_Silver_RTP_383_174Hz.bin"},
	{"aw8697_Elegantquiet_RTP_384_174Hz.bin"},
	{"aw8697_Summerbeach_RTP_385_174Hz.bin"},
	{"aw8697_Summernight_RTP_386_174Hz.bin"},
	{"aw8697_Icesnow_RTP_387_174Hz.bin"},
	{"aw8697_Wintersnow_RTP_388_174Hz.bin"},
	{"aw8697_Rainforest_RTP_389_174Hz.bin"},
	{"aw8697_Raineverything_RTP_390_174Hz.bin"},
	{"aw8697_Staracross_RTP_391_174Hz.bin"},
	{"aw8697_Fullmoon_RTP_392_174Hz.bin"},
	{"aw8697_Clouds_RTP_393_174Hz.bin"},
	{"aw8697_Wonderland_RTP_394_174Hz.bin"},
	{"aw8697_Still_RTP_395_174Hz.bin"},
	{"aw8697_Haunting_RTP_396_174Hz.bin"},
	{"aw8697_Dragonfly_RTP_397_174Hz.bin"},
	{"aw8697_Dropwater_RTP_398_174Hz.bin"},
	{"aw8697_Fluctuation_RTP_399_174Hz.bin"},
	{"aw8697_Blow_RTP_400_174Hz.bin"},
	{"aw8697_Leaveslight_RTP_401_174Hz.bin"},
	{"aw8697_Warmsun_RTP_402_174Hz.bin"},
	{"aw8697_Snowflake_RTP_403_174Hz.bin"},
	{"aw8697_Crystalclear_RTP_404_174Hz.bin"},
	{"aw8697_Insects_RTP_405_174Hz.bin"},
	{"aw8697_Dew_RTP_406_174Hz.bin"},
	{"aw8697_Shine_RTP_407_174Hz.bin"},
	{"aw8697_Frost_RTP_408_174Hz.bin"},
	{"aw8697_Rainsplash_RTP_409_174Hz.bin"},
	{"aw8697_Raindrop_RTP_410_174Hz.bin"},
	/* Add for OS14 End */
};

static char aw_rtp_name_178Hz[][AW_RTP_NAME_MAX] = {
	{"aw8697_rtp.bin"},
#ifdef OPLUS_FEATURE_CHG_BASIC
	{"aw8697_Hearty_channel_RTP_1.bin"},
	{"aw8697_Instant_channel_RTP_2_178Hz.bin"},
	{"aw8697_Music_channel_RTP_3.bin"},
	{"aw8697_Percussion_channel_RTP_4.bin"},
	{"aw8697_Ripple_channel_RTP_5.bin"},
	{"aw8697_Bright_channel_RTP_6.bin"},
	{"aw8697_Fun_channel_RTP_7.bin"},
	{"aw8697_Glittering_channel_RTP_8.bin"},
	{"aw8697_Granules_channel_RTP_9_178Hz.bin"},
	{"aw8697_Harp_channel_RTP_10.bin"},
	{"aw8697_Impression_channel_RTP_11.bin"},
	{"aw8697_Ingenious_channel_RTP_12_178Hz.bin"},
	{"aw8697_Joy_channel_RTP_13_178Hz.bin"},
	{"aw8697_Overtone_channel_RTP_14.bin"},
	{"aw8697_Receive_channel_RTP_15_178Hz.bin"},
	{"aw8697_Splash_channel_RTP_16_178Hz.bin"},
	{"aw8697_About_School_RTP_17_178Hz.bin"},
	{"aw8697_Bliss_RTP_18.bin"},
	{"aw8697_Childhood_RTP_19_178Hz.bin"},
	{"aw8697_Commuting_RTP_20_178Hz.bin"},
	{"aw8697_Dream_RTP_21.bin"},
	{"aw8697_Firefly_RTP_22_178Hz.bin"},
	{"aw8697_Gathering_RTP_23.bin"},
	{"aw8697_Gaze_RTP_24_178Hz.bin"},
	{"aw8697_Lakeside_RTP_25_178Hz.bin"},
	{"aw8697_Lifestyle_RTP_26.bin"},
	{"aw8697_Memories_RTP_27_178Hz.bin"},
	{"aw8697_Messy_RTP_28_178Hz.bin"},
	{"aw8697_Night_RTP_29_178Hz.bin"},
	{"aw8697_Passionate_Dance_RTP_30_178Hz.bin"},
	{"aw8697_Playground_RTP_31_178Hz.bin"},
	{"aw8697_Relax_RTP_32_178Hz.bin"},
	{"aw8697_Reminiscence_RTP_33.bin"},
	{"aw8697_Silence_From_Afar_RTP_34_178Hz.bin"},
	{"aw8697_Silence_RTP_35_178Hz.bin"},
	{"aw8697_Stars_RTP_36_178Hz.bin"},
	{"aw8697_Summer_RTP_37_178Hz.bin"},
	{"aw8697_Toys_RTP_38_178Hz.bin"},
	{"aw8697_Travel_RTP_39.bin"},
	{"aw8697_Vision_RTP_40.bin"},

	{"aw8697_waltz_channel_RTP_41_178Hz.bin"},
	{"aw8697_cut_channel_RTP_42_178Hz.bin"},
	{"aw8697_clock_channel_RTP_43_178Hz.bin"},
	{"aw8697_long_sound_channel_RTP_44_178Hz.bin"},
	{"aw8697_short_channel_RTP_45_178Hz.bin"},
	{"aw8697_two_error_remaind_RTP_46_178Hz.bin"},

	{"aw8697_kill_program_RTP_47_178Hz.bin"},
	{"aw8697_Simple_channel_RTP_48.bin"},
	{"aw8697_Pure_RTP_49_178Hz.bin"},
	{"aw8697_reserved_sound_channel_RTP_50.bin"},

	{"aw8697_high_temp_high_humidity_channel_RTP_51.bin"},
	{"aw8697_old_steady_test_RTP_52.bin"},
	{"aw8697_listen_pop_53.bin"},
	{"aw8697_desk_7_RTP_54_178Hz.bin"},
	{"aw8697_nfc_10_RTP_55_178Hz.bin"},
	{"aw8697_vibrator_remain_12_RTP_56.bin"},
	{"aw8697_notice_13_RTP_57.bin"},
	{"aw8697_third_ring_14_RTP_58.bin"},
	{"aw8697_reserved_59.bin"},

	{"aw8697_honor_fisrt_kill_RTP_60_178Hz.bin"},
	{"aw8697_honor_two_kill_RTP_61_178Hz.bin"},
	{"aw8697_honor_three_kill_RTP_62_178Hz.bin"},
	{"aw8697_honor_four_kill_RTP_63_178Hz.bin"},
	{"aw8697_honor_five_kill_RTP_64_178Hz.bin"},
	{"aw8697_honor_three_continu_kill_RTP_65_178Hz.bin"},
	{"aw8697_honor_four_continu_kill_RTP_66_178Hz.bin"},
	{"aw8697_honor_unstoppable_RTP_67_178Hz.bin"},
	{"aw8697_honor_thousands_kill_RTP_68_178Hz.bin"},
	{"aw8697_honor_lengendary_RTP_69_178Hz.bin"},

	{"aw8697_Freshmorning_RTP_70_178Hz.bin"},
	{"aw8697_Peaceful_RTP_71_178Hz.bin"},
	{"aw8697_Cicada_RTP_72_178Hz.bin"},
	{"aw8697_Electronica_RTP_73_178Hz.bin"},
	{"aw8697_Holiday_RTP_74_178Hz.bin"},
	{"aw8697_Funk_RTP_75_178Hz.bin"},
	{"aw8697_House_RTP_76_178Hz.bin"},
	{"aw8697_Temple_RTP_77_178Hz.bin"},
	{"aw8697_Dreamyjazz_RTP_78_178Hz.bin"},
	{"aw8697_Modern_RTP_79_178Hz.bin"},

	{"aw8697_Round_RTP_80_178Hz.bin"},
	{"aw8697_Rising_RTP_81_178Hz.bin"},
	{"aw8697_Wood_RTP_82_178Hz.bin"},
	{"aw8697_Heys_RTP_83_178Hz.bin"},
	{"aw8697_Mbira_RTP_84_178Hz.bin"},
	{"aw8697_News_RTP_85_178Hz.bin"},
	{"aw8697_Peak_RTP_86_178Hz.bin"},
	{"aw8697_Crisp_RTP_87_178Hz.bin"},
	{"aw8697_Singingbowls_RTP_88_178Hz.bin"},
	{"aw8697_Bounce_RTP_89_178Hz.bin"},

	{"aw8697_reserved_90.bin"},
	{"aw8697_reserved_91.bin"},
	{"aw8697_reserved_92.bin"},
	{"aw8697_reserved_93.bin"},
	{"aw8697_ALCloudscape_94_178HZ.bin"},
	{"aw8697_ALGoodenergy_95_178HZ.bin"},
	{"aw8697_NTblink_96_178HZ.bin"},
	{"aw8697_NTwhoop_97_178HZ.bin"},
	{"aw8697_Newfeeling_98_178HZ.bin"},
	{"aw8697_nature_99_178HZ.bin"},

	{"aw8697_soldier_first_kill_RTP_100.bin"},
	{"aw8697_soldier_second_kill_RTP_101.bin"},
	{"aw8697_soldier_third_kill_RTP_102.bin"},
	{"aw8697_soldier_fourth_kill_RTP_103.bin"},
	{"aw8697_soldier_fifth_kill_RTP_104_178Hz.bin"},
	{"aw8697_stepable_regulate_RTP_105.bin"},
	{"aw8697_voice_level_bar_edge_RTP_106_178Hz.bin"},
	{"aw8697_strength_level_bar_edge_RTP_107_178Hz.bin"},
	{"aw8697_charging_simulation_RTP_108_178Hz.bin"},
	{"aw8697_fingerprint_success_RTP_109_178Hz.bin"},

	{"aw8697_fingerprint_effect1_RTP_110.bin"},
	{"aw8697_fingerprint_effect2_RTP_111.bin"},
	{"aw8697_fingerprint_effect3_RTP_112.bin"},
	{"aw8697_fingerprint_effect4_RTP_113.bin"},
	{"aw8697_fingerprint_effect5_RTP_114.bin"},
	{"aw8697_fingerprint_effect6_RTP_115.bin"},
	{"aw8697_fingerprint_effect7_RTP_116.bin"},
	{"aw8697_fingerprint_effect8_RTP_117.bin"},
	{"aw8697_breath_simulation_RTP_118_178Hz.bin"},
	{"aw8697_Telcel_Torreblanca_RTP_119_178Hz.bin"},

	{"aw8697_Miss_RTP_120.bin"},
	{"aw8697_Scenic_RTP_121_178Hz.bin"},
	{"aw8697_voice_assistant_RTP_122_178Hz.bin"},
	{"aw8697_Appear_channel_RTP_123_178Hz.bin"},
	{"aw8697_Miss_RTP_124_178Hz.bin"},
	{"aw8697_Music_channel_RTP_125_178Hz.bin"},
	{"aw8697_Percussion_channel_RTP_126_178Hz.bin"},
	{"aw8697_Ripple_channel_RTP_127_178Hz.bin"},
	{"aw8697_Bright_channel_RTP_128_178Hz.bin"},
	{"aw8697_Fun_channel_RTP_129_178Hz.bin"},
	{"aw8697_Glittering_channel_RTP_130_178Hz.bin"},
	{"aw8697_Harp_channel_RTP_131_178Hz.bin"},
	{"aw8697_Overtone_channel_RTP_132_178Hz.bin"},
	{"aw8697_Simple_channel_RTP_133_178Hz.bin"},
	{"aw8697_Seine_past_RTP_134_178Hz.bin"},
	{"aw8697_Classical_ring_RTP_135_178Hz.bin"},
	{"aw8697_Long_for_RTP_136_178Hz.bin"},
	{"aw8697_Romantic_RTP_137_178Hz.bin"},
	{"aw8697_Bliss_RTP_138_178Hz.bin"},
	{"aw8697_Dream_RTP_139_178Hz.bin"},
	{"aw8697_Relax_RTP_140_178Hz.bin"},
	{"aw8697_Joy_channel_RTP_141_178Hz.bin"},
	{"aw8697_weather_wind_RTP_142.bin"},
	{"aw8697_weather_cloudy_RTP_143.bin"},
	{"aw8697_weather_thunderstorm_RTP_144.bin"},
	{"aw8697_weather_default_RTP_145.bin"},
	{"aw8697_weather_sunny_RTP_146.bin"},
	{"aw8697_weather_smog_RTP_147.bin"},
	{"aw8697_weather_snow_RTP_148.bin"},
	{"aw8697_weather_rain_RTP_149.bin"},
#endif

	{"aw8697_Master_Notification_RTP_150_178Hz.bin"},
	{"aw8697_Master_Artist_Ringtong_RTP_151_178Hz.bin"},
	{"aw8697_Master_Text_RTP_152_178Hz.bin"},
	{"aw8697_Master_Artist_Alarm_RTP_153_178Hz.bin"},
	{"aw8697_reserved_154.bin"},
	{"aw8697_reserved_155.bin"},
	{"aw8697_reserved_156.bin"},
	{"aw8697_reserved_157.bin"},
	{"aw8697_reserved_158.bin"},
	{"aw8697_reserved_159.bin"},
	{"aw8697_reserved_160.bin"},

	{"aw8697_oplus_its_oplus_RTP_161_178Hz.bin"},
	{"aw8697_oplus_tune_RTP_162_178Hz.bin"},
	{"aw8697_oplus_jingle_RTP_163_178Hz.bin"},
	{"aw8697_reserved_164.bin"},
	{"aw8697_reserved_165.bin"},
	{"aw8697_reserved_166.bin"},
	{"aw8697_reserved_167.bin"},
	{"aw8697_reserved_168.bin"},
	{"aw8697_reserved_169.bin"},
	{"aw8697_oplus_gt_RTP_170_178Hz.bin"},

	{"aw8697_Threefingers_Long_RTP_171.bin"},
	{"aw8697_Threefingers_Up_RTP_172.bin"},
	{"aw8697_Threefingers_Screenshot_RTP_173.bin"},
	{"aw8697_Unfold_RTP_174.bin"},
	{"aw8697_Close_RTP_175.bin"},
	{"aw8697_HalfLap_RTP_176.bin"},
	{"aw8697_Twofingers_Down_RTP_177.bin"},
	{"aw8697_Twofingers_Long_RTP_178.bin"},
	{"aw8697_Compatible_1_RTP_179.bin"},
	{"aw8697_Compatible_2_RTP_180.bin"},
	{"aw8697_Styleswitch_RTP_181.bin"},
	{"aw8697_Waterripple_RTP_182.bin"},
	{"aw8697_Suspendbutton_Bottomout_RTP_183.bin"},
	{"aw8697_Suspendbutton_Menu_RTP_184.bin"},
	{"aw8697_Complete_RTP_185.bin"},
	{"aw8697_Bulb_RTP_186.bin"},
	{"aw8697_Elasticity_RTP_187.bin"},
	{"aw8697_reserved_188.bin"},
	{"aw8697_reserved_189.bin"},
	{"aw8697_reserved_190.bin"},
	{"aw8697_reserved_191.bin"},
	{"aw8697_reserved_192.bin"},
	{"aw8697_reserved_193.bin"},
	{"aw8697_reserved_194.bin"},
	{"aw8697_reserved_195.bin"},
	{"aw8697_reserved_196.bin"},
	{"aw8697_reserved_197.bin"},
	{"alarm_Pacman_RTP_198.bin"},
	{"notif_Pacman_RTP_199.bin"},
	{"ringtone_Pacman_RTP_200.bin"},
	{"ringtone_Alacrity_RTP_201_178Hz.bin"},
	{"ring_Amenity_RTP_202_178Hz.bin"},
	{"ringtone_Blues_RTP_203_178Hz.bin"},
	{"ring_Bounce_RTP_204_178Hz.bin"},
	{"ring_Calm_RTP_205_178Hz.bin"},
	{"ringtone_Cloud_RTP_206_178Hz.bin"},
	{"ringtone_Cyclotron_RTP_207_178Hz.bin"},
	{"ringtone_Distinct_RTP_208_178Hz.bin"},
	{"ringtone_Dynamic_RTP_209_178Hz.bin"},
	{"ringtone_Echo_RTP_210_178Hz.bin"},
	{"ringtone_Expect_RTP_211_178Hz.bin"},
	{"ringtone_Fanatical_RTP_212_178Hz.bin"},
	{"ringtone_Funky_RTP_213_178Hz.bin"},
	{"ringtone_Guitar_RTP_214_178Hz.bin"},
	{"ringtone_Harping_RTP_215_178Hz.bin"},
	{"ringtone_Highlight_RTP_216_178Hz.bin"},
	{"ringtone_Idyl_RTP_217_178Hz.bin"},
	{"ringtone_Innocence_RTP_218_178Hz.bin"},
	{"ringtone_Journey_RTP_219_178Hz.bin"},
	{"ringtone_Joyous_RTP_220_178Hz.bin"},
	{"ring_Lazy_RTP_221_178Hz.bin"},
	{"ringtone_Marimba_RTP_222_178Hz.bin"},
	{"ring_Mystical_RTP_223_178Hz.bin"},
	{"ringtone_Old_telephone_RTP_224_178Hz.bin"},
	{"ringtone_Oneplus_tune_RTP_225_178Hz.bin"},
	{"ringtone_Rhythm_RTP_226_178Hz.bin"},
	{"ringtone_Optimistic_RTP_227_178Hz.bin"},
	{"ringtone_Piano_RTP_228_178Hz.bin"},
	{"ring_Whirl_RTP_229_178Hz.bin"},
	{"VZW_Alrwave_RTP_230.bin"},
	{"t-jingle_RTP_231.bin"},
	{"ringtone_Eager_232_178Hz.bin"},
	{"ringtone_Ebullition_233_178Hz.bin"},
	{"ringtone_Friendship_234_178Hz.bin"},
	{"ringtone_Jazz_life_RTP_235_178Hz.bin"},
	{"ringtone_Sun_glittering_RTP_236_178Hz.bin"},
	{"notif_Allay_RTP_237_178Hz.bin"},
	{"notif_Allusion_RTP_238_178Hz.bin"},
	{"notif_Amiable_RTP_239_178Hz.bin"},
	{"notif_Blare_RTP_240_178Hz.bin"},
	{"notif_Blissful_RTP_241_178Hz.bin"},
	{"notif_Brisk_RTP_242_178Hz.bin"},
	{"notif_Bubble_RTP_243_178Hz.bin"},
	{"notif_Cheerful_RTP_244_178Hz.bin"},
	{"notif_Clear_RTP_245_178Hz.bin"},
	{"notif_Comely_RTP_246_178Hz.bin"},
	{"notif_Cozy_RTP_247_178Hz.bin"},
	{"notif_Ding_RTP_248_178Hz.bin"},
	{"notif_Effervesce_RTP_249_178Hz.bin"},
	{"notif_Elegant_RTP_250_178Hz.bin"},
	{"notif_Free_RTP_251_178Hz.bin"},
	{"notif_Hallucination_RTP_252_178Hz.bin"},
	{"notif_Inbound_RTP_253_178Hz.bin"},
	{"notif_Light_RTP_254_178Hz.bin"},
	{"notif_Meet_RTP_255_178Hz.bin"},
	{"notif_Naivety_RTP_256_178Hz.bin"},
	{"notif_Quickly_RTP_257_178Hz.bin"},
	{"notif_Rhythm_RTP_258_178Hz.bin"},
	{"notif_Surprise_RTP_259_178Hz.bin"},
	{"notif_Twinkle_RTP_260_178Hz.bin"},
	{"Version_Alert_RTP_261.bin"},
	{"alarm_Alarm_clock_RTP_262_178Hz.bin"},
	{"alarm_Beep_RTP_263_178Hz.bin"},
	{"alarm_Breeze_RTP_264_178Hz.bin"},
	{"alarm_Dawn_RTP_265_178Hz.bin"},
	{"alarm_Dream_RTP_266_178Hz.bin"},
	{"alarm_Fluttering_RTP_267_178Hz.bin"},
	{"alarm_Flyer_RTP_268_178Hz.bin"},
	{"alarm_Interesting_RTP_269_178Hz.bin"},
	{"alarm_Leisurely_RTP_270_178Hz.bin"},
	{"alarm_Memory_RTP_271_178Hz.bin"},
	{"alarm_Relieved_RTP_272_178Hz.bin"},
	{"alarm_Ripple_RTP_273_178Hz.bin"},
	{"alarm_Slowly_RTP_274_178Hz.bin"},
	{"alarm_spring_RTP_275_178Hz.bin"},
	{"alarm_Stars_RTP_276_178Hz.bin"},
	{"alarm_Surging_RTP_277_178Hz.bin"},
	{"alarm_tactfully_RTP_278_178Hz.bin"},
	{"alarm_The_wind_RTP_279_178Hz.bin"},
	{"alarm_Walking_in_the_rain_RTP_280_178Hz.bin"},
	{"BoHaoPanAnJian_281.bin"},
	{"BoHaoPanAnNiu_282.bin"},
	{"BoHaoPanKuaiJie_283.bin"},
	{"DianHuaGuaDuan_284.bin"},
	{"DianJinMoShiQieHuan_285.bin"},
	{"HuaDongTiaoZhenDong_286.bin"},
	{"LeiShen_287.bin"},
	{"XuanZhongYouXi_288.bin"},
	{"YeJianMoShiDaZi_289.bin"},
	{"YouXiSheZhiKuang_290.bin"},
	{"ZhuanYeMoShi_291.bin"},
	{"Climber_RTP_292_178Hz.bin"},
	{"Chase_RTP_293_178Hz.bin"},
	{"shuntai24k_rtp_294.bin"},
	{"wentai24k_rtp_295.bin"},
	{"20ms_RTP_296.bin"},
	{"40ms_RTP_297.bin"},
	{"60ms_RTP_298.bin"},
	{"80ms_RTP_299.bin"},
	{"100ms_RTP_300.bin"},
	{"120ms_RTP_301.bin"},
	{"140ms_RTP_302.bin"},
	{"160ms_RTP_303.bin"},
	{"180ms_RTP_304.bin"},
	{"200ms_RTP_305.bin"},
	{"220ms_RTP_306.bin"},
	{"240ms_RTP_307.bin"},
	{"260ms_RTP_308.bin"},
	{"280ms_RTP_309.bin"},
	{"300ms_RTP_310.bin"},
	{"320ms_RTP_311.bin"},
	{"340ms_RTP_312.bin"},
	{"360ms_RTP_313.bin"},
	{"380ms_RTP_314.bin"},
	{"400ms_RTP_315.bin"},
	{"420ms_RTP_316.bin"},
	{"440ms_RTP_317.bin"},
	{"460ms_RTP_318.bin"},
	{"480ms_RTP_319.bin"},
	{"500ms_RTP_320.bin"},
	{"AT500ms_RTP_321.bin"},

	{"aw8697_reserved_322.bin"},
	{"aw8697_reserved_323.bin"},
	{"aw8697_reserved_324.bin"},
	{"aw8697_reserved_325.bin"},
	{"aw8697_reserved_326.bin"},
	{"aw8697_reserved_327.bin"},
	{"aw8697_reserved_328.bin"},
	{"aw8697_reserved_329.bin"},
	{"aw8697_reserved_330.bin"},
	{"aw8697_reserved_331.bin"},
	{"aw8697_reserved_332.bin"},
	{"aw8697_reserved_333.bin"},
	{"aw8697_reserved_334.bin"},
	{"aw8697_reserved_335.bin"},
	{"aw8697_reserved_336.bin"},
	{"aw8697_reserved_337.bin"},
	{"aw8697_reserved_338.bin"},
	{"aw8697_reserved_339.bin"},
	{"aw8697_reserved_340.bin"},
	{"aw8697_reserved_341.bin"},
	{"aw8697_reserved_342.bin"},
	{"aw8697_reserved_343.bin"},
	{"aw8697_reserved_344.bin"},
	{"aw8697_reserved_345.bin"},
	{"aw8697_reserved_346.bin"},
	{"aw8697_reserved_347.bin"},
	{"aw8697_reserved_348.bin"},
	{"aw8697_reserved_349.bin"},
	{"aw8697_reserved_350.bin"},
	{"aw8697_reserved_351.bin"},
	{"aw8697_reserved_352.bin"},
	{"aw8697_reserved_353.bin"},
	{"aw8697_reserved_354.bin"},
	{"aw8697_reserved_355.bin"},
	{"aw8697_reserved_356.bin"},
	{"aw8697_reserved_357.bin"},
	{"aw8697_reserved_358.bin"},
	{"aw8697_reserved_359.bin"},
	{"aw8697_reserved_360.bin"},
	{"aw8697_reserved_361.bin"},
	{"aw8697_reserved_362.bin"},
	{"aw8697_programs_clean_RTP_363_178Hz.bin"},
	{"aw8697_reserved_364.bin"},
	{"aw8697_reserved_365.bin"},
	{"aw8697_reserved_366.bin"},
	{"aw8697_reserved_367.bin"},
	{"aw8697_reserved_368.bin"},
	{"aw8697_reserved_369.bin"},
	{"aw8697_reserved_370.bin"},

	/* Add for OS14 Start */
	{"aw8697_Nightsky_RTP_371_178Hz.bin"},
	{"aw8697_TheStars_RTP_372_178Hz.bin"},
	{"aw8697_TheSunrise_RTP_373_178Hz.bin"},
	{"aw8697_TheSunset_RTP_374_178Hz.bin"},
	{"aw8697_Meditate_RTP_375_178Hz.bin"},
	{"aw8697_Distant_RTP_376_178Hz.bin"},
	{"aw8697_Pond_RTP_377_178Hz.bin"},
	{"aw8697_Moonlotus_RTP_378_178Hz.bin"},
	{"aw8697_Ripplingwater_RTP_379_178Hz.bin"},
	{"aw8697_Shimmer_RTP_380_178Hz.bin"},
	{"aw8697_Batheearth_RTP_381_178Hz.bin"},
	{"aw8697_Junglemorning_RTP_382_178Hz.bin"},
	{"aw8697_Silver_RTP_383_178Hz.bin"},
	{"aw8697_Elegantquiet_RTP_384_178Hz.bin"},
	{"aw8697_Summerbeach_RTP_385_178Hz.bin"},
	{"aw8697_Summernight_RTP_386_178Hz.bin"},
	{"aw8697_Icesnow_RTP_387_178Hz.bin"},
	{"aw8697_Wintersnow_RTP_388_178Hz.bin"},
	{"aw8697_Rainforest_RTP_389_178Hz.bin"},
	{"aw8697_Raineverything_RTP_390_178Hz.bin"},
	{"aw8697_Staracross_RTP_391_178Hz.bin"},
	{"aw8697_Fullmoon_RTP_392_178Hz.bin"},
	{"aw8697_Clouds_RTP_393_178Hz.bin"},
	{"aw8697_Wonderland_RTP_394_178Hz.bin"},
	{"aw8697_Still_RTP_395_178Hz.bin"},
	{"aw8697_Haunting_RTP_396_178Hz.bin"},
	{"aw8697_Dragonfly_RTP_397_178Hz.bin"},
	{"aw8697_Dropwater_RTP_398_178Hz.bin"},
	{"aw8697_Fluctuation_RTP_399_178Hz.bin"},
	{"aw8697_Blow_RTP_400_178Hz.bin"},
	{"aw8697_Leaveslight_RTP_401_178Hz.bin"},
	{"aw8697_Warmsun_RTP_402_178Hz.bin"},
	{"aw8697_Snowflake_RTP_403_178Hz.bin"},
	{"aw8697_Crystalclear_RTP_404_178Hz.bin"},
	{"aw8697_Insects_RTP_405_178Hz.bin"},
	{"aw8697_Dew_RTP_406_178Hz.bin"},
	{"aw8697_Shine_RTP_407_178Hz.bin"},
	{"aw8697_Frost_RTP_408_178Hz.bin"},
	{"aw8697_Rainsplash_RTP_409_178Hz.bin"},
	{"aw8697_Raindrop_RTP_410_178Hz.bin"},
	/* Add for OS14 End */
};
#endif

#ifndef KERNEL_VERSION_510
static char aw_rtp_name_165Hz[][AW_RTP_NAME_MAX] = {
	{"aw8697_rtp.bin"},
	{"aw8697_Hearty_channel_RTP_1.bin"},
	{"aw8697_Instant_channel_RTP_2_165Hz.bin"},
	{"aw8697_Music_channel_RTP_3.bin"},
	{"aw8697_Percussion_channel_RTP_4.bin"},
	{"aw8697_Ripple_channel_RTP_5.bin"},
	{"aw8697_Bright_channel_RTP_6.bin"},
	{"aw8697_Fun_channel_RTP_7.bin"},
	{"aw8697_Glittering_channel_RTP_8.bin"},
	{"aw8697_Granules_channel_RTP_9_165Hz.bin"},
	{"aw8697_Harp_channel_RTP_10.bin"},
	{"aw8697_Impression_channel_RTP_11.bin"},
	{"aw8697_Ingenious_channel_RTP_12_165Hz.bin"},
	{"aw8697_Joy_channel_RTP_13_165Hz.bin"},
	{"aw8697_Overtone_channel_RTP_14.bin"},
	{"aw8697_Receive_channel_RTP_15_165Hz.bin"},
	{"aw8697_Splash_channel_RTP_16_165Hz.bin"},

	{"aw8697_About_School_RTP_17_165Hz.bin"},
	{"aw8697_Bliss_RTP_18.bin"},
	{"aw8697_Childhood_RTP_19_165Hz.bin"},
	{"aw8697_Commuting_RTP_20_165Hz.bin"},
	{"aw8697_Dream_RTP_21.bin"},
	{"aw8697_Firefly_RTP_22_165Hz.bin"},
	{"aw8697_Gathering_RTP_23.bin"},
	{"aw8697_Gaze_RTP_24_165Hz.bin"},
	{"aw8697_Lakeside_RTP_25_165Hz.bin"},
	{"aw8697_Lifestyle_RTP_26.bin"},
	{"aw8697_Memories_RTP_27_165Hz.bin"},
	{"aw8697_Messy_RTP_28_165Hz.bin"},
	{"aw8697_Night_RTP_29_165Hz.bin"},
	{"aw8697_Passionate_Dance_RTP_30_165Hz.bin"},
	{"aw8697_Playground_RTP_31_165Hz.bin"},
	{"aw8697_Relax_RTP_32_165Hz.bin"},
	{"aw8697_Reminiscence_RTP_33.bin"},
	{"aw8697_Silence_From_Afar_RTP_34_165Hz.bin"},
	{"aw8697_Silence_RTP_35_165Hz.bin"},
	{"aw8697_Stars_RTP_36_165Hz.bin"},
	{"aw8697_Summer_RTP_37_165Hz.bin"},
	{"aw8697_Toys_RTP_38_165Hz.bin"},
	{"aw8697_Travel_RTP_39.bin"},
	{"aw8697_Vision_RTP_40.bin"},

	{"aw8697_waltz_channel_RTP_41_165Hz.bin"},
	{"aw8697_cut_channel_RTP_42_165Hz.bin"},
	{"aw8697_clock_channel_RTP_43_165Hz.bin"},
	{"aw8697_long_sound_channel_RTP_44_165Hz.bin"},
	{"aw8697_short_channel_RTP_45_165Hz.bin"},
	{"aw8697_two_error_remaind_RTP_46_165Hz.bin"},

	{"aw8697_kill_program_RTP_47_165Hz.bin"},
	{"aw8697_Simple_channel_RTP_48.bin"},
	{"aw8697_Pure_RTP_49_165Hz.bin"},
	{"aw8697_reserved_sound_channel_RTP_50.bin"},

	{"aw8697_high_temp_high_humidity_channel_RTP_51.bin"},

	{"aw8697_old_steady_test_RTP_52.bin"},
	{"aw8697_listen_pop_53.bin"},
	{"aw8697_desk_7_RTP_54_165Hz.bin"},
	{"aw8697_nfc_10_RTP_55_165Hz.bin"},
	{"aw8697_vibrator_remain_12_RTP_56_165Hz.bin"},
	{"aw8697_notice_13_RTP_57.bin"},
	{"aw8697_third_ring_14_RTP_58.bin"},
	{"aw8697_reserved_59.bin"},

	{"aw8697_honor_fisrt_kill_RTP_60_165Hz.bin"},
	{"aw8697_honor_two_kill_RTP_61_165Hz.bin"},
	{"aw8697_honor_three_kill_RTP_62_165Hz.bin"},
	{"aw8697_honor_four_kill_RTP_63_165Hz.bin"},
	{"aw8697_honor_five_kill_RTP_64_165Hz.bin"},
	{"aw8697_honor_three_continu_kill_RTP_65_165Hz.bin"},
	{"aw8697_honor_four_continu_kill_RTP_66_165Hz.bin"},
	{"aw8697_honor_unstoppable_RTP_67_165Hz.bin"},
	{"aw8697_honor_thousands_kill_RTP_68_165Hz.bin"},
	{"aw8697_honor_lengendary_RTP_69_165Hz.bin"},


	{"aw8697_reserved_70.bin"},
	{"aw8697_reserved_71.bin"},
	{"aw8697_reserved_72.bin"},
	{"aw8697_reserved_73.bin"},
	{"aw8697_reserved_74.bin"},
	{"aw8697_reserved_75.bin"},
	{"aw8697_reserved_76.bin"},
	{"aw8697_reserved_77.bin"},
	{"aw8697_reserved_78.bin"},
	{"aw8697_reserved_79.bin"},

	{"aw8697_reserved_80.bin"},
	{"aw8697_reserved_81.bin"},
	{"aw8697_reserved_82.bin"},
	{"aw8697_reserved_83.bin"},
	{"aw8697_reserved_84.bin"},
	{"aw8697_reserved_85.bin"},
	{"aw8697_reserved_86.bin"},
	{"aw8697_reserved_87.bin"},
	{"aw8697_reserved_88.bin"},
	{"aw8697_reserved_89.bin"},

	{"aw8697_reserved_90.bin"},
	{"aw8697_reserved_91.bin"},
	{"aw8697_reserved_92.bin"},
	{"aw8697_reserved_93.bin"},
	{"aw8697_ALCloudscape_94_165HZ.bin"},
	{"aw8697_ALGoodenergy_95_165HZ.bin"},
	{"aw8697_NTblink_96_165HZ.bin"},
	{"aw8697_NTwhoop_97_165HZ.bin"},
	{"aw8697_Newfeeling_98_165HZ.bin"},
	{"aw8697_nature_99_165HZ.bin"},


	{"aw8697_soldier_first_kill_RTP_100_165Hz.bin"},
	{"aw8697_soldier_second_kill_RTP_101_165Hz.bin"},
	{"aw8697_soldier_third_kill_RTP_102_165Hz.bin"},
	{"aw8697_soldier_fourth_kill_RTP_103_165Hz.bin"},
	{"aw8697_soldier_fifth_kill_RTP_104_165Hz.bin"},
	{"aw8697_stepable_regulate_RTP_105.bin"},
	{"aw8697_voice_level_bar_edge_RTP_106.bin"},
	{"aw8697_strength_level_bar_edge_RTP_107.bin"},
	{"aw8697_charging_simulation_RTP_108.bin"},
	{"aw8697_fingerprint_success_RTP_109.bin"},

	{"aw8697_fingerprint_effect1_RTP_110.bin"},
	{"aw8697_fingerprint_effect2_RTP_111.bin"},
	{"aw8697_fingerprint_effect3_RTP_112.bin"},
	{"aw8697_fingerprint_effect4_RTP_113.bin"},
	{"aw8697_fingerprint_effect5_RTP_114.bin"},
	{"aw8697_fingerprint_effect6_RTP_115.bin"},
	{"aw8697_fingerprint_effect7_RTP_116.bin"},
	{"aw8697_fingerprint_effect8_RTP_117.bin"},
	{"aw8697_breath_simulation_RTP_118.bin"},
	{"aw8697_reserved_119.bin"},

	{"aw8697_Miss_RTP_120.bin"},
	{"aw8697_Scenic_RTP_121_165Hz.bin"},
	{"aw8697_voice_assistant_RTP_122.bin"},
/* used for 7 */
	{"aw8697_Appear_channel_RTP_123_165Hz.bin"},
	{"aw8697_Miss_RTP_124_165Hz.bin"},
	{"aw8697_Music_channel_RTP_125_165Hz.bin"},
	{"aw8697_Percussion_channel_RTP_126_165Hz.bin"},
	{"aw8697_Ripple_channel_RTP_127_165Hz.bin"},
	{"aw8697_Bright_channel_RTP_128_165Hz.bin"},
	{"aw8697_Fun_channel_RTP_129_165Hz.bin"},
	{"aw8697_Glittering_channel_RTP_130_165Hz.bin"},
	{"aw8697_Harp_channel_RTP_131_165Hz.bin"},
	{"aw8697_Overtone_channel_RTP_132_165Hz.bin"},
	{"aw8697_Simple_channel_RTP_133_165Hz.bin"},

	{"aw8697_Seine_past_RTP_134_165Hz.bin"},
	{"aw8697_Classical_ring_RTP_135_165Hz.bin"},
	{"aw8697_Long_for_RTP_136_165Hz.bin"},
	{"aw8697_Romantic_RTP_137_165Hz.bin"},
	{"aw8697_Bliss_RTP_138_165Hz.bin"},
	{"aw8697_Dream_RTP_139_165Hz.bin"},
	{"aw8697_Relax_RTP_140_165Hz.bin"},
	{"aw8697_Joy_channel_RTP_141_165Hz.bin"},
	{"aw8697_weather_wind_RTP_142_165Hz.bin"},
	{"aw8697_weather_cloudy_RTP_143_165Hz.bin"},
	{"aw8697_weather_thunderstorm_RTP_144_165Hz.bin"},
	{"aw8697_weather_default_RTP_145_165Hz.bin"},
	{"aw8697_weather_sunny_RTP_146_165Hz.bin"},
	{"aw8697_weather_smog_RTP_147_165Hz.bin"},
	{"aw8697_weather_snow_RTP_148_165Hz.bin"},
	{"aw8697_weather_rain_RTP_149_165Hz.bin"},

/* used for 7 end*/
	{"aw8697_rtp_lighthouse.bin"},
	{"aw8697_rtp_silk.bin"},
	{"aw8697_reserved_152.bin"},
	{"aw8697_reserved_153.bin"},
	{"aw8697_reserved_154.bin"},
	{"aw8697_reserved_155.bin"},
	{"aw8697_reserved_156.bin"},
	{"aw8697_reserved_157.bin"},
	{"aw8697_reserved_158.bin"},
	{"aw8697_reserved_159.bin"},
	{"aw8697_reserved_160.bin"},

	{"aw8697_reserved_161.bin"},
	{"aw8697_reserved_162.bin"},
	{"aw8697_reserved_163.bin"},
	{"aw8697_reserved_164.bin"},
	{"aw8697_reserved_165.bin"},
	{"aw8697_reserved_166.bin"},
	{"aw8697_reserved_167.bin"},
	{"aw8697_reserved_168.bin"},
	{"aw8697_reserved_169.bin"},
	{"aw8697_reserved_170.bin"},
	{"aw8697_Threefingers_Long_RTP_171_165Hz.bin"},
	{"aw8697_Threefingers_Up_RTP_172_165Hz.bin"},
	{"aw8697_Threefingers_Screenshot_RTP_173_165Hz.bin"},
	{"aw8697_Unfold_RTP_174_165Hz.bin"},
	{"aw8697_Close_RTP_175_165Hz.bin"},
	{"aw8697_HalfLap_RTP_176_165Hz.bin"},
	{"aw8697_Twofingers_Down_RTP_177_165Hz.bin"},
	{"aw8697_Twofingers_Long_RTP_178_165Hz.bin"},
	{"aw8697_Compatible_1_RTP_179_165Hz.bin"},
	{"aw8697_Compatible_2_RTP_180_165Hz.bin"},
	{"aw8697_Styleswitch_RTP_181_165Hz.bin"},
	{"aw8697_Waterripple_RTP_182_165Hz.bin"},
	{"aw8697_Suspendbutton_Bottomout_RTP_183_165Hz.bin"},
	{"aw8697_Suspendbutton_Menu_RTP_184_165Hz.bin"},
	{"aw8697_Complete_RTP_185_165Hz.bin"},
	{"aw8697_Bulb_RTP_186_165Hz.bin"},
	{"aw8697_Elasticity_RTP_187_165Hz.bin"},
	{"aw8697_reserved_188.bin"},
	{"aw8697_reserved_189.bin"},
	{"aw8697_reserved_190.bin"},
	{"aw8697_reserved_191.bin"},
	{"aw8697_reserved_192.bin"},
	{"aw8697_reserved_193.bin"},
	{"aw8697_reserved_194.bin"},
	{"aw8697_reserved_195.bin"},
	{"aw8697_reserved_196.bin"},
	{"aw8697_reserved_197.bin"},
	{"alarm_Pacman_RTP_198.bin"},
	{"notif_Pacman_RTP_199.bin"},
	{"ringtone_Pacman_RTP_200.bin"},
	{"ringtone_Alacrity_RTP_201.bin"},
	{"ring_Amenity_RTP_202.bin"},
	{"ringtone_Blues_RTP_203.bin"},
	{"ring_Bounce_RTP_204.bin"},
	{"ring_Calm_RTP_205.bin"},
	{"ringtone_Cloud_RTP_206.bin"},
	{"ringtone_Cyclotron_RTP_207.bin"},
	{"ringtone_Distinct_RTP_208.bin"},
	{"ringtone_Dynamic_RTP_209.bin"},
	{"ringtone_Echo_RTP_210.bin"},
	{"ringtone_Expect_RTP_211.bin"},
	{"ringtone_Fanatical_RTP_212.bin"},
	{"ringtone_Funky_RTP_213.bin"},
	{"ringtone_Guitar_RTP_214.bin"},
	{"ringtone_Harping_RTP_215.bin"},
	{"ringtone_Highlight_RTP_216.bin"},
	{"ringtone_Idyl_RTP_217.bin"},
	{"ringtone_Innocence_RTP_218.bin"},
	{"ringtone_Journey_RTP_219.bin"},
	{"ringtone_Joyous_RTP_220.bin"},
	{"ring_Lazy_RTP_221.bin"},
	{"ringtone_Marimba_RTP_222.bin"},
	{"ring_Mystical_RTP_223.bin"},
	{"ringtone_Old_telephone_RTP_224.bin"},
	{"ringtone_Oneplus_tune_RTP_225.bin"},
	{"ringtone_Rhythm_RTP_226.bin"},
	{"ringtone_Optimistic_RTP_227.bin"},
	{"ringtone_Piano_RTP_228.bin"},
	{"ring_Whirl_RTP_229.bin"},
	{"VZW_Alrwave_RTP_230.bin"},
	{"t-jingle_RTP_231.bin"},
	{"ringtone_Eager_232.bin"},
	{"ringtone_Ebullition_233.bin"},
	{"ringtone_Friendship_234.bin"},
	{"ringtone_Jazz_life_RTP_235.bin"},
	{"ringtone_Sun_glittering_RTP_236.bin"},
	{"notif_Allay_RTP_237.bin"},
	{"notif_Allusion_RTP_238.bin"},
	{"notif_Amiable_RTP_239.bin"},
	{"notif_Blare_RTP_240.bin"},
	{"notif_Blissful_RTP_241.bin"},
	{"notif_Brisk_RTP_242.bin"},
	{"notif_Bubble_RTP_243.bin"},
	{"notif_Cheerful_RTP_244.bin"},
	{"notif_Clear_RTP_245.bin"},
	{"notif_Comely_RTP_246.bin"},
	{"notif_Cozy_RTP_247.bin"},
	{"notif_Ding_RTP_248.bin"},
	{"notif_Effervesce_RTP_249.bin"},
	{"notif_Elegant_RTP_250.bin"},
	{"notif_Free_RTP_251.bin"},
	{"notif_Hallucination_RTP_252.bin"},
	{"notif_Inbound_RTP_253.bin"},
	{"notif_Light_RTP_254.bin"},
	{"notif_Meet_RTP_255.bin"},
	{"notif_Naivety_RTP_256.bin"},
	{"notif_Quickly_RTP_257.bin"},
	{"notif_Rhythm_RTP_258.bin"},
	{"notif_Surprise_RTP_259.bin"},
	{"notif_Twinkle_RTP_260.bin"},
	{"Version_Alert_RTP_261.bin"},
	{"alarm_Alarm_clock_RTP_262.bin"},
	{"alarm_Beep_RTP_263.bin"},
	{"alarm_Breeze_RTP_264.bin"},
	{"alarm_Dawn_RTP_265.bin"},
	{"alarm_Dream_RTP_266.bin"},
	{"alarm_Fluttering_RTP_267.bin"},
	{"alarm_Flyer_RTP_268.bin"},
	{"alarm_Interesting_RTP_269.bin"},
	{"alarm_Leisurely_RTP_270.bin"},
	{"alarm_Memory_RTP_271.bin"},
	{"alarm_Relieved_RTP_272.bin"},
	{"alarm_Ripple_RTP_273.bin"},
	{"alarm_Slowly_RTP_274.bin"},
	{"alarm_spring_RTP_275.bin"},
	{"alarm_Stars_RTP_276.bin"},
	{"alarm_Surging_RTP_277.bin"},
	{"alarm_tactfully_RTP_278.bin"},
	{"alarm_The_wind_RTP_279.bin"},
	{"alarm_Walking_in_the_rain_RTP_280.bin"},
	{"BoHaoPanAnJian_281.bin"},
	{"BoHaoPanAnNiu_282.bin"},
	{"BoHaoPanKuaiJie_283.bin"},
	{"DianHuaGuaDuan_284.bin"},
	{"DianJinMoShiQieHuan_285.bin"},
	{"HuaDongTiaoZhenDong_286.bin"},
	{"LeiShen_287.bin"},
	{"XuanZhongYouXi_288.bin"},
	{"YeJianMoShiDaZi_289.bin"},
	{"YouXiSheZhiKuang_290.bin"},
	{"ZhuanYeMoShi_291.bin"},
	{"Climber_RTP_292.bin"},
	{"Chase_RTP_293.bin"},
	{"shuntai24k_rtp_294.bin"},
	{"wentai24k_rtp_295.bin"},
	{"20ms_RTP_296.bin"},
	{"40ms_RTP_297.bin"},
	{"60ms_RTP_298.bin"},
	{"80ms_RTP_299.bin"},
	{"100ms_RTP_300.bin"},
	{"120ms_RTP_301.bin"},
	{"140ms_RTP_302.bin"},
	{"160ms_RTP_303.bin"},
	{"180ms_RTP_304.bin"},
	{"200ms_RTP_305.bin"},
	{"220ms_RTP_306.bin"},
	{"240ms_RTP_307.bin"},
	{"260ms_RTP_308.bin"},
	{"280ms_RTP_309.bin"},
	{"300ms_RTP_310.bin"},
	{"320ms_RTP_311.bin"},
	{"340ms_RTP_312.bin"},
	{"360ms_RTP_313.bin"},
	{"380ms_RTP_314.bin"},
	{"400ms_RTP_315.bin"},
	{"420ms_RTP_316.bin"},
	{"440ms_RTP_317.bin"},
	{"460ms_RTP_318.bin"},
	{"480ms_RTP_319.bin"},
	{"500ms_RTP_320.bin"},
	{"AT500ms_RTP_321.bin"},
};
#endif

static char aw_rtp_name[][AW_RTP_NAME_MAX] = {
	{"aw8697_rtp.bin"},
#ifdef OPLUS_FEATURE_CHG_BASIC
	{"aw8697_Hearty_channel_RTP_1.bin"},
	{"aw8697_Instant_channel_RTP_2.bin"},
	{"aw8697_Music_channel_RTP_3.bin"},
	{"aw8697_Percussion_channel_RTP_4.bin"},
	{"aw8697_Ripple_channel_RTP_5.bin"},
	{"aw8697_Bright_channel_RTP_6.bin"},
	{"aw8697_Fun_channel_RTP_7.bin"},
	{"aw8697_Glittering_channel_RTP_8.bin"},
	{"aw8697_Granules_channel_RTP_9.bin"},
	{"aw8697_Harp_channel_RTP_10.bin"},
	{"aw8697_Impression_channel_RTP_11.bin"},
	{"aw8697_Ingenious_channel_RTP_12.bin"},
	{"aw8697_Joy_channel_RTP_13.bin"},
	{"aw8697_Overtone_channel_RTP_14.bin"},
	{"aw8697_Receive_channel_RTP_15.bin"},
	{"aw8697_Splash_channel_RTP_16.bin"},
	{"aw8697_About_School_RTP_17.bin"},
	{"aw8697_Bliss_RTP_18.bin"},
	{"aw8697_Childhood_RTP_19.bin"},
	{"aw8697_Commuting_RTP_20.bin"},
	{"aw8697_Dream_RTP_21.bin"},
	{"aw8697_Firefly_RTP_22.bin"},
	{"aw8697_Gathering_RTP_23.bin"},
	{"aw8697_Gaze_RTP_24.bin"},
	{"aw8697_Lakeside_RTP_25.bin"},
	{"aw8697_Lifestyle_RTP_26.bin"},
	{"aw8697_Memories_RTP_27.bin"},
	{"aw8697_Messy_RTP_28.bin"},
	{"aw8697_Night_RTP_29.bin"},
	{"aw8697_Passionate_Dance_RTP_30.bin"},
	{"aw8697_Playground_RTP_31.bin"},
	{"aw8697_Relax_RTP_32.bin"},
	{"aw8697_Reminiscence_RTP_33.bin"},
	{"aw8697_Silence_From_Afar_RTP_34.bin"},
	{"aw8697_Silence_RTP_35.bin"},
	{"aw8697_Stars_RTP_36.bin"},
	{"aw8697_Summer_RTP_37.bin"},
	{"aw8697_Toys_RTP_38.bin"},
	{"aw8697_Travel_RTP_39.bin"},
	{"aw8697_Vision_RTP_40.bin"},

	{"aw8697_waltz_channel_RTP_41.bin"},
	{"aw8697_cut_channel_RTP_42.bin"},
	{"aw8697_clock_channel_RTP_43.bin"},
	{"aw8697_long_sound_channel_RTP_44.bin"},
	{"aw8697_short_channel_RTP_45.bin"},
	{"aw8697_two_error_remaind_RTP_46.bin"},

	{"aw8697_kill_program_RTP_47.bin"},
	{"aw8697_Simple_channel_RTP_48.bin"},
	{"aw8697_Pure_RTP_49.bin"},
	{"aw8697_reserved_sound_channel_RTP_50.bin"},

	{"aw8697_high_temp_high_humidity_channel_RTP_51.bin"},
	{"aw8697_old_steady_test_RTP_52.bin"},
	{"aw8697_listen_pop_53.bin"},
	{"aw8697_desk_7_RTP_54.bin"},
	{"aw8697_nfc_10_RTP_55.bin"},
	{"aw8697_vibrator_remain_12_RTP_56.bin"},
	{"aw8697_notice_13_RTP_57.bin"},
	{"aw8697_third_ring_14_RTP_58.bin"},
	{"aw8697_reserved_59.bin"},

	{"aw8697_honor_fisrt_kill_RTP_60.bin"},
	{"aw8697_honor_two_kill_RTP_61.bin"},
	{"aw8697_honor_three_kill_RTP_62.bin"},
	{"aw8697_honor_four_kill_RTP_63.bin"},
	{"aw8697_honor_five_kill_RTP_64.bin"},
	{"aw8697_honor_three_continu_kill_RTP_65.bin"},
	{"aw8697_honor_four_continu_kill_RTP_66.bin"},
	{"aw8697_honor_unstoppable_RTP_67.bin"},
	{"aw8697_honor_thousands_kill_RTP_68.bin"},
	{"aw8697_honor_lengendary_RTP_69.bin"},

	{"aw8697_Freshmorning_RTP_70.bin"},
	{"aw8697_Peaceful_RTP_71.bin"},
	{"aw8697_Cicada_RTP_72.bin"},
	{"aw8697_Electronica_RTP_73.bin"},
	{"aw8697_Holiday_RTP_74.bin"},
	{"aw8697_Funk_RTP_75.bin"},
	{"aw8697_House_RTP_76.bin"},
	{"aw8697_Temple_RTP_77.bin"},
	{"aw8697_Dreamyjazz_RTP_78.bin"},
	{"aw8697_Modern_RTP_79.bin"},

	{"aw8697_Round_RTP_80.bin"},
	{"aw8697_Rising_RTP_81.bin"},
	{"aw8697_Wood_RTP_82.bin"},
	{"aw8697_Heys_RTP_83.bin"},
	{"aw8697_Mbira_RTP_84.bin"},
	{"aw8697_News_RTP_85.bin"},
	{"aw8697_Peak_RTP_86.bin"},
	{"aw8697_Crisp_RTP_87.bin"},
	{"aw8697_Singingbowls_RTP_88.bin"},
	{"aw8697_Bounce_RTP_89.bin"},

	{"aw8697_reserved_90.bin"},
	{"aw8697_reserved_91.bin"},
	{"aw8697_reserved_92.bin"},
	{"aw8697_reserved_93.bin"},
	{"aw8697_ALCloudscape_94_170HZ.bin"},
	{"aw8697_ALGoodenergy_95_170HZ.bin"},
	{"aw8697_NTblink_96_170HZ.bin"},
	{"aw8697_NTwhoop_97_170HZ.bin"},
	{"aw8697_Newfeeling_98_170HZ.bin"},
	{"aw8697_nature_99_170HZ.bin"},

	{"aw8697_soldier_first_kill_RTP_100.bin"},
	{"aw8697_soldier_second_kill_RTP_101.bin"},
	{"aw8697_soldier_third_kill_RTP_102.bin"},
	{"aw8697_soldier_fourth_kill_RTP_103.bin"},
	{"aw8697_soldier_fifth_kill_RTP_104.bin"},
	{"aw8697_stepable_regulate_RTP_105.bin"},
	{"aw8697_voice_level_bar_edge_RTP_106.bin"},
	{"aw8697_strength_level_bar_edge_RTP_107.bin"},
	{"aw8697_charging_simulation_RTP_108.bin"},
	{"aw8697_fingerprint_success_RTP_109.bin"},

	{"aw8697_fingerprint_effect1_RTP_110.bin"},
	{"aw8697_fingerprint_effect2_RTP_111.bin"},
	{"aw8697_fingerprint_effect3_RTP_112.bin"},
	{"aw8697_fingerprint_effect4_RTP_113.bin"},
	{"aw8697_fingerprint_effect5_RTP_114.bin"},
	{"aw8697_fingerprint_effect6_RTP_115.bin"},
	{"aw8697_fingerprint_effect7_RTP_116.bin"},
	{"aw8697_fingerprint_effect8_RTP_117.bin"},
	{"aw8697_breath_simulation_RTP_118.bin"},
	{"aw8697_Telcel_Torreblanca_RTP_119.bin"},

	{"aw8697_Miss_RTP_120.bin"},
	{"aw8697_Scenic_RTP_121.bin"},
	{"aw8697_voice_assistant_RTP_122.bin"},
	{"aw8697_Appear_channel_RTP_123.bin"},
	{"aw8697_Miss_RTP_124.bin"},
	{"aw8697_Music_channel_RTP_125.bin"},
	{"aw8697_Percussion_channel_RTP_126.bin"},
	{"aw8697_Ripple_channel_RTP_127.bin"},
	{"aw8697_Bright_channel_RTP_128.bin"},
	{"aw8697_Fun_channel_RTP_129.bin"},
	{"aw8697_Glittering_channel_RTP_130.bin"},
	{"aw8697_Harp_channel_RTP_131.bin"},
	{"aw8697_Overtone_channel_RTP_132.bin"},
	{"aw8697_Simple_channel_RTP_133.bin"},

	{"aw8697_Seine_past_RTP_134.bin"},
	{"aw8697_Classical_ring_RTP_135.bin"},
	{"aw8697_Long_for_RTP_136.bin"},
	{"aw8697_Romantic_RTP_137.bin"},
	{"aw8697_Bliss_RTP_138.bin"},
	{"aw8697_Dream_RTP_139.bin"},
	{"aw8697_Relax_RTP_140.bin"},
	{"aw8697_Joy_channel_RTP_141.bin"},
	{"aw8697_weather_wind_RTP_142.bin"},
	{"aw8697_weather_cloudy_RTP_143.bin"},
	{"aw8697_weather_thunderstorm_RTP_144.bin"},
	{"aw8697_weather_default_RTP_145.bin"},
	{"aw8697_weather_sunny_RTP_146.bin"},
	{"aw8697_weather_smog_RTP_147.bin"},
	{"aw8697_weather_snow_RTP_148.bin"},
	{"aw8697_weather_rain_RTP_149.bin"},
#endif

	{"aw8697_Master_Notification_RTP_150.bin"},
	{"aw8697_Master_Artist_Ringtong_RTP_151.bin"},
	{"aw8697_Master_Text_RTP_152.bin"},
	{"aw8697_Master_Artist_Alarm_RTP_153.bin"},
	{"aw8697_reserved_154.bin"},
	{"aw8697_reserved_155.bin"},
	{"aw8697_reserved_156.bin"},
	{"aw8697_reserved_157.bin"},
	{"aw8697_reserved_158.bin"},
	{"aw8697_reserved_159.bin"},
	{"aw8697_reserved_160.bin"},

	{"aw8697_oplus_its_oplus_RTP_161_170Hz.bin"},
	{"aw8697_oplus_tune_RTP_162_170Hz.bin"},
	{"aw8697_oplus_jingle_RTP_163_170Hz.bin"},
	{"aw8697_reserved_164.bin"},
	{"aw8697_reserved_165.bin"},
	{"aw8697_reserved_166.bin"},
	{"aw8697_reserved_167.bin"},
	{"aw8697_reserved_168.bin"},
	{"aw8697_reserved_169.bin"},
	{"aw8697_oplus_gt_RTP_170_170Hz.bin"},

	{"aw8697_Threefingers_Long_RTP_171.bin"},
	{"aw8697_Threefingers_Up_RTP_172.bin"},
	{"aw8697_Threefingers_Screenshot_RTP_173.bin"},
	{"aw8697_Unfold_RTP_174.bin"},
	{"aw8697_Close_RTP_175.bin"},
	{"aw8697_HalfLap_RTP_176.bin"},
	{"aw8697_Twofingers_Down_RTP_177.bin"},
	{"aw8697_Twofingers_Long_RTP_178.bin"},
	{"aw8697_Compatible_1_RTP_179.bin"},
	{"aw8697_Compatible_2_RTP_180.bin"},
	{"aw8697_Styleswitch_RTP_181.bin"},
	{"aw8697_Waterripple_RTP_182.bin"},
	{"aw8697_Suspendbutton_Bottomout_RTP_183.bin"},
	{"aw8697_Suspendbutton_Menu_RTP_184.bin"},
	{"aw8697_Complete_RTP_185.bin"},
	{"aw8697_Bulb_RTP_186.bin"},
	{"aw8697_Elasticity_RTP_187.bin"},
	{"aw8697_reserved_188.bin"},
	{"aw8697_reserved_189.bin"},
	{"aw8697_reserved_190.bin"},
	{"aw8697_reserved_191.bin"},
	{"aw8697_reserved_192.bin"},
	{"aw8697_reserved_193.bin"},
	{"aw8697_reserved_194.bin"},
	{"aw8697_reserved_195.bin"},
	{"aw8697_reserved_196.bin"},
	{"aw8697_reserved_197.bin"},
	{"alarm_Pacman_RTP_198.bin"},
	{"notif_Pacman_RTP_199.bin"},
	{"ringtone_Pacman_RTP_200.bin"},
	{"ringtone_Alacrity_RTP_201.bin"},
	{"ring_Amenity_RTP_202.bin"},
	{"ringtone_Blues_RTP_203.bin"},
	{"ring_Bounce_RTP_204.bin"},
	{"ring_Calm_RTP_205.bin"},
	{"ringtone_Cloud_RTP_206.bin"},
	{"ringtone_Cyclotron_RTP_207.bin"},
	{"ringtone_Distinct_RTP_208.bin"},
	{"ringtone_Dynamic_RTP_209.bin"},
	{"ringtone_Echo_RTP_210.bin"},
	{"ringtone_Expect_RTP_211.bin"},
	{"ringtone_Fanatical_RTP_212.bin"},
	{"ringtone_Funky_RTP_213.bin"},
	{"ringtone_Guitar_RTP_214.bin"},
	{"ringtone_Harping_RTP_215.bin"},
	{"ringtone_Highlight_RTP_216.bin"},
	{"ringtone_Idyl_RTP_217.bin"},
	{"ringtone_Innocence_RTP_218.bin"},
	{"ringtone_Journey_RTP_219.bin"},
	{"ringtone_Joyous_RTP_220.bin"},
	{"ring_Lazy_RTP_221.bin"},
	{"ringtone_Marimba_RTP_222.bin"},
	{"ring_Mystical_RTP_223.bin"},
	{"ringtone_Old_telephone_RTP_224.bin"},
	{"ringtone_Oneplus_tune_RTP_225.bin"},
	{"ringtone_Rhythm_RTP_226.bin"},
	{"ringtone_Optimistic_RTP_227.bin"},
	{"ringtone_Piano_RTP_228.bin"},
	{"ring_Whirl_RTP_229.bin"},
	{"VZW_Alrwave_RTP_230.bin"},
	{"t-jingle_RTP_231.bin"},
	{"ringtone_Eager_232.bin"},
	{"ringtone_Ebullition_233.bin"},
	{"ringtone_Friendship_234.bin"},
	{"ringtone_Jazz_life_RTP_235.bin"},
	{"ringtone_Sun_glittering_RTP_236.bin"},
	{"notif_Allay_RTP_237.bin"},
	{"notif_Allusion_RTP_238.bin"},
	{"notif_Amiable_RTP_239.bin"},
	{"notif_Blare_RTP_240.bin"},
	{"notif_Blissful_RTP_241.bin"},
	{"notif_Brisk_RTP_242.bin"},
	{"notif_Bubble_RTP_243.bin"},
	{"notif_Cheerful_RTP_244.bin"},
	{"notif_Clear_RTP_245.bin"},
	{"notif_Comely_RTP_246.bin"},
	{"notif_Cozy_RTP_247.bin"},
	{"notif_Ding_RTP_248.bin"},
	{"notif_Effervesce_RTP_249.bin"},
	{"notif_Elegant_RTP_250.bin"},
	{"notif_Free_RTP_251.bin"},
	{"notif_Hallucination_RTP_252.bin"},
	{"notif_Inbound_RTP_253.bin"},
	{"notif_Light_RTP_254.bin"},
	{"notif_Meet_RTP_255.bin"},
	{"notif_Naivety_RTP_256.bin"},
	{"notif_Quickly_RTP_257.bin"},
	{"notif_Rhythm_RTP_258.bin"},
	{"notif_Surprise_RTP_259.bin"},
	{"notif_Twinkle_RTP_260.bin"},
	{"Version_Alert_RTP_261.bin"},
	{"alarm_Alarm_clock_RTP_262.bin"},
	{"alarm_Beep_RTP_263.bin"},
	{"alarm_Breeze_RTP_264.bin"},
	{"alarm_Dawn_RTP_265.bin"},
	{"alarm_Dream_RTP_266.bin"},
	{"alarm_Fluttering_RTP_267.bin"},
	{"alarm_Flyer_RTP_268.bin"},
	{"alarm_Interesting_RTP_269.bin"},
	{"alarm_Leisurely_RTP_270.bin"},
	{"alarm_Memory_RTP_271.bin"},
	{"alarm_Relieved_RTP_272.bin"},
	{"alarm_Ripple_RTP_273.bin"},
	{"alarm_Slowly_RTP_274.bin"},
	{"alarm_spring_RTP_275.bin"},
	{"alarm_Stars_RTP_276.bin"},
	{"alarm_Surging_RTP_277.bin"},
	{"alarm_tactfully_RTP_278.bin"},
	{"alarm_The_wind_RTP_279.bin"},
	{"alarm_Walking_in_the_rain_RTP_280.bin"},
	{"BoHaoPanAnJian_281.bin"},
	{"BoHaoPanAnNiu_282.bin"},
	{"BoHaoPanKuaiJie_283.bin"},
	{"DianHuaGuaDuan_284.bin"},
	{"DianJinMoShiQieHuan_285.bin"},
	{"HuaDongTiaoZhenDong_286.bin"},
	{"LeiShen_287.bin"},
	{"XuanZhongYouXi_288.bin"},
	{"YeJianMoShiDaZi_289.bin"},
	{"YouXiSheZhiKuang_290.bin"},
	{"ZhuanYeMoShi_291.bin"},
	{"Climber_RTP_292.bin"},
	{"Chase_RTP_293.bin"},
	{"shuntai24k_rtp_294.bin"},
	{"wentai24k_rtp_295.bin"},
	{"20ms_RTP_296.bin"},
	{"40ms_RTP_297.bin"},
	{"60ms_RTP_298.bin"},
	{"80ms_RTP_299.bin"},
	{"100ms_RTP_300.bin"},
	{"120ms_RTP_301.bin"},
	{"140ms_RTP_302.bin"},
	{"160ms_RTP_303.bin"},
	{"180ms_RTP_304.bin"},
	{"200ms_RTP_305.bin"},
	{"220ms_RTP_306.bin"},
	{"240ms_RTP_307.bin"},
	{"260ms_RTP_308.bin"},
	{"280ms_RTP_309.bin"},
	{"300ms_RTP_310.bin"},
	{"320ms_RTP_311.bin"},
	{"340ms_RTP_312.bin"},
	{"360ms_RTP_313.bin"},
	{"380ms_RTP_314.bin"},
	{"400ms_RTP_315.bin"},
	{"420ms_RTP_316.bin"},
	{"440ms_RTP_317.bin"},
	{"460ms_RTP_318.bin"},
	{"480ms_RTP_319.bin"},
	{"500ms_RTP_320.bin"},
	{"AT500ms_RTP_321.bin"},

	{"aw8697_reserved_322.bin"},
	{"aw8697_reserved_323.bin"},
	{"aw8697_reserved_324.bin"},
	{"aw8697_reserved_325.bin"},
	{"aw8697_reserved_326.bin"},
	{"aw8697_reserved_327.bin"},
	{"aw8697_reserved_328.bin"},
	{"aw8697_reserved_329.bin"},
	{"aw8697_reserved_330.bin"},
	{"aw8697_reserved_331.bin"},
	{"aw8697_reserved_332.bin"},
	{"aw8697_reserved_333.bin"},
	{"aw8697_reserved_334.bin"},
	{"aw8697_reserved_335.bin"},
	{"aw8697_reserved_336.bin"},
	{"aw8697_reserved_337.bin"},
	{"aw8697_reserved_338.bin"},
	{"aw8697_reserved_339.bin"},
	{"aw8697_reserved_340.bin"},
	{"aw8697_reserved_341.bin"},
	{"aw8697_reserved_342.bin"},
	{"aw8697_reserved_343.bin"},
	{"aw8697_reserved_344.bin"},
	{"aw8697_reserved_345.bin"},
	{"aw8697_reserved_346.bin"},
	{"aw8697_reserved_347.bin"},
	{"aw8697_reserved_348.bin"},
	{"aw8697_reserved_349.bin"},
	{"aw8697_reserved_350.bin"},
	{"aw8697_reserved_351.bin"},
	{"aw8697_reserved_352.bin"},
	{"aw8697_reserved_353.bin"},
	{"aw8697_reserved_354.bin"},
	{"aw8697_reserved_355.bin"},
	{"aw8697_reserved_356.bin"},
	{"aw8697_reserved_357.bin"},
	{"aw8697_reserved_358.bin"},
	{"aw8697_reserved_359.bin"},
	{"aw8697_reserved_360.bin"},
	{"aw8697_reserved_361.bin"},
	{"aw8697_reserved_362.bin"},
	{"aw8697_programs_clean_RTP_363.bin"},
	{"aw8697_reserved_364.bin"},
	{"aw8697_reserved_365.bin"},
	{"aw8697_reserved_366.bin"},
	{"aw8697_reserved_367.bin"},
	{"aw8697_reserved_368.bin"},
	{"aw8697_reserved_369.bin"},
	{"aw8697_reserved_370.bin"},

	/* Add for OS14 Start */
	{"aw8697_Nightsky_RTP_371.bin"},
	{"aw8697_TheStars_RTP_372.bin"},
	{"aw8697_TheSunrise_RTP_373.bin"},
	{"aw8697_TheSunset_RTP_374.bin"},
	{"aw8697_Meditate_RTP_375.bin"},
	{"aw8697_Distant_RTP_376.bin"},
	{"aw8697_Pond_RTP_377.bin"},
	{"aw8697_Moonlotus_RTP_378.bin"},
	{"aw8697_Ripplingwater_RTP_379.bin"},
	{"aw8697_Shimmer_RTP_380.bin"},
	{"aw8697_Batheearth_RTP_381.bin"},
	{"aw8697_Junglemorning_RTP_382.bin"},
	{"aw8697_Silver_RTP_383.bin"},
	{"aw8697_Elegantquiet_RTP_384.bin"},
	{"aw8697_Summerbeach_RTP_385.bin"},
	{"aw8697_Summernight_RTP_386.bin"},
	{"aw8697_Icesnow_RTP_387.bin"},
	{"aw8697_Wintersnow_RTP_388.bin"},
	{"aw8697_Rainforest_RTP_389.bin"},
	{"aw8697_Raineverything_RTP_390.bin"},
	{"aw8697_Staracross_RTP_391.bin"},
	{"aw8697_Fullmoon_RTP_392.bin"},
	{"aw8697_Clouds_RTP_393.bin"},
	{"aw8697_Wonderland_RTP_394.bin"},
	{"aw8697_Still_RTP_395.bin"},
	{"aw8697_Haunting_RTP_396.bin"},
	{"aw8697_Dragonfly_RTP_397.bin"},
	{"aw8697_Dropwater_RTP_398.bin"},
	{"aw8697_Fluctuation_RTP_399.bin"},
	{"aw8697_Blow_RTP_400.bin"},
	{"aw8697_Leaveslight_RTP_401.bin"},
	{"aw8697_Warmsun_RTP_402.bin"},
	{"aw8697_Snowflake_RTP_403.bin"},
	{"aw8697_Crystalclear_RTP_404.bin"},
	{"aw8697_Insects_RTP_405.bin"},
	{"aw8697_Dew_RTP_406.bin"},
	{"aw8697_Shine_RTP_407.bin"},
	{"aw8697_Frost_RTP_408.bin"},
	{"aw8697_Rainsplash_RTP_409.bin"},
	{"aw8697_Raindrop_RTP_410.bin"},
	/* Add for OS14 End */
};

#ifndef KERNEL_VERSION_510
static char aw_rtp_name_175Hz[][AW_RTP_NAME_MAX] = {
	{"aw8697_rtp.bin"},
	{"aw8697_Hearty_channel_RTP_1.bin"},
	{"aw8697_Instant_channel_RTP_2_175Hz.bin"},
	{"aw8697_Music_channel_RTP_3.bin"},
	{"aw8697_Percussion_channel_RTP_4.bin"},
	{"aw8697_Ripple_channel_RTP_5.bin"},
	{"aw8697_Bright_channel_RTP_6.bin"},
	{"aw8697_Fun_channel_RTP_7.bin"},
	{"aw8697_Glittering_channel_RTP_8.bin"},
	{"aw8697_Granules_channel_RTP_9_175Hz.bin"},
	{"aw8697_Harp_channel_RTP_10.bin"},
	{"aw8697_Impression_channel_RTP_11.bin"},
	{"aw8697_Ingenious_channel_RTP_12_175Hz.bin"},
	{"aw8697_Joy_channel_RTP_13_175Hz.bin"},
	{"aw8697_Overtone_channel_RTP_14.bin"},
	{"aw8697_Receive_channel_RTP_15_175Hz.bin"},
	{"aw8697_Splash_channel_RTP_16_175Hz.bin"},

	{"aw8697_About_School_RTP_17_175Hz.bin"},
	{"aw8697_Bliss_RTP_18.bin"},
	{"aw8697_Childhood_RTP_19_175Hz.bin"},
	{"aw8697_Commuting_RTP_20_175Hz.bin"},
	{"aw8697_Dream_RTP_21.bin"},
	{"aw8697_Firefly_RTP_22_175Hz.bin"},
	{"aw8697_Gathering_RTP_23.bin"},
	{"aw8697_Gaze_RTP_24_175Hz.bin"},
	{"aw8697_Lakeside_RTP_25_175Hz.bin"},
	{"aw8697_Lifestyle_RTP_26.bin"},
	{"aw8697_Memories_RTP_27_175Hz.bin"},
	{"aw8697_Messy_RTP_28_175Hz.bin"},
	{"aw8697_Night_RTP_29_175Hz.bin"},
	{"aw8697_Passionate_Dance_RTP_30_175Hz.bin"},
	{"aw8697_Playground_RTP_31_175Hz.bin"},
	{"aw8697_Relax_RTP_32_175Hz.bin"},
	{"aw8697_Reminiscence_RTP_33.bin"},
	{"aw8697_Silence_From_Afar_RTP_34_175Hz.bin"},
	{"aw8697_Silence_RTP_35_175Hz.bin"},
	{"aw8697_Stars_RTP_36_175Hz.bin"},
	{"aw8697_Summer_RTP_37_175Hz.bin"},
	{"aw8697_Toys_RTP_38_175Hz.bin"},
	{"aw8697_Travel_RTP_39.bin"},
	{"aw8697_Vision_RTP_40.bin"},

	{"aw8697_waltz_channel_RTP_41_175Hz.bin"},
	{"aw8697_cut_channel_RTP_42_175Hz.bin"},
	{"aw8697_clock_channel_RTP_43_175Hz.bin"},
	{"aw8697_long_sound_channel_RTP_44_175Hz.bin"},
	{"aw8697_short_channel_RTP_45_175Hz.bin"},
	{"aw8697_two_error_remaind_RTP_46_175Hz.bin"},

	{"aw8697_kill_program_RTP_47_175Hz.bin"},
	{"aw8697_Simple_channel_RTP_48.bin"},
	{"aw8697_Pure_RTP_49_175Hz.bin"},
	{"aw8697_reserved_sound_channel_RTP_50.bin"},

	{"aw8697_high_temp_high_humidity_channel_RTP_51.bin"},

	{"aw8697_old_steady_test_RTP_52.bin"},
	{"aw8697_listen_pop_53.bin"},
	{"aw8697_desk_7_RTP_54_175Hz.bin"},
	{"aw8697_nfc_10_RTP_55_175Hz.bin"},
	{"aw8697_vibrator_remain_12_RTP_56_175Hz.bin"},
	{"aw8697_notice_13_RTP_57.bin"},
	{"aw8697_third_ring_14_RTP_58.bin"},
	{"aw8697_reserved_59.bin"},

	{"aw8697_honor_fisrt_kill_RTP_60_175Hz.bin"},
	{"aw8697_honor_two_kill_RTP_61_175Hz.bin"},
	{"aw8697_honor_three_kill_RTP_62_175Hz.bin"},
	{"aw8697_honor_four_kill_RTP_63_175Hz.bin"},
	{"aw8697_honor_five_kill_RTP_64_175Hz.bin"},
	{"aw8697_honor_three_continu_kill_RTP_65_175Hz.bin"},
	{"aw8697_honor_four_continu_kill_RTP_66_175Hz.bin"},
	{"aw8697_honor_unstoppable_RTP_67_175Hz.bin"},
	{"aw8697_honor_thousands_kill_RTP_68_175Hz.bin"},
	{"aw8697_honor_lengendary_RTP_69_175Hz.bin"},


	{"aw8697_reserved_70.bin"},
	{"aw8697_reserved_71.bin"},
	{"aw8697_reserved_72.bin"},
	{"aw8697_reserved_73.bin"},
	{"aw8697_reserved_74.bin"},
	{"aw8697_reserved_75.bin"},
	{"aw8697_reserved_76.bin"},
	{"aw8697_reserved_77.bin"},
	{"aw8697_reserved_78.bin"},
	{"aw8697_reserved_79.bin"},

	{"aw8697_reserved_80.bin"},
	{"aw8697_reserved_81.bin"},
	{"aw8697_reserved_82.bin"},
	{"aw8697_reserved_83.bin"},
	{"aw8697_reserved_84.bin"},
	{"aw8697_reserved_85.bin"},
	{"aw8697_reserved_86.bin"},
	{"aw8697_reserved_87.bin"},
	{"aw8697_reserved_88.bin"},
	{"aw8697_reserved_89.bin"},

	{"aw8697_reserved_90.bin"},
	{"aw8697_reserved_91.bin"},
	{"aw8697_reserved_92.bin"},
	{"aw8697_reserved_93.bin"},
	{"aw8697_ALCloudscape_94_175HZ.bin"},
	{"aw8697_ALGoodenergy_95_175HZ.bin"},
	{"aw8697_NTblink_96_175HZ.bin"},
	{"aw8697_NTwhoop_97_175HZ.bin"},
	{"aw8697_Newfeeling_98_175HZ.bin"},
	{"aw8697_nature_99_175HZ.bin"},

	{"aw8697_soldier_first_kill_RTP_100_175Hz.bin"},
	{"aw8697_soldier_second_kill_RTP_101_175Hz.bin"},
	{"aw8697_soldier_third_kill_RTP_102_175Hz.bin"},
	{"aw8697_soldier_fourth_kill_RTP_103_175Hz.bin"},
	{"aw8697_soldier_fifth_kill_RTP_104_175Hz.bin"},
	{"aw8697_stepable_regulate_RTP_105.bin"},
	{"aw8697_voice_level_bar_edge_RTP_106.bin"},
	{"aw8697_strength_level_bar_edge_RTP_107.bin"},
	{"aw8697_charging_simulation_RTP_108.bin"},
	{"aw8697_fingerprint_success_RTP_109.bin"},

	{"aw8697_fingerprint_effect1_RTP_110.bin"},
	{"aw8697_fingerprint_effect2_RTP_111.bin"},
	{"aw8697_fingerprint_effect3_RTP_112.bin"},
	{"aw8697_fingerprint_effect4_RTP_113.bin"},
	{"aw8697_fingerprint_effect5_RTP_114.bin"},
	{"aw8697_fingerprint_effect6_RTP_115.bin"},
	{"aw8697_fingerprint_effect7_RTP_116.bin"},
	{"aw8697_fingerprint_effect8_RTP_117.bin"},
	{"aw8697_breath_simulation_RTP_118.bin"},
	{"aw8697_reserved_119.bin"},

	{"aw8697_Miss_RTP_120.bin"},
	{"aw8697_Scenic_RTP_121_175Hz.bin"},
	{"aw8697_voice_assistant_RTP_122.bin"},
/* used for 7 */
	{"aw8697_Appear_channel_RTP_123_175Hz.bin"},
	{"aw8697_Miss_RTP_124_175Hz.bin"},
	{"aw8697_Music_channel_RTP_125_175Hz.bin"},
	{"aw8697_Percussion_channel_RTP_126_175Hz.bin"},
	{"aw8697_Ripple_channel_RTP_127_175Hz.bin"},
	{"aw8697_Bright_channel_RTP_128_175Hz.bin"},
	{"aw8697_Fun_channel_RTP_129_175Hz.bin"},
	{"aw8697_Glittering_channel_RTP_130_175Hz.bin"},
	{"aw8697_Harp_channel_RTP_131_175Hz.bin"},
	{"aw8697_Overtone_channel_RTP_132_175Hz.bin"},
	{"aw8697_Simple_channel_RTP_133_175Hz.bin"},

	{"aw8697_Seine_past_RTP_134_175Hz.bin"},
	{"aw8697_Classical_ring_RTP_135_175Hz.bin"},
	{"aw8697_Long_for_RTP_136_175Hz.bin"},
	{"aw8697_Romantic_RTP_137_175Hz.bin"},
	{"aw8697_Bliss_RTP_138_175Hz.bin"},
	{"aw8697_Dream_RTP_139_175Hz.bin"},
	{"aw8697_Relax_RTP_140_175Hz.bin"},
	{"aw8697_Joy_channel_RTP_141_175Hz.bin"},
	{"aw8697_weather_wind_RTP_142_175Hz.bin"},
	{"aw8697_weather_cloudy_RTP_143_175Hz.bin"},
	{"aw8697_weather_thunderstorm_RTP_144_175Hz.bin"},
	{"aw8697_weather_default_RTP_145_175Hz.bin"},
	{"aw8697_weather_sunny_RTP_146_175Hz.bin"},
	{"aw8697_weather_smog_RTP_147_175Hz.bin"},
	{"aw8697_weather_snow_RTP_148_175Hz.bin"},
	{"aw8697_weather_rain_RTP_149_175Hz.bin"},
/* used for 7 end*/
	{"aw8697_rtp_lighthouse.bin"},
	{"aw8697_rtp_silk.bin"},
	{"aw8697_reserved_152.bin"},
	{"aw8697_reserved_153.bin"},
	{"aw8697_reserved_154.bin"},
	{"aw8697_reserved_155.bin"},
	{"aw8697_reserved_156.bin"},
	{"aw8697_reserved_157.bin"},
	{"aw8697_reserved_158.bin"},
	{"aw8697_reserved_159.bin"},
	{"aw8697_reserved_160.bin"},

	{"aw8697_reserved_161.bin"},
	{"aw8697_reserved_162.bin"},
	{"aw8697_reserved_163.bin"},
	{"aw8697_reserved_164.bin"},
	{"aw8697_reserved_165.bin"},
	{"aw8697_reserved_166.bin"},
	{"aw8697_reserved_167.bin"},
	{"aw8697_reserved_168.bin"},
	{"aw8697_reserved_169.bin"},
	{"aw8697_reserved_170.bin"},
	{"aw8697_Threefingers_Long_RTP_171_175Hz.bin"},
	{"aw8697_Threefingers_Up_RTP_172_175Hz.bin"},
	{"aw8697_Threefingers_Screenshot_RTP_173_175Hz.bin"},
	{"aw8697_Unfold_RTP_174_175Hz.bin"},
	{"aw8697_Close_RTP_175_175Hz.bin"},
	{"aw8697_HalfLap_RTP_176_175Hz.bin"},
	{"aw8697_Twofingers_Down_RTP_177_175Hz.bin"},
	{"aw8697_Twofingers_Long_RTP_178_175Hz.bin"},
	{"aw8697_Compatible_1_RTP_179_175Hz.bin"},
	{"aw8697_Compatible_2_RTP_180_175Hz.bin"},
	{"aw8697_Styleswitch_RTP_181_175Hz.bin"},
	{"aw8697_Waterripple_RTP_182_175Hz.bin"},
	{"aw8697_Suspendbutton_Bottomout_RTP_183_175Hz.bin"},
	{"aw8697_Suspendbutton_Menu_RTP_184_175Hz.bin"},
	{"aw8697_Complete_RTP_185_175Hz.bin"},
	{"aw8697_Bulb_RTP_186_175Hz.bin"},
	{"aw8697_Elasticity_RTP_187_175Hz.bin"},
	{"aw8697_reserved_188.bin"},
	{"aw8697_reserved_189.bin"},
	{"aw8697_reserved_190.bin"},
	{"aw8697_reserved_191.bin"},
	{"aw8697_reserved_192.bin"},
	{"aw8697_reserved_193.bin"},
	{"aw8697_reserved_194.bin"},
	{"aw8697_reserved_195.bin"},
	{"aw8697_reserved_196.bin"},
	{"aw8697_reserved_197.bin"},
	{"aw8697_reserved_198.bin"},
	{"aw8697_reserved_199.bin"},
	{"aw8697_reserved_200.bin"},
};
#endif

static char aw_ram_name_19065[5][30] = {
	{"aw8697_haptic_235.bin"},
	{"aw8697_haptic_235.bin"},
	{"aw8697_haptic_235.bin"},
	{"aw8697_haptic_235.bin"},
	{"aw8697_haptic_235.bin"},
};

static char aw_ram_name_19161[5][30] = {
	{"aw8697_haptic_235_19161.bin"},
	{"aw8697_haptic_235_19161.bin"},
	{"aw8697_haptic_235_19161.bin"},
	{"aw8697_haptic_235_19161.bin"},
	{"aw8697_haptic_235_19161.bin"},
};

#ifdef OPLUS_FEATURE_CHG_BASIC
static char aw_rtp_name_19065_226Hz[][AW_RTP_NAME_MAX] = {
	{"aw8697_rtp.bin"},
	{"aw8697_Hearty_channel_RTP_1.bin"},
	{"aw8697_Instant_channel_RTP_2_226Hz.bin"},
	{"aw8697_Music_channel_RTP_3.bin"},
	{"aw8697_Percussion_channel_RTP_4.bin"},
	{"aw8697_Ripple_channel_RTP_5.bin"},
	{"aw8697_Bright_channel_RTP_6.bin"},
	{"aw8697_Fun_channel_RTP_7.bin"},
	{"aw8697_Glittering_channel_RTP_8.bin"},
	{"aw8697_Granules_channel_RTP_9_226Hz.bin"},
	{"aw8697_Harp_channel_RTP_10.bin"},
	{"aw8697_Impression_channel_RTP_11.bin"},
	{"aw8697_Ingenious_channel_RTP_12_226Hz.bin"},
	{"aw8697_Joy_channel_RTP_13_226Hz.bin"},
	{"aw8697_Overtone_channel_RTP_14.bin"},
	{"aw8697_Receive_channel_RTP_15_226Hz.bin"},
	{"aw8697_Splash_channel_RTP_16_226Hz.bin"},

	{"aw8697_About_School_RTP_17_226Hz.bin"},
	{"aw8697_Bliss_RTP_18.bin"},
	{"aw8697_Childhood_RTP_19_226Hz.bin"},
	{"aw8697_Commuting_RTP_20_226Hz.bin"},
	{"aw8697_Dream_RTP_21.bin"},
	{"aw8697_Firefly_RTP_22_226Hz.bin"},
	{"aw8697_Gathering_RTP_23.bin"},
	{"aw8697_Gaze_RTP_24_226Hz.bin"},
	{"aw8697_Lakeside_RTP_25_226Hz.bin"},
	{"aw8697_Lifestyle_RTP_26.bin"},
	{"aw8697_Memories_RTP_27_226Hz.bin"},
	{"aw8697_Messy_RTP_28_226Hz.bin"},
	{"aw8697_Night_RTP_29_226Hz.bin"},
	{"aw8697_Passionate_Dance_RTP_30_226Hz.bin"},
	{"aw8697_Playground_RTP_31_226Hz.bin"},
	{"aw8697_Relax_RTP_32_226Hz.bin"},
	{"aw8697_Reminiscence_RTP_33.bin"},
	{"aw8697_Silence_From_Afar_RTP_34_226Hz.bin"},
	{"aw8697_Silence_RTP_35_226Hz.bin"},
	{"aw8697_Stars_RTP_36_226Hz.bin"},
	{"aw8697_Summer_RTP_37_226Hz.bin"},
	{"aw8697_Toys_RTP_38_226Hz.bin"},
	{"aw8697_Travel_RTP_39.bin"},
	{"aw8697_Vision_RTP_40.bin"},

	{"aw8697_waltz_channel_RTP_41_226Hz.bin"},
	{"aw8697_cut_channel_RTP_42_226Hz.bin"},
	{"aw8697_clock_channel_RTP_43_226Hz.bin"},
	{"aw8697_long_sound_channel_RTP_44_226Hz.bin"},
	{"aw8697_short_channel_RTP_45_226Hz.bin"},
	{"aw8697_two_error_remaind_RTP_46_226Hz.bin"},

	{"aw8697_kill_program_RTP_47_226Hz.bin"},
	{"aw8697_Simple_channel_RTP_48.bin"},
	{"aw8697_Pure_RTP_49_226Hz.bin"},
	{"aw8697_reserved_sound_channel_RTP_50.bin"},

	{"aw8697_high_temp_high_humidity_channel_RTP_51.bin"},

	{"aw8697_old_steady_test_RTP_52.bin"},
	{"aw8697_listen_pop_53_235Hz.bin"},
	{"aw8697_desk_7_RTP_54_226Hz.bin"},
	{"aw8697_nfc_10_RTP_55_226Hz.bin"},
	{"aw8697_vibrator_remain_12_RTP_56_226Hz.bin"},
	{"aw8697_notice_13_RTP_57.bin"},
	{"aw8697_third_ring_14_RTP_58.bin"},
	{"aw8697_reserved_59.bin"},

	{"aw8697_honor_fisrt_kill_RTP_60_226Hz.bin"},
	{"aw8697_honor_two_kill_RTP_61_226Hz.bin"},
	{"aw8697_honor_three_kill_RTP_62_226Hz.bin"},
	{"aw8697_honor_four_kill_RTP_63_226Hz.bin"},
	{"aw8697_honor_five_kill_RTP_64_226Hz.bin"},
	{"aw8697_honor_three_continu_kill_RTP_65_226Hz.bin"},
	{"aw8697_honor_four_continu_kill_RTP_66_226Hz.bin"},
	{"aw8697_honor_unstoppable_RTP_67_226Hz.bin"},
	{"aw8697_honor_thousands_kill_RTP_68_226Hz.bin"},
	{"aw8697_honor_lengendary_RTP_69_226Hz.bin"},


	{"aw8697_reserved_70.bin"},
	{"aw8697_reserved_71.bin"},
	{"aw8697_reserved_72.bin"},
	{"aw8697_reserved_73.bin"},
	{"aw8697_reserved_74.bin"},
	{"aw8697_reserved_75.bin"},
	{"aw8697_reserved_76.bin"},
	{"aw8697_reserved_77.bin"},
	{"aw8697_reserved_78.bin"},
	{"aw8697_reserved_79.bin"},

	{"aw8697_reserved_80.bin"},
	{"aw8697_reserved_81.bin"},
	{"aw8697_reserved_82.bin"},
	{"aw8697_reserved_83.bin"},
	{"aw8697_reserved_84.bin"},
	{"aw8697_reserved_85.bin"},
	{"aw8697_reserved_86.bin"},
	{"aw8697_reserved_87.bin"},
	{"aw8697_reserved_88.bin"},
	{"aw8697_reserved_89.bin"},

	{"aw8697_reserved_90.bin"},
	{"aw8697_reserved_91.bin"},
	{"aw8697_reserved_92.bin"},
	{"aw8697_reserved_93.bin"},
	{"aw8697_reserved_94.bin"},
	{"aw8697_reserved_95.bin"},
	{"aw8697_reserved_96.bin"},
	{"aw8697_reserved_97.bin"},
	{"aw8697_reserved_98.bin"},
	{"aw8697_reserved_99.bin"},

	{"aw8697_soldier_first_kill_RTP_100_226Hz.bin"},
	{"aw8697_soldier_second_kill_RTP_101_226Hz.bin"},
	{"aw8697_soldier_third_kill_RTP_102_226Hz.bin"},
	{"aw8697_soldier_fourth_kill_RTP_103_226Hz.bin"},
	{"aw8697_soldier_fifth_kill_RTP_104_226Hz.bin"},
	{"aw8697_stepable_regulate_RTP_105_226Hz.bin"},
	{"aw8697_voice_level_bar_edge_RTP_106_226Hz.bin"},
	{"aw8697_strength_level_bar_edge_RTP_107_226Hz.bin"},
	{"aw8697_charging_simulation_RTP_108_226Hz.bin"},
	{"aw8697_fingerprint_success_RTP_109_226Hz.bin"},

	{"aw8697_fingerprint_effect1_RTP_110_226Hz.bin"},
	{"aw8697_fingerprint_effect2_RTP_111_226Hz.bin"},
	{"aw8697_fingerprint_effect3_RTP_112_226Hz.bin"},
	{"aw8697_fingerprint_effect4_RTP_113_226Hz.bin"},
	{"aw8697_fingerprint_effect5_RTP_114_226Hz.bin"},
	{"aw8697_fingerprint_effect6_RTP_115_226Hz.bin"},
	{"aw8697_fingerprint_effect7_RTP_116_226Hz.bin"},
	{"aw8697_fingerprint_effect8_RTP_117_226Hz.bin"},
	{"aw8697_breath_simulation_RTP_118_226Hz.bin"},
	{"aw8697_reserved_119.bin"},

	{"aw8697_Miss_RTP_120.bin"},
	{"aw8697_Scenic_RTP_121_226Hz.bin"},
	{"aw8697_voice_assistant_RTP_122_226Hz.bin"},
/* used for 7 */
	{"aw8697_Appear_channel_RTP_123_226Hz.bin"},
	{"aw8697_Miss_RTP_124_226Hz.bin"},
	{"aw8697_Music_channel_RTP_125_226Hz.bin"},
	{"aw8697_Percussion_channel_RTP_126_226Hz.bin"},
	{"aw8697_Ripple_channel_RTP_127_226Hz.bin"},
	{"aw8697_Bright_channel_RTP_128_226Hz.bin"},
	{"aw8697_Fun_channel_RTP_129_226Hz.bin"},
	{"aw8697_Glittering_channel_RTP_130_226Hz.bin"},
	{"aw8697_Harp_channel_RTP_131_226Hz.bin"},
	{"aw8697_Overtone_channel_RTP_132_226Hz.bin"},
	{"aw8697_Simple_channel_RTP_133_226Hz.bin"},

	{"aw8697_Seine_past_RTP_134_226Hz.bin"},
	{"aw8697_Classical_ring_RTP_135_226Hz.bin"},
	{"aw8697_Long_for_RTP_136_226Hz.bin"},
	{"aw8697_Romantic_RTP_137_226Hz.bin"},
	{"aw8697_Bliss_RTP_138_226Hz.bin"},
	{"aw8697_Dream_RTP_139_226Hz.bin"},
	{"aw8697_Relax_RTP_140_226Hz.bin"},
	{"aw8697_Joy_channel_RTP_141_226Hz.bin"},
	{"aw8697_weather_wind_RTP_142_226Hz.bin"},
	{"aw8697_weather_cloudy_RTP_143_226Hz.bin"},
	{"aw8697_weather_thunderstorm_RTP_144_226Hz.bin"},
	{"aw8697_weather_default_RTP_145_226Hz.bin"},
	{"aw8697_weather_sunny_RTP_146_226Hz.bin"},
	{"aw8697_weather_smog_RTP_147_226Hz.bin"},
	{"aw8697_weather_snow_RTP_148_226Hz.bin"},
	{"aw8697_weather_rain_RTP_149_226Hz.bin"},
/* used for 7 end*/
	{"aw8697_rtp_lighthouse.bin"},
	{"aw8697_rtp_silk_19081.bin"},
	{"aw8697_reserved_152.bin"},
	{"aw8697_reserved_153.bin"},
	{"aw8697_reserved_154.bin"},
	{"aw8697_reserved_155.bin"},
	{"aw8697_reserved_156.bin"},
	{"aw8697_reserved_157.bin"},
	{"aw8697_reserved_158.bin"},
	{"aw8697_reserved_159.bin"},
	{"aw8697_reserved_160.bin"},

	{"aw8697_oplus_its_oplus_RTP_161_235Hz.bin"},
	{"aw8697_oplus_tune_RTP_162_235Hz.bin"},
	{"aw8697_oplus_jingle_RTP_163_235Hz.bin"},
	{"aw8697_reserved_164.bin"},
	{"aw8697_reserved_165.bin"},
	{"aw8697_reserved_166.bin"},
	{"aw8697_reserved_167.bin"},
	{"aw8697_reserved_168.bin"},
	{"aw8697_reserved_169.bin"},
	{"aw8697_reserved_170.bin"},
};
#endif /* OPLUS_FEATURE_CHG_BASIC */

#ifdef OPLUS_FEATURE_CHG_BASIC
static char aw_rtp_name_19065_230Hz[][AW_RTP_NAME_MAX] = {
	{"aw8697_rtp.bin"},
	{"aw8697_Hearty_channel_RTP_1.bin"},
	{"aw8697_Instant_channel_RTP_2_230Hz.bin"},
	{"aw8697_Music_channel_RTP_3.bin"},
	{"aw8697_Percussion_channel_RTP_4.bin"},
	{"aw8697_Ripple_channel_RTP_5.bin"},
	{"aw8697_Bright_channel_RTP_6.bin"},
	{"aw8697_Fun_channel_RTP_7.bin"},
	{"aw8697_Glittering_channel_RTP_8.bin"},
	{"aw8697_Granules_channel_RTP_9_230Hz.bin"},
	{"aw8697_Harp_channel_RTP_10.bin"},
	{"aw8697_Impression_channel_RTP_11.bin"},
	{"aw8697_Ingenious_channel_RTP_12_230Hz.bin"},
	{"aw8697_Joy_channel_RTP_13_230Hz.bin"},
	{"aw8697_Overtone_channel_RTP_14.bin"},
	{"aw8697_Receive_channel_RTP_15_230Hz.bin"},
	{"aw8697_Splash_channel_RTP_16_230Hz.bin"},

	{"aw8697_About_School_RTP_17_230Hz.bin"},
	{"aw8697_Bliss_RTP_18.bin"},
	{"aw8697_Childhood_RTP_19_230Hz.bin"},
	{"aw8697_Commuting_RTP_20_230Hz.bin"},
	{"aw8697_Dream_RTP_21.bin"},
	{"aw8697_Firefly_RTP_22_230Hz.bin"},
	{"aw8697_Gathering_RTP_23.bin"},
	{"aw8697_Gaze_RTP_24_230Hz.bin"},
	{"aw8697_Lakeside_RTP_25_230Hz.bin"},
	{"aw8697_Lifestyle_RTP_26.bin"},
	{"aw8697_Memories_RTP_27_230Hz.bin"},
	{"aw8697_Messy_RTP_28_230Hz.bin"},
	{"aw8697_Night_RTP_29_230Hz.bin"},
	{"aw8697_Passionate_Dance_RTP_30_230Hz.bin"},
	{"aw8697_Playground_RTP_31_230Hz.bin"},
	{"aw8697_Relax_RTP_32_230Hz.bin"},
	{"aw8697_Reminiscence_RTP_33.bin"},
	{"aw8697_Silence_From_Afar_RTP_34_230Hz.bin"},
	{"aw8697_Silence_RTP_35_230Hz.bin"},
	{"aw8697_Stars_RTP_36_230Hz.bin"},
	{"aw8697_Summer_RTP_37_230Hz.bin"},
	{"aw8697_Toys_RTP_38_230Hz.bin"},
	{"aw8697_Travel_RTP_39.bin"},
	{"aw8697_Vision_RTP_40.bin"},

	{"aw8697_waltz_channel_RTP_41_230Hz.bin"},
	{"aw8697_cut_channel_RTP_42_230Hz.bin"},
	{"aw8697_clock_channel_RTP_43_230Hz.bin"},
	{"aw8697_long_sound_channel_RTP_44_230Hz.bin"},
	{"aw8697_short_channel_RTP_45_230Hz.bin"},
	{"aw8697_two_error_remaind_RTP_46_230Hz.bin"},

	{"aw8697_kill_program_RTP_47_230Hz.bin"},
	{"aw8697_Simple_channel_RTP_48.bin"},
	{"aw8697_Pure_RTP_49_230Hz.bin"},
	{"aw8697_reserved_sound_channel_RTP_50.bin"},

	{"aw8697_high_temp_high_humidity_channel_RTP_51.bin"},

	{"aw8697_old_steady_test_RTP_52.bin"},
	{"aw8697_listen_pop_53_235Hz.bin"},
	{"aw8697_desk_7_RTP_54_230Hz.bin"},
	{"aw8697_nfc_10_RTP_55_230Hz.bin"},
	{"aw8697_vibrator_remain_12_RTP_56_230Hz.bin"},
	{"aw8697_notice_13_RTP_57.bin"},
	{"aw8697_third_ring_14_RTP_58.bin"},
	{"aw8697_reserved_59.bin"},

	{"aw8697_honor_fisrt_kill_RTP_60_230Hz.bin"},
	{"aw8697_honor_two_kill_RTP_61_230Hz.bin"},
	{"aw8697_honor_three_kill_RTP_62_230Hz.bin"},
	{"aw8697_honor_four_kill_RTP_63_230Hz.bin"},
	{"aw8697_honor_five_kill_RTP_64_230Hz.bin"},
	{"aw8697_honor_three_continu_kill_RTP_65_230Hz.bin"},
	{"aw8697_honor_four_continu_kill_RTP_66_230Hz.bin"},
	{"aw8697_honor_unstoppable_RTP_67_230Hz.bin"},
	{"aw8697_honor_thousands_kill_RTP_68_230Hz.bin"},
	{"aw8697_honor_lengendary_RTP_69_230Hz.bin"},


	{"aw8697_reserved_70.bin"},
	{"aw8697_reserved_71.bin"},
	{"aw8697_reserved_72.bin"},
	{"aw8697_reserved_73.bin"},
	{"aw8697_reserved_74.bin"},
	{"aw8697_reserved_75.bin"},
	{"aw8697_reserved_76.bin"},
	{"aw8697_reserved_77.bin"},
	{"aw8697_reserved_78.bin"},
	{"aw8697_reserved_79.bin"},

	{"aw8697_reserved_80.bin"},
	{"aw8697_reserved_81.bin"},
	{"aw8697_reserved_82.bin"},
	{"aw8697_reserved_83.bin"},
	{"aw8697_reserved_84.bin"},
	{"aw8697_reserved_85.bin"},
	{"aw8697_reserved_86.bin"},
	{"aw8697_reserved_87.bin"},
	{"aw8697_reserved_88.bin"},
	{"aw8697_reserved_89.bin"},

	{"aw8697_reserved_90.bin"},
	{"aw8697_reserved_91.bin"},
	{"aw8697_reserved_92.bin"},
	{"aw8697_reserved_93.bin"},
	{"aw8697_reserved_94.bin"},
	{"aw8697_reserved_95.bin"},
	{"aw8697_reserved_96.bin"},
	{"aw8697_reserved_97.bin"},
	{"aw8697_reserved_98.bin"},
	{"aw8697_reserved_99.bin"},

	{"aw8697_soldier_first_kill_RTP_100_230Hz.bin"},
	{"aw8697_soldier_second_kill_RTP_101_230Hz.bin"},
	{"aw8697_soldier_third_kill_RTP_102_230Hz.bin"},
	{"aw8697_soldier_fourth_kill_RTP_103_230Hz.bin"},
	{"aw8697_soldier_fifth_kill_RTP_104_230Hz.bin"},
	{"aw8697_stepable_regulate_RTP_105_230Hz.bin"},
	{"aw8697_voice_level_bar_edge_RTP_106_230Hz.bin"},
	{"aw8697_strength_level_bar_edge_RTP_107_230Hz.bin"},
	{"aw8697_charging_simulation_RTP_108_230Hz.bin"},
	{"aw8697_fingerprint_success_RTP_109_230Hz.bin"},

	{"aw8697_fingerprint_effect1_RTP_110_230Hz.bin"},
	{"aw8697_fingerprint_effect2_RTP_111_230Hz.bin"},
	{"aw8697_fingerprint_effect3_RTP_112_230Hz.bin"},
	{"aw8697_fingerprint_effect4_RTP_113_230Hz.bin"},
	{"aw8697_fingerprint_effect5_RTP_114_230Hz.bin"},
	{"aw8697_fingerprint_effect6_RTP_115_230Hz.bin"},
	{"aw8697_fingerprint_effect7_RTP_116_230Hz.bin"},
	{"aw8697_fingerprint_effect8_RTP_117_230Hz.bin"},
	{"aw8697_breath_simulation_RTP_118_230Hz.bin"},
	{"aw8697_reserved_119.bin"},

	{"aw8697_Miss_RTP_120.bin"},
	{"aw8697_Scenic_RTP_121_230Hz.bin"},
	{"aw8697_voice_assistant_RTP_122_230Hz.bin"},
/* used for 7 */
	{"aw8697_Appear_channel_RTP_123_230Hz.bin"},
	{"aw8697_Miss_RTP_124_230Hz.bin"},
	{"aw8697_Music_channel_RTP_125_230Hz.bin"},
	{"aw8697_Percussion_channel_RTP_126_230Hz.bin"},
	{"aw8697_Ripple_channel_RTP_127_230Hz.bin"},
	{"aw8697_Bright_channel_RTP_128_230Hz.bin"},
	{"aw8697_Fun_channel_RTP_129_230Hz.bin"},
	{"aw8697_Glittering_channel_RTP_130_230Hz.bin"},
	{"aw8697_Harp_channel_RTP_131_230Hz.bin"},
	{"aw8697_Overtone_channel_RTP_132_230Hz.bin"},
	{"aw8697_Simple_channel_RTP_133_230Hz.bin"},

	{"aw8697_Seine_past_RTP_134_230Hz.bin"},
	{"aw8697_Classical_ring_RTP_135_230Hz.bin"},
	{"aw8697_Long_for_RTP_136_230Hz.bin"},
	{"aw8697_Romantic_RTP_137_230Hz.bin"},
	{"aw8697_Bliss_RTP_138_230Hz.bin"},
	{"aw8697_Dream_RTP_139_230Hz.bin"},
	{"aw8697_Relax_RTP_140_230Hz.bin"},
	{"aw8697_Joy_channel_RTP_141_230Hz.bin"},
	{"aw8697_weather_wind_RTP_142_230Hz.bin"},
	{"aw8697_weather_cloudy_RTP_143_230Hz.bin"},
	{"aw8697_weather_thunderstorm_RTP_144_230Hz.bin"},
	{"aw8697_weather_default_RTP_145_230Hz.bin"},
	{"aw8697_weather_sunny_RTP_146_230Hz.bin"},
	{"aw8697_weather_smog_RTP_147_230Hz.bin"},
	{"aw8697_weather_snow_RTP_148_230Hz.bin"},
	{"aw8697_weather_rain_RTP_149_230Hz.bin"},
/* used for 7 end*/
	{"aw8697_rtp_lighthouse.bin"},
	{"aw8697_rtp_silk_19081.bin"},
	{"aw8697_reserved_152.bin"},
	{"aw8697_reserved_153.bin"},
	{"aw8697_reserved_154.bin"},
	{"aw8697_reserved_155.bin"},
	{"aw8697_reserved_156.bin"},
	{"aw8697_reserved_157.bin"},
	{"aw8697_reserved_158.bin"},
	{"aw8697_reserved_159.bin"},
	{"aw8697_reserved_160.bin"},

	{"aw8697_oplus_its_oplus_RTP_161_235Hz.bin"},
	{"aw8697_oplus_tune_RTP_162_235Hz.bin"},
	{"aw8697_oplus_jingle_RTP_163_235Hz.bin"},
	{"aw8697_reserved_164.bin"},
	{"aw8697_reserved_165.bin"},
	{"aw8697_reserved_166.bin"},
	{"aw8697_reserved_167.bin"},
	{"aw8697_reserved_168.bin"},
	{"aw8697_reserved_169.bin"},
	{"aw8697_reserved_170.bin"},
};
#endif /* OPLUS_FEATURE_CHG_BASIC */

static char aw_rtp_name_19065_234Hz[][AW_RTP_NAME_MAX] = {
	{"aw8697_rtp.bin"},
#ifdef OPLUS_FEATURE_CHG_BASIC
	{"aw8697_Hearty_channel_RTP_1.bin"},
	{"aw8697_Instant_channel_RTP_2_234Hz.bin"},
	{"aw8697_Music_channel_RTP_3.bin"},
	{"aw8697_Percussion_channel_RTP_4.bin"},
	{"aw8697_Ripple_channel_RTP_5.bin"},
	{"aw8697_Bright_channel_RTP_6.bin"},
	{"aw8697_Fun_channel_RTP_7.bin"},
	{"aw8697_Glittering_channel_RTP_8.bin"},
	{"aw8697_Granules_channel_RTP_9_234Hz.bin"},
	{"aw8697_Harp_channel_RTP_10.bin"},
	{"aw8697_Impression_channel_RTP_11.bin"},
	{"aw8697_Ingenious_channel_RTP_12_234Hz.bin"},
	{"aw8697_Joy_channel_RTP_13_234Hz.bin"},
	{"aw8697_Overtone_channel_RTP_14.bin"},
	{"aw8697_Receive_channel_RTP_15_234Hz.bin"},
	{"aw8697_Splash_channel_RTP_16_234Hz.bin"},

	{"aw8697_About_School_RTP_17_234Hz.bin"},
	{"aw8697_Bliss_RTP_18.bin"},
	{"aw8697_Childhood_RTP_19_234Hz.bin"},
	{"aw8697_Commuting_RTP_20_234Hz.bin"},
	{"aw8697_Dream_RTP_21.bin"},
	{"aw8697_Firefly_RTP_22_234Hz.bin"},
	{"aw8697_Gathering_RTP_23.bin"},
	{"aw8697_Gaze_RTP_24_234Hz.bin"},
	{"aw8697_Lakeside_RTP_25_234Hz.bin"},
	{"aw8697_Lifestyle_RTP_26.bin"},
	{"aw8697_Memories_RTP_27_234Hz.bin"},
	{"aw8697_Messy_RTP_28_234Hz.bin"},
	{"aw8697_Night_RTP_29_234Hz.bin"},
	{"aw8697_Passionate_Dance_RTP_30_234Hz.bin"},
	{"aw8697_Playground_RTP_31_234Hz.bin"},
	{"aw8697_Relax_RTP_32_234Hz.bin"},
	{"aw8697_Reminiscence_RTP_33.bin"},
	{"aw8697_Silence_From_Afar_RTP_34_234Hz.bin"},
	{"aw8697_Silence_RTP_35_234Hz.bin"},
	{"aw8697_Stars_RTP_36_234Hz.bin"},
	{"aw8697_Summer_RTP_37_234Hz.bin"},
	{"aw8697_Toys_RTP_38_234Hz.bin"},
	{"aw8697_Travel_RTP_39.bin"},
	{"aw8697_Vision_RTP_40.bin"},

	{"aw8697_waltz_channel_RTP_41_234Hz.bin"},
	{"aw8697_cut_channel_RTP_42_234Hz.bin"},
	{"aw8697_clock_channel_RTP_43_234Hz.bin"},
	{"aw8697_long_sound_channel_RTP_44_234Hz.bin"},
	{"aw8697_short_channel_RTP_45_234Hz.bin"},
	{"aw8697_two_error_remaind_RTP_46_234Hz.bin"},

	{"aw8697_kill_program_RTP_47_234Hz.bin"},
	{"aw8697_Simple_channel_RTP_48.bin"},
	{"aw8697_Pure_RTP_49_234Hz.bin"},
	{"aw8697_reserved_sound_channel_RTP_50.bin"},

	{"aw8697_high_temp_high_humidity_channel_RTP_51.bin"},

	{"aw8697_old_steady_test_RTP_52.bin"},
	{"aw8697_listen_pop_53_235Hz.bin"},
	{"aw8697_desk_7_RTP_54_234Hz.bin"},
	{"aw8697_nfc_10_RTP_55_234Hz.bin"},
	{"aw8697_vibrator_remain_12_RTP_56_234Hz.bin"},
	{"aw8697_notice_13_RTP_57.bin"},
	{"aw8697_third_ring_14_RTP_58.bin"},
	{"aw8697_reserved_59.bin"},

	{"aw8697_honor_fisrt_kill_RTP_60_234Hz.bin"},
	{"aw8697_honor_two_kill_RTP_61_234Hz.bin"},
	{"aw8697_honor_three_kill_RTP_62_234Hz.bin"},
	{"aw8697_honor_four_kill_RTP_63_234Hz.bin"},
	{"aw8697_honor_five_kill_RTP_64_234Hz.bin"},
	{"aw8697_honor_three_continu_kill_RTP_65_234Hz.bin"},
	{"aw8697_honor_four_continu_kill_RTP_66_234Hz.bin"},
	{"aw8697_honor_unstoppable_RTP_67_234Hz.bin"},
	{"aw8697_honor_thousands_kill_RTP_68_234Hz.bin"},
	{"aw8697_honor_lengendary_RTP_69_234Hz.bin"},


	{"aw8697_reserved_70.bin"},
	{"aw8697_reserved_71.bin"},
	{"aw8697_reserved_72.bin"},
	{"aw8697_reserved_73.bin"},
	{"aw8697_reserved_74.bin"},
	{"aw8697_reserved_75.bin"},
	{"aw8697_reserved_76.bin"},
	{"aw8697_reserved_77.bin"},
	{"aw8697_reserved_78.bin"},
	{"aw8697_reserved_79.bin"},

	{"aw8697_reserved_80.bin"},
	{"aw8697_reserved_81.bin"},
	{"aw8697_reserved_82.bin"},
	{"aw8697_reserved_83.bin"},
	{"aw8697_reserved_84.bin"},
	{"aw8697_reserved_85.bin"},
	{"aw8697_reserved_86.bin"},
	{"aw8697_reserved_87.bin"},
	{"aw8697_reserved_88.bin"},
	{"aw8697_reserved_89.bin"},

	{"aw8697_reserved_90.bin"},
	{"aw8697_reserved_91.bin"},
	{"aw8697_reserved_92.bin"},
	{"aw8697_reserved_93.bin"},
	{"aw8697_reserved_94.bin"},
	{"aw8697_reserved_95.bin"},
	{"aw8697_reserved_96.bin"},
	{"aw8697_reserved_97.bin"},
	{"aw8697_reserved_98.bin"},
	{"aw8697_reserved_99.bin"},

	{"aw8697_soldier_first_kill_RTP_100_234Hz.bin"},
	{"aw8697_soldier_second_kill_RTP_101_234Hz.bin"},
	{"aw8697_soldier_third_kill_RTP_102_234Hz.bin"},
	{"aw8697_soldier_fourth_kill_RTP_103_234Hz.bin"},
	{"aw8697_soldier_fifth_kill_RTP_104_234Hz.bin"},
	{"aw8697_stepable_regulate_RTP_105_234Hz.bin"},
	{"aw8697_voice_level_bar_edge_RTP_106_234Hz.bin"},
	{"aw8697_strength_level_bar_edge_RTP_107_234Hz.bin"},
	{"aw8697_charging_simulation_RTP_108_234Hz.bin"},
	{"aw8697_fingerprint_success_RTP_109_234Hz.bin"},

	{"aw8697_fingerprint_effect1_RTP_110_234Hz.bin"},
	{"aw8697_fingerprint_effect2_RTP_111_234Hz.bin"},
	{"aw8697_fingerprint_effect3_RTP_112_234Hz.bin"},
	{"aw8697_fingerprint_effect4_RTP_113_234Hz.bin"},
	{"aw8697_fingerprint_effect5_RTP_114_234Hz.bin"},
	{"aw8697_fingerprint_effect6_RTP_115_234Hz.bin"},
	{"aw8697_fingerprint_effect7_RTP_116_234Hz.bin"},
	{"aw8697_fingerprint_effect8_RTP_117_234Hz.bin"},
	{"aw8697_breath_simulation_RTP_118_234Hz.bin"},
	{"aw8697_reserved_119.bin"},

	{"aw8697_Miss_RTP_120.bin"},
	{"aw8697_Scenic_RTP_121_234Hz.bin"},
	{"aw8697_voice_assistant_RTP_122_234Hz.bin"},
/* used for 7 */
	{"aw8697_Appear_channel_RTP_123_234Hz.bin"},
	{"aw8697_Miss_RTP_124_234Hz.bin"},
	{"aw8697_Music_channel_RTP_125_234Hz.bin"},
	{"aw8697_Percussion_channel_RTP_126_234Hz.bin"},
	{"aw8697_Ripple_channel_RTP_127_234Hz.bin"},
	{"aw8697_Bright_channel_RTP_128_234Hz.bin"},
	{"aw8697_Fun_channel_RTP_129_234Hz.bin"},
	{"aw8697_Glittering_channel_RTP_130_234Hz.bin"},
	{"aw8697_Harp_channel_RTP_131_234Hz.bin"},
	{"aw8697_Overtone_channel_RTP_132_234Hz.bin"},
	{"aw8697_Simple_channel_RTP_133_234Hz.bin"},

	{"aw8697_Seine_past_RTP_134_234Hz.bin"},
	{"aw8697_Classical_ring_RTP_135_234Hz.bin"},
	{"aw8697_Long_for_RTP_136_234Hz.bin"},
	{"aw8697_Romantic_RTP_137_234Hz.bin"},
	{"aw8697_Bliss_RTP_138_234Hz.bin"},
	{"aw8697_Dream_RTP_139_234Hz.bin"},
	{"aw8697_Relax_RTP_140_234Hz.bin"},
	{"aw8697_Joy_channel_RTP_141_234Hz.bin"},
	{"aw8697_weather_wind_RTP_142_234Hz.bin"},
	{"aw8697_weather_cloudy_RTP_143_234Hz.bin"},
	{"aw8697_weather_thunderstorm_RTP_144_234Hz.bin"},
	{"aw8697_weather_default_RTP_145_234Hz.bin"},
	{"aw8697_weather_sunny_RTP_146_234Hz.bin"},
	{"aw8697_weather_smog_RTP_147_234Hz.bin"},
	{"aw8697_weather_snow_RTP_148_234Hz.bin"},
	{"aw8697_weather_rain_RTP_149_234Hz.bin"},

#endif
	{"aw8697_rtp_lighthouse.bin"},
	{"aw8697_rtp_silk_19081.bin"},
	{"aw8697_reserved_152.bin"},
	{"aw8697_reserved_153.bin"},
	{"aw8697_reserved_154.bin"},
	{"aw8697_reserved_155.bin"},
	{"aw8697_reserved_156.bin"},
	{"aw8697_reserved_157.bin"},
	{"aw8697_reserved_158.bin"},
	{"aw8697_reserved_159.bin"},
	{"aw8697_reserved_160.bin"},

	{"aw8697_oplus_its_oplus_RTP_161_235Hz.bin"},
	{"aw8697_oplus_tune_RTP_162_235Hz.bin"},
	{"aw8697_oplus_jingle_RTP_163_235Hz.bin"},
	{"aw8697_reserved_164.bin"},
	{"aw8697_reserved_165.bin"},
	{"aw8697_reserved_166.bin"},
	{"aw8697_reserved_167.bin"},
	{"aw8697_reserved_168.bin"},
	{"aw8697_reserved_169.bin"},
	{"aw8697_reserved_170.bin"},
};

static char aw_rtp_name_19065_237Hz[][AW_RTP_NAME_MAX] = {
	{"aw8697_rtp.bin"},
#ifdef OPLUS_FEATURE_CHG_BASIC
	{"aw8697_Hearty_channel_RTP_1.bin"},
	{"aw8697_Instant_channel_RTP_2_237Hz.bin"},
	{"aw8697_Music_channel_RTP_3.bin"},
	{"aw8697_Percussion_channel_RTP_4.bin"},
	{"aw8697_Ripple_channel_RTP_5.bin"},
	{"aw8697_Bright_channel_RTP_6.bin"},
	{"aw8697_Fun_channel_RTP_7.bin"},
	{"aw8697_Glittering_channel_RTP_8.bin"},
	{"aw8697_Granules_channel_RTP_9_237Hz.bin"},
	{"aw8697_Harp_channel_RTP_10.bin"},
	{"aw8697_Impression_channel_RTP_11.bin"},
	{"aw8697_Ingenious_channel_RTP_12_237Hz.bin"},
	{"aw8697_Joy_channel_RTP_13_237Hz.bin"},
	{"aw8697_Overtone_channel_RTP_14.bin"},
	{"aw8697_Receive_channel_RTP_15_237Hz.bin"},
	{"aw8697_Splash_channel_RTP_16_237Hz.bin"},

	{"aw8697_About_School_RTP_17_237Hz.bin"},
	{"aw8697_Bliss_RTP_18.bin"},
	{"aw8697_Childhood_RTP_19_237Hz.bin"},
	{"aw8697_Commuting_RTP_20_237Hz.bin"},
	{"aw8697_Dream_RTP_21.bin"},
	{"aw8697_Firefly_RTP_22_237Hz.bin"},
	{"aw8697_Gathering_RTP_23.bin"},
	{"aw8697_Gaze_RTP_24_237Hz.bin"},
	{"aw8697_Lakeside_RTP_25_237Hz.bin"},
	{"aw8697_Lifestyle_RTP_26.bin"},
	{"aw8697_Memories_RTP_27_237Hz.bin"},
	{"aw8697_Messy_RTP_28_237Hz.bin"},
	{"aw8697_Night_RTP_29_237Hz.bin"},
	{"aw8697_Passionate_Dance_RTP_30_237Hz.bin"},
	{"aw8697_Playground_RTP_31_237Hz.bin"},
	{"aw8697_Relax_RTP_32_237Hz.bin"},
	{"aw8697_Reminiscence_RTP_33.bin"},
	{"aw8697_Silence_From_Afar_RTP_34_237Hz.bin"},
	{"aw8697_Silence_RTP_35_237Hz.bin"},
	{"aw8697_Stars_RTP_36_237Hz.bin"},
	{"aw8697_Summer_RTP_37_237Hz.bin"},
	{"aw8697_Toys_RTP_38_237Hz.bin"},
	{"aw8697_Travel_RTP_39.bin"},
	{"aw8697_Vision_RTP_40.bin"},

	{"aw8697_waltz_channel_RTP_41_237Hz.bin"},
	{"aw8697_cut_channel_RTP_42_237Hz.bin"},
	{"aw8697_clock_channel_RTP_43_237Hz.bin"},
	{"aw8697_long_sound_channel_RTP_44_237Hz.bin"},
	{"aw8697_short_channel_RTP_45_237Hz.bin"},
	{"aw8697_two_error_remaind_RTP_46_237Hz.bin"},

	{"aw8697_kill_program_RTP_47_237Hz.bin"},
	{"aw8697_Simple_channel_RTP_48.bin"},
	{"aw8697_Pure_RTP_49_237Hz.bin"},
	{"aw8697_reserved_sound_channel_RTP_50.bin"},

	{"aw8697_high_temp_high_humidity_channel_RTP_51.bin"},

	{"aw8697_old_steady_test_RTP_52.bin"},
	{"aw8697_listen_pop_53_235Hz.bin"},
	{"aw8697_desk_7_RTP_54_237Hz.bin"},
	{"aw8697_nfc_10_RTP_55_237Hz.bin"},
	{"aw8697_vibrator_remain_12_RTP_56_237Hz.bin"},
	{"aw8697_notice_13_RTP_57.bin"},
	{"aw8697_third_ring_14_RTP_58.bin"},
	{"aw8697_emergency_warning_RTP_59_234Hz.bin"},

	{"aw8697_honor_fisrt_kill_RTP_60_237Hz.bin"},
	{"aw8697_honor_two_kill_RTP_61_237Hz.bin"},
	{"aw8697_honor_three_kill_RTP_62_237Hz.bin"},
	{"aw8697_honor_four_kill_RTP_63_237Hz.bin"},
	{"aw8697_honor_five_kill_RTP_64_237Hz.bin"},
	{"aw8697_honor_three_continu_kill_RTP_65_237Hz.bin"},
	{"aw8697_honor_four_continu_kill_RTP_66_237Hz.bin"},
	{"aw8697_honor_unstoppable_RTP_67_237Hz.bin"},
	{"aw8697_honor_thousands_kill_RTP_68_237Hz.bin"},
	{"aw8697_honor_lengendary_RTP_69_237Hz.bin"},


	{"aw8697_reserved_70.bin"},
	{"aw8697_reserved_71.bin"},
	{"aw8697_reserved_72.bin"},
	{"aw8697_reserved_73.bin"},
	{"aw8697_reserved_74.bin"},
	{"aw8697_reserved_75.bin"},
	{"aw8697_reserved_76.bin"},
	{"aw8697_reserved_77.bin"},
	{"aw8697_reserved_78.bin"},
	{"aw8697_reserved_79.bin"},

	{"aw8697_reserved_80.bin"},
	{"aw8697_reserved_81.bin"},
	{"aw8697_reserved_82.bin"},
	{"aw8697_reserved_83.bin"},
	{"aw8697_reserved_84.bin"},
	{"aw8697_reserved_85.bin"},
	{"aw8697_reserved_86.bin"},
	{"aw8697_reserved_87.bin"},
	{"aw8697_reserved_88.bin"},
	{"aw8697_reserved_89.bin"},

	{"aw8697_reserved_90.bin"},
	{"aw8697_reserved_91.bin"},
	{"aw8697_reserved_92.bin"},
	{"aw8697_reserved_93.bin"},
	{"aw8697_reserved_94.bin"},
	{"aw8697_reserved_95.bin"},
	{"aw8697_reserved_96.bin"},
	{"aw8697_reserved_97.bin"},
	{"aw8697_reserved_98.bin"},
	{"aw8697_reserved_99.bin"},

	{"aw8697_soldier_first_kill_RTP_100_237Hz.bin"},
	{"aw8697_soldier_second_kill_RTP_101_237Hz.bin"},
	{"aw8697_soldier_third_kill_RTP_102_237Hz.bin"},
	{"aw8697_soldier_fourth_kill_RTP_103_237Hz.bin"},
	{"aw8697_soldier_fifth_kill_RTP_104_237Hz.bin"},
	{"aw8697_stepable_regulate_RTP_105_237Hz.bin"},
	{"aw8697_voice_level_bar_edge_RTP_106_237Hz.bin"},
	{"aw8697_strength_level_bar_edge_RTP_107_237Hz.bin"},
	{"aw8697_charging_simulation_RTP_108_237Hz.bin"},
	{"aw8697_fingerprint_success_RTP_109_237Hz.bin"},

	{"aw8697_fingerprint_effect1_RTP_110_237Hz.bin"},
	{"aw8697_fingerprint_effect2_RTP_111_237Hz.bin"},
	{"aw8697_fingerprint_effect3_RTP_112_237Hz.bin"},
	{"aw8697_fingerprint_effect4_RTP_113_237Hz.bin"},
	{"aw8697_fingerprint_effect5_RTP_114_237Hz.bin"},
	{"aw8697_fingerprint_effect6_RTP_115_237Hz.bin"},
	{"aw8697_fingerprint_effect7_RTP_116_237Hz.bin"},
	{"aw8697_fingerprint_effect8_RTP_117_237Hz.bin"},
	{"aw8697_breath_simulation_RTP_118_237Hz.bin"},
	{"aw8697_reserved_119.bin"},

	{"aw8697_Miss_RTP_120.bin"},
	{"aw8697_Scenic_RTP_121_237Hz.bin"},
	{"aw8697_voice_assistant_RTP_122_237Hz.bin"},
/* used for 7 */
	{"aw8697_Appear_channel_RTP_123_237Hz.bin"},
	{"aw8697_Miss_RTP_124_237Hz.bin"},
	{"aw8697_Music_channel_RTP_125_237Hz.bin"},
	{"aw8697_Percussion_channel_RTP_126_237Hz.bin"},
	{"aw8697_Ripple_channel_RTP_127_237Hz.bin"},
	{"aw8697_Bright_channel_RTP_128_237Hz.bin"},
	{"aw8697_Fun_channel_RTP_129_237Hz.bin"},
	{"aw8697_Glittering_channel_RTP_130_237Hz.bin"},
	{"aw8697_Harp_channel_RTP_131_237Hz.bin"},
	{"aw8697_Overtone_channel_RTP_132_237Hz.bin"},
	{"aw8697_Simple_channel_RTP_133_237Hz.bin"},

	{"aw8697_Seine_past_RTP_134_237Hz.bin"},
	{"aw8697_Classical_ring_RTP_135_237Hz.bin"},
	{"aw8697_Long_for_RTP_136_237Hz.bin"},
	{"aw8697_Romantic_RTP_137_237Hz.bin"},
	{"aw8697_Bliss_RTP_138_237Hz.bin"},
	{"aw8697_Dream_RTP_139_237Hz.bin"},
	{"aw8697_Relax_RTP_140_237Hz.bin"},
	{"aw8697_Joy_channel_RTP_141_237Hz.bin"},
	{"aw8697_weather_wind_RTP_142_237Hz.bin"},
	{"aw8697_weather_cloudy_RTP_143_237Hz.bin"},
	{"aw8697_weather_thunderstorm_RTP_144_237Hz.bin"},
	{"aw8697_weather_default_RTP_145_237Hz.bin"},
	{"aw8697_weather_sunny_RTP_146_237Hz.bin"},
	{"aw8697_weather_smog_RTP_147_237Hz.bin"},
	{"aw8697_weather_snow_RTP_148_237Hz.bin"},
	{"aw8697_weather_rain_RTP_149_237Hz.bin"},

#endif
	{"aw8697_rtp_lighthouse.bin"},
	{"aw8697_rtp_silk_19081.bin"},
};

static int container_init(int size)
{
	if (!aw_rtp || size > aw_container_size) {
		if (aw_rtp) {
			vfree(aw_rtp);
		}
		aw_rtp = vmalloc(size);
		if (!aw_rtp) {
			aw_dev_err("%s: error allocating memory\n", __func__);
#ifdef CONFIG_HAPTIC_FEEDBACK_MODULE
			(void)oplus_haptic_track_mem_alloc_err(HAPTIC_MEM_ALLOC_TRACK, size, __func__);
#endif
			return -ENOMEM;
		}
		aw_container_size = size;
	}

	memset(aw_rtp, 0, size);

	return 0;
}

/*********************************************************
 *
 * I2C Read/Write
 *
 *********************************************************/
#if IS_ENABLED(CONFIG_OPLUS_FEATURE_FAULT_INJECT_AW8692X)
noinline
#endif
int i2c_r_bytes(struct aw_haptic *aw_haptic, uint8_t reg_addr, uint8_t *buf,
		uint32_t len)
{
	int ret;
	struct i2c_msg msg[] = {
		[0] = {
			.addr = aw_haptic->i2c->addr,
			.flags = 0,
			.len = sizeof(uint8_t),
			.buf = &reg_addr,
			},
		[1] = {
			.addr = aw_haptic->i2c->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
			},
	};

	ret = i2c_transfer(aw_haptic->i2c->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0) {
		aw_dev_err("%s: transfer failed.", __func__);
#ifdef CONFIG_HAPTIC_FEEDBACK_MODULE
		(void)oplus_haptic_track_dev_err(HAPTIC_I2C_READ_TRACK_ERR, reg_addr, ret);
#endif
		return ret;
	} else if (ret != 2) {
		aw_dev_err("%s: transfer failed(size error).", __func__);
		return -ENXIO;
	}

	return ret;
}

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_FAULT_INJECT_AW8692X)
noinline
#endif
int i2c_w_bytes(struct aw_haptic *aw_haptic, uint8_t reg_addr, uint8_t *buf,
		uint32_t len)
{
	uint8_t *data = NULL;
	int ret = -1;

	data = kmalloc(len + 1, GFP_KERNEL);
	data[0] = reg_addr;
	memcpy(&data[1], buf, len);
	ret = i2c_master_send(aw_haptic->i2c, data, len + 1);
	if (ret < 0) {
		aw_dev_err("%s: i2c master send 0x%02x error\n",
			   __func__, reg_addr);
#ifdef CONFIG_HAPTIC_FEEDBACK_MODULE
		(void)oplus_haptic_track_dev_err(HAPTIC_I2C_WRITE_TRACK_ERR, reg_addr, ret);
#endif
	}
	kfree(data);
	return ret;
}

int i2c_w_bits(struct aw_haptic *aw_haptic, uint8_t reg_addr, uint32_t mask,
	       uint8_t reg_data)
{
	uint8_t reg_val = 0;
	int ret = -1;

	ret = i2c_r_bytes(aw_haptic, reg_addr, &reg_val, AW_I2C_BYTE_ONE);
	if (ret < 0) {
		aw_dev_err("%s: i2c read error, ret=%d\n",
			   __func__, ret);
		return ret;
	}
	reg_val &= mask;
	reg_val |= reg_data;
	ret = i2c_w_bytes(aw_haptic, reg_addr, &reg_val, AW_I2C_BYTE_ONE);
	if (ret < 0) {
		aw_dev_err("%s: i2c write error, ret=%d\n",
			   __func__, ret);
		return ret;
	}
	return 0;
}

#define DEFAULT_BOOST_VOLT 0x4F
#define DEFAULT_BOOST_VOLT_AW86907 0x1F
#define VMAX_GAIN_NUM 17
static int parse_dt(struct device *dev, struct aw_haptic *aw_haptic,
			 struct device_node *np)
{
	aw_haptic->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (aw_haptic->reset_gpio < 0) {
		aw_dev_err("%s: no reset gpio provide\n", __func__);
		return -EPERM;
	}
	aw_dev_info("%s: reset gpio provide ok %d\n", __func__,
		aw_haptic->reset_gpio);
	aw_haptic->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (aw_haptic->irq_gpio < 0)
		aw_dev_err("%s: no irq gpio provided.\n", __func__);
	else
		aw_dev_info("%s: irq gpio provide ok irq = %d.\n",
			__func__, aw_haptic->irq_gpio);

#ifdef OPLUS_FEATURE_CHG_BASIC
	if (of_property_read_u32(np, "qcom,device_id", &aw_haptic->device_id))
		aw_haptic->device_id = 815;
	aw_dev_info("%s: device_id=%d\n", __func__, aw_haptic->device_id);
#endif

	return 0;
}

static int parse_vibrate_param_dts(struct device *dev, struct aw_haptic *aw_haptic,
			 struct device_node *np)
{
#ifdef OPLUS_FEATURE_CHG_BASIC

	int max_boost_voltage = 0;
	uint8_t vmax[VMAX_GAIN_NUM];
	uint8_t gain[VMAX_GAIN_NUM];
	uint32_t val = 0;
	int i = 0;

	if (aw_haptic->chipid == AW86907_CHIPID) {
		if (of_property_read_u32(np, "oplus,aw86907_boost_voltage", &max_boost_voltage))
			AW86927_HAPTIC_HIGH_LEVEL_REG_VAL = DEFAULT_BOOST_VOLT_AW86907; /* boost 8.4V */
		else
			AW86927_HAPTIC_HIGH_LEVEL_REG_VAL = max_boost_voltage;
		aw_dev_info("%s: aw86907 boost_voltage=%d\n", __func__, AW86927_HAPTIC_HIGH_LEVEL_REG_VAL);

		val = of_property_read_u8_array(np, "haptic_hv_aw86907_vmax",
							vmax, ARRAY_SIZE(vmax));
		if (val != 0) {
			aw_dev_info("aw_haptic_aw86907_vmax not found");
		} else {
			for (i = 0; i < ARRAY_SIZE(vmax); i++) {
				vmax_map[i].vmax = vmax[i];
				aw_dev_info("_aw86907 vmax_map vmax: 0x%x vmax: 0x%x", vmax_map[i].vmax, vmax[i]);
			}
		}
		val = of_property_read_u8_array(np, "haptic_hv_aw86907_gain",
							gain, ARRAY_SIZE(gain));
		if (val != 0) {
			aw_dev_info("aw_haptic_aw86907_gain not found");
		} else {
			for (i = 0; i < ARRAY_SIZE(gain); i++) {
				vmax_map[i].gain = gain[i];
				aw_dev_info("_aw86907 vmax_map gain: 0x%x gain: 0x%x", vmax_map[i].gain, gain[i]);
			}
		}
	}else {
		if (of_property_read_u32(np, "oplus,aw86927_boost_voltage", &max_boost_voltage))
			AW86927_HAPTIC_HIGH_LEVEL_REG_VAL = DEFAULT_BOOST_VOLT; /* boost 8.4V */
		else
			AW86927_HAPTIC_HIGH_LEVEL_REG_VAL = max_boost_voltage;
		aw_dev_info("%s: aw86927 boost_voltage=%d\n", __func__, AW86927_HAPTIC_HIGH_LEVEL_REG_VAL);

		val = of_property_read_u8_array(np, "haptic_hv_vmax",
							vmax, ARRAY_SIZE(vmax));
		if (val != 0) {
			aw_dev_info("aw_haptic_vmax not found");
		} else {
			for (i = 0; i < ARRAY_SIZE(vmax); i++) {
				vmax_map[i].vmax = vmax[i];
				aw_dev_info("vmax_map vmax: 0x%x vmax: 0x%x", vmax_map[i].vmax, vmax[i]);
			}
		}
		val = of_property_read_u8_array(np, "haptic_hv_gain",
							gain, ARRAY_SIZE(gain));
		if (val != 0) {
			aw_dev_info("aw_haptic_gain not found");
		} else {
			for (i = 0; i < ARRAY_SIZE(gain); i++) {
				vmax_map[i].gain = gain[i];
				aw_dev_info("vmax_map gain: 0x%x gain: 0x%x", vmax_map[i].gain, gain[i]);
			}
		}
	}
#endif

	return 0;
}

static void hw_reset(struct aw_haptic *aw_haptic)
{
	aw_dev_info("%s: enter\n", __func__);
	if (aw_haptic && gpio_is_valid(aw_haptic->reset_gpio)) {
		gpio_set_value_cansleep(aw_haptic->reset_gpio, 0);
		usleep_range(1000, 2000);
		gpio_set_value_cansleep(aw_haptic->reset_gpio, 1);
		usleep_range(8000, 8500);
	} else {
		aw_dev_err("%s: failed\n", __func__);
	}
}

static void sw_reset(struct aw_haptic *aw_haptic)
{
	uint8_t reset = AW_BIT_RESET;

	aw_dev_dbg("%s: enter\n", __func__);
	i2c_w_bytes(aw_haptic, AW_REG_CHIPID, &reset, AW_I2C_BYTE_ONE);
	usleep_range(3000, 3500);
}

static int judge_value(uint8_t reg)
{
	int ret = 0;

	if (!reg)
		return -ERANGE;
	switch (reg) {
	case AW86925_BIT_RSTCFG_PRE_VAL:
	case AW86926_BIT_RSTCFG_PRE_VAL:
	case AW86927_BIT_RSTCFG_PRE_VAL:
	case AW86928_BIT_RSTCFG_PRE_VAL:
	case AW86925_BIT_RSTCFG_VAL:
	case AW86926_BIT_RSTCFG_VAL:
	case AW86927_BIT_RSTCFG_VAL:
	case AW86928_BIT_RSTCFG_VAL:
		ret = -ERANGE;
		break;
	default:
		break;
	}
	return ret;
}

static int read_chipid(struct aw_haptic *aw_haptic, uint32_t *reg_val)
{
	uint8_t value[2] = {0};
	int ret = 0;

	aw_dev_dbg("%s: enter!\n", __func__);
	/* try the old way of read chip id */
	ret = i2c_r_bytes(aw_haptic, AW_REG_CHIPID, &value[0], AW_I2C_BYTE_ONE);
	if (ret < 0)
		return ret;

	ret = judge_value(value[0]);
	if (!ret) {
		*reg_val = value[0];
		return ret;
	}
	/* try the new way of read chip id */
	ret = i2c_r_bytes(aw_haptic, AW_REG_CHIPIDH, value, AW_I2C_BYTE_TWO);
	if (ret < 0)
		return ret;
	*reg_val = value[0] << 8 | value[1];
	return ret;
}

static int parse_chipid(struct aw_haptic *aw_haptic)
{
	int ret = -1;
	uint32_t reg = 0;
	uint8_t cnt = 0;

	for (cnt = 0; cnt < AW_READ_CHIPID_RETRIES; cnt++) {
		ret = read_chipid(aw_haptic, &reg);
		aw_dev_info("%s: reg_val = 0x%02X\n",
			    __func__, reg);
		if (ret < 0) {
			aw_dev_err("%s: failed to read AW_REG_ID: %d\n",
				   __func__, ret);
		}
		switch (reg) {
		case AW8695_CHIPID:
			aw_haptic->chipid = AW8695_CHIPID;
			aw_haptic->bst_pc = AW_BST_PC_L1;
			aw_haptic->i2s_config = false;
			aw_dev_info("%s: detected aw8695.\n",
				    __func__);
			return 0;
/*
		case AW8697_CHIPID:
			aw_haptic->chipid = AW8697_CHIPID;
			aw_haptic->bst_pc = AW_BST_PC_L2;
			aw_haptic->i2s_config = false;
			aw_dev_info("%s: detected aw8697.\n",
				    __func__);
			return 0;
*/
		case AW86905_CHIPID:
			aw_haptic->chipid = AW86905_CHIPID;
			aw_haptic->bst_pc = AW_BST_PC_L1;
			aw_haptic->i2s_config = false;
			aw_dev_info("%s: detected aw86905.\n", __func__);
			return 0;
		case AW86907_CHIPID:
			aw_haptic->chipid = AW86907_CHIPID;
			aw_haptic->bst_pc = AW_BST_PC_L2;
			aw_haptic->i2s_config = false;
			aw_dev_info("%s: detected aw86907.\n", __func__);
			return 0;
		case AW86915_CHIPID:
			aw_haptic->chipid = AW86915_CHIPID;
			aw_haptic->bst_pc = AW_BST_PC_L1;
			aw_haptic->i2s_config = true;
			aw_dev_info("%s: detected aw86915.\n", __func__);
			return 0;
		case AW86917_CHIPID:
			aw_haptic->chipid = AW86917_CHIPID;
			aw_haptic->bst_pc = AW_BST_PC_L2;
			aw_haptic->i2s_config = true;
			aw_dev_info("%s: detected aw86917.\n", __func__);
			return 0;

		case AW86925_CHIPID:
			aw_haptic->chipid = AW86925_CHIPID;
			aw_dev_info("%s: detected aw86925.\n",
				    __func__);
			return 0;

		case AW86926_CHIPID:
			aw_haptic->chipid = AW86926_CHIPID;
			aw_dev_info("%s: detected aw86926.\n",
				    __func__);
			return 0;
		case AW86927_CHIPID:
			aw_haptic->chipid = AW86927_CHIPID;
			aw_dev_info("%s: detected aw86927.\n",
				    __func__);
			return 0;
		case AW86928_CHIPID:
			aw_haptic->chipid = AW86928_CHIPID;
			aw_dev_info("%s: detected aw86928.\n",
				    __func__);
			return 0;
		default:
			aw_dev_info("%s: unsupport device revision (0x%02X)\n",
				    __func__, reg);
			break;
		}
		usleep_range(AW_READ_CHIPID_RETRY_DELAY * 1000,
			     AW_READ_CHIPID_RETRY_DELAY * 1000 + 500);
	}
	return -EINVAL;
}

static int ctrl_init(struct aw_haptic *aw_haptic)
{
	uint32_t reg = 0;
	uint8_t cnt = 0;

	aw_dev_info("%s: enter\n", __func__);
	for (cnt = 0; cnt < AW_READ_CHIPID_RETRIES; cnt++) {
		/* hardware reset */
		hw_reset(aw_haptic);
		if (read_chipid(aw_haptic, &reg) < 0)
			aw_dev_err("%s: read chip id fail\n", __func__);
		switch (reg) {
		//case AW8695_CHIPID:
		//case AW8697_CHIPID:
			//aw_haptic->func = &aw869x_func_list;
			//return 0;
		case AW86905_CHIPID:
		case AW86907_CHIPID:
		case AW86915_CHIPID:
		case AW86917_CHIPID:
			aw_haptic->func = &aw869xx_func_list;
			return 0;
		case AW86925_CHIPID:
		case AW86926_CHIPID:
		case AW86927_CHIPID:
		case AW86928_CHIPID:
			aw_haptic->func = &aw8692x_func_list;
			return 0;
		default:
			aw_dev_err("%s: unexpected chipid\n", __func__);
			break;
		}
		usleep_range(AW_READ_CHIPID_RETRY_DELAY * 1000,
			     AW_READ_CHIPID_RETRY_DELAY * 1000 + 500);
	}
	return -EINVAL;
}

static void ram_play(struct aw_haptic *aw_haptic, uint8_t mode)
{
	aw_dev_dbg("%s: enter\n", __func__);
	aw_haptic->func->play_mode(aw_haptic, mode);
	aw_haptic->func->play_go(aw_haptic, true);
}

static int get_ram_num(struct aw_haptic *aw_haptic)
{
	uint8_t wave_addr[2] = {0};
	uint32_t first_wave_addr = 0;

	aw_dev_dbg("%s: enter!\n", __func__);
	if (!aw_haptic->ram_init) {
		aw_dev_err("%s: ram init faild, ram_num = 0!\n",
			   __func__);
		return -EPERM;
	}
	mutex_lock(&aw_haptic->lock);
	/* RAMINIT Enable */
	aw_haptic->func->ram_init(aw_haptic, true);
	aw_haptic->func->play_stop(aw_haptic);
	aw_haptic->func->set_ram_addr(aw_haptic, aw_haptic->ram.base_addr);
	aw_haptic->func->get_first_wave_addr(aw_haptic, wave_addr);
	first_wave_addr = (wave_addr[0] << 8 | wave_addr[1]);
	aw_haptic->ram.ram_num =
			(first_wave_addr - aw_haptic->ram.base_addr - 1) / 4;
	aw_dev_info("%s: first waveform addr = 0x%04x\n",
		    __func__, first_wave_addr);
	aw_dev_info("%s: ram_num = %d\n",
		    __func__, aw_haptic->ram.ram_num);
	/* RAMINIT Disable */
	aw_haptic->func->ram_init(aw_haptic, false);
	mutex_unlock(&aw_haptic->lock);
	return 0;
}

static void rtp_loaded(const struct firmware *cont, void *context)
{
	struct aw_haptic *aw_haptic = context;
	int ret = 0;
	aw_dev_info("%s enter\n", __func__);

	if (!cont) {
		aw_dev_err("%s: failed to read %s\n", __func__,
			   aw_rtp_name[aw_haptic->rtp_file_num]);
		release_firmware(cont);
		return;
	}

	aw_dev_info("%s: loaded %s - size: %zu\n", __func__,
		    aw_rtp_name[aw_haptic->rtp_file_num],
		    cont ? cont->size : 0);

	/* aw_haptic rtp update */
	mutex_lock(&aw_haptic->rtp_lock);
#ifndef OPLUS_FEATURE_CHG_BASIC
	aw_rtp = kzalloc(cont->size+sizeof(int), GFP_KERNEL);
	if (!aw_rtp) {
		release_firmware(cont);
		mutex_unlock(&aw_haptic->rtp_lock);
		aw_dev_err("%s: Error allocating memory\n", __func__);
		return;
	}
#else
	ret = container_init(cont->size + sizeof(int));
	if (ret < 0) {
		release_firmware(cont);
		mutex_unlock(&aw_haptic->rtp_lock);
		aw_dev_err("%s: Error allocating memory\n", __func__);
		return;
	}
#endif
	aw_rtp->len = cont->size;
	aw_dev_info("%s: rtp size = %d\n", __func__, aw_rtp->len);
	memcpy(aw_rtp->data, cont->data, cont->size);
	release_firmware(cont);
	mutex_unlock(&aw_haptic->rtp_lock);

	aw_haptic->rtp_init = true;
	aw_dev_info("%s: rtp update complete\n", __func__);
}

static int rtp_update(struct aw_haptic *aw_haptic)
{
	aw_dev_info("%s enter\n", __func__);

	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				       aw_rtp_name[aw_haptic->rtp_file_num],
				       aw_haptic->dev, GFP_KERNEL,
				       aw_haptic, rtp_loaded);
}

static void ram_load(const struct firmware *cont, void *context)
{
	uint16_t check_sum = 0;
	int i = 0;
	int ret = 0;
	struct aw_haptic *aw_haptic = context;
	struct aw_haptic_container *awinic_fw;

#ifdef AW_READ_BIN_FLEXBALLY
	static uint8_t load_cont;
	int ram_timer_val = 1000;

	load_cont++;
#endif
	aw_dev_info("%s: enter\n", __func__);

	if (!cont) {
		aw_dev_err("%s: failed to read ram firmware!\n",
			   __func__);
		release_firmware(cont);
#ifdef AW_READ_BIN_FLEXBALLY
		if (load_cont <= 20) {
			schedule_delayed_work(&aw_haptic->ram_work,
					      msecs_to_jiffies(ram_timer_val));
			aw_dev_info("%s:start hrtimer:load_cont%d\n",
				    __func__, load_cont);
		}
#endif
		return;
	}
	aw_dev_info("%s: loaded ram - size: %zu\n",
		    __func__, cont ? cont->size : 0);
	/* check sum */
	for (i = 2; i < cont->size; i++)
		check_sum += cont->data[i];
	if (check_sum != (uint16_t)((cont->data[0] << 8) | (cont->data[1]))) {
		aw_dev_err("%s: check sum err: check_sum=0x%04x\n",
			   __func__, check_sum);
		release_firmware(cont);
		return;
	}
	aw_dev_info("%s: check sum pass : 0x%04x\n",
		    __func__, check_sum);
	aw_haptic->ram.check_sum = check_sum;

	/* aw ram update */
	awinic_fw = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!awinic_fw) {
		release_firmware(cont);
		aw_dev_err("%s: Error allocating memory\n",
			   __func__);
		return;
	}
	awinic_fw->len = cont->size;
	memcpy(awinic_fw->data, cont->data, cont->size);
	release_firmware(cont);
	ret = aw_haptic->func->container_update(aw_haptic, awinic_fw);
	if (ret) {
		aw_dev_err("%s: ram firmware update failed!\n",
			   __func__);
	} else {
		aw_haptic->ram_init = true;
		aw_haptic->ram.len = awinic_fw->len - aw_haptic->ram.ram_shift;
		aw_dev_info("%s: ram firmware update complete!\n", __func__);
		get_ram_num(aw_haptic);
	}
	kfree(awinic_fw);
#ifdef AW_BOOT_OSC_CALI
	aw_haptic->func->upload_lra(aw_haptic, AW_WRITE_ZERO);
	rtp_osc_cali(aw_haptic);
	rtp_trim_lra_cali(aw_haptic);
#endif
	rtp_update(aw_haptic);
}

static int ram_update(struct aw_haptic *aw_haptic)
{
	uint8_t index = 0;

	aw_haptic->ram_init = false;
	aw_haptic->rtp_init = false;

	if (aw_haptic->device_id == 815) {
		if (aw_haptic->f0 < F0_VAL_MIN_0815 ||
		    aw_haptic->f0 > F0_VAL_MAX_0815)
			aw_haptic->f0 = 1700;

	} else if (aw_haptic->device_id == 81538) {
		if (aw_haptic->f0 < F0_VAL_MIN_081538 ||
		    aw_haptic->f0 > F0_VAL_MAX_081538)
			aw_haptic->f0 = 1500;

	} else if (aw_haptic->device_id == 832) {
		if (aw_haptic->f0 < F0_VAL_MIN_0832 ||
		    aw_haptic->f0 > F0_VAL_MAX_0832)
			aw_haptic->f0 = 2350;

	} else {
		if (aw_haptic->f0 < F0_VAL_MIN_0833 ||
		    aw_haptic->f0 > F0_VAL_MAX_0833)
			aw_haptic->f0 = 2350;
	}

	/* get f0 from nvram */
	aw_haptic->haptic_real_f0 = (aw_haptic->f0 / 10);
	aw_dev_info("%s: haptic_real_f0 [%d]\n", __func__, aw_haptic->haptic_real_f0);

/*
 *	if (aw8697->haptic_real_f0 <167) {
 *		index = 0;
 *	} else if (aw8697->haptic_real_f0 <169) {
 *		index = 1;
 *	} else if (aw8697->haptic_real_f0 <171) {
 *		index = 2;
 *	} else if (aw8697->haptic_real_f0 <173) {
 *		index = 3;
 *	} else {
 *		index = 4;
 *	}
 */

	if (aw_haptic->device_id == 832) {
		aw_dev_info("%s:19065 haptic bin name  %s\n", __func__,
			    aw_ram_name_19065[index]);
		return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
			aw_ram_name_19065[index], aw_haptic->dev, GFP_KERNEL,
			aw_haptic, ram_load);
	} else if (aw_haptic->device_id == 833) {
		aw_dev_info("%s:19065 haptic bin name  %s\n", __func__,
			    aw_ram_name_19161[index]);
		return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
			aw_ram_name_19161[index], aw_haptic->dev, GFP_KERNEL,
			aw_haptic, ram_load);
	} else if (aw_haptic->device_id == 81538) {
		if (aw_haptic->vibration_style == AW_HAPTIC_VIBRATION_CRISP_STYLE) {
			aw_dev_info("%s:150Hz haptic bin name  %s\n", __func__,
				    aw_ram_name_150[index]);
			return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				aw_ram_name_150[index], aw_haptic->dev, GFP_KERNEL,
				aw_haptic, ram_load);
		} else if (aw_haptic->vibration_style == AW_HAPTIC_VIBRATION_SOFT_STYLE) {
			aw_dev_info("%s:150Hz haptic bin name  %s\n", __func__,
				    aw_ram_name_150_soft[index]);
			return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				aw_ram_name_150_soft[index], aw_haptic->dev, GFP_KERNEL,
				aw_haptic, ram_load);
		} else {
			aw_dev_info("%s:150Hz haptic bin name  %s\n", __func__,
				    aw_ram_name_150[index]);
			return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				aw_ram_name_150[index], aw_haptic->dev, GFP_KERNEL,
				aw_haptic, ram_load);
		}
	} else {
		if (aw_haptic->vibration_style == AW_HAPTIC_VIBRATION_CRISP_STYLE) {
			aw_dev_info("%s:170Hz haptic bin name %s\n", __func__,
				    aw_ram_name[index]);
			return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				aw_ram_name[index], aw_haptic->dev, GFP_KERNEL,
				aw_haptic, ram_load);
		} else if (aw_haptic->vibration_style == AW_HAPTIC_VIBRATION_SOFT_STYLE) {
			aw_dev_info("%s:170Hz soft haptic bin name %s\n", __func__,
				    aw_ram_name_170_soft[index]);
			return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				aw_ram_name_170_soft[index], aw_haptic->dev, GFP_KERNEL,
				aw_haptic, ram_load);
		} else {
			aw_dev_info("%s:haptic bin name  %s\n", __func__,
					aw_ram_name[index]);
			return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				aw_ram_name[index], aw_haptic->dev, GFP_KERNEL,
				aw_haptic, ram_load);
		}
	}
	return 0;

}

#ifdef AWINIC_RAM_UPDATE_DELAY
static void ram_work_routine(struct work_struct *work)
{
	struct aw_haptic *aw_haptic = container_of(work, struct aw_haptic,
					     ram_work.work);

	aw_dev_info("%s: enter\n", __func__);
	ram_update(aw_haptic);
}
#endif

static void ram_work_init(struct aw_haptic *aw_haptic)
{
#ifdef AWINIC_RAM_UPDATE_DELAY
	int ram_timer_val = AW_RAM_WORK_DELAY_INTERVAL;

	aw_dev_info("%s: enter\n", __func__);
	INIT_DELAYED_WORK(&aw_haptic->ram_work, ram_work_routine);
	schedule_delayed_work(&aw_haptic->ram_work,
			      msecs_to_jiffies(ram_timer_val));
#else
	ram_update(aw_haptic);
#endif
}

static void ram_vbat_comp(struct aw_haptic *aw_haptic, bool flag)
{
	int temp_gain = 0;

	aw_dev_dbg("%s: enter\n", __func__);
	if (flag) {
		if (aw_haptic->ram_vbat_comp == AW_RAM_VBAT_COMP_ENABLE) {
			aw_haptic->func->get_vbat(aw_haptic);
			if (aw_haptic->vbat > AW_VBAT_REFER) {
				aw_dev_dbg("%s: not need to vbat compensate!\n",
					   __func__);
				return;
			}
			temp_gain = aw_haptic->gain * AW_VBAT_REFER /
				aw_haptic->vbat;
			if (temp_gain >
			    (128 * AW_VBAT_REFER / AW_VBAT_MIN)) {
				temp_gain = 128 * AW_VBAT_REFER / AW_VBAT_MIN;
				aw_dev_dbg("%s: gain limit=%d\n",
					   __func__, temp_gain);
			}
			aw_haptic->func->set_gain(aw_haptic, temp_gain);
		} else {
			aw_haptic->func->set_gain(aw_haptic, aw_haptic->gain);
		}
	} else {
		aw_haptic->func->set_gain(aw_haptic, aw_haptic->gain);
	}
}

static int f0_cali(struct aw_haptic *aw_haptic)
{
	char f0_cali_lra = 0;
	uint32_t f0_limit = 0;
	uint32_t f0_cali_min = aw_haptic->info.f0_pre *
				(100 - aw_haptic->info.f0_cali_percent) / 100;
	uint32_t f0_cali_max = aw_haptic->info.f0_pre *
				(100 + aw_haptic->info.f0_cali_percent) / 100;
	int ret = 0;
	int f0_cali_step = 0;

	aw_dev_info("%s: enter\n", __func__);
	aw_haptic->func->upload_lra(aw_haptic, AW_WRITE_ZERO);
	if (aw_haptic->func->get_f0(aw_haptic)) {
		aw_dev_err("%s: get f0 error, user defafult f0\n",
			   __func__);
#ifdef CONFIG_HAPTIC_FEEDBACK_MODULE
		(void)oplus_haptic_track_fre_cail(HAPTIC_F0_CALI_TRACK, aw_haptic->f0, 0, "aw_haptic->func->get_f0 is null");
#endif
	} else {
		/* max and min limit */
		f0_limit = aw_haptic->f0;
		aw_dev_info("%s: f0_pre = %d, f0_cali_min = %d, f0_cali_max = %d, f0 = %d\n",
			    __func__, aw_haptic->info.f0_pre,
			    f0_cali_min, f0_cali_max, aw_haptic->f0);

		if ((aw_haptic->f0 < f0_cali_min) ||
			aw_haptic->f0 > f0_cali_max) {
			aw_dev_err("%s: f0 calibration out of range = %d!\n",
				   __func__, aw_haptic->f0);
			f0_limit = aw_haptic->info.f0_pre;
#ifdef CONFIG_HAPTIC_FEEDBACK_MODULE
			(void)oplus_haptic_track_fre_cail(HAPTIC_F0_CALI_TRACK, aw_haptic->f0, -ERANGE, "f0 out of range");
#endif
			return -ERANGE;
		}
		aw_dev_info("%s: f0_limit = %d\n", __func__,
			    (int)f0_limit);
		/* calculate cali step */
		f0_cali_step = 100000 * ((int)f0_limit -
			       (int)aw_haptic->info.f0_pre) /
			       ((int)f0_limit * AW_OSC_CALI_ACCURACY);
		aw_dev_info("%s: f0_cali_step = %d\n", __func__,
			    f0_cali_step);
		if (f0_cali_step >= 0) {	/*f0_cali_step >= 0 */
			if (f0_cali_step % 10 >= 5)
				f0_cali_step = 32 + (f0_cali_step / 10 + 1);
			else
				f0_cali_step = 32 + f0_cali_step / 10;
		} else {	/* f0_cali_step < 0 */
			if (f0_cali_step % 10 <= -5)
				f0_cali_step = 32 + (f0_cali_step / 10 - 1);
			else
				f0_cali_step = 32 + f0_cali_step / 10;
		}
		if (f0_cali_step > 31)
			f0_cali_lra = (char)f0_cali_step - 32;
		else
			f0_cali_lra = (char)f0_cali_step + 32;
		/* update cali step */
		aw_haptic->f0_cali_data = (int)f0_cali_lra;

		aw_dev_info("%s: f0_cali_data = 0x%02X\n",
			    __func__, aw_haptic->f0_cali_data);
	}
	aw_haptic->func->upload_lra(aw_haptic, AW_F0_CALI_LRA);
	/* restore standby work mode */
	aw_haptic->func->play_stop(aw_haptic);
	return ret;
}

void get_f0_cali_data(struct aw_haptic *aw_haptic)
{
	char f0_cali_lra = 0;
	uint32_t f0_limit = 0;
	uint32_t f0_cali_min = aw_haptic->info.f0_pre *
				(100 - aw_haptic->info.f0_cali_percent) / 100;
	uint32_t f0_cali_max = aw_haptic->info.f0_pre *
				(100 + aw_haptic->info.f0_cali_percent) / 100;
	int f0_cali_step = 0;

/* max and min limit */
	f0_limit = aw_haptic->f0;
	aw_dev_info("%s: f0_pre = %d, f0_cali_min = %d, f0_cali_max = %d, f0 = %d\n",
			__func__, aw_haptic->info.f0_pre,
			f0_cali_min, f0_cali_max, aw_haptic->f0);

	if ((aw_haptic->f0 < f0_cali_min) ||
		aw_haptic->f0 > f0_cali_max) {
		aw_dev_err("%s: f0 calibration out of range = %d!\n",
				__func__, aw_haptic->f0);
		f0_limit = aw_haptic->info.f0_pre;
#ifdef CONFIG_HAPTIC_FEEDBACK_MODULE
		(void)oplus_haptic_track_fre_cail(HAPTIC_F0_CALI_TRACK, aw_haptic->f0, -ERANGE, "f0 out of range");
#endif
		return;
	}
	aw_dev_info("%s: f0_limit = %d\n", __func__,
			(int)f0_limit);
	/* calculate cali step */
	f0_cali_step = 100000 * ((int)f0_limit -
				(int)aw_haptic->info.f0_pre) /
				((int)f0_limit * AW_OSC_CALI_ACCURACY);
	aw_dev_info("%s: f0_cali_step = %d\n", __func__,
			f0_cali_step);
	if (f0_cali_step >= 0) {	/*f0_cali_step >= 0 */
		if (f0_cali_step % 10 >= 5)
			f0_cali_step = 32 + (f0_cali_step / 10 + 1);
		else
			f0_cali_step = 32 + f0_cali_step / 10;
	} else {	/* f0_cali_step < 0 */
		if (f0_cali_step % 10 <= -5)
			f0_cali_step = 32 + (f0_cali_step / 10 - 1);
		else
			f0_cali_step = 32 + f0_cali_step / 10;
	}
	if (f0_cali_step > 31)
		f0_cali_lra = (char)f0_cali_step - 32;
	else
		f0_cali_lra = (char)f0_cali_step + 32;
	/* update cali step */
	aw_haptic->f0_cali_data = (int)f0_cali_lra;

	aw_dev_info("%s: f0_cali_data = 0x%02X\n",
			__func__, aw_haptic->f0_cali_data);
}

static void rtp_trim_lra_cali(struct aw_haptic *aw_haptic)
{
	uint32_t lra_trim_code = 0;
	/*0.1 percent below no need to calibrate */
	uint32_t osc_cali_threshold = 10;
	uint32_t real_code = 0;
	uint32_t theory_time = 0;
	uint32_t real_time = aw_haptic->microsecond;

	aw_dev_info("%s: enter\n", __func__);

	theory_time = aw_haptic->func->get_theory_time(aw_haptic);
	if (theory_time == real_time) {
		aw_dev_info("%s: theory_time == real_time: %d, no need to calibrate!\n",
			    __func__, real_time);
		return;
	} else if (theory_time < real_time) {
		if ((real_time - theory_time) >
			(theory_time / AW_OSC_TRIM_PARAM)) {
			aw_dev_info("%s: (real_time - theory_time) > (theory_time/50), can't calibrate!\n",
				    __func__);
			return;
		}

		if ((real_time - theory_time) <
		    (osc_cali_threshold * theory_time / 10000)) {
			aw_dev_info("%s: real_time: %d, theory_time: %d, no need to calibrate!\n",
				    __func__, real_time, theory_time);
			return;
		}

		real_code = 100000 * ((real_time - theory_time)) /
			    (theory_time * AW_OSC_CALI_ACCURACY);
		real_code = ((real_code % 10 < 5) ? 0 : 1) + real_code / 10;
		real_code = 32 + real_code;
	} else if (theory_time > real_time) {
		if ((theory_time - real_time) >
			(theory_time / AW_OSC_TRIM_PARAM)) {
			aw_dev_info("%s: (theory_time - real_time) > (theory_time / 50), can't calibrate!\n",
				    __func__);
			return;
		}
		if ((theory_time - real_time) <
		    (osc_cali_threshold * theory_time / 10000)) {
			aw_dev_info("%s: real_time: %d, theory_time: %d, no need to calibrate!\n",
				    __func__, real_time, theory_time);
			return;
		}

		real_code = (theory_time - real_time) / (theory_time / 100000) / AW_OSC_CALI_ACCURACY;
		real_code = ((real_code % 10 < 5) ? 0 : 1) + real_code / 10;
		real_code = 32 - real_code;
	}
	if (real_code > 31)
		lra_trim_code = real_code - 32;
	else
		lra_trim_code = real_code + 32;
	aw_dev_info("%s: real_time: %d, theory_time: %d\n",
		    __func__, real_time, theory_time);
	aw_dev_info("%s: real_code: %d, trim_lra: 0x%02X\n",
		    __func__, real_code, lra_trim_code);
	if (lra_trim_code >= 0) {
		aw_haptic->osc_cali_data = lra_trim_code;
		aw_haptic->func->upload_lra(aw_haptic, AW_OSC_CALI_LRA);
	}
}

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_FAULT_INJECT_AW8692X)
noinline
#endif
static int rtp_osc_cali(struct aw_haptic *aw_haptic)
{
	uint32_t buf_len = 0;
	int ret = -1;
	const struct firmware *rtp_file;

	aw_haptic->rtp_cnt = 0;
	aw_haptic->timeval_flags = 1;

	aw_dev_info("%s: enter\n", __func__);
	/* fw loaded */
	ret = request_firmware(&rtp_file, aw_rtp_name[0], aw_haptic->dev);
	if (ret < 0) {
		aw_dev_err("%s: failed to read %s\n", __func__,
			   aw_rtp_name[0]);
#ifdef CONFIG_HAPTIC_FEEDBACK_MODULE
		(void)oplus_haptic_track_fre_cail(HAPTIC_OSC_CALI_TRACK, aw_haptic->f0, ret, "rtp_osc_cali request_firmware fail");
#endif
		return ret;
	}
	/*aw_haptic add stop,for irq interrupt during calibrate */
	aw_haptic->func->play_stop(aw_haptic);
	aw_haptic->rtp_init = false;
	mutex_lock(&aw_haptic->rtp_lock);
#ifndef OPLUS_FEATURE_CHG_BASIC
	kfree(aw_rtp);
	aw_rtp = kzalloc(rtp_file->size+sizeof(int), GFP_KERNEL);
	if (!aw_rtp) {
		release_firmware(rtp_file);
		mutex_unlock(&aw_haptic->rtp_lock);
		aw_dev_err("%s: error allocating memory\n", __func__);
		return -ENOMEM;
	}
#else
	ret = container_init(rtp_file->size+sizeof(int));
	if (ret < 0) {
		release_firmware(rtp_file);
		mutex_unlock(&aw_haptic->rtp_lock);
		aw_dev_err("%s: error allocating memory\n", __func__);
		return -ENOMEM;
	}
#endif
	aw_rtp->len = rtp_file->size;
	aw_haptic->rtp_len = rtp_file->size;
	aw_dev_info("%s: rtp file:[%s] size = %dbytes\n",
		    __func__, aw_rtp_name[0], aw_rtp->len);
	memcpy(aw_rtp->data, rtp_file->data, rtp_file->size);
	release_firmware(rtp_file);
	mutex_unlock(&aw_haptic->rtp_lock);
	/* gain */
	ram_vbat_comp(aw_haptic, false);
	/* rtp mode config */
	aw_haptic->func->play_mode(aw_haptic, AW_RTP_MODE);
	/* bst mode */
	aw_haptic->func->bst_mode_config(aw_haptic, AW_BST_BYPASS_MODE);
	disable_irq(gpio_to_irq(aw_haptic->irq_gpio));
	/* haptic go */
	aw_haptic->func->play_go(aw_haptic, true);
	while (1) {
		if (!aw_haptic->func->rtp_get_fifo_afs(aw_haptic)) {
#ifdef AW_ENABLE_RTP_PRINT_LOG
			aw_dev_info("%s: not almost_full, aw_haptic->rtp_cnt=%d\n",
				 __func__, aw_haptic->rtp_cnt);
#endif
			mutex_lock(&aw_haptic->rtp_lock);
			cpu_latency_qos_add_request(&pm_qos_req, CPU_LATENCY_QOC_VALUE);
			if (aw_haptic->rtp_cnt < aw_haptic->ram.base_addr) {
				if (aw_rtp->len - aw_haptic->rtp_cnt < aw_haptic->ram.base_addr)
					buf_len = aw_rtp->len - aw_haptic->rtp_cnt;
				else
					buf_len = aw_haptic->ram.base_addr;
			} else if ((aw_rtp->len - aw_haptic->rtp_cnt) <
			    (aw_haptic->ram.base_addr >> 2))
				buf_len = aw_rtp->len - aw_haptic->rtp_cnt;
			else
				buf_len = (aw_haptic->ram.base_addr >> 2);

			if (aw_haptic->rtp_cnt != aw_rtp->len) {
				if (aw_haptic->timeval_flags == 1) {
					aw_haptic->kstart = ktime_get();
					aw_haptic->timeval_flags = 0;
				}
				aw_haptic->func->set_rtp_data(
						aw_haptic, &aw_rtp->data
						[aw_haptic->rtp_cnt], buf_len);
				aw_haptic->rtp_cnt += buf_len;
			}
			cpu_latency_qos_remove_request(&pm_qos_req);
			mutex_unlock(&aw_haptic->rtp_lock);
		}
		if (aw_haptic->func->get_osc_status(aw_haptic)) {
			aw_haptic->kend = ktime_get();
			aw_dev_info("%s: osc trim playback done aw_haptic->rtp_cnt= %d\n",
				    __func__, aw_haptic->rtp_cnt);
			break;
		}
		aw_haptic->kend = ktime_get();
		aw_haptic->microsecond = ktime_to_us(ktime_sub(aw_haptic->kend,
							    aw_haptic->kstart));
		if (aw_haptic->microsecond > AW_OSC_CALI_MAX_LENGTH) {
			aw_dev_info("%s osc trim time out! aw_haptic->rtp_cnt %d\n",
				    __func__, aw_haptic->rtp_cnt);
			break;
		}
	}
	enable_irq(gpio_to_irq(aw_haptic->irq_gpio));
	aw_haptic->microsecond = ktime_to_us(ktime_sub(aw_haptic->kend,
						       aw_haptic->kstart));
	/*calibration osc */
	aw_dev_info("%s: aw_haptic_microsecond: %ld\n",
		    __func__, aw_haptic->microsecond);
	aw_dev_info("%s: exit\n", __func__);
	return 0;
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	struct aw_haptic *aw_haptic = container_of(timer, struct aw_haptic,
						   timer);

	aw_dev_info("%s: enter\n", __func__);
	aw_haptic->state = 0;
	/* schedule_work(&aw_haptic->vibrator_work); */
	queue_work(system_highpri_wq, &aw_haptic->vibrator_work);
	return HRTIMER_NORESTART;
}

static void vibrator_work_routine(struct work_struct *work)
{
	struct aw_haptic *aw_haptic = container_of(work, struct aw_haptic,
						   vibrator_work);

	aw_dev_dbg("%s: enter!\n", __func__);

#ifdef OPLUS_FEATURE_CHG_BASIC
	aw_haptic->activate_mode = AW_RAM_LOOP_MODE;
	aw_dev_info("%s enter, aw_haptic->state[%d], aw_haptic->activate_mode[%d], aw_haptic->ram_vbat_comp[%d]\n",
		    __func__, aw_haptic->state, aw_haptic->activate_mode,
		    aw_haptic->ram_vbat_comp);
#endif

	mutex_lock(&aw_haptic->lock);
	/* Enter standby mode */
	aw_haptic->func->play_stop(aw_haptic);
	if (aw_haptic->state) {
		aw_haptic->func->upload_lra(aw_haptic, AW_F0_CALI_LRA);
		if (aw_haptic->activate_mode == AW_RAM_LOOP_MODE) {
			if (aw_haptic->device_id == 832
				|| aw_haptic->device_id == 833
				|| aw_haptic->device_id == 815) {
				ram_vbat_comp(aw_haptic, false);
				aw_haptic->func->bst_mode_config(aw_haptic, AW_BST_BOOST_MODE);
			} else {
				ram_vbat_comp(aw_haptic, true);
				aw_haptic->func->bst_mode_config(aw_haptic, AW_BST_BYPASS_MODE);
			}
			ram_play(aw_haptic, AW_RAM_LOOP_MODE);
			/* run ms timer */
			hrtimer_start(&aw_haptic->timer,
				      ktime_set(aw_haptic->duration / 1000,
						(aw_haptic->duration % 1000) *
						1000000), HRTIMER_MODE_REL);
		} else if (aw_haptic->activate_mode == AW_CONT_MODE) {
			aw_haptic->func->cont_config(aw_haptic);
			/* run ms timer */
			hrtimer_start(&aw_haptic->timer,
				      ktime_set(aw_haptic->duration / 1000,
						(aw_haptic->duration % 1000) *
						1000000), HRTIMER_MODE_REL);
		} else {
			aw_dev_err("%s: activate_mode error\n",
				   __func__);
		}
	}
	mutex_unlock(&aw_haptic->lock);
}

static void rtp_play(struct aw_haptic *aw_haptic)
{
	uint8_t glb_state_val = 0;
	uint32_t buf_len = 0;

	aw_dev_info("%s: enter\n", __func__);
	aw_haptic->rtp_cnt = 0;
	mutex_lock(&aw_haptic->rtp_lock);
	cpu_latency_qos_add_request(&pm_qos_req, CPU_LATENCY_QOC_VALUE);
	aw_haptic->func->dump_rtp_regs(aw_haptic);
	while ((!aw_haptic->func->rtp_get_fifo_afs(aw_haptic))
	       && (aw_haptic->play_mode == AW_RTP_MODE)) {
#ifdef AW_ENABLE_RTP_PRINT_LOG
		aw_dev_info("%s: rtp cnt = %d\n", __func__,
			    aw_haptic->rtp_cnt);
#endif
		if (!aw_rtp) {
			aw_dev_info("%s:aw_rtp is null, break!\n", __func__);
			break;
		}
		if (aw_haptic->rtp_cnt < (aw_haptic->ram.base_addr)) {
			if ((aw_rtp->len - aw_haptic->rtp_cnt) <
			    (aw_haptic->ram.base_addr)) {
				buf_len = aw_rtp->len - aw_haptic->rtp_cnt;
			} else {
				buf_len = aw_haptic->ram.base_addr;
			}
		} else if ((aw_rtp->len - aw_haptic->rtp_cnt) <
			   (aw_haptic->ram.base_addr >> 2)) {
			buf_len = aw_rtp->len - aw_haptic->rtp_cnt;
		} else {
			buf_len = aw_haptic->ram.base_addr >> 2;
		}
#ifdef AW_ENABLE_RTP_PRINT_LOG
		aw_dev_info("%s: buf_len = %d\n", __func__,
			    buf_len);
#endif
		aw_haptic->func->set_rtp_data(aw_haptic,
					      &aw_rtp->data[aw_haptic->rtp_cnt],
					      buf_len);
		aw_haptic->rtp_cnt += buf_len;
		glb_state_val = aw_haptic->func->get_glb_state(aw_haptic);
		if ((aw_haptic->rtp_cnt >= aw_rtp->len)
		    || ((glb_state_val & AW_GLBRD_STATE_MASK) ==
							AW_STATE_STANDBY)) {
			if (aw_haptic->rtp_cnt != aw_rtp->len)
				aw_dev_err("%s: rtp play suspend!\n", __func__);
			else
				aw_dev_info("%s: rtp update complete!\n",
					    __func__);
			aw_haptic->rtp_cnt = 0;
			aw_haptic->func->dump_rtp_regs(aw_haptic);
			break;
		}
	}

	if (aw_haptic->play_mode == AW_RTP_MODE)
		aw_haptic->func->set_rtp_aei(aw_haptic, true);
	cpu_latency_qos_remove_request(&pm_qos_req);
	aw_dev_info("%s: exit\n", __func__);
	mutex_unlock(&aw_haptic->rtp_lock);
}

static const struct firmware *old_work_file_load_accord_f0(struct aw_haptic *aw_haptic)
{
	const struct firmware *rtp_file;
	unsigned int f0_file_num = 1024;
	int ret = -1;

	if (aw_haptic->rtp_file_num == AW_WAVEFORM_INDEX_OLD_STEADY ||
	    aw_haptic->rtp_file_num == AW_WAVEFORM_INDEX_HIGH_TEMP) {
		if (aw_haptic->device_id == 815) {
			 if (aw_haptic->f0 <= 1610)
				f0_file_num = 0;
			else if (aw_haptic->f0 <= 1630)
				f0_file_num = 1;
			else if (aw_haptic->f0 <= 1650)
				f0_file_num = 2;
			else if (aw_haptic->f0 <= 1670)
				f0_file_num = 3;
			else if (aw_haptic->f0 <= 1690)
				f0_file_num = 4;
			else if (aw_haptic->f0 <= 1710)
				f0_file_num = 5;
			else if (aw_haptic->f0 <= 1730)
				f0_file_num = 6;
			else if (aw_haptic->f0 <= 1750)
				f0_file_num = 7;
			else if (aw_haptic->f0 <= 1770)
				f0_file_num = 8;
			else if (aw_haptic->f0 <= 1790)
				f0_file_num = 9;
			else
				f0_file_num = 10;
		} else if (aw_haptic->device_id == 81538) {
			if (aw_haptic->f0 <= 1410)
				f0_file_num = 0;
			else if (aw_haptic->f0 <= 1430)
				f0_file_num = 1;
			else if (aw_haptic->f0 <= 1450)
				f0_file_num = 2;
			else if (aw_haptic->f0 <= 1470)
				f0_file_num = 3;
			else if (aw_haptic->f0 <= 1490)
				f0_file_num = 4;
			else if (aw_haptic->f0 <= 1510)
				f0_file_num = 5;
			else if (aw_haptic->f0 <= 1530)
				f0_file_num = 6;
			else if (aw_haptic->f0 <= 1550)
				f0_file_num = 7;
			else if (aw_haptic->f0 <= 1570)
				f0_file_num = 8;
			else if (aw_haptic->f0 <= 1590)
				f0_file_num = 9;
			else
				f0_file_num = 10;
		} else if (aw_haptic->device_id == 832 || aw_haptic->device_id == 833) {
			if (aw_haptic->f0 <= 2255)
				f0_file_num = 0;
			else if (aw_haptic->f0 <= 2265)
				f0_file_num = 1;
			else if (aw_haptic->f0 <= 2275)
				f0_file_num = 2;
			else if (aw_haptic->f0 <= 2285)
				f0_file_num = 3;
			else if (aw_haptic->f0 <= 2295)
				f0_file_num = 4;
			else if (aw_haptic->f0 <= 2305)
				f0_file_num = 5;
			else if (aw_haptic->f0 <= 2315)
				f0_file_num = 6;
			else if (aw_haptic->f0 <= 2325)
				f0_file_num = 7;
			else if (aw_haptic->f0 <= 2335)
				f0_file_num = 8;
			else if (aw_haptic->f0 <= 2345)
				f0_file_num = 9;
			else
				f0_file_num = 10;
		}
		if (aw_haptic->rtp_file_num == AW_WAVEFORM_INDEX_OLD_STEADY) {
			if (aw_haptic->device_id == 815) {
				ret = request_firmware(&rtp_file,
					aw_old_steady_test_rtp_name_0815[f0_file_num],
					aw_haptic->dev);
			} else if (aw_haptic->device_id == 81538) {
				ret = request_firmware(&rtp_file,
					aw_old_steady_test_rtp_name_081538[f0_file_num],
					aw_haptic->dev);
			} else if (aw_haptic->device_id == 832 || aw_haptic->device_id == 833) {
				ret = request_firmware(&rtp_file,
					aw_old_steady_test_rtp_name_0832[f0_file_num],
					aw_haptic->dev);
			}
	} else {
		if (aw_haptic->device_id == 815) {
				ret = request_firmware(&rtp_file,
					aw_high_temp_high_humidity_0815[f0_file_num],
					aw_haptic->dev);
		} else if (aw_haptic->device_id == 81538) {
				ret = request_firmware(&rtp_file,
					aw_high_temp_high_humidity_081538[f0_file_num],
					aw_haptic->dev);
		} else if (aw_haptic->device_id == 832 || aw_haptic->device_id == 833) {
				ret = request_firmware(&rtp_file,
					aw_high_temp_high_humidity_0832[f0_file_num],
					aw_haptic->dev);
		}
	}
	if (ret < 0) {
		aw_dev_err("%s: failed to read id[%d],index[%d]\n",
			   __func__, aw_haptic->device_id, f0_file_num);
		aw_haptic->rtp_routine_on = 0;
		return NULL;
	}
	return rtp_file;
	}
	return NULL;
}

static const struct firmware *rtp_load_file_accord_f0(struct aw_haptic *aw_haptic)
{
	const struct firmware *rtp_file;
	unsigned int f0_file_num = 1024;
	int ret = -1;

	if (aw_haptic->rtp_file_num == AW_WAVEFORM_INDEX_OLD_STEADY ||
	    aw_haptic->rtp_file_num == AW_WAVEFORM_INDEX_HIGH_TEMP) {
		return old_work_file_load_accord_f0(aw_haptic);
	}

	return NULL;

	if ((aw_haptic->rtp_file_num >=  RINGTONES_START_INDEX && aw_haptic->rtp_file_num <= RINGTONES_END_INDEX)
		|| (aw_haptic->rtp_file_num >=  NEW_RING_START && aw_haptic->rtp_file_num <= NEW_RING_END)
		|| (aw_haptic->rtp_file_num >=  OS12_NEW_RING_START && aw_haptic->rtp_file_num <= OS12_NEW_RING_END)
		|| aw_haptic->rtp_file_num == RINGTONES_SIMPLE_INDEX
		|| aw_haptic->rtp_file_num == RINGTONES_PURE_INDEX) {
		if (aw_haptic->f0 <= 1670) {
			f0_file_num = aw_haptic->rtp_file_num;
			aw_dev_info("%s  ringtone f0_file_num[%d]\n", __func__, f0_file_num);
			ret = request_firmware(&rtp_file,
					aw_ringtone_rtp_f0_170_name[f0_file_num],
					aw_haptic->dev);
			if (ret < 0) {
				aw_dev_err("%s: failed to read %s\n", __func__,
						aw_ringtone_rtp_f0_170_name[f0_file_num]);
				aw_haptic->rtp_routine_on = 0;
				return NULL;
			}
			return rtp_file;
		}
		return NULL;
	} else if (aw_haptic->rtp_file_num == AW_RTP_LONG_SOUND_INDEX ||
		   aw_haptic->rtp_file_num == AW_WAVEFORM_INDEX_OLD_STEADY) {
		if (aw_haptic->f0 <= 1650)
			f0_file_num = 0;
		else if (aw_haptic->f0 <= 1670)
			f0_file_num = 1;
		else if (aw_haptic->f0 <= 1700)
			f0_file_num = 2;
		else
			f0_file_num = 3;
		aw_dev_info("%s long sound or old steady test f0_file_num[%d], aw_haptic->rtp_file_num[%d]\n", __func__, f0_file_num, aw_haptic->rtp_file_num);

		if (aw_haptic->rtp_file_num == AW_RTP_LONG_SOUND_INDEX) {
			ret = request_firmware(&rtp_file,
					aw_long_sound_rtp_name[f0_file_num],
					aw_haptic->dev);
		} else if (aw_haptic->rtp_file_num == AW_WAVEFORM_INDEX_OLD_STEADY) {
			ret = request_firmware(&rtp_file,
					aw_old_steady_test_rtp_name_0815[f0_file_num],
					aw_haptic->dev);
		}
		if (ret < 0) {
			aw_dev_err("%s: failed to read %s\n", __func__,
					aw_long_sound_rtp_name[f0_file_num]);
			aw_haptic->rtp_routine_on = 0;
			return NULL;
		}
		return rtp_file;
	}
	return NULL;
}

static void op_clean_status(struct aw_haptic *aw_haptic)
{
	aw_haptic->audio_ready = false;
	aw_haptic->haptic_ready = false;
	aw_haptic->pre_haptic_number = 0;
	aw_haptic->rtp_routine_on = 0;

	aw_dev_info("%s enter\n", __func__);
}

static void rtp_work_routine(struct work_struct *work)
{
	bool rtp_work_flag = false;
	uint8_t reg_val = 0;
	int cnt = 200;
	int ret = -1;
	const struct firmware *rtp_file;
	struct aw_haptic *aw_haptic = container_of(work, struct aw_haptic,
						   rtp_work);

	aw_dev_info("%s: enter device_id = %d, f0 = %d\n", __func__, aw_haptic->device_id, aw_haptic->f0);
	mutex_lock(&aw_haptic->rtp_lock);
	aw_haptic->rtp_routine_on = 1;
	/* fw loaded */

	rtp_file = rtp_load_file_accord_f0(aw_haptic);
	if (!rtp_file) {
		aw_dev_info("%s: rtp_file_num[%d]\n", __func__,
			    aw_haptic->rtp_file_num);
		aw_haptic->rtp_routine_on = 1;
#ifdef KERNEL_VERSION_510
		if (aw_haptic->device_id == DEVICE_ID_0815) {
			if (aw_haptic->f0 <= DEVICE_ID_0815_F0_1630) {
				ret = request_firmware(&rtp_file,
				aw_rtp_name_162Hz[aw_haptic->rtp_file_num],
				aw_haptic->dev);
			} else if (aw_haptic->f0 <= DEVICE_ID_0815_F0_1670) {
				ret = request_firmware(&rtp_file,
				aw_rtp_name_166Hz[aw_haptic->rtp_file_num],
				aw_haptic->dev);
			} else if (aw_haptic->f0 <= DEVICE_ID_0815_F0_1710) {
				ret = request_firmware(&rtp_file,
				aw_rtp_name[aw_haptic->rtp_file_num],
				aw_haptic->dev);
			} else if (aw_haptic->f0 <= DEVICE_ID_0815_F0_1750) {
				ret = request_firmware(&rtp_file,
				aw_rtp_name_174Hz[aw_haptic->rtp_file_num],
				aw_haptic->dev);
			} else {
				ret = request_firmware(&rtp_file,
				aw_rtp_name_178Hz[aw_haptic->rtp_file_num],
				aw_haptic->dev);
			}
#else
		if (aw_haptic->device_id == 815) {
			if (aw_haptic->f0 <= 1670) {
				ret = request_firmware(&rtp_file,
				aw_rtp_name_165Hz[aw_haptic->rtp_file_num],
				aw_haptic->dev);
			} else if (aw_haptic->f0 <= 1725) {
				ret = request_firmware(&rtp_file,
				aw_rtp_name[aw_haptic->rtp_file_num],
				aw_haptic->dev);
			} else {
				ret = request_firmware(&rtp_file,
				aw_rtp_name_175Hz[aw_haptic->rtp_file_num],
				aw_haptic->dev);
			}
#endif
		} else if (aw_haptic->device_id == 81538) {
			ret = request_firmware(&rtp_file,
			aw_rtp_name_150Hz[aw_haptic->rtp_file_num],
			aw_haptic->dev);
		} else if (aw_haptic->device_id == 832) {
			if (aw_haptic->f0 <= 2280) {
			    ret = request_firmware(&rtp_file,
			    aw_rtp_name_19065_226Hz[aw_haptic->rtp_file_num],
			    aw_haptic->dev);
			} else if (aw_haptic->f0 <= 2320) {
			    ret = request_firmware(&rtp_file,
			    aw_rtp_name_19065_230Hz[aw_haptic->rtp_file_num],
			    aw_haptic->dev);
			} else {
			    ret = request_firmware(&rtp_file,
			    aw_rtp_name_19065_234Hz[aw_haptic->rtp_file_num],
			    aw_haptic->dev);
			}
		} else {
			if (aw_haptic->f0 <= 2280) {
				ret = request_firmware(&rtp_file,
				aw_rtp_name_19065_226Hz[aw_haptic->rtp_file_num],
				aw_haptic->dev);
			} else if (aw_haptic->f0 <= 2320) {
				ret = request_firmware(&rtp_file,
				aw_rtp_name_19065_230Hz[aw_haptic->rtp_file_num],
				aw_haptic->dev);
			} else if (aw_haptic->f0 <= 2350) {
				ret = request_firmware(&rtp_file,
				aw_rtp_name_19065_234Hz[aw_haptic->rtp_file_num],
				aw_haptic->dev);
			} else {
				ret = request_firmware(&rtp_file,
				aw_rtp_name_19065_237Hz[aw_haptic->rtp_file_num],
				aw_haptic->dev);
			}
		}
		if (ret < 0) {
			aw_dev_err("%s: failed to read %s, aw_haptic->f0=%d\n",
				   __func__,
				   aw_rtp_name[aw_haptic->rtp_file_num],
				   aw_haptic->f0);
			aw_haptic->rtp_routine_on = 0;
			mutex_unlock(&aw_haptic->rtp_lock);
			return;
		}
	}
	aw_haptic->rtp_init = false;
#ifndef OPLUS_FEATURE_CHG_BASIC
	vfree(aw_rtp);
	aw_rtp = vmalloc(rtp_file->size + sizeof(int));
	if (!aw_rtp) {
		release_firmware(rtp_file);
		aw_dev_err("%s: error allocating memory\n",
			   __func__);
		aw_haptic->rtp_routine_on = 0;
		mutex_unlock(&aw_haptic->rtp_lock);
		return;
	}
#else
	ret = container_init(rtp_file->size + sizeof(int));
	if (ret < 0) {
		release_firmware(rtp_file);
		mutex_unlock(&aw_haptic->rtp_lock);
		aw_dev_err("%s: error allocating memory\n", __func__);

		op_clean_status(aw_haptic);
		aw_haptic->rtp_routine_on = 0;
		return;
	}
#endif
	aw_rtp->len = rtp_file->size;
	aw_dev_info("%s: rtp file:[%s] size = %dbytes\n",
		    __func__, aw_rtp_name[aw_haptic->rtp_file_num],
		    aw_rtp->len);
	memcpy(aw_rtp->data, rtp_file->data, rtp_file->size);
	mutex_unlock(&aw_haptic->rtp_lock);
	release_firmware(rtp_file);
	if (aw_haptic->device_id == 815) {
		aw_dev_info("%s: rtp file [%s] size = %d, f0 = %d\n", __func__,
		aw_rtp_name[aw_haptic->rtp_file_num], aw_rtp->len, aw_haptic->f0);
	} else if (aw_haptic->device_id == 81538) {
		aw_dev_info("%s: rtp file [%s] size = %d, f0 = %d\n", __func__,
		aw_rtp_name_150Hz[aw_haptic->rtp_file_num], aw_rtp->len, aw_haptic->f0);
	} else {
		aw_dev_info("%s: rtp file [%s] size = %d, f0 = %d\n", __func__,
		aw_rtp_name_19065_230Hz[aw_haptic->rtp_file_num], aw_rtp->len, aw_haptic->f0);
	}
	mutex_lock(&aw_haptic->lock);
	aw_haptic->rtp_init = true;

	aw_haptic->func->upload_lra(aw_haptic, AW_OSC_CALI_LRA);
	aw_haptic->func->set_rtp_aei(aw_haptic, false);
	aw_haptic->func->irq_clear(aw_haptic);
	aw_haptic->func->play_stop(aw_haptic);

	if (aw_haptic->rtp_file_num == AW_WAVEFORM_INDEX_ZERO) {
		aw_haptic->rtp_init = false;
		op_clean_status(aw_haptic);
		aw_dev_info("%s: vibrator stopped \n", __func__);
		mutex_unlock(&aw_haptic->lock);
		return;
	}
	/* gain */
	ram_vbat_comp(aw_haptic, false);
	/* boost voltage */
	/*
	if (aw_haptic->info.bst_vol_rtp <= aw_haptic->info.max_bst_vol &&
		aw_haptic->info.bst_vol_rtp > 0)
		aw_haptic->func->set_bst_vol(aw_haptic,
					   aw_haptic->info.bst_vol_rtp);
	else
		aw_haptic->func->set_bst_vol(aw_haptic, aw_haptic->vmax);
	*/
	/* rtp mode config */
	aw_haptic->func->play_mode(aw_haptic, AW_RTP_MODE);
	/* haptic go */
	aw_haptic->func->play_go(aw_haptic, true);
	usleep_range(2000, 2500);
	while (cnt) {
		reg_val = aw_haptic->func->get_glb_state(aw_haptic);
		if ((reg_val & AW_GLBRD_STATE_MASK) == AW_STATE_RTP) {
			cnt = 0;
			rtp_work_flag = true;
			aw_dev_info("%s: RTP_GO! glb_state=0x08\n", __func__);
		} else {
			cnt--;
			aw_dev_dbg("%s: wait for RTP_GO, glb_state=0x%02X\n",
				   __func__, reg_val);
		}
		usleep_range(2000, 2500);
	}
	if (rtp_work_flag) {
		rtp_play(aw_haptic);
	} else {
		/* enter standby mode */
		aw_haptic->func->play_stop(aw_haptic);
		aw_dev_err("%s: failed to enter RTP_GO status!\n", __func__);
	}
	op_clean_status(aw_haptic);
	mutex_unlock(&aw_haptic->lock);
}

static irqreturn_t irq_handle(int irq, void *data)
{
	uint8_t glb_state_val = 0;
	uint32_t buf_len = 0;
	struct aw_haptic *aw_haptic = data;

	aw_dev_dbg("%s: enter\n", __func__);
	if (!aw_haptic->func->get_irq_state(aw_haptic)) {
		aw_dev_dbg("%s: aw_haptic rtp fifo almost empty\n", __func__);
		if (aw_haptic->rtp_init) {
			while ((!aw_haptic->func->rtp_get_fifo_afs(aw_haptic))
			       && (aw_haptic->play_mode == AW_RTP_MODE)) {
				mutex_lock(&aw_haptic->rtp_lock);
				cpu_latency_qos_add_request(&pm_qos_req, CPU_LATENCY_QOC_VALUE);
				if (!aw_haptic->rtp_cnt) {
					aw_dev_info("%s:aw_haptic->rtp_cnt is 0!\n",
						    __func__);
					cpu_latency_qos_remove_request(&pm_qos_req);
					mutex_unlock(&aw_haptic->rtp_lock);
					break;
				}
#ifdef AW_ENABLE_RTP_PRINT_LOG
				aw_dev_info("%s:rtp mode fifo update, cnt=%d\n",
					    __func__, aw_haptic->rtp_cnt);
#endif
				if (!aw_rtp) {
					aw_dev_info("%s:aw_rtp is null, break!\n",
						    __func__);
					cpu_latency_qos_remove_request(&pm_qos_req);
					mutex_unlock(&aw_haptic->rtp_lock);
					break;
				}
				if ((aw_rtp->len - aw_haptic->rtp_cnt) <
				    (aw_haptic->ram.base_addr >> 2)) {
					buf_len =
					    aw_rtp->len - aw_haptic->rtp_cnt;
				} else {
					buf_len = (aw_haptic->ram.base_addr >>
						   2);
				}
				aw_haptic->func->set_rtp_data(aw_haptic,
						     &aw_rtp->data
						     [aw_haptic->rtp_cnt],
						     buf_len);
				aw_haptic->rtp_cnt += buf_len;
				glb_state_val =
				      aw_haptic->func->get_glb_state(aw_haptic);
				if ((aw_haptic->rtp_cnt >= aw_rtp->len)
				    || ((glb_state_val & AW_GLBRD_STATE_MASK) ==
							AW_STATE_STANDBY)) {
					if (aw_haptic->rtp_cnt !=
					    aw_rtp->len)
						aw_dev_err("%s: rtp play suspend!\n",
							   __func__);
					else
						aw_dev_info("%s: rtp update complete!\n",
							    __func__);
					op_clean_status(aw_haptic);
					aw_haptic->func->set_rtp_aei(aw_haptic,
								     false);
					aw_haptic->rtp_cnt = 0;
					aw_haptic->rtp_init = false;
					cpu_latency_qos_remove_request(&pm_qos_req);
					mutex_unlock(&aw_haptic->rtp_lock);
					break;
				}
				cpu_latency_qos_remove_request(&pm_qos_req);
				mutex_unlock(&aw_haptic->rtp_lock);
			}
		} else {
			aw_dev_info("%s: init error\n",
				    __func__);
		}
	}
	if (aw_haptic->play_mode != AW_RTP_MODE)
		aw_haptic->func->set_rtp_aei(aw_haptic, false);
	aw_dev_dbg("%s: exit\n", __func__);
	return IRQ_HANDLED;
}

static int irq_config(struct device *dev, struct aw_haptic *aw_haptic)
{
	int ret = -1;
	int irq_flags = 0;

	if (gpio_is_valid(aw_haptic->irq_gpio) &&
	    !(aw_haptic->flags & AW_FLAG_SKIP_INTERRUPTS)) {
		/* register irq handler */
		aw_haptic->func->interrupt_setup(aw_haptic);
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		ret = devm_request_threaded_irq(dev,
					       gpio_to_irq(aw_haptic->irq_gpio),
					       NULL, irq_handle, irq_flags,
					       "aw_haptic", aw_haptic);
		if (ret != 0) {
			aw_dev_err("%s: failed to request IRQ %d: %d\n",
				   __func__, gpio_to_irq(aw_haptic->irq_gpio),
				   ret);
			return ret;
		}
	} else {
		dev_info(dev, "%s: skipping IRQ registration\n", __func__);
		/* disable feature support if gpio was invalid */
		aw_haptic->flags |= AW_FLAG_SKIP_INTERRUPTS;
	}
	return 0;
}

/*****************************************************
 *
 * haptic audio
 *
 *****************************************************/
static int audio_ctrl_list_ins(struct aw_haptic_audio *haptic_audio,
			       struct aw_haptic_ctr *haptic_ctr)
{
	struct aw_haptic_ctr *p_new = NULL;

	p_new = (struct aw_haptic_ctr *)kzalloc(
		sizeof(struct aw_haptic_ctr), GFP_KERNEL);
	if (p_new == NULL) {
		aw_dev_err("%s: kzalloc memory fail\n", __func__);
		return -ENOMEM;
	}
	/* update new list info */
	p_new->cnt = haptic_ctr->cnt;
	p_new->cmd = haptic_ctr->cmd;
	p_new->play = haptic_ctr->play;
	p_new->wavseq = haptic_ctr->wavseq;
	p_new->loop = haptic_ctr->loop;
	p_new->gain = haptic_ctr->gain;
	INIT_LIST_HEAD(&(p_new->list));
	list_add(&(p_new->list), &(haptic_audio->ctr_list));
	return 0;
}

static void audio_ctrl_list_clr(struct aw_haptic_audio *haptic_audio)
{
	struct aw_haptic_ctr *p_ctr = NULL;
	struct aw_haptic_ctr *p_ctr_bak = NULL;

	list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak,
					 &(haptic_audio->ctr_list), list) {
		list_del(&p_ctr->list);
		kfree(p_ctr);
	}
}

static void audio_init(struct aw_haptic *aw_haptic)
{
	aw_dev_dbg("%s: enter!\n", __func__);
	aw_haptic->func->set_wav_seq(aw_haptic, 0x01, 0x00);
}

static void audio_off(struct aw_haptic *aw_haptic)
{
	aw_dev_info("%s: enter\n", __func__);
	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->set_gain(aw_haptic, 0x80);
	aw_haptic->func->play_stop(aw_haptic);
	audio_ctrl_list_clr(&aw_haptic->haptic_audio);
	aw_haptic->gun_type = 0xff;
	aw_haptic->bullet_nr = 0;
	aw_haptic->gun_mode = 0;
	mutex_unlock(&aw_haptic->lock);
}

static enum hrtimer_restart audio_timer_func(struct hrtimer *timer)
{
	struct aw_haptic *aw_haptic =
	    container_of(timer, struct aw_haptic, haptic_audio.timer);

	aw_dev_dbg("%s: enter\n", __func__);
	schedule_work(&aw_haptic->haptic_audio.work);

	hrtimer_start(&aw_haptic->haptic_audio.timer,
		      ktime_set(aw_haptic->haptic_audio.timer_val / 1000000,
				(aw_haptic->haptic_audio.timer_val % 1000000) *
				1000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static void audio_work_routine(struct work_struct *work)
{
	struct aw_haptic *aw_haptic =
	    container_of(work, struct aw_haptic, haptic_audio.work);
	struct aw_haptic_audio *haptic_audio = NULL;
	struct aw_haptic_ctr *p_ctr = NULL;
	struct aw_haptic_ctr *p_ctr_bak = NULL;
	uint32_t ctr_list_flag = 0;
	uint32_t ctr_list_input_cnt = 0;
	uint32_t ctr_list_output_cnt = 0;
	uint32_t ctr_list_diff_cnt = 0;
	uint32_t ctr_list_del_cnt = 0;
	int rtp_is_going_on = 0;

	aw_dev_dbg("%s: enter\n", __func__);
	haptic_audio = &(aw_haptic->haptic_audio);
	mutex_lock(&aw_haptic->haptic_audio.lock);
	memset(&aw_haptic->haptic_audio.ctr, 0,
	       sizeof(struct aw_haptic_ctr));
	ctr_list_flag = 0;
	list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak,
					 &(haptic_audio->ctr_list), list) {
		ctr_list_flag = 1;
		break;
	}
	if (ctr_list_flag == 0)
		aw_dev_info("%s: ctr list empty\n", __func__);
	if (ctr_list_flag == 1) {
		list_for_each_entry_safe(p_ctr, p_ctr_bak,
					 &(haptic_audio->ctr_list), list) {
			ctr_list_input_cnt = p_ctr->cnt;
			break;
		}
		list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak,
						 &(haptic_audio->ctr_list),
						 list) {
			ctr_list_output_cnt = p_ctr->cnt;
			break;
		}
		if (ctr_list_input_cnt > ctr_list_output_cnt) {
			ctr_list_diff_cnt =
			    ctr_list_input_cnt - ctr_list_output_cnt;
		}
		if (ctr_list_input_cnt < ctr_list_output_cnt) {
			ctr_list_diff_cnt =
			    32 + ctr_list_input_cnt - ctr_list_output_cnt;
		}
		if (ctr_list_diff_cnt > 2) {
			list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak,
							 &(haptic_audio->
							   ctr_list), list) {
				if ((p_ctr->play == 0)
				    && (AW_CMD_ENABLE ==
					(AW_CMD_HAPTIC & p_ctr->
					 cmd))) {
					list_del(&p_ctr->list);
					kfree(p_ctr);
					ctr_list_del_cnt++;
				}
				if (ctr_list_del_cnt == ctr_list_diff_cnt)
					break;
			}
		}
	}
	/* get the last data from list */
	list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak,
					 &(haptic_audio->ctr_list), list) {
		aw_haptic->haptic_audio.ctr.cnt = p_ctr->cnt;
		aw_haptic->haptic_audio.ctr.cmd = p_ctr->cmd;
		aw_haptic->haptic_audio.ctr.play = p_ctr->play;
		aw_haptic->haptic_audio.ctr.wavseq = p_ctr->wavseq;
		aw_haptic->haptic_audio.ctr.loop = p_ctr->loop;
		aw_haptic->haptic_audio.ctr.gain = p_ctr->gain;
		list_del(&p_ctr->list);
		kfree(p_ctr);
		break;
	}
	if (aw_haptic->haptic_audio.ctr.play) {
		aw_dev_info("%s: cnt=%d, cmd=%d, play=%d, wavseq=%d, loop=%d, gain=%d\n",
			    __func__, aw_haptic->haptic_audio.ctr.cnt,
			    aw_haptic->haptic_audio.ctr.cmd,
			    aw_haptic->haptic_audio.ctr.play,
			    aw_haptic->haptic_audio.ctr.wavseq,
			    aw_haptic->haptic_audio.ctr.loop,
			    aw_haptic->haptic_audio.ctr.gain);
	}
	rtp_is_going_on = aw_haptic->func->juge_rtp_going(aw_haptic);
	if (rtp_is_going_on) {
		mutex_unlock(&aw_haptic->haptic_audio.lock);
		return;
	}
	mutex_unlock(&aw_haptic->haptic_audio.lock);
	if (aw_haptic->haptic_audio.ctr.cmd == AW_CMD_ENABLE) {
		if (aw_haptic->haptic_audio.ctr.play ==
		    AW_PLAY_ENABLE) {
			aw_dev_info("%s: haptic_audio_play_start\n", __func__);
			mutex_lock(&aw_haptic->lock);
			aw_haptic->func->play_stop(aw_haptic);
			aw_haptic->func->play_mode(aw_haptic, AW_RAM_MODE);
			aw_haptic->func->set_wav_seq(aw_haptic, 0x00,
					    aw_haptic->haptic_audio.ctr.wavseq);
			aw_haptic->func->set_wav_seq(aw_haptic, 0x01, 0x00);
			aw_haptic->func->set_wav_loop(aw_haptic, 0x00,
					      aw_haptic->haptic_audio.ctr.loop);
			aw_haptic->func->set_gain(aw_haptic,
					  aw_haptic->haptic_audio.ctr.gain);
			aw_haptic->func->play_go(aw_haptic, true);
			mutex_unlock(&aw_haptic->lock);
		} else if (AW_PLAY_STOP ==
			   aw_haptic->haptic_audio.ctr.play) {
			mutex_lock(&aw_haptic->lock);
			aw_haptic->func->play_stop(aw_haptic);
			mutex_unlock(&aw_haptic->lock);
		} else if (AW_PLAY_GAIN ==
			   aw_haptic->haptic_audio.ctr.play) {
			mutex_lock(&aw_haptic->lock);
			aw_haptic->func->set_gain(aw_haptic,
					  aw_haptic->haptic_audio.ctr.gain);
			mutex_unlock(&aw_haptic->lock);
		}
	}
}

/*****************************************************
 *
 * node
 *
 *****************************************************/
#ifdef TIMED_OUTPUT
static int vibrator_get_time(struct timed_output_dev *dev)
{
	struct aw_haptic *aw_haptic = container_of(dev, struct aw_haptic,
						   vib_dev);

	if (hrtimer_active(&aw_haptic->timer)) {
		ktime_t r = hrtimer_get_remaining(&aw_haptic->timer);

		return ktime_to_ms(r);
	}
	return 0;
}

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
	struct aw_haptic *aw_haptic = container_of(dev, struct aw_haptic,
						   vib_dev);

	aw_dev_info("%s: enter\n", __func__);
	if (!aw_haptic->ram_init) {
		aw_dev_err("%s: ram init failed, not allow to play!\n",
			   __func__);
		return;
	}
	mutex_lock(&aw_haptic->lock);

	aw_haptic->func->upload_lra(aw_haptic, AW_F0_CALI_LRA);
	aw_haptic->func->play_stop(aw_haptic);
	if (value > 0) {
		ram_vbat_comp(aw_haptic, false);
		ram_play(aw_haptic, AW_RAM_MODE);
	}
	mutex_unlock(&aw_haptic->lock);
	aw_dev_info("%s: exit\n", __func__);
}
#else
static enum led_brightness brightness_get(struct led_classdev *cdev)
{
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	return aw_haptic->amplitude;
}

static void brightness_set(struct led_classdev *cdev, enum led_brightness level)
{
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	aw_dev_info("%s: enter\n", __func__);
	if (!aw_haptic->ram_init) {
		aw_dev_err("%s: ram init failed, not allow to play!\n",
			   __func__);
		return;
	}
	aw_haptic->amplitude = level;
	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->play_stop(aw_haptic);
	if (aw_haptic->amplitude > 0) {
		aw_haptic->func->upload_lra(aw_haptic, AW_F0_CALI_LRA);
		ram_vbat_comp(aw_haptic, false);
		ram_play(aw_haptic, AW_RAM_MODE);
	}
	mutex_unlock(&aw_haptic->lock);
}
#endif

static ssize_t state_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", aw_haptic->state);
}

static ssize_t state_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	return count;
}

static ssize_t duration_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	ktime_t time_rem;
	s64 time_ms = 0;

	if (hrtimer_active(&aw_haptic->timer)) {
		time_rem = hrtimer_get_remaining(&aw_haptic->timer);
		time_ms = ktime_to_ms(time_rem);
	}
	return snprintf(buf, PAGE_SIZE, "%lldms\n", time_ms);
}

static ssize_t duration_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
#ifdef OPLUS_FEATURE_CHG_BASIC
	aw_dev_info("%s: value=%d\n", __func__, val);
#endif

	/* setting 0 on duration is NOP for now */
	if (val <= 0)
		return count;
	aw_haptic->duration = val;
	return count;
}

static ssize_t activate_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", aw_haptic->state);
}

static ssize_t activate_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	uint32_t val = 0;
	int rc = 0;
	int rtp_is_going_on = 0;

	rtp_is_going_on = aw_haptic->func->juge_rtp_going(aw_haptic);
	if (rtp_is_going_on) {
		aw_dev_info("%s: rtp is going\n", __func__);
		return count;
	}
	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_dev_info("%s: value=%d\n", __func__, val);
	if (!aw_haptic->ram_init) {
		aw_dev_err("%s: ram init failed, not allow to play!\n",
			   __func__);
		return count;
	}
	mutex_lock(&aw_haptic->lock);
	hrtimer_cancel(&aw_haptic->timer);
	aw_haptic->state = val;
	mutex_unlock(&aw_haptic->lock);
#ifdef OPLUS_FEATURE_CHG_BASIC
	if (aw_haptic->state) {
		aw_dev_info("%s: gain=0x%02x\n", __func__, aw_haptic->gain);
		if (aw_haptic->gain >= AW_HAPTIC_RAM_VBAT_COMP_GAIN)
			aw_haptic->gain = AW_HAPTIC_RAM_VBAT_COMP_GAIN;

		mutex_lock(&aw_haptic->lock);

		if (aw_haptic->device_id == 815 || aw_haptic->device_id == 81538)
			aw_haptic->func->set_gain(aw_haptic, aw_haptic->gain);
		aw_haptic->func->set_repeat_seq(aw_haptic,
						AW_WAVEFORM_INDEX_SINE_CYCLE);
		mutex_unlock(&aw_haptic->lock);
		cancel_work_sync(&aw_haptic->vibrator_work);
		queue_work(system_highpri_wq, &aw_haptic->vibrator_work);
	} else {
		mutex_lock(&aw_haptic->lock);
		aw_haptic->func->play_stop(aw_haptic);
		mutex_unlock(&aw_haptic->lock);
	}
#endif
	return count;
}

static ssize_t activate_mode_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	return snprintf(buf, PAGE_SIZE, "activate_mode = %d\n",
			aw_haptic->activate_mode);
}

static ssize_t activate_mode_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	mutex_lock(&aw_haptic->lock);
	aw_haptic->activate_mode = val;
	mutex_unlock(&aw_haptic->lock);
	return count;
}

static ssize_t index_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	uint8_t seq = 0;
	ssize_t count = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->get_wav_seq(aw_haptic, &seq, 1);
	mutex_unlock(&aw_haptic->lock);
	aw_haptic->index = seq;
	count += snprintf(buf, PAGE_SIZE, "%d\n", aw_haptic->index);
	return count;
}

static ssize_t index_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val > aw_haptic->ram.ram_num) {
		aw_dev_err("%s: input value out of range!\n", __func__);
		return count;
	}
	aw_dev_info("%s: value=%d\n", __func__, val);
	mutex_lock(&aw_haptic->lock);
	aw_haptic->index = val;
	aw_haptic->func->set_repeat_seq(aw_haptic, aw_haptic->index);
	mutex_unlock(&aw_haptic->lock);
	return count;
}

static ssize_t vmax_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw_haptic->vmax);
}

static int convert_level_to_vmax(struct aw_haptic *aw_haptic, int val)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(vmax_map); i++) {
		if (val == vmax_map[i].level) {
			aw_haptic->vmax = vmax_map[i].vmax;
			aw_haptic->gain = vmax_map[i].gain;
			break;
		}
	}
	if (i == ARRAY_SIZE(vmax_map)) {
		aw_haptic->vmax = vmax_map[i - 1].vmax;
		aw_haptic->gain = vmax_map[i - 1].gain;
	}

	if (aw_haptic->vmax > AW86927_HAPTIC_HIGH_LEVEL_REG_VAL)
		aw_haptic->vmax = AW86927_HAPTIC_HIGH_LEVEL_REG_VAL;

	return i;
}

static ssize_t vmax_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_dev_info("%s: value=%d\n", __func__, val);
	mutex_lock(&aw_haptic->lock);
#ifdef OPLUS_FEATURE_CHG_BASIC
	if (val <= 255) {
		aw_haptic->gain = (val * AW_HAPTIC_RAM_VBAT_COMP_GAIN) / 255;
	} else if (val <= 2400) {
		convert_level_to_vmax(aw_haptic, val);
	} else {
		aw_haptic->vmax = AW86927_HAPTIC_HIGH_LEVEL_REG_VAL;
		aw_haptic->gain = 0x80;
	}

	if (val == 2550) {  /* for old test only */
		aw_haptic->gain = AW_HAPTIC_RAM_VBAT_COMP_GAIN;
	}

	if (aw_haptic->device_id == 833) {
		aw_haptic->vmax = AW86927_HAPTIC_HIGH_LEVEL_REG_VAL;
		aw_haptic->gain = 0x80;
	}

	aw_haptic->func->set_gain(aw_haptic, aw_haptic->gain);
	aw_haptic->func->set_bst_vol(aw_haptic, aw_haptic->vmax);
#else
	aw_haptic->vmax = val;
	aw_haptic->func->set_bst_vol(aw_haptic, aw_haptic->vmax);
#endif
	mutex_unlock(&aw_haptic->lock);
	aw_dev_info("%s: gain[0x%x], vmax[0x%x] end\n", __func__,
		    aw_haptic->gain, aw_haptic->vmax);

	return count;
}

static ssize_t gain_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	return snprintf(buf, PAGE_SIZE, "0x%02X\n", aw_haptic->gain);
}

static ssize_t gain_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_dev_info("%s: value=0x%02x\n", __func__, val);
	mutex_lock(&aw_haptic->lock);
	aw_haptic->gain = val;
	aw_haptic->func->set_gain(aw_haptic, aw_haptic->gain);
	mutex_unlock(&aw_haptic->lock);
	return count;
}

static ssize_t seq_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	size_t count = 0;
	int i = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->get_wav_seq(aw_haptic, aw_haptic->seq,
				     AW_SEQUENCER_SIZE);
	mutex_unlock(&aw_haptic->lock);
	for (i = 0; i < AW_SEQUENCER_SIZE; i++) {
		count += snprintf(buf + count, PAGE_SIZE - count,
				  "seq%d = %d\n", i + 1, aw_haptic->seq[i]);
	}
	return count;
}

static ssize_t seq_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	uint32_t databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		if (databuf[0] >= AW_SEQUENCER_SIZE ||
		    databuf[1] > aw_haptic->ram.ram_num) {
			aw_dev_err("%s: input value out of range!\n", __func__);
			return count;
		}
		aw_dev_info("%s: seq%d=0x%02X\n", __func__,
			    databuf[0], databuf[1]);
		mutex_lock(&aw_haptic->lock);
		aw_haptic->seq[databuf[0]] = (uint8_t)databuf[1];
		aw_haptic->func->set_wav_seq(aw_haptic, (uint8_t)databuf[0],
					     aw_haptic->seq[databuf[0]]);
		mutex_unlock(&aw_haptic->lock);
	}
	return count;
}

static ssize_t loop_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	size_t count = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	mutex_lock(&aw_haptic->lock);
	count = aw_haptic->func->get_wav_loop(aw_haptic, buf);
	mutex_unlock(&aw_haptic->lock);
	count += snprintf(buf+count, PAGE_SIZE-count,
 			  "rtp_loop: 0x%02x\n", aw_haptic->rtp_loop);
	return count;
}

static ssize_t loop_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	uint32_t databuf[2] = { 0, 0 };
	uint32_t val = 0;
	int rc = 0;

	aw_haptic->rtp_loop = 0;

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw_dev_info("%s: seq%d loop=0x%02X\n", __func__,
			    databuf[0], databuf[1]);
		mutex_lock(&aw_haptic->lock);
		aw_haptic->loop[databuf[0]] = (uint8_t)databuf[1];
		aw_haptic->func->set_wav_loop(aw_haptic, (uint8_t)databuf[0],
					      aw_haptic->loop[databuf[0]]);
		mutex_unlock(&aw_haptic->lock);
	} else {
		rc = kstrtouint(buf, 0, &val);
		if (rc < 0)
			return count;
		aw_haptic->rtp_loop = val;
		aw_dev_info("%s: rtp_loop = 0x%02X", __func__,
			    aw_haptic->rtp_loop);
	}

	return count;
}

static ssize_t reg_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	ssize_t len = 0;

	mutex_lock(&aw_haptic->lock);
	len = aw_haptic->func->get_reg(aw_haptic, len, buf);
	mutex_unlock(&aw_haptic->lock);
	return len;
}

static ssize_t reg_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	uint8_t val = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	uint32_t databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		val = (uint8_t)databuf[1];
		if (aw_haptic->func == &aw8692x_func_list &&
		    (uint8_t)databuf[0] == AW8692X_REG_ANACFG20)
			val &= AW8692X_BIT_ANACFG20_TRIM_LRA;
		mutex_lock(&aw_haptic->lock);
		i2c_w_bytes(aw_haptic, (uint8_t)databuf[0], &val,
			    AW_I2C_BYTE_ONE);
		mutex_unlock(&aw_haptic->lock);
	}
	return count;
}

static ssize_t rtp_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "rtp_cnt = %d\n",
			aw_haptic->rtp_cnt);
	return len;
}

static ssize_t rtp_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	uint32_t val = 0;
	int rc = 0;
	int rtp_is_going_on = 0;
	static bool mute = false;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0) {
		aw_dev_err("%s: kstrtouint fail\n", __func__);
		return rc;
	}
	aw_dev_info("%s: val [%d] \n", __func__, val);

	if (val == 1025) {
		mute = true;
		return count;
	} else if (val == 1026) {
		mute = false;
		return count;
	}

	mutex_lock(&aw_haptic->lock);
	/*OP add for juge rtp on begin*/
	rtp_is_going_on = aw_haptic->func->juge_rtp_going(aw_haptic);
	if (rtp_is_going_on && (val == AUDIO_READY_STATUS)) {
		aw_dev_info("%s: seem audio status rtp[%d]\n", __func__, val);
		mutex_unlock(&aw_haptic->lock);
		return count;
	}
	/*OP add for juge rtp on end*/
	if (((val >=  RINGTONES_START_INDEX && val <= RINGTONES_END_INDEX)
		|| (val >=  NEW_RING_START && val <= NEW_RING_END)
		|| (val >=  OS12_NEW_RING_START && val <= OS12_NEW_RING_END)
		|| (val >=  OPLUS_RING_START && val <= OPLUS_RING_END)
		|| (val >=  OPLUS_NEW_RING_1_START && val <= OPLUS_NEW_RING_1_END)
		|| (val >=  OPLUS_NEW_RING_2_START && val <= OPLUS_NEW_RING_2_END)
		|| (val >=  OS14_NEW_RING_START && val <= OS14_NEW_RING_END)
		|| val == RINGTONES_SIMPLE_INDEX
		|| val == RINGTONES_PURE_INDEX
		|| val == AUDIO_READY_STATUS)) {
		if (val == AUDIO_READY_STATUS)
			aw_haptic->audio_ready = true;
		else
			aw_haptic->haptic_ready = true;

		aw_dev_info("%s:audio[%d]and haptic[%d] ready\n", __func__,
			    aw_haptic->audio_ready, aw_haptic->haptic_ready);

		if (aw_haptic->haptic_ready && !aw_haptic->audio_ready)
			aw_haptic->pre_haptic_number = val;

		if (!aw_haptic->audio_ready || !aw_haptic->haptic_ready) {
			mutex_unlock(&aw_haptic->lock);
			return count;
		}
	}
	if (val == AUDIO_READY_STATUS && aw_haptic->pre_haptic_number) {
		aw_dev_info("pre_haptic_number:%d\n",
			    aw_haptic->pre_haptic_number);
		val = aw_haptic->pre_haptic_number;
	}
	if (!val)
		op_clean_status(aw_haptic);

	aw_haptic->func->play_stop(aw_haptic);
	aw_haptic->func->set_rtp_aei(aw_haptic, false);
	aw_haptic->func->irq_clear(aw_haptic);
	mutex_unlock(&aw_haptic->lock);
	if (val < (sizeof(aw_rtp_name)/AW_RTP_NAME_MAX)) {
		aw_haptic->rtp_file_num = val;
		if (val) {
			if (aw_haptic->device_id == 815 &&
			    ((val >= 302 && val <= 305) || (val >= 110 && val <= 112))) {
				queue_work(system_unbound_wq, &aw_haptic->rtp_key_work);
			} else {
				queue_work(system_unbound_wq, &aw_haptic->rtp_work);
			}
		}

	} else {
		aw_dev_err("%s: rtp_file_num 0x%02x over max value \n",
			   __func__, aw_haptic->rtp_file_num);
	}
	return count;
}

static ssize_t ram_update_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	int i = 0;
	int j = 0;
	int size = 0;
	ssize_t len = 0;
	uint8_t ram_data[AW_RAMDATA_RD_BUFFER_SIZE] = {0};
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	mutex_lock(&aw_haptic->lock);
	/* RAMINIT Enable */
	aw_haptic->func->ram_init(aw_haptic, true);
	aw_haptic->func->play_stop(aw_haptic);
	aw_haptic->func->set_ram_addr(aw_haptic, aw_haptic->ram.base_addr);
	len += snprintf(buf + len, PAGE_SIZE - len, "aw_haptic_ram:\n");
	while (i < aw_haptic->ram.len) {
		if ((aw_haptic->ram.len - i) < AW_RAMDATA_RD_BUFFER_SIZE)
			size = aw_haptic->ram.len - i;
		else
			size = AW_RAMDATA_RD_BUFFER_SIZE;

		aw_haptic->func->get_ram_data(aw_haptic, ram_data, size);
		for (j = 0; j < size; j++) {
			len += snprintf(buf + len, PAGE_SIZE - len,
					"0x%02X,", ram_data[j]);
		}
		i += size;
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	/* RAMINIT Disable */
	aw_haptic->func->ram_init(aw_haptic, false);
	mutex_unlock(&aw_haptic->lock);
	return len;
}

static ssize_t ram_update_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val)
		ram_update(aw_haptic);
	return count;
}

static ssize_t ram_num_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	ssize_t len = 0;

	get_ram_num(aw_haptic);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"ram_num = %d\n", aw_haptic->ram.ram_num);
	return len;
}

static ssize_t f0_show(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	ssize_t len = 0;

	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->upload_lra(aw_haptic, AW_WRITE_ZERO);
	aw_haptic->func->get_f0(aw_haptic);
	mutex_unlock(&aw_haptic->lock);
	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", aw_haptic->f0);
	return len;
}

static ssize_t f0_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw_dev_info("%s: f0 = %d\n", __func__, val);

	if(aw_haptic->device_id == 815) {
		aw_haptic->f0 = val;
		if (aw_haptic->f0 < F0_VAL_MIN_0815 ||
		    aw_haptic->f0 > F0_VAL_MAX_0815)
			aw_haptic->f0 = 1700;

	} else if(aw_haptic->device_id == 81538) {
		aw_haptic->f0 = val;
		if (aw_haptic->f0 < F0_VAL_MIN_081538 ||
		    aw_haptic->f0 > F0_VAL_MAX_081538)
			aw_haptic->f0 = 1500;

	} else if(aw_haptic->device_id == 832) {
		aw_haptic->f0 = val;
		if (aw_haptic->f0 < F0_VAL_MIN_0832 ||
		    aw_haptic->f0 > F0_VAL_MAX_0832)
			aw_haptic->f0 = 2300;

	} else if(aw_haptic->device_id == 833) {
		aw_haptic->f0 = val;
		if (aw_haptic->f0 < F0_VAL_MIN_0833 ||
		    aw_haptic->f0 > F0_VAL_MAX_0833)
			aw_haptic->f0 = 2330;

	}
	ram_update(aw_haptic);

	return count;
}

static ssize_t cali_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	ssize_t len = 0;

	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->upload_lra(aw_haptic, AW_F0_CALI_LRA);
	aw_haptic->func->get_f0(aw_haptic);
	mutex_unlock(&aw_haptic->lock);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"%d\n", aw_haptic->f0);
	return len;
}

static ssize_t cali_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val) {
		mutex_lock(&aw_haptic->lock);
		f0_cali(aw_haptic);
		mutex_unlock(&aw_haptic->lock);
	}
	return count;
}

static ssize_t cont_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	ssize_t len = 0;

	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->read_f0(aw_haptic);
	mutex_unlock(&aw_haptic->lock);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"cont_f0 = %d\n", aw_haptic->cont_f0);
	return len;
}

static ssize_t cont_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->play_stop(aw_haptic);
	if (val) {
		aw_haptic->func->upload_lra(aw_haptic, AW_F0_CALI_LRA);
		aw_haptic->func->cont_config(aw_haptic);
	}
	mutex_unlock(&aw_haptic->lock);
	return count;
}

static ssize_t vbat_monitor_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	ssize_t len = 0;

	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->get_vbat(aw_haptic);
	len += snprintf(buf + len, PAGE_SIZE - len, "vbat_monitor = %d\n",
			aw_haptic->vbat);
	mutex_unlock(&aw_haptic->lock);

	return len;
}

static ssize_t lra_resistance_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	ssize_t len = 0;

	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->get_lra_resistance(aw_haptic);
	mutex_unlock(&aw_haptic->lock);
	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n",
			aw_haptic->lra);
	return len;
}

static ssize_t auto_boost_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "auto_boost = %d\n",
			aw_haptic->auto_boost);

	return len;
}

static ssize_t auto_boost_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw_haptic->lock);
	aw_haptic->func->play_stop(aw_haptic);
	aw_haptic->func->auto_bst_enable(aw_haptic, val);
	aw_haptic->auto_boost = val;
	mutex_unlock(&aw_haptic->lock);

	return count;
}

static ssize_t prct_mode_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	ssize_t len = 0;
	uint8_t reg_val = 0;

	mutex_lock(&aw_haptic->lock);
	reg_val = aw_haptic->func->get_prctmode(aw_haptic);
	mutex_unlock(&aw_haptic->lock);
	len += snprintf(buf + len, PAGE_SIZE - len, "prctmode = %d\n", reg_val);
	return len;
}

static ssize_t prct_mode_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	uint32_t databuf[2] = { 0, 0 };
	uint32_t prtime = 0;
	uint32_t prlvl = 0;

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		prtime = databuf[0];
		prlvl = databuf[1];
		mutex_lock(&aw_haptic->lock);
		aw_haptic->func->protect_config(aw_haptic, prtime, prlvl);
		mutex_unlock(&aw_haptic->lock);
	}
	return count;
}

static ssize_t ram_vbat_comp_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"ram_vbat_comp = %d\n",
			aw_haptic->ram_vbat_comp);

	return len;
}

static ssize_t ram_vbat_comp_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	mutex_lock(&aw_haptic->lock);
	if (val)
		aw_haptic->ram_vbat_comp = AW_RAM_VBAT_COMP_ENABLE;
	else
		aw_haptic->ram_vbat_comp = AW_RAM_VBAT_COMP_DISABLE;
	mutex_unlock(&aw_haptic->lock);

	return count;
}

static ssize_t osc_cali_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	ssize_t len = 0;

	aw_dev_info("microsecond:%ld \n", aw_haptic->microsecond);
	len += snprintf(buf+len, PAGE_SIZE-len, "%ld\n",
			aw_haptic->microsecond);
	return len;
}

static ssize_t osc_cali_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	mutex_lock(&aw_haptic->lock);
	if (val == 3) {
		aw_haptic->func->upload_lra(aw_haptic, AW_WRITE_ZERO);
		rtp_osc_cali(aw_haptic);
		rtp_trim_lra_cali(aw_haptic);
	} else if (val == 1) {
		aw_haptic->func->upload_lra(aw_haptic, AW_OSC_CALI_LRA);
		rtp_osc_cali(aw_haptic);
	}
	mutex_unlock(&aw_haptic->lock);

	return count;
}

static ssize_t haptic_audio_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len, "%d\n",
			aw_haptic->haptic_audio.ctr.cnt);
	return len;
}

static ssize_t haptic_audio_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	uint32_t databuf[6] = { 0 };
	struct aw_haptic_ctr *hap_ctr = NULL;

	if (!aw_haptic->ram_init) {
		aw_dev_err("%s: ram init failed, not allow to play!\n",
			   __func__);
		return count;
	}
	if (sscanf(buf, "%d %d %d %d %d %d", &databuf[0], &databuf[1],
		   &databuf[2], &databuf[3], &databuf[4], &databuf[5]) == 6) {
		if (databuf[2]) {
			aw_dev_info("%s: cnt=%d, cmd=%d, play=%d, wavseq=%d, loop=%d, gain=%d\n",
				    __func__, databuf[0], databuf[1],
				    databuf[2], databuf[3], databuf[4],
				    databuf[5]);
		}

		hap_ctr = (struct aw_haptic_ctr *)kzalloc(
			sizeof(struct aw_haptic_ctr), GFP_KERNEL);
		if (hap_ctr == NULL) {
			aw_dev_err("%s: kzalloc memory fail\n", __func__);
			return count;
		}
		mutex_lock(&aw_haptic->haptic_audio.lock);
		hap_ctr->cnt = (uint8_t)databuf[0];
		hap_ctr->cmd = (uint8_t)databuf[1];
		hap_ctr->play = (uint8_t)databuf[2];
		hap_ctr->wavseq = (uint8_t)databuf[3];
		hap_ctr->loop = (uint8_t)databuf[4];
		hap_ctr->gain = (uint8_t)databuf[5];
		audio_ctrl_list_ins(&aw_haptic->haptic_audio, hap_ctr);
		if (hap_ctr->cmd == 0xff) {
			aw_dev_info("%s: haptic_audio stop\n",
				    __func__);
			if (hrtimer_active(&aw_haptic->haptic_audio.timer)) {
				aw_dev_info("%s: cancel haptic_audio_timer\n",
					    __func__);
				hrtimer_cancel(&aw_haptic->haptic_audio.timer);
				aw_haptic->haptic_audio.ctr.cnt = 0;
				audio_off(aw_haptic);
			}
		} else {
			if (hrtimer_active(&aw_haptic->haptic_audio.timer)) {
				/* */
			} else {
				aw_dev_info("%s: start haptic_audio_timer\n",
					    __func__);
				audio_init(aw_haptic);
				hrtimer_start(&aw_haptic->haptic_audio.timer,
					      ktime_set(aw_haptic->haptic_audio.
							delay_val / 1000000,
							(aw_haptic->haptic_audio.
							 delay_val % 1000000) *
							1000),
					      HRTIMER_MODE_REL);
			}
		}
		mutex_unlock(&aw_haptic->haptic_audio.lock);
		kfree(hap_ctr);
	}
	return count;
}

static ssize_t haptic_audio_time_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"haptic_audio.delay_val=%dus\n",
			aw_haptic->haptic_audio.delay_val);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"haptic_audio.timer_val=%dus\n",
			aw_haptic->haptic_audio.timer_val);
	return len;
}

static ssize_t haptic_audio_time_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	uint32_t databuf[2] = { 0 };

	if (sscanf(buf, "%d %d", &databuf[0], &databuf[1]) == 2) {
		aw_haptic->haptic_audio.delay_val = databuf[0];
		aw_haptic->haptic_audio.timer_val = databuf[1];
	}
	return count;
}

static ssize_t gun_type_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw_haptic->gun_type);
}

static ssize_t gun_type_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw_dev_dbg("%s: value=%d\n", __func__, val);

	mutex_lock(&aw_haptic->lock);
	aw_haptic->gun_type = val;
	mutex_unlock(&aw_haptic->lock);
	return count;
}

static ssize_t bullet_nr_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw_haptic->bullet_nr);
}

static ssize_t bullet_nr_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	uint32_t val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_dev_dbg("%s: value=%d\n", __func__, val);
	mutex_lock(&aw_haptic->lock);
	aw_haptic->bullet_nr = val;
	mutex_unlock(&aw_haptic->lock);
	return count;
}

static ssize_t awrw_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	int i = 0;
	ssize_t len = 0;

	if (aw_haptic->i2c_info.flag != AW_SEQ_READ) {
		aw_dev_err("%s: no read mode\n", __func__);
		return -ERANGE;
	}
	if (aw_haptic->i2c_info.reg_data == NULL) {
		aw_dev_err("%s: awrw lack param\n", __func__);
		return -ERANGE;
	}
	for (i = 0; i < aw_haptic->i2c_info.reg_num; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"0x%02x,", aw_haptic->i2c_info.reg_data[i]);
	}
	len += snprintf(buf + len - 1, PAGE_SIZE - len, "\n");
	return len;
}

static ssize_t awrw_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	uint8_t value = 0;
	char data_buf[5] = { 0 };
	uint32_t flag = 0;
	uint32_t reg_num = 0;
	uint32_t reg_addr = 0;
	int i = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	if (sscanf(buf, "%x %x %x", &flag, &reg_num, &reg_addr) == 3) {
		if (!reg_num) {
			aw_dev_err("%s: param error\n",
				   __func__);
			return -ERANGE;
		}
		aw_haptic->i2c_info.flag = flag;
		aw_haptic->i2c_info.reg_num = reg_num;
		if (aw_haptic->i2c_info.reg_data != NULL)
			kfree(aw_haptic->i2c_info.reg_data);
		aw_haptic->i2c_info.reg_data = kmalloc(reg_num, GFP_KERNEL);
		if (flag == AW_SEQ_WRITE) {
			if ((reg_num * 5) != (strlen(buf) - 3 * 5)) {
				aw_dev_err("%s: param error\n",
					   __func__);
				return -ERANGE;
			}
			for (i = 0; i < reg_num; i++) {
				memcpy(data_buf, &buf[15 + i * 5], 4);
				data_buf[4] = '\0';
				rc = kstrtou8(data_buf, 0, &value);
				if (rc < 0) {
					aw_dev_err("%s: param error", __func__);
					return -ERANGE;
				}
				aw_haptic->i2c_info.reg_data[i] = value;
			}
			if (aw_haptic->func == &aw8692x_func_list &&
				 reg_addr == AW8692X_REG_ANACFG20)
					aw_haptic->i2c_info.reg_data[0] &=
						AW8692X_BIT_ANACFG20_TRIM_LRA;
			else if (aw_haptic->func == &aw8692x_func_list &&
				 reg_addr < AW8692X_REG_ANACFG20 &&
				 (reg_addr + reg_num) > AW8692X_REG_ANACFG20)
				    aw_haptic->i2c_info.reg_data[reg_num - 1] &=
						AW8692X_BIT_ANACFG20_TRIM_LRA;
			mutex_lock(&aw_haptic->lock);
			i2c_w_bytes(aw_haptic, (uint8_t)reg_addr,
				    aw_haptic->i2c_info.reg_data, reg_num);
			mutex_unlock(&aw_haptic->lock);
		} else if (flag == AW_SEQ_READ) {
			mutex_lock(&aw_haptic->lock);
			i2c_r_bytes(aw_haptic, reg_addr,
				    aw_haptic->i2c_info.reg_data, reg_num);
			mutex_unlock(&aw_haptic->lock);
		}
	} else {
		aw_dev_err("%s: param error\n", __func__);
	}
	return count;
}

static ssize_t f0_data_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n",
			aw_haptic->f0_cali_data);

	return len;
}

static ssize_t f0_data_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	uint32_t val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	mutex_lock(&aw_haptic->lock);
	aw_haptic->f0_cali_data = val;
	aw_dev_info("%s: f0_cali_data = %d\n", __func__, aw_haptic->f0_cali_data);
	if (aw_haptic->f0_cali_data == 0) {
		get_f0_cali_data(aw_haptic);
	}
	aw_haptic->func->upload_lra(aw_haptic, AW_F0_CALI_LRA);
	mutex_unlock(&aw_haptic->lock);
	return count;
}

static ssize_t osc_data_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n",
			aw_haptic->osc_cali_data);

	return len;
}

static ssize_t osc_data_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	uint32_t val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	mutex_lock(&aw_haptic->lock);
	aw_haptic->osc_cali_data = val;
	aw_dev_info("%s: osc_cali_data = %d\n", __func__, aw_haptic->osc_cali_data);
	aw_haptic->func->upload_lra(aw_haptic, AW_OSC_CALI_LRA);
	mutex_unlock(&aw_haptic->lock);
	return count;
}


#ifdef OPLUS_FEATURE_CHG_BASIC
static void motor_old_test_work(struct work_struct *work)
{
	struct aw_haptic *aw_haptic = container_of(work, struct aw_haptic, motor_old_test_work);

	aw_dev_err("%s: motor_old_test_mode = %d. gain[0x%02x]\n", __func__,
		   aw_haptic->motor_old_test_mode, aw_haptic->gain);

	if (aw_haptic->motor_old_test_mode == MOTOR_OLD_TEST_TRANSIENT) {
		mutex_lock(&aw_haptic->lock);

		aw_haptic->func->play_stop(aw_haptic);
		aw_haptic->gain = 0x80;
		aw_haptic->func->set_gain(aw_haptic, aw_haptic->gain);
		aw_haptic->func->set_bst_vol(aw_haptic, AW86927_HAPTIC_HIGH_LEVEL_REG_VAL);
		aw_haptic->func->set_wav_seq(aw_haptic, 0,
					     AW_WAVEFORM_INDEX_TRANSIENT);
		aw_haptic->func->set_wav_loop(aw_haptic, 0, 0);
		ram_play(aw_haptic, AW_RAM_MODE);
		mutex_unlock(&aw_haptic->lock);
	} else if (aw_haptic->motor_old_test_mode == MOTOR_OLD_TEST_STEADY) {
		mutex_lock(&aw_haptic->lock);
		aw_haptic->func->play_stop(aw_haptic);
		aw_haptic->gain = 0x80;
		aw_haptic->func->set_gain(aw_haptic, aw_haptic->gain);
		aw_haptic->func->set_bst_vol(aw_haptic, AW86927_HAPTIC_HIGH_LEVEL_REG_VAL);
		aw_haptic->func->set_rtp_aei(aw_haptic, false);
		aw_haptic->func->irq_clear(aw_haptic);
		mutex_unlock(&aw_haptic->lock);
		if (AW_WAVEFORM_INDEX_OLD_STEADY < (sizeof(aw_rtp_name)/AW_RTP_NAME_MAX)) {
			aw_haptic->rtp_file_num = AW_WAVEFORM_INDEX_OLD_STEADY;
			if (AW_WAVEFORM_INDEX_OLD_STEADY) {
				/* schedule_work(&aw_haptic->rtp_work); */
				queue_work(system_unbound_wq, &aw_haptic->rtp_work);
			}
		} else {
			aw_dev_err("%s: rtp_file_num 0x%02x over max value\n",
				   __func__, aw_haptic->rtp_file_num);
		}
	} else if (aw_haptic->motor_old_test_mode ==
		   MOTOR_OLD_TEST_HIGH_TEMP_HUMIDITY) {
		mutex_lock(&aw_haptic->lock);
		aw_haptic->func->play_stop(aw_haptic);
		aw_haptic->gain = 0x80;
		aw_haptic->func->set_gain(aw_haptic, aw_haptic->gain);
		aw_haptic->func->set_bst_vol(aw_haptic, AW86927_HAPTIC_HIGH_LEVEL_REG_VAL);
		aw_haptic->func->set_rtp_aei(aw_haptic, false);
		aw_haptic->func->irq_clear(aw_haptic);
		mutex_unlock(&aw_haptic->lock);
		if (AW_WAVEFORM_INDEX_HIGH_TEMP < (sizeof(aw_rtp_name)/AW_RTP_NAME_MAX)) {
			aw_haptic->rtp_file_num = AW_WAVEFORM_INDEX_HIGH_TEMP;
			if (AW_WAVEFORM_INDEX_HIGH_TEMP) {
				/* schedule_work(&aw_haptic->rtp_work); */
				queue_work(system_unbound_wq,
					   &aw_haptic->rtp_work);
			}
		} else {
			aw_dev_err("%s: rtp_file_num 0x%02x over max value\n",
				   __func__, aw_haptic->rtp_file_num);
		}
	} else if (aw_haptic->motor_old_test_mode == MOTOR_OLD_TEST_LISTEN_POP) {
		mutex_lock(&aw_haptic->lock);
		aw_haptic->func->play_stop(aw_haptic);
		aw_haptic->gain = 0x80;
		aw_haptic->func->set_gain(aw_haptic, aw_haptic->gain);
		aw_haptic->func->set_bst_vol(aw_haptic, AW86927_HAPTIC_HIGH_LEVEL_REG_VAL);
		aw_haptic->func->set_rtp_aei(aw_haptic, false);
		aw_haptic->func->irq_clear(aw_haptic);
		mutex_unlock(&aw_haptic->lock);
		if (AW_WAVEFORM_INDEX_LISTEN_POP < (sizeof(aw_rtp_name)/AW_RTP_NAME_MAX)) {
			aw_haptic->rtp_file_num = AW_WAVEFORM_INDEX_LISTEN_POP;
			if (AW_WAVEFORM_INDEX_LISTEN_POP) {
				/* schedule_work(&aw_haptic->rtp_work); */
				queue_work(system_unbound_wq,
					  &aw_haptic->rtp_work);
			}
		} else {
			aw_dev_err("%s: rtp_file_num 0x%02x over max value\n",
				   __func__, aw_haptic->rtp_file_num);
		}
	} else {
		aw_haptic->motor_old_test_mode = 0;
		mutex_lock(&aw_haptic->lock);
		aw_haptic->func->play_stop(aw_haptic);
		mutex_unlock(&aw_haptic->lock);
	}
}


static ssize_t motor_old_test_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	return 0;
}

static ssize_t motor_old_test_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	unsigned int databuf[1] = {0};

	if (1 == sscanf(buf, "%x", &databuf[0])) {
		if (databuf[0] == 0) {
			cancel_work_sync(&aw_haptic->motor_old_test_work);
			mutex_lock(&aw_haptic->lock);
			aw_haptic->func->play_stop(aw_haptic);
			mutex_unlock(&aw_haptic->lock);
		} else if (databuf[0] <= MOTOR_OLD_TEST_ALL_NUM) {
			cancel_work_sync(&aw_haptic->motor_old_test_work);
			aw_haptic->motor_old_test_mode = databuf[0];
			aw_dev_err("%s: motor_old_test_mode = %d.\n", __func__,
				   aw_haptic->motor_old_test_mode);
			schedule_work(&aw_haptic->motor_old_test_work);
		}
	}

	return count;
}

static ssize_t waveform_index_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	return 0;
}

static ssize_t waveform_index_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	unsigned int databuf[1] = {0};

	if (aw_haptic->device_id == 833) {
		mutex_lock(&aw_haptic->lock);
		aw_haptic->vmax = AW86927_HAPTIC_HIGH_LEVEL_REG_VAL;
		aw_haptic->gain = 0x80;
		aw_haptic->func->set_gain(aw_haptic, aw_haptic->gain);
		aw_haptic->func->set_bst_vol(aw_haptic, aw_haptic->vmax);
		mutex_unlock(&aw_haptic->lock);
	}

	if (1 == sscanf(buf, "%d", &databuf[0])) {
		aw_dev_err("%s: waveform_index = %d\n", __func__, databuf[0]);
		mutex_lock(&aw_haptic->lock);
		aw_haptic->seq[0] = (unsigned char)databuf[0];
		aw_haptic->func->set_wav_seq(aw_haptic, 0, aw_haptic->seq[0]);
		aw_haptic->func->set_wav_seq(aw_haptic, 1, 0);
		aw_haptic->func->set_wav_loop(aw_haptic, 0, 0);
		mutex_unlock(&aw_haptic->lock);
	}
	return count;
}

static ssize_t haptic_ram_test_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	ssize_t len = 0;
	unsigned int ram_test_result = 0;

	if (aw_haptic->ram_test_flag_0 != 0 ||
	    aw_haptic->ram_test_flag_1 != 0) {
		ram_test_result = 1; /* failed */
		len += snprintf(buf+len, PAGE_SIZE-len, "%d\n", ram_test_result);
	} else {
		ram_test_result = 0; /* pass */
		len += snprintf(buf+len, PAGE_SIZE-len, "%d\n", ram_test_result);
	}
	return len;
}

static ssize_t haptic_ram_test_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	struct aw_haptic_container *aw_ramtest;
	int i, j = 0;
	int rc = 0;
	unsigned int val = 0;
	unsigned int start_addr;
	unsigned int tmp_len, retries;
	char *pbuf = NULL;

	aw_dev_info("%s enter\n", __func__);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	start_addr = 0;
	aw_haptic->ram_test_flag_0 = 0;
	aw_haptic->ram_test_flag_1 = 0;
	tmp_len = 1024 ;  /* 1K */
	retries = 8;  /* tmp_len * retries = 8 * 1024 */
	aw_ramtest = kzalloc(tmp_len * sizeof(char) + sizeof(int), GFP_KERNEL);
	if (!aw_ramtest) {
		aw_dev_err("%s: error allocating memory\n", __func__);
		return count;
	}
	pbuf = kzalloc(tmp_len * sizeof(char), GFP_KERNEL);
	if (!pbuf) {
		aw_dev_err("%s: Error allocating memory\n", __func__);
		kfree(aw_ramtest);
		return count;
	}
	aw_ramtest->len = tmp_len;

	if (val == 1) {
		mutex_lock(&aw_haptic->lock);
		/* RAMINIT Enable */
		aw_haptic->func->ram_init(aw_haptic, true);
		for (j = 0; j < retries; j++) {
			/*test 1-----------start*/
			memset(aw_ramtest->data, 0xff, aw_ramtest->len);
			memset(pbuf, 0x00, aw_ramtest->len);
			/* write ram 1 test */
			aw_haptic->func->set_ram_addr(aw_haptic, start_addr);
			aw_haptic->func->set_ram_data(aw_haptic,
						      aw_ramtest->data,
						      aw_ramtest->len);

			/* read ram 1 test */
			aw_haptic->func->set_ram_addr(aw_haptic, start_addr);
			aw_haptic->func->get_ram_data(aw_haptic, pbuf,
						      aw_ramtest->len);

			for (i = 0; i < aw_ramtest->len; i++) {
				if (pbuf[i] != 0xff)
					aw_haptic->ram_test_flag_1++;
			}
			 /*test 1------------end*/

			/*test 0----------start*/
			memset(aw_ramtest->data, 0x00, aw_ramtest->len);
			memset(pbuf, 0xff, aw_ramtest->len);

			/* write ram 0 test */
			aw_haptic->func->set_ram_addr(aw_haptic, start_addr);
			aw_haptic->func->set_ram_data(aw_haptic,
						      aw_ramtest->data,
						      aw_ramtest->len);
			/* read ram 0 test */
			aw_haptic->func->set_ram_addr(aw_haptic, start_addr);
			aw_haptic->func->get_ram_data(aw_haptic, pbuf,
						      aw_ramtest->len);
			for (i = 0; i < aw_ramtest->len; i++) {
				if (pbuf[i] != 0)
					 aw_haptic->ram_test_flag_0++;
			}
			/*test 0 end*/
			start_addr += tmp_len;
		}
		/* RAMINIT Disable */
		aw_haptic->func->ram_init(aw_haptic, false);
		mutex_unlock(&aw_haptic->lock);
	}
	kfree(aw_ramtest);
	kfree(pbuf);
	pbuf = NULL;
	aw_dev_info("%s exit\n", __func__);
	return count;
}

static ssize_t device_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", aw_haptic->device_id);
}

static ssize_t device_id_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t rtp_going_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	ssize_t len = 0;
	int val = -1;

	mutex_lock(&aw_haptic->lock);
	val = aw_haptic->func->juge_rtp_going(aw_haptic);
	mutex_unlock(&aw_haptic->lock);
	len += snprintf(buf+len, PAGE_SIZE-len, "%d\n", val);
	return len;
}

static ssize_t rtp_going_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	return count;
}

#endif

static ssize_t gun_mode_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw_haptic->gun_mode);
}
static ssize_t gun_mode_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw_dev_dbg("%s: value=%d\n", __func__, val);

	mutex_lock(&aw_haptic->lock);
	aw_haptic->gun_mode = val;
	mutex_unlock(&aw_haptic->lock);
	return count;
}

static ssize_t rtp_num_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	ssize_t len = 0;
	unsigned char i = 0;

	for (i = 0; i < AW_RTP_NUM; i++) {
		len += snprintf(buf+len, PAGE_SIZE-len, "num: %d, serial:%d \n",
				i, aw_haptic->rtp_serial[i]);
	}
	return len;
}

static ssize_t rtp_num_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw_haptic *aw_haptic = container_of(cdev, struct aw_haptic,
						   vib_dev);
	/* datebuf[0] is connect number */
	/* databuf[1]-databuf[x] is sequence and number */
	/* custom modify it,if you want */
	unsigned int databuf[AW_RTP_NUM] = {0, 0, 0, 0, 0, 0};
	unsigned int val = 0;
	int rc = 0;

	if (sscanf(buf, "%x %x %x %x %x %x", &databuf[0], &databuf[1],
		  &databuf[2], &databuf[3],
		  &databuf[4], &databuf[5]) == AW_RTP_NUM) {
		for (val = 0; val < AW_RTP_NUM; val++) {
			aw_dev_info("%s: databuf = %d \n", __func__,
				    databuf[val]);
			aw_haptic->rtp_serial[val] = (unsigned char)databuf[val];
		}
	} else {
		rc = kstrtouint(buf, 0, &val);
		if (rc < 0)
			return rc;
		if (val == 0)
			aw_haptic->rtp_serial[0] = 0;
	}
	rtp_regroup_work(aw_haptic);
	return count;
}

 /* Select [S_IWGRP] for ftm selinux */
static DEVICE_ATTR(duration, S_IWUSR | S_IWGRP | S_IRUGO, duration_show, duration_store);
static DEVICE_ATTR(activate, S_IWUSR | S_IWGRP | S_IRUGO, activate_show, activate_store);
static DEVICE_ATTR(state, S_IWUSR | S_IRUGO, state_show, state_store);
static DEVICE_ATTR(oplus_duration, S_IWUSR | S_IWGRP | S_IRUGO, duration_show, duration_store);
static DEVICE_ATTR(oplus_activate, S_IWUSR | S_IWGRP | S_IRUGO, activate_show, activate_store);
static DEVICE_ATTR(oplus_state, S_IWUSR | S_IRUGO, state_show, state_store);
static DEVICE_ATTR(f0, S_IWUSR | S_IRUGO, f0_show, f0_store);
static DEVICE_ATTR(seq, S_IWUSR | S_IRUGO, seq_show, seq_store);
static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO, reg_show, reg_store);
static DEVICE_ATTR(vmax, S_IWUSR | S_IRUGO, vmax_show, vmax_store);
static DEVICE_ATTR(gain, S_IWUSR | S_IRUGO, gain_show, gain_store);
static DEVICE_ATTR(loop, S_IWUSR | S_IRUGO, loop_show, loop_store);
static DEVICE_ATTR(rtp, S_IWUSR | S_IRUGO, rtp_show,  rtp_store);
static DEVICE_ATTR(cali, S_IWUSR | S_IRUGO, cali_show, cali_store);
static DEVICE_ATTR(cont, S_IWUSR | S_IRUGO, cont_show, cont_store);
static DEVICE_ATTR(awrw, S_IWUSR | S_IRUGO, awrw_show, awrw_store);
static DEVICE_ATTR(index, S_IWUSR | S_IRUGO, index_show, index_store);
static DEVICE_ATTR(ram_num, S_IWUSR | S_IRUGO, ram_num_show, NULL);
static DEVICE_ATTR(osc_cali, S_IWUSR | S_IRUGO, osc_cali_show, osc_cali_store);
static DEVICE_ATTR(gun_type, S_IWUSR | S_IRUGO, gun_type_show, gun_type_store);
static DEVICE_ATTR(prctmode, S_IWUSR | S_IRUGO, prct_mode_show,
		   prct_mode_store);
static DEVICE_ATTR(bullet_nr, S_IWUSR | S_IRUGO, bullet_nr_show,
		   bullet_nr_store);
static DEVICE_ATTR(auto_boost, S_IWUSR | S_IRUGO, auto_boost_show,
		   auto_boost_store);
static DEVICE_ATTR(ram_update, S_IWUSR | S_IRUGO, ram_update_show,
		   ram_update_store);
static DEVICE_ATTR(haptic_audio, S_IWUSR | S_IRUGO, haptic_audio_show,
		   haptic_audio_store);
static DEVICE_ATTR(vbat_monitor, S_IWUSR | S_IRUGO, vbat_monitor_show, NULL);
static DEVICE_ATTR(activate_mode, S_IWUSR | S_IRUGO, activate_mode_show,
		   activate_mode_store);
static DEVICE_ATTR(ram_vbat_comp, S_IWUSR | S_IRUGO, ram_vbat_comp_show,
		   ram_vbat_comp_store);
static DEVICE_ATTR(lra_resistance, S_IWUSR | S_IRUGO, lra_resistance_show,
		   NULL);
static DEVICE_ATTR(haptic_audio_time, S_IWUSR | S_IRUGO, haptic_audio_time_show,
		   haptic_audio_time_store);
static DEVICE_ATTR(osc_data, S_IWUSR | S_IRUGO, osc_data_show, osc_data_store);
static DEVICE_ATTR(f0_data, S_IWUSR | S_IRUGO, f0_data_show, f0_data_store);

#ifdef OPLUS_FEATURE_CHG_BASIC
static DEVICE_ATTR(motor_old, S_IWUSR | S_IRUGO, motor_old_test_show,
		   motor_old_test_store);
static DEVICE_ATTR(waveform_index, S_IWUSR | S_IRUGO, waveform_index_show,
		   waveform_index_store);
static DEVICE_ATTR(ram_test, S_IWUSR | S_IRUGO, haptic_ram_test_show,
		   haptic_ram_test_store);
static DEVICE_ATTR(rtp_going, S_IWUSR | S_IRUGO, rtp_going_show,
		   rtp_going_store);
static DEVICE_ATTR(device_id, S_IWUSR | S_IRUGO, device_id_show,
		   device_id_store);
#endif

static DEVICE_ATTR(gun_mode, S_IWUSR | S_IRUGO, gun_mode_show, gun_mode_store);
static DEVICE_ATTR(rtp_num, S_IWUSR | S_IRUGO, rtp_num_show, rtp_num_store);

static struct attribute *vibrator_attributes[] = {
	&dev_attr_state.attr,
	&dev_attr_duration.attr,
	&dev_attr_activate.attr,
	&dev_attr_oplus_state.attr,
	&dev_attr_oplus_duration.attr,
	&dev_attr_oplus_activate.attr,
	&dev_attr_activate_mode.attr,
	&dev_attr_index.attr,
	&dev_attr_vmax.attr,
	&dev_attr_gain.attr,
	&dev_attr_seq.attr,
	&dev_attr_loop.attr,
	&dev_attr_reg.attr,
	&dev_attr_rtp.attr,
	&dev_attr_ram_update.attr,
	&dev_attr_ram_num.attr,
	&dev_attr_f0.attr,
	&dev_attr_f0_data.attr,
	&dev_attr_cali.attr,
	&dev_attr_cont.attr,
	&dev_attr_vbat_monitor.attr,
	&dev_attr_lra_resistance.attr,
	&dev_attr_auto_boost.attr,
	&dev_attr_prctmode.attr,
	&dev_attr_ram_vbat_comp.attr,
	&dev_attr_osc_cali.attr,
	&dev_attr_osc_data.attr,
	&dev_attr_haptic_audio.attr,
	&dev_attr_haptic_audio_time.attr,
	&dev_attr_gun_type.attr,
	&dev_attr_bullet_nr.attr,
	&dev_attr_awrw.attr,
#ifdef OPLUS_FEATURE_CHG_BASIC
	&dev_attr_motor_old.attr,
	&dev_attr_waveform_index.attr,
	&dev_attr_ram_test.attr,
	&dev_attr_rtp_going.attr,
	&dev_attr_device_id.attr,
#endif
	&dev_attr_gun_mode.attr,
	&dev_attr_rtp_num.attr,
	NULL
};

static struct attribute_group vibrator_attribute_group = {
	.attrs = vibrator_attributes
};

#define OPLUS_162HZ_F0 1630
#define OPLUS_166HZ_F0 1670
#define OPLUS_170HZ_F0 1710
#define OPLUS_174HZ_F0 1750
#define OPLUS_178HZ_F0 1780

#define SG_INPUT_DOWN_HIGH	302
#define SG_INPUT_UP_HIGH	303
#define SG_INPUT_DOWN_LOW	304
#define SG_INPUT_UP_LOW		305
#define INPUT_HIGH		112
#define INPUT_MEDI		111
#define INUTP_LOW		110

static void rtp_key_work_routine(struct work_struct *work)
{
	struct aw_haptic *aw_haptic = container_of(work, struct aw_haptic, rtp_key_work);
	uint8_t *aw_haptic_rtp_key_data = NULL;
	uint32_t aw_haptic_rtp_key_data_len = 0;
	bool rtp_work_flag = false;
	uint8_t reg_val = 0;
	int cnt = 200;

	aw_haptic->rtp_init = false;
	mutex_lock(&aw_haptic->rtp_lock);
	switch(aw_haptic->rtp_file_num) {
	case SG_INPUT_DOWN_HIGH:
		if (aw_haptic->f0 <= OPLUS_162HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_302_162Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_302_162Hz);
		} else if (aw_haptic->f0 <= OPLUS_166HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_302_166Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_302_166Hz);
		} else if (aw_haptic->f0 <= OPLUS_170HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_302_170Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_302_170Hz);
		} else if (aw_haptic->f0 <= OPLUS_174HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_302_174Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_302_174Hz);
		} else {
			aw_haptic_rtp_key_data = aw_haptic_rtp_302_178Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_302_178Hz);
		}
		break;
	case SG_INPUT_UP_HIGH:
		if (aw_haptic->f0 <= OPLUS_162HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_303_162Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_303_162Hz);
		} else if (aw_haptic->f0 <= OPLUS_166HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_303_166Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_303_166Hz);
		} else if (aw_haptic->f0 <= OPLUS_170HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_303_170Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_303_170Hz);
		} else if (aw_haptic->f0 <= OPLUS_174HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_303_174Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_303_174Hz);
		} else {
			aw_haptic_rtp_key_data = aw_haptic_rtp_303_178Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_303_178Hz);
		}
		break;
	case SG_INPUT_DOWN_LOW:
		if (aw_haptic->f0 <= OPLUS_162HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_304_162Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_304_162Hz);
		} else if (aw_haptic->f0 <= OPLUS_166HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_304_166Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_304_166Hz);
		} else if (aw_haptic->f0 <= OPLUS_170HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_304_170Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_304_170Hz);
		} else if (aw_haptic->f0 <= OPLUS_174HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_304_174Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_304_174Hz);
		} else {
			aw_haptic_rtp_key_data = aw_haptic_rtp_304_178Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_304_178Hz);
		}
		break;
	case SG_INPUT_UP_LOW:
		if (aw_haptic->f0 <= OPLUS_162HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_305_162Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_305_162Hz);
		} else if (aw_haptic->f0 <= OPLUS_166HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_305_166Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_305_166Hz);
		} else if (aw_haptic->f0 <= OPLUS_170HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_305_170Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_305_170Hz);
		} else if (aw_haptic->f0 <= OPLUS_174HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_305_174Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_305_174Hz);
		} else {
			aw_haptic_rtp_key_data = aw_haptic_rtp_305_178Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_305_178Hz);
		}
		break;
	case INUTP_LOW:
		if (aw_haptic->f0 <= OPLUS_162HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_110_162Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_110_162Hz);
		} else if (aw_haptic->f0 <= OPLUS_166HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_110_166Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_110_166Hz);
		} else if (aw_haptic->f0 <= OPLUS_170HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_110_170Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_110_170Hz);
		} else if (aw_haptic->f0 <= OPLUS_174HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_110_174Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_110_174Hz);
		} else {
			aw_haptic_rtp_key_data = aw_haptic_rtp_110_178Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_110_178Hz);
		}
		break;
	case INPUT_MEDI:
		if (aw_haptic->f0 <= OPLUS_162HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_111_162Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_111_162Hz);
		} else if (aw_haptic->f0 <= OPLUS_166HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_111_166Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_111_166Hz);
		} else if (aw_haptic->f0 <= OPLUS_170HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_111_170Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_111_170Hz);
		} else if (aw_haptic->f0 <= OPLUS_174HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_111_174Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_111_174Hz);
		} else {
			aw_haptic_rtp_key_data = aw_haptic_rtp_111_178Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_111_178Hz);
		}
		break;
	case INPUT_HIGH:
		if (aw_haptic->f0 <= OPLUS_162HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_112_162Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_112_162Hz);
		} else if (aw_haptic->f0 <= OPLUS_166HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_112_166Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_112_166Hz);
		} else if (aw_haptic->f0 <= OPLUS_170HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_112_170Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_112_170Hz);
		} else if (aw_haptic->f0 <= OPLUS_174HZ_F0) {
			aw_haptic_rtp_key_data = aw_haptic_rtp_112_174Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_112_174Hz);
		} else {
			aw_haptic_rtp_key_data = aw_haptic_rtp_112_178Hz;
			aw_haptic_rtp_key_data_len = sizeof(aw_haptic_rtp_112_178Hz);
		}
		break;
	default:
		goto undef_rtp;
		break;
	}
#ifndef OPLUS_FEATURE_CHG_BASIC
	kfree(aw_rtp);
	aw_rtp = kzalloc(aw_haptic_rtp_key_data_len + sizeof(int), GFP_KERNEL);
	if (!aw_rtp) {
		mutex_unlock(&aw_haptic->rtp_lock);//vincent
		aw_dev_err("%s: error allocating memory\n", __func__);
		return;
	}
#else
	if (container_init(aw_haptic_rtp_key_data_len + sizeof(int)) < 0) {
		mutex_unlock(&aw_haptic->rtp_lock);
		aw_dev_err("%s: error allocating memory\n", __func__);
		return;
	}
#endif
	aw_rtp->len = aw_haptic_rtp_key_data_len;
	memcpy(aw_rtp->data, aw_haptic_rtp_key_data, aw_haptic_rtp_key_data_len);
	mutex_unlock(&aw_haptic->rtp_lock);

	mutex_lock(&aw_haptic->lock);
	aw_haptic->rtp_init = true;
	aw_haptic->func->upload_lra(aw_haptic, AW_OSC_CALI_LRA);
	aw_haptic->func->set_rtp_aei(aw_haptic, false);
	aw_haptic->func->irq_clear(aw_haptic);
	aw_haptic->func->play_stop(aw_haptic);
	/* gain */
	ram_vbat_comp(aw_haptic, false);
	/* rtp mode config */
	aw_haptic->func->play_mode(aw_haptic, AW_RTP_MODE);
	/* haptic go */
	aw_haptic->func->play_go(aw_haptic, true);
	usleep_range(2000, 2500);
	while (cnt) {
		reg_val = aw_haptic->func->get_glb_state(aw_haptic);
		if ((reg_val & AW_GLBRD_STATE_MASK) == AW_STATE_RTP) {
			cnt = 0;
			rtp_work_flag = true;
			aw_dev_info("%s: RTP_GO! glb_state=0x08\n", __func__);
		} else {
			cnt--;
			aw_dev_dbg("%s: wait for RTP_GO, glb_state=0x%02X\n",
				   __func__, reg_val);
		}
		usleep_range(2000, 2500);
	}
	if (rtp_work_flag) {
		rtp_play(aw_haptic);
	} else {
		/* enter standby mode */
		aw_haptic->func->play_stop(aw_haptic);
		aw_dev_err("%s: failed to enter RTP_GO status!\n", __func__);
	}
	op_clean_status(aw_haptic);
	mutex_unlock(&aw_haptic->lock);
	return;
undef_rtp:
	mutex_unlock(&aw_haptic->rtp_lock);
	return;
}

static void rtp_single_cycle_routine(struct work_struct *work)
{
	struct aw_haptic *aw_haptic = container_of(work, struct aw_haptic,
						   rtp_single_cycle_work);
	const struct firmware *rtp_file;
	unsigned char reg_val = 0;
	unsigned int buf_len = 0;
	unsigned int cyc_cont = 150;
	int ret = -1;

	aw_haptic->rtp_cnt = 0;
	aw_dev_info("%s enter\n", __func__);
	aw_dev_info("%s---%d\n", __func__, __LINE__);
	/* fw loaded */
	if (aw_haptic->rtp_loop == 0xFF)
		ret = request_firmware(&rtp_file,
				       aw_rtp_name[aw_haptic->rtp_serial[1]],
				       aw_haptic->dev);
	else
		aw_dev_info("%s A single cycle : err value\n", __func__);

	if (ret < 0) {
		aw_dev_err("%s: failed to read %s\n", __func__,
			   aw_rtp_name[aw_haptic->rtp_serial[1]]);
		return;
	}
	aw_haptic->rtp_init = false;
#ifndef OPLUS_FEATURE_CHG_BASIC
	kfree(aw_rtp);
	aw_dev_info("%s---%d\n", __func__, __LINE__);
	aw_rtp = kzalloc(rtp_file->size + sizeof(int), GFP_KERNEL);
	if (!aw_rtp) {
		release_firmware(rtp_file);
		aw_dev_err("%s: error allocating memory\n", __func__);
		return;
	}
#else
	ret = container_init(rtp_file->size + sizeof(int));
	if (ret < 0) {
		release_firmware(rtp_file);
		aw_dev_err("%s: error allocating memory\n", __func__);
		return;
	}
#endif
	aw_rtp->len = rtp_file->size;
	aw_dev_info("%s: rtp file [%s] size = %d\n", __func__,
		    aw_rtp_name[aw_haptic->rtp_serial[1]], aw_rtp->len);
	memcpy(aw_rtp->data, rtp_file->data, rtp_file->size);
	aw_dev_info("%s---%d\n", __func__, __LINE__);
	release_firmware(rtp_file);

	/* Don't enter irq,because osc calibration use while (1) function */
	/* gain */
	ram_vbat_comp(aw_haptic, false);
	aw_dev_info("%s---%d\n", __func__, __LINE__);
	/* rtp mode config */
	aw_haptic->func->play_mode(aw_haptic, AW_RTP_MODE);

	/* haptic start */
	aw_haptic->func->play_go(aw_haptic, true);

	while (aw_haptic->rtp_cycle_flag == 1) {
		if (!aw_haptic->func->rtp_get_fifo_afs(aw_haptic)) {
			if ((aw_rtp->len - aw_haptic->rtp_cnt) < (aw_haptic->ram.base_addr >> 2))
				buf_len = aw_rtp->len - aw_haptic->rtp_cnt;
			else
				buf_len = (aw_haptic->ram.base_addr >> 2);

			aw_haptic->func->set_rtp_data(aw_haptic,
						&aw_rtp->data[aw_haptic->rtp_cnt],
						buf_len);
			aw_haptic->rtp_cnt += buf_len;

			if (aw_haptic->rtp_cnt == aw_rtp->len) {
				/* aw_dev_info("%s: rtp update complete,enter again\n",
				 *	       __func__);
				 */
				aw_haptic->rtp_cnt = 0;
			}
		} else {
			reg_val = aw_haptic->func->read_irq_state(aw_haptic);
			if (reg_val & AW_BIT_SYSST_DONES) {
				aw_dev_info("%s chip playback done\n",
					    __func__);
				break;
			}
			while (1) {
				if (aw_haptic->func->rtp_get_fifo_aes(aw_haptic)) {
					aw_dev_info("--%s---%d--while(1)--\n",
						    __func__, __LINE__);
					break;
				}
				cyc_cont--;
				if (cyc_cont == 0) {
					cyc_cont = 150;
					break;
				}
			}
		}
	}
	aw_dev_info("%s exit\n", __func__);
}


static void rtp_regroup_routine(struct work_struct *work)
{
	struct aw_haptic *aw_haptic = container_of(work, struct aw_haptic,
						   rtp_regroup_work);
	const struct firmware *rtp_file;
	unsigned int buf_len = 0;
	unsigned char reg_val = 0;
	unsigned int  cyc_cont = 150;
	int rtp_len_tmp = 0;
	int aw_rtp_len = 0;
	int i, ret = 0;
	unsigned char *p = NULL;

	aw_haptic->rtp_cnt = 0;
	aw_dev_info("%s enter\n", __func__);
	for (i = 1; i <= aw_haptic->rtp_serial[0]; i++) {
		if ((request_firmware(&rtp_file,
				     aw_rtp_name[aw_haptic->rtp_serial[i]],
				     aw_haptic->dev)) < 0) {
			aw_dev_err("%s: failed to read %s\n", __func__,
				   aw_rtp_name[aw_haptic->rtp_serial[i]]);
		}
		aw_rtp_len = rtp_len_tmp + rtp_file->size;
		rtp_len_tmp = aw_rtp_len;
		aw_dev_info("%s: rtp file [%s] size = %d\n", __func__,
			aw_rtp_name[aw_haptic->rtp_serial[i]], aw_rtp_len);
		release_firmware(rtp_file);
	}

	rtp_len_tmp = 0;
	aw_haptic->rtp_init = false;
#ifndef OPLUS_FEATURE_CHG_BASIC
	kfree(aw_rtp);
	aw_rtp = kzalloc(aw_rtp_len + sizeof(int), GFP_KERNEL);
	if (!aw_rtp) {
		aw_dev_err("%s: error allocating memory\n", __func__);
		return;
	}
#else
	ret = container_init(aw_rtp_len+sizeof(int));
	if (ret < 0) {
		aw_dev_err("%s: error allocating memory\n", __func__);
		return;
	}
#endif
	aw_rtp->len = aw_rtp_len;
	for (i = 1; i <= aw_haptic->rtp_serial[0]; i++) {
		if ((request_firmware(&rtp_file,
				      aw_rtp_name[aw_haptic->rtp_serial[i]],
				      aw_haptic->dev)) < 0) {
			aw_dev_err("%s: failed to read %s\n", __func__,
				   aw_rtp_name[aw_haptic->rtp_serial[i]]);
		}
		p = &(aw_rtp->data[0]) + rtp_len_tmp;
		memcpy(p, rtp_file->data, rtp_file->size);
		rtp_len_tmp += rtp_file->size;
		release_firmware(rtp_file);
		aw_dev_info("%s: rtp file [%s]\n", __func__,
			    aw_rtp_name[aw_haptic->rtp_serial[i]]);
	}

	/*for (j=0; j<aw_rtp_len; j++) {
	 *	aw_dev_info("%s: addr:%d, data:%d\n", __func__, j,
	 *		    aw_rtp->data[j]);
	 *}
	 */
	 /* Don't enter aw_irq,because osc calibration use while (1) function */
	/* aw_haptic->rtp_init = true; */
	/* gain */
	ram_vbat_comp(aw_haptic, false);
	/* rtp mode config */
	aw_haptic->func->play_mode(aw_haptic, AW_RTP_MODE);
	/* haptic start */
	aw_haptic->func->play_go(aw_haptic, true);

	while (aw_haptic->rtp_cycle_flag == 1) {
		if (!aw_haptic->func->rtp_get_fifo_afs(aw_haptic)) {
			if ((aw_rtp->len - aw_haptic->rtp_cnt) < (aw_haptic->ram.base_addr >> 2))
				buf_len = aw_rtp->len - aw_haptic->rtp_cnt;
			else
				buf_len = (aw_haptic->ram.base_addr >> 2);

			aw_haptic->func->set_rtp_data(aw_haptic,
						&aw_rtp->data[aw_haptic->rtp_cnt],
						buf_len);
			aw_haptic->rtp_cnt += buf_len;
			if (aw_haptic->rtp_cnt == aw_rtp->len) {
				aw_dev_info("%s: rtp update complete\n", __func__);
				aw_haptic->rtp_cnt = 0;
				aw_haptic->rtp_loop--;
				if (aw_haptic->rtp_loop == 0)
					return;
			}
		} else {
			reg_val = aw_haptic->func->read_irq_state(aw_haptic);
			if (reg_val & AW_BIT_SYSST_DONES) {
				aw_dev_info("%s chip playback done\n",
					      __func__);
				break;
			}
			while (1) {
				if (aw_haptic->func->rtp_get_fifo_aes(aw_haptic)) {
					aw_dev_info("-----%s---%d----while (1)--\n",
						    __func__, __LINE__);
					break;
				}
				cyc_cont--;
				if (cyc_cont == 0) {
					cyc_cont = 150;
					break;
				}
			}
		}
	}
	aw_dev_info("%s exit\n", __func__);
}

static int rtp_regroup_work(struct aw_haptic *aw_haptic)
{
	aw_haptic->func->play_stop(aw_haptic);
	if (aw_haptic->rtp_serial[0] > 0) {
		aw_dev_info("%s---%d\n", __func__, __LINE__);
		aw_haptic->func->set_rtp_aei(aw_haptic, false);
		aw_haptic->func->irq_clear(aw_haptic);
		if (aw_haptic->rtp_serial[0] <= (sizeof(aw_rtp_name)/AW_RTP_NAME_MAX)) {
			/* if aw_haptic->rtp_loop = 0xff then single cycle ; */
			if (aw_haptic->rtp_loop == 0xFF) {
				aw_haptic->rtp_cycle_flag = 1;
				schedule_work(&aw_haptic->rtp_single_cycle_work);
			} else if ((aw_haptic->rtp_loop > 0) && (aw_haptic->rtp_loop < 0xff)) {
				aw_haptic->rtp_cycle_flag = 1;
				schedule_work(&aw_haptic->rtp_regroup_work);
			} else {
				aw_dev_info("%s---%d\n", __func__, __LINE__);
			}
		}
	} else {
	  aw_haptic->rtp_cycle_flag  = 0;
	}
	return 0;
}

static int vibrator_init(struct aw_haptic *aw_haptic)
{
	int ret = 0;

	aw_dev_info("%s: enter\n", __func__);

#ifdef TIMED_OUTPUT
	aw_dev_info("%s: TIMED_OUT FRAMEWORK!\n", __func__);
	aw_haptic->vib_dev.name = "vibrator";
	aw_haptic->vib_dev.get_time = vibrator_get_time;
	aw_haptic->vib_dev.enable = vibrator_enable;

	ret = timed_output_dev_register(&(aw_haptic->vib_dev));
	if (ret < 0) {
		aw_dev_err("%s: fail to create timed output dev\n", __func__);
		return ret;
	}
	ret = sysfs_create_group(&aw_haptic->vib_dev.dev->kobj,
				 &vibrator_attribute_group);
	if (ret < 0) {
		aw_dev_err("%s: error creating sysfs attr files\n", __func__);
		return ret;
	}
#else
	aw_dev_info("%s: loaded in leds_cdev framework!\n",
		    __func__);
	aw_haptic->vib_dev.name = "vibrator";
	aw_haptic->vib_dev.brightness_get = brightness_get;
	aw_haptic->vib_dev.brightness_set = brightness_set;
	ret = devm_led_classdev_register(&aw_haptic->i2c->dev,
					 &aw_haptic->vib_dev);
	if (ret < 0) {
		aw_dev_err("%s: fail to create led dev\n",
			   __func__);
		return ret;
	}
	ret = sysfs_create_group(&aw_haptic->vib_dev.dev->kobj,
				 &vibrator_attribute_group);
	if (ret < 0) {
		aw_dev_err("%s: error creating sysfs attr files\n", __func__);
		return ret;
	}
#endif
	hrtimer_init(&aw_haptic->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw_haptic->timer.function = vibrator_timer_func;
	INIT_WORK(&aw_haptic->vibrator_work, vibrator_work_routine);
	INIT_WORK(&aw_haptic->rtp_work, rtp_work_routine);
	INIT_WORK(&aw_haptic->rtp_key_work, rtp_key_work_routine);
	INIT_WORK(&aw_haptic->rtp_single_cycle_work, rtp_single_cycle_routine);
	INIT_WORK(&aw_haptic->rtp_regroup_work, rtp_regroup_routine);
	mutex_init(&aw_haptic->lock);
	mutex_init(&aw_haptic->rtp_lock);

	return 0;
}

#ifdef AAC_RICHTAP
static void haptic_clean_buf(struct aw_haptic *aw_haptic, int status)
{
	struct mmap_buf_format *opbuf = aw_haptic->start_buf;
	int i = 0;

	for (i = 0; i < RICHTAP_MMAP_BUF_SUM; i++) {
		opbuf->status = status;
		opbuf = opbuf->kernel_next;
	}
}

static inline unsigned int aw_get_sys_msecs(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	struct timespec64 ts64;

	ktime_get_coarse_real_ts64(&ts64);
#else
	struct timespec64 ts64 = current_kernel_time64();
#endif
	return jiffies_to_msecs(timespec64_to_jiffies(&ts64));
}

static void rtp_work_proc(struct work_struct *work)
{
	struct aw_haptic *aw_haptic = container_of(work, struct aw_haptic,
						   haptic_rtp_work);
	struct mmap_buf_format *opbuf = aw_haptic->start_buf;
	uint32_t count = 100;
	uint8_t reg_val = 0x10;
	unsigned int write_start;
	int cnt = 200;
	bool rtp_work_flag = false;

	opbuf = aw_haptic->start_buf;
	count = 100;
	aw_haptic->rtp_cnt = 0;
	while (true && count--) {
		if (opbuf->status == MMAP_BUF_DATA_VALID) {
			mutex_lock(&aw_haptic->lock);
			aw_haptic->func->play_mode(aw_haptic, AW_RTP_MODE);
			aw_haptic->func->set_rtp_aei(aw_haptic, true);
			aw_haptic->func->irq_clear(aw_haptic);
			aw_haptic->func->play_go(aw_haptic, true);

			while (cnt) {
				usleep_range(2000, 2500);
				reg_val = aw_haptic->func->get_glb_state(aw_haptic);
				if ((reg_val & AW_GLBRD_STATE_MASK) == AW_STATE_RTP) {
					cnt = 0;
					rtp_work_flag = true;
					aw_dev_info("%s: RTP_GO! glb_state=0x08\n", __func__);
				} else {
					cnt--;
					aw_dev_dbg("%s: wait for RTP_GO, glb_state=0x%02X\n",
							__func__, reg_val);
				}
			}

			if (!rtp_work_flag) {
				aw_haptic->func->set_rtp_aei(aw_haptic, false);
				aw_haptic->haptic_rtp_mode = false;
				aw_dev_err("%s: failed to enter RTP_GO status!\n", __func__);
				mutex_unlock(&aw_haptic->lock);
				return;
			}
			mutex_unlock(&aw_haptic->lock);
			break;
		} else {
			msleep(1);
		}
	}
	write_start = aw_get_sys_msecs();
	reg_val = 0x10;
	while (true) {
		if (aw_get_sys_msecs() > (write_start + 800)) {
			aw_dev_info("Failed ! %s endless loop\n", __func__);
			break;
		}
		if (reg_val & AW_BIT_SYSST_DONES || (aw_haptic->done_flag == true) || (opbuf->status == MMAP_BUF_DATA_FINISHED) \
		   || (opbuf->status == MMAP_BUF_DATA_INVALID)) {
			break;
		} else if (opbuf->status == MMAP_BUF_DATA_VALID && (reg_val & 0x01 << 4)) {
			aw_haptic->func->set_rtp_data(aw_haptic, opbuf->data,
						      opbuf->length);
			memset(opbuf->data, 0, opbuf->length);
			opbuf->status = MMAP_BUF_DATA_INVALID;
			opbuf = opbuf->kernel_next;
			write_start = aw_get_sys_msecs();
		} else {
			msleep(1);
		}
		reg_val = aw_haptic->func->get_chip_state(aw_haptic);
	}
	aw_haptic->func->set_rtp_aei(aw_haptic, false);
	aw_haptic->haptic_rtp_mode = false;
}
#endif

static ssize_t aw_file_read(struct file *filp, char *buff,
			    size_t len, loff_t *offset)
{
	struct aw_haptic *aw_haptic = (struct aw_haptic *)filp->private_data;
	int ret = 0;
	int i = 0;
	unsigned char reg_val = 0;
	unsigned char *pbuff = NULL;

	mutex_lock(&aw_haptic->lock);

	aw_dev_info("%s: len=%zu\n", __func__, len);

	switch (aw_haptic->fileops.cmd) {
	case AW_HAPTIC_CMD_READ_REG:
		pbuff = (unsigned char *)kzalloc(len, GFP_KERNEL);
		if (pbuff != NULL) {
			for (i = 0; i < len; i++) {
				i2c_r_bytes(aw_haptic,
					    aw_haptic->fileops.reg+i,
					    &reg_val,
					    AW_I2C_BYTE_ONE);
				pbuff[i] = reg_val;
			}
			for (i = 0; i < len; i++) {
				aw_dev_info("%s: pbuff[%d]=0x%02x\n",
					    __func__, i, pbuff[i]);
			}
			ret = copy_to_user(buff, pbuff, len);
			if (ret)
				aw_dev_err("%s: copy to user fail\n",
					   __func__);
			kfree(pbuff);
		} else {
			aw_dev_err("%s: alloc memory fail\n",
				   __func__);
		}
		break;
	default:
		aw_dev_err("%s, unknown cmd %d\n", __func__,
			   aw_haptic->fileops.cmd);
		break;
	}

	mutex_unlock(&aw_haptic->lock);

	return len;
}

static ssize_t aw_file_write(struct file *filp, const char *buff, size_t len,
			     loff_t *off)
{
	struct aw_haptic *aw_haptic = (struct aw_haptic *)filp->private_data;
	int i = 0;
	int ret = 0;
	unsigned char *pbuff = NULL;

	pbuff = (unsigned char *)kzalloc(len, GFP_KERNEL);
	if (pbuff == NULL) {
		aw_dev_err("%s: alloc memory fail\n", __func__);
		return len;
	}
	ret = copy_from_user(pbuff, buff, len);
	if (ret) {
#ifdef OPLUS_FEATURE_CHG_BASIC
	if (pbuff != NULL)
		kfree(pbuff);

#endif
		aw_dev_err("%s: copy from user fail\n", __func__);
		return len;
	}

	for (i = 0; i < len; i++)
		aw_dev_info("%s: pbuff[%d]=0x%02x\n", __func__, i, pbuff[i]);


	mutex_lock(&aw_haptic->lock);

	aw_haptic->fileops.cmd = pbuff[0];

	switch (aw_haptic->fileops.cmd) {
	case AW_HAPTIC_CMD_READ_REG:
		if (len == 2)
			aw_haptic->fileops.reg = pbuff[1];
		else
			aw_dev_err("%s: read cmd len %zu err\n",
				   __func__, len);
		break;
	case AW_HAPTIC_CMD_WRITE_REG:
		if (len > 2) {
			for (i = 0; i < len - 2; i++) {
				aw_dev_err("%s: write reg0x%02x=0x%02x\n",
					   __func__,
					   pbuff[1]+i, pbuff[i+2]);
				i2c_w_bytes(aw_haptic, pbuff[1]+i,
					    &pbuff[2+i],
					    AW_I2C_BYTE_ONE);
			}
		} else {
			aw_dev_err("%s: write cmd len %zu err\n",
				   __func__, len);
		}
		break;
	default:
		aw_dev_err("%s, unknown cmd %d\n", __func__,
			   aw_haptic->fileops.cmd);
		break;
	}

	mutex_unlock(&aw_haptic->lock);

	if (pbuff != NULL)
		kfree(pbuff);

	return len;
}

static long aw_file_unlocked_ioctl(struct file *file, unsigned int cmd,
				   unsigned long arg)
{
	struct aw_haptic *aw_haptic = (struct aw_haptic *)file->private_data;
	uint32_t tmp;
	int ret = 0;

	aw_dev_info("%s: cmd=0x%x, arg=0x%lx\n", __func__, cmd, arg);

	mutex_lock(&aw_haptic->lock);
#ifdef AAC_RICHTAP
	switch (cmd) {
	case RICHTAP_GET_HWINFO:
		/* need to check */
		tmp = RICHTAP_HAPTIC_HV;
		if (copy_to_user((void __user *)arg, &tmp, sizeof(uint32_t)))
			ret = -EFAULT;
		break;
	case RICHTAP_RTP_MODE:
		aw_haptic->func->play_stop(aw_haptic);
		if (copy_from_user(aw_haptic->rtp_ptr, (void __user *)arg,
		   RICHTAP_MMAP_BUF_SIZE * RICHTAP_MMAP_BUF_SUM)) {
			ret = -EFAULT;
			break;
		}
		tmp = *((uint32_t *)aw_haptic->rtp_ptr);
		if (tmp > (RICHTAP_MMAP_BUF_SIZE * RICHTAP_MMAP_BUF_SUM - 4)) {
			aw_dev_err("rtp mode date len error %d\n", tmp);
			ret = -EINVAL;
			break;
		}
		if (aw_haptic->chipid == AW86927_CHIPID)
			aw_haptic->func->set_bst_vol(aw_haptic, 0x4F);//boost 8.414V
		else if (aw_haptic->chipid == AW86907_CHIPID)
			aw_haptic->func->set_bst_vol(aw_haptic, 0x1D); /*boost 8.287V*/
		else
			aw_haptic->func->set_bst_vol(aw_haptic, 0x4F);//boost 8.414V

		aw_haptic->func->upload_lra(aw_haptic, AW_OSC_CALI_LRA);
		aw_haptic->func->play_mode(aw_haptic, AW_RTP_MODE);
		aw_haptic->func->play_go(aw_haptic, true);
		usleep_range(2000, 2500);
		aw_haptic->func->set_rtp_data(aw_haptic,
					      &aw_haptic->rtp_ptr[4],
					      tmp);
		break;
	case RICHTAP_OFF_MODE:
			break;
	case RICHTAP_GET_F0:
		tmp = aw_haptic->f0;
		if (copy_to_user((void __user *)arg, &tmp, sizeof(uint32_t)))
			ret = -EFAULT;
		break;
	case RICHTAP_SETTING_GAIN:
		if (arg > 0x80)
			arg = 0x80;
		aw_haptic->func->set_gain(aw_haptic, arg);
		break;
	case RICHTAP_STREAM_MODE:
		haptic_clean_buf(aw_haptic, MMAP_BUF_DATA_INVALID);
		aw_haptic->func->play_stop(aw_haptic);
		aw_haptic->done_flag = false;
		aw_haptic->haptic_rtp_mode = true;
		if (aw_haptic->chipid == AW86927_CHIPID)
			aw_haptic->func->set_bst_vol(aw_haptic, 0x4F);//boost 8.414V
		else if (aw_haptic->chipid == AW86907_CHIPID)
			aw_haptic->func->set_bst_vol(aw_haptic, 0x1D); /*boost 8.287V*/
		else
			aw_haptic->func->set_bst_vol(aw_haptic, 0x4F);//boost 8.414V

		aw_haptic->func->upload_lra(aw_haptic, AW_OSC_CALI_LRA);
		schedule_work(&aw_haptic->haptic_rtp_work);
		break;
	case RICHTAP_STOP_MODE:
		aw_haptic->done_flag = true;
		op_clean_status(aw_haptic);
		/* hrtimer_cancel(&aw_haptic->timer);
		 * aw_haptic->state = 0;
		 * haptic_clean_buf(aw_haptic, MMAP_BUF_DATA_FINISHED);
		 */
		usleep_range(2000, 2100);
		aw_haptic->func->set_rtp_aei(aw_haptic, false);
		aw_haptic->func->play_stop(aw_haptic);
		aw_haptic->haptic_rtp_mode = false;
		/* wait_event_interruptible(haptic->doneQ,
		 *			    !haptic->task_flag);
		 */
		break;
	default:
		aw_dev_err("%s, unknown cmd = %d\n", __func__, cmd);
		break;
	}
#else
	if (_IOC_TYPE(cmd) != AW_HAPTIC_IOCTL_MAGIC) {
		aw_dev_err("%s: cmd magic err\n", __func__);
		mutex_unlock(&aw_haptic->lock);
		return -EINVAL;
	}

	switch (cmd) {
	default:
		aw_dev_err("%s, unknown cmd\n", __func__);
		break;
	}
#endif
	mutex_unlock(&aw_haptic->lock);

	return ret;
}

#ifdef AAC_RICHTAP
static int aw_file_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long phys;
	struct aw_haptic *aw_haptic = (struct aw_haptic *)filp->private_data;
	int ret = 0;

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 7, 0)
	/* only accept PROT_READ, PROT_WRITE and MAP_SHARED from the API of mmap */
	vm_flags_t vm_flags = calc_vm_prot_bits(PROT_READ|PROT_WRITE, 0) | calc_vm_flag_bits(MAP_SHARED);
	vm_flags |= current->mm->def_flags | VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC | VM_SHARED | VM_MAYSHARE;
	if (vma && (pgprot_val(vma->vm_page_prot) != pgprot_val(vm_get_page_prot(vm_flags))))
		return -EPERM;

	if (vma && ((vma->vm_end - vma->vm_start) != (PAGE_SIZE << RICHTAP_MMAP_PAGE_ORDER)))
		return -ENOMEM;
#endif
	phys = virt_to_phys(aw_haptic->start_buf);

	ret = remap_pfn_range(vma, vma->vm_start, (phys >> PAGE_SHIFT), (vma->vm_end - vma->vm_start), vma->vm_page_prot);
	if (ret) {
		aw_dev_err("Error mmap failed\n");
		return ret;
	}

	return ret;
}
#endif

static int aw_file_open(struct inode *inode, struct file *file)
{
	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

	file->private_data = (void *)g_aw_haptic;

	return 0;
}

static int aw_file_release(struct inode *inode, struct file *file)
{
	file->private_data = (void *)NULL;

	module_put(THIS_MODULE);

	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = aw_file_read,
	.write = aw_file_write,
#ifdef AAC_RICHTAP
	.mmap = aw_file_mmap,
#endif
	.unlocked_ioctl = aw_file_unlocked_ioctl,
	.open = aw_file_open,
	.release = aw_file_release,
};

static struct miscdevice haptic_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = HAPTIC_NAME,
	.fops = &fops,
};



static ssize_t proc_vibration_style_read(struct file *filp, char __user *buf,
				     size_t count, loff_t *ppos)
{
	struct aw_haptic *aw_haptic = (struct aw_haptic *)filp->private_data;
	uint8_t ret = 0;
	int style = 0;
	char page[10];

	style = aw_haptic->vibration_style;

	aw_dev_info("%s: touch_style=%d\n", __func__, style);
	sprintf(page, "%d\n", style);
	ret = simple_read_from_buffer(buf, count, ppos, page, strlen(page));
	return ret;
}

static ssize_t proc_vibration_style_write(struct file *filp, const char __user *buf,
				      size_t count, loff_t *lo)
{
	struct aw_haptic *aw_haptic = (struct aw_haptic *)filp->private_data;
	char buffer[5] = { 0 };
	int rc = 0;
	int val;
	if (count > sizeof(buffer)) {
		return -EFAULT;
	}
	if (buf == NULL) {
		return -EFAULT;
	}
	if (copy_from_user(buffer, buf, count)) {
		aw_dev_err("%s: error.\n", __func__);
		return -EFAULT;
	}

	aw_dev_err("buffer=%s", buffer);
	rc = kstrtoint(buffer, 0, &val);
	if (rc < 0)
		return count;
	aw_dev_err("val = %d", val);

	if (val == 0) {
		aw_haptic->vibration_style = AW_HAPTIC_VIBRATION_CRISP_STYLE;
		ram_update(aw_haptic);
	} else if (val == 1) {
		aw_haptic->vibration_style = AW_HAPTIC_VIBRATION_SOFT_STYLE;
		ram_update(aw_haptic);
	} else {
		aw_haptic->vibration_style = AW_HAPTIC_VIBRATION_CRISP_STYLE;
	}
	return count;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0))
static const struct file_operations proc_vibration_style_ops = {
	.read = proc_vibration_style_read,
	.write = proc_vibration_style_write,
	.open =  aw_file_open,
	.owner = THIS_MODULE,
};
#else
static const struct proc_ops proc_vibration_style_ops = {
	.proc_read = proc_vibration_style_read,
	.proc_write = proc_vibration_style_write,
	.proc_open =  aw_file_open,
};
#endif

static int init_vibrator_proc(struct aw_haptic *aw_haptic)
{
	int ret = 0;

	aw_haptic->prEntry_da = proc_mkdir("vibrator", NULL);
	if (aw_haptic->prEntry_da == NULL) {
		ret = -ENOMEM;
		aw_dev_err("%s: Couldn't create vibrator proc entry\n",
			   __func__);
	}
	aw_haptic->prEntry_tmp = proc_create_data("touch_style", 0664,
						  aw_haptic->prEntry_da,
						  &proc_vibration_style_ops,
						  aw_haptic);
	if (aw_haptic->prEntry_tmp == NULL) {
		ret = -ENOMEM;
		aw_dev_err("%s: Couldn't create proc entry\n", __func__);
	}
	return 0;
}

static void haptic_init(struct aw_haptic *aw_haptic)
{
	int ret = 0;

	aw_dev_info("%s: enter\n", __func__);
	ret = misc_register(&haptic_misc);
	if (ret) {
		aw_dev_err("%s: misc fail: %d\n", __func__, ret);
		return;
	}
	init_vibrator_proc(aw_haptic);
	/* haptic audio */
	aw_haptic->haptic_audio.delay_val = 1;
	aw_haptic->haptic_audio.timer_val = 21318;
	INIT_LIST_HEAD(&(aw_haptic->haptic_audio.ctr_list));
	hrtimer_init(&aw_haptic->haptic_audio.timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	aw_haptic->haptic_audio.timer.function = audio_timer_func;
	INIT_WORK(&aw_haptic->haptic_audio.work, audio_work_routine);
	INIT_LIST_HEAD(&(aw_haptic->haptic_audio.list));
	mutex_init(&aw_haptic->haptic_audio.lock);
	mutex_init(&aw_haptic->qos_lock);
	aw_haptic->gun_type = 0xFF;
	aw_haptic->bullet_nr = 0x00;
	aw_haptic->gun_mode = 0x00;
	op_clean_status(aw_haptic);

	/* haptic init */
	mutex_lock(&aw_haptic->lock);
	aw_haptic->rtp_routine_on = 0;
	aw_haptic->activate_mode = AW_CONT_MODE;
	aw_haptic->vibration_style = AW_HAPTIC_VIBRATION_CRISP_STYLE;
	aw_haptic->func->play_mode(aw_haptic, AW_STANDBY_MODE);
	aw_haptic->func->set_pwm(aw_haptic, AW_PWM_24K);
	/* misc value init */
	aw_haptic->func->misc_para_init(aw_haptic);

	aw_haptic->func->set_bst_peak_cur(aw_haptic);
	aw_haptic->func->auto_bst_enable(aw_haptic, false);
	aw_haptic->func->offset_cali(aw_haptic);
	/* vbat compensation */
	aw_haptic->func->vbat_mode_config(aw_haptic, AW_CONT_VBAT_HW_COMP_MODE);
	aw_haptic->ram_vbat_comp = AW_RAM_VBAT_COMP_ENABLE;

	//aw_haptic->func->trig_init(aw_haptic);
	mutex_unlock(&aw_haptic->lock);

	/* f0 calibration */
	mutex_lock(&aw_haptic->lock);
#ifndef OPLUS_FEATURE_CHG_BASIC
	f0_cali(aw_haptic);
#endif
	mutex_unlock(&aw_haptic->lock);
}

#ifdef AAC_RICHTAP
static int aac_init(struct aw_haptic *aw_haptic)
{
	aw_haptic->rtp_ptr = kmalloc(RICHTAP_MMAP_BUF_SIZE * RICHTAP_MMAP_BUF_SUM, GFP_KERNEL);
	if (aw_haptic->rtp_ptr == NULL) {
		aw_dev_err("%s: malloc rtp memory failed\n", __func__);
		return -ENOMEM;
	}

	aw_haptic->start_buf = (struct mmap_buf_format *)__get_free_pages(GFP_KERNEL, RICHTAP_MMAP_PAGE_ORDER);
	if (aw_haptic->start_buf == NULL) {
		aw_dev_err("%s: Error __get_free_pages failed\n", __func__);
		return -ENOMEM;
	}
	SetPageReserved(virt_to_page(aw_haptic->start_buf));
	{
		struct mmap_buf_format *temp;
		uint32_t i = 0;

		temp = aw_haptic->start_buf;
		for (i = 1; i < RICHTAP_MMAP_BUF_SUM; i++) {
			temp->kernel_next = (aw_haptic->start_buf + i);
			temp = temp->kernel_next;
		}
		temp->kernel_next = aw_haptic->start_buf;
	}
	INIT_WORK(&aw_haptic->haptic_rtp_work, rtp_work_proc);
	/* init_waitqueue_head(&aw8697->doneQ); */
	aw_haptic->done_flag = true;
	aw_haptic->haptic_rtp_mode = false;
	return 0;
}
#endif

static int awinic_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	int ret = 0;
	struct aw_haptic *aw_haptic;
	struct device_node *np = i2c->dev.of_node;

	aw_dev_info("%s: enter\n", __func__);
	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		aw_dev_err("check_functionality failed\n");
		return -EIO;
	}

	aw_haptic = devm_kzalloc(&i2c->dev, sizeof(struct aw_haptic),
				 GFP_KERNEL);
	if (aw_haptic == NULL)
		return -ENOMEM;

	aw_haptic->dev = &i2c->dev;
	aw_haptic->i2c = i2c;

	i2c_set_clientdata(i2c, aw_haptic);
	dev_set_drvdata(&i2c->dev, aw_haptic);

	/* aw_haptic rst & int */
	if (np) {
		ret = parse_dt(&i2c->dev, aw_haptic, np);
		if (ret) {
			aw_dev_err("%s: failed to parse gpio\n",
				   __func__);
			goto err_parse_dt;
		}
	} else {
		aw_haptic->reset_gpio = -1;
		aw_haptic->irq_gpio = -1;
	}

	if (gpio_is_valid(aw_haptic->reset_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, aw_haptic->reset_gpio,
					    GPIOF_OUT_INIT_LOW, "awinic_rst");
		if (ret) {
			aw_dev_err("%s: rst request failed\n",
				   __func__);
			goto err_reset_gpio_request;
		}
	}

#ifdef AW_ENABLE_PIN_CONTROL
	aw_haptic->pinctrl = devm_pinctrl_get(&i2c->dev);
	aw_haptic->pinctrl_state = pinctrl_lookup_state(aw_haptic->pinctrl,
							"irq_active");
	if (aw_haptic->pinctrl != NULL && aw_haptic->pinctrl_state != NULL)
		pinctrl_select_state(aw_haptic->pinctrl,
				     aw_haptic->pinctrl_state);
	else
		aw_dev_err("%s: pinctrl error!\n", __func__);
#endif

	if (gpio_is_valid(aw_haptic->irq_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, aw_haptic->irq_gpio,
					    GPIOF_DIR_IN, "awinic_int");
		if (ret) {
			aw_dev_err("%s: int request failed\n",
				   __func__);
			goto err_irq_gpio_request;
		}
	}

	/* aw func ptr init */
	ret = ctrl_init(aw_haptic);
	if (ret < 0) {
		aw_dev_err("%s: ctrl_init failed ret=%d\n", __func__, ret);
		goto err_ctrl_init;
	}

	ret = aw_haptic->func->check_qualify(aw_haptic);
	if (ret < 0) {
		aw_dev_err("%s: qualify check failed ret=%d", __func__, ret);
		goto err_ctrl_init;
	}

	/* aw_haptic chip id */
	ret = parse_chipid(aw_haptic);
	if (ret < 0) {
		aw_dev_err("%s: read_chipid failed ret=%d\n", __func__, ret);
		goto err_id;
	}

	parse_vibrate_param_dts(&i2c->dev, aw_haptic, np);

	sw_reset(aw_haptic);

	ret = container_init(aw_container_size);
	if (ret < 0)
		aw_dev_err("%s: rtp alloc memory failed\n", __func__);

	aw_haptic->func->haptic_value_init(aw_haptic);

	/* aw_haptic irq */
	ret = irq_config(&i2c->dev, aw_haptic);
	if (ret != 0) {
		aw_dev_err("%s: irq_config failed ret=%d\n", __func__, ret);
		goto err_irq_config;
	}
#ifdef AAC_RICHTAP
	aac_init(aw_haptic);
#endif
	g_aw_haptic = aw_haptic;
	vibrator_init(aw_haptic);
	haptic_init(aw_haptic);
	aw_haptic->func->creat_node(aw_haptic);
	ram_work_init(aw_haptic);
#ifdef OPLUS_FEATURE_CHG_BASIC
	INIT_WORK(&aw_haptic->motor_old_test_work, motor_old_test_work);
	aw_haptic->motor_old_test_mode = 0;
#endif

	/* ram init */
	ram_update(aw_haptic);

	aw_dev_info("%s:1010 test  probe completed successfully!\n", __func__);

	return 0;

err_id:
err_ctrl_init:
err_irq_config:
	if (gpio_is_valid(aw_haptic->irq_gpio))
		devm_gpio_free(&i2c->dev, aw_haptic->irq_gpio);

err_irq_gpio_request:
	if (gpio_is_valid(aw_haptic->reset_gpio))
		devm_gpio_free(&i2c->dev, aw_haptic->reset_gpio);

err_parse_dt:
err_reset_gpio_request:
	devm_kfree(&i2c->dev, aw_haptic);
	aw_haptic = NULL;
	return ret;
}

static int awinic_i2c_remove(struct i2c_client *i2c)
{
	struct aw_haptic *aw_haptic = i2c_get_clientdata(i2c);

	aw_dev_info("%s: enter.\n", __func__);

	cancel_delayed_work_sync(&aw_haptic->ram_work);
	cancel_work_sync(&aw_haptic->haptic_audio.work);
	hrtimer_cancel(&aw_haptic->haptic_audio.timer);
	cancel_work_sync(&aw_haptic->rtp_work);
	cancel_work_sync(&aw_haptic->vibrator_work);
	hrtimer_cancel(&aw_haptic->timer);
	mutex_destroy(&aw_haptic->lock);
	mutex_destroy(&aw_haptic->rtp_lock);
	mutex_destroy(&aw_haptic->qos_lock);
	mutex_destroy(&aw_haptic->haptic_audio.lock);
	misc_deregister(&haptic_misc);
	remove_proc_entry("vibrator", aw_haptic->prEntry_da);
#ifndef OPLUS_FEATURE_CHG_BASIC
	kfree(aw_rtp);
#else
	vfree(aw_rtp);
#endif
#ifdef AAC_RICHTAP
	kfree(aw_haptic->rtp_ptr);
	free_pages((unsigned long)aw_haptic->start_buf, RICHTAP_MMAP_PAGE_ORDER);
#endif
#ifdef TIMED_OUTPUT
	timed_output_dev_unregister(&aw_haptic->vib_dev);
#endif
	devm_free_irq(&i2c->dev, gpio_to_irq(aw_haptic->irq_gpio), aw_haptic);
	if (gpio_is_valid(aw_haptic->irq_gpio))
		devm_gpio_free(&i2c->dev, aw_haptic->irq_gpio);
	if (gpio_is_valid(aw_haptic->reset_gpio))
		devm_gpio_free(&i2c->dev, aw_haptic->reset_gpio);

	return 0;
}

static const struct i2c_device_id awinic_i2c_id[] = {
	{AW_I2C_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, awinic_i2c_id);

static const struct of_device_id awinic_dt_match[] = {
	{.compatible = "awinic,aw8697_haptic"},
	{},
};

static struct i2c_driver awinic_i2c_driver = {
	.driver = {
		   .name = AW_I2C_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(awinic_dt_match),
		   },
	.probe = awinic_i2c_probe,
	.remove = awinic_i2c_remove,
	.id_table = awinic_i2c_id,
};

static int __init awinic_i2c_init(void)
{
	int ret = 0;

	aw_dev_info("aw_haptic driver version %s\n", HAPTIC_HV_DRIVER_VERSION);
	ret = i2c_add_driver(&awinic_i2c_driver);
	if (ret) {
		aw_dev_err("%s: fail to add aw_haptic device into i2c\n", __func__);
		return ret;
	}
	return 0;
}
module_init(awinic_i2c_init);

static void __exit awinic_i2c_exit(void)
{
	i2c_del_driver(&awinic_i2c_driver);
}
module_exit(awinic_i2c_exit);

MODULE_DESCRIPTION("AWINIC Haptic Driver");
MODULE_LICENSE("GPL v2");
