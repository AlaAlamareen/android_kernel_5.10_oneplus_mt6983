# SPDX-License-Identifier: GPL-2.0
export MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
obj-$(CONFIG_MTK_BLOCK_IO_TRACER) += blocktag/
obj-$(CONFIG_MTK_SLBC) += slbc/
obj-$(CONFIG_MTK_CM_MGR) += cm_mgr/
obj-$(CONFIG_MTK_QOS_FRAMEWORK) += qos/
obj-$(CONFIG_MTK_CCCI_DEVICES)	+=  ccci_util/
obj-$(CONFIG_MTK_NET_CCMNI) +=  ccmni/
obj-$(CONFIG_MTK_NET_RPS) += rps/
obj-$(CONFIG_MEDIATEK_CPUIDLE) += cpuidle/
obj-$(CONFIG_NFC_CHIP_SUPPORT) += nfc/
obj-$(CONFIG_MTK_ECCCI_DRIVER) +=  eccci/
obj-$(CONFIG_MTK_ECCCI_C2K_USB) += c2k_usb/
obj-$(CONFIG_MTK_PID_MAP) += pidmap/
obj-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += sspm/
obj-$(CONFIG_MTK_TINYSYS_SCMI) += tinysys_scmi/
obj-$(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT) += mcupm/
obj-$(CONFIG_MTK_SUBPMIC_MISC) += subpmic/
obj-$(CONFIG_MMPROFILE) += mmp/
obj-$(CONFIG_MTK_APUSYS_SUPPORT) += apusys/
obj-$(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM) += trusted_mem/
obj-$(CONFIG_MTK_ENABLE_GENIEZONE) += geniezone/
obj-$(CONFIG_MTK_SECURITY_SW_SUPPORT) += masp/
obj-${CONFIG_MTK_ATF_LOGGER} += atf/
obj-$(CONFIG_MTK_USB_TYPEC) += typec/
obj-$(CONFIG_MTPROF) += mtprof/
obj-$(CONFIG_MTK_IRQ_MONITOR) += mtprof/
obj-$(CONFIG_MTK_LOCKING_AEE) += locking/
obj-$(CONFIG_MTK_CAMERA_ISP_SUPPORT) += cameraisp/
obj-$(CONFIG_MTK_CAMERA_MEM_SUPPORT) += camera_mem/
obj-$(CONFIG_MTK_CAM_TIMESYNC) += cam_timesync/
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq/
obj-$(CONFIG_MTK_MDP) += mdp/
obj-$(CONFIG_MTK_JPEG) += jpeg/
obj-$(CONFIG_MTK_VDEC_FMT) += vdec_fmt/
obj-$(CONFIG_MTK_DVFSRC_HELPER) += dvfsrc/
obj-$(CONFIG_MTK_V4L2_LENS) += lens/
obj-$(CONFIG_MTK_LENS)      += lens/
obj-$(CONFIG_MTK_FLASHLIGHT) += flashlight/
obj-$(CONFIG_MTK_V4L2_FLASHLIGHT) += flashlight/v4l2/
obj-$(CONFIG_MTK_DCM_COMMON) += dcm/
obj-$(CONFIG_MEDIATEK_DCM_COMMON) += dcm_v1/
obj-$(CONFIG_MMSTAT_TRACER) += mmstat/
obj-$(CONFIG_MTK_LOW_POWER_MODULE) += lpm/
obj-$(CONFIG_MTK_SSC_MODULE) += ssc/
obj-$(CONFIG_MTK_SWPM_MODULE) += swpm/
obj-$(CONFIG_MTK_PMSR) += pmsr/
obj-$(CONFIG_MTK_IMGSENSOR) += imgsensor/src/
obj-$(CONFIG_MTK_V4L2_IMGSENSOR) += imgsensor/src-v4l2/
obj-$(CONFIG_MTK_IOMMU_MISC) += iommu/
obj-$(CONFIG_MTK_COMBO) += connectivity/
obj-$(CONFIG_MTK_CONN_SCP) += conn_scp/
obj-$(CONFIG_MTK_BTIF) += btif/
obj-$(CONFIG_MTK_CONN_MD) += conn_md/
obj-$(CONFIG_MTK_CAM_CAL) += cam_cal/
obj-$(CONFIG_MTK_AEE_FEATURE) += aee/
obj-$(CONFIG_MTK_CLKBUF) += clkbuf/
obj-$(CONFIG_MTK_CLK_BUFFER) += clkbuf/
obj-$(CONFIG_MTK_SRCLKEN_RC) += srclken_rc/
obj-$(CONFIG_MTK_POWER_THROTTLING_COMMON) += power_throttling/
obj-$(CONFIG_MTK_PMIC_PROTECT) += pmic_protect/
obj-$(CONFIG_MTK_MDPM) += mdpm/
obj-$(CONFIG_MTK_MDPM_LEGACY) += mdpm_v1/
obj-$(CONFIG_MTK_PWM)	    += pwm/
obj-$(CONFIG_MTK_IRTX_PWM_SUPPORT) += irtx/
obj-$(CONFIG_MTK_PBM) += pbm/
obj-$(CONFIG_MTK_CCU) += ccu/src/
obj-$(CONFIG_MEDIATEK_CPUFREQ_DEBUG) += cpufreq_v1/
obj-$(CONFIG_MEDIATEK_CPU_DVFS) += cpufreq_v2/
obj-$(CONFIG_MEDIATEK_MCDI) += mcdi/
obj-$(CONFIG_MTK_CPUHOTPLUG) += cpuhotplug/
obj-$(CONFIG_MTK_UNIFIED_POWER) += upower/
obj-$(CONFIG_MTK_STATIC_POWER_LEGACY) += leakage_table_v2/
obj-$(CONFIG_MEDIATEK_CPUFREQ_DEBUG_LITE) += cpufreq_lite/
obj-$(CONFIG_MTK_PTPOD_GPU) += eemgpu/
obj-$(CONFIG_MTK_PTPOD_LEGACY) += eem_v2/
obj-$(CONFIG_MTK_MKP) += mkp/
obj-$(CONFIG_MTK_STATIC_POWER) += leakage_table/
obj-$(CONFIG_EXTCON_MTK_USB) += extcon/
obj-$(CONFIG_MTK_VOW_SUPPORT) += vow/
obj-$(CONFIG_MTK_PERFORMANCE_MODULE) += performance/
obj-$(CONFIG_MTK_PERF_COMMON) += perf_common/
obj-$(CONFIG_MTK_AUDIODSP_SUPPORT) += adsp/
obj-$(CONFIG_MTK_AUDIO_IPI) += audio_ipi/
obj-$(CONFIG_MTK_HANG_DETECT) += monitor_hang/
obj-$(CONFIG_USB_CONFIGFS_F_MTP) += usb_mtp/
obj-$(CONFIG_MTK_USB_CONFIGFS_RNDIS) += usb_rndis/
obj-$(CONFIG_MTK_SCHEDULER) += sched/
obj-$(CONFIG_MTK_AMMS) += memory-amms/
obj-$(CONFIG_MTK_SDA) += sda/
obj-$(CONFIG_MTK_MDDP_SUPPORT) += mddp/
obj-$(CONFIG_MTK_TINYSYS_SCP_SUPPORT) += scp/
obj-$(CONFIG_MTK_TINYSYS_VCP_SUPPORT) += vcp/
ifeq ($(subst $(quote),,$(CONFIG_MTK_SENSOR_ARCHITECTURE)), 1.0)
obj-y += sensors-1.0/
else
obj-y += sensor/
endif
obj-$(CONFIG_MTK_DRAM_LOG_STORE) += log_store/
obj-$(CONFIG_MTK_PRINTK) += mtprintk/
obj-$(CONFIG_MTK_TASK_TURBO) += task_turbo/
obj-$(CONFIG_MTK_SELINUX_AEE_WARNING) += selinux_warning/
obj-$(CONFIG_INTERCONNECT_MTK_EXTENSION) += mtk-interconnect/
obj-$(CONFIG_MTK_MMDVFS) += mmdvfs/
obj-$(CONFIG_MTK_MMINFRA) += mminfra/
obj-$(CONFIG_MTK_VMM_SPM) += vmm_spm/
obj-$(CONFIG_MTK_VMM_DBG) += vmm_dbg/
obj-$(CONFIG_MTK_SMI) += smi/
obj-$(CONFIG_MTK_USB_BOOST) += usb_boost/
obj-$(CONFIG_MTK_USB_META) += usb_meta/
obj-$(CONFIG_USB_MTK_HDRC) += usb20/
obj-$(CONFIG_MTK_CPUQOS_V3) += cache-auditor/
obj-$(CONFIG_MTK_GATE_IC) += gate_ic/
obj-$(CONFIG_MTK_WIDEVINE_DRM) += widevine_drm/
obj-$(CONFIG_INTERCONNECT_MTK_MMQOS_COMMON) += mmqos/
#ifdef OPLUS_BUG_STABILITY
obj-$(CONFIG_MTK_BOOT_COMMON) += boot_common/
#endif
obj-$(CONFIG_MTK_TIMER_ACCURACY_TEST) += timer/
obj-$(CONFIG_MTK_CAMERA_LDO) += wl2868c/
obj-$(CONFIG_MTK_CAMERA_LDO) += aw37004/
#ifdef OPLUS_FEATURE_CAMERA_COMMON
#Add for DFX camera log report
obj-$(CONFIG_MTK_V4L2_IMGSENSOR) += imgsensor/oplus/
#endif /* OPLUS_FEATURE_CAMERA_COMMON */
obj-$(CONFIG_MTK_PPM_V3) += ppm_v3/
obj-$(CONFIG_MTK_SPM) += spm/
obj-$(CONFIG_MTK_SPM_V0) += spm/
obj-$(CONFIG_MTK_VIDEOCODEC_DRIVER) += videocodec/
obj-$(CONFIG_MTK_LEGACY_THERMAL) += thermal/
