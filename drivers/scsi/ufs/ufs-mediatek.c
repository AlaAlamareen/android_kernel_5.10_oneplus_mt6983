// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Authors:
 *	Stanley Chu <stanley.chu@mediatek.com>
 *	Peter Wang <peter.wang@mediatek.com>
 */

#include <linux/arm-smccc.h>
#include <linux/bitfield.h>
#include <linux/cpumask.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/regulator/consumer.h>
#include <linux/reset.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/tracepoint.h>
#include <linux/rtc.h>
/*google patch Random W/R performance improvement*/
#include <linux/irq.h>

#if IS_ENABLED(CONFIG_RPMB)
#include <asm/unaligned.h>
#include <linux/async.h>
#include <linux/rpmb.h>
#endif

#include <scsi/scsi.h>
#include <scsi/scsi_ioctl.h>
#include <scsi/scsi_cmnd.h>
#include <linux/async.h>

#include "mtk_blocktag.h"
#include "ufshcd.h"
#include "ufshcd-crypto.h"
#include "ufshcd-pltfrm.h"
#include "ufs_quirks.h"
#include "unipro.h"
#include "ufs-mediatek.h"
#include "ufs-mediatek-dbg.h"
#include "../sd.h"

#define ONE_DAY_SEC 86400
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
static int ufs_abort_aee_count;
#endif

#define CREATE_TRACE_POINTS
#include "ufs-mediatek-trace.h"
#undef CREATE_TRACE_POINTS

#define ufs_mtk_va09_pwr_ctrl(res, on) \
	ufs_mtk_smc(UFS_MTK_SIP_VA09_PWR_CTRL, res, on)

#define ufs_mtk_crypto_ctrl(res, enable) \
	ufs_mtk_smc(UFS_MTK_SIP_CRYPTO_CTRL, res, enable)

#define ufs_mtk_ref_clk_notify(on, res) \
	ufs_mtk_smc(UFS_MTK_SIP_REF_CLK_NOTIFICATION, res, on)

#define ufs_mtk_device_reset_ctrl(high, res) \
	ufs_mtk_smc(UFS_MTK_SIP_DEVICE_RESET, res, high)

#define ufs_mtk_host_pwr_ctrl(on, res) \
	ufs_mtk_smc(UFS_MTK_SIP_HOST_PWR_CTRL, res, on)

/*feature-devinfo-v001-1-begin*/
struct ufs_transmission_status_t ufs_transmission_status;
struct device_attribute ufs_transmission_status_attr;
/*feature-devinfo-v001-1-end*/
/*feature-flashaging806-v001-1-begin*/
struct unipro_signal_quality_ctrl signalCtrl;
/*feature-flashaging806-v001-1-end*/

#define ufs_mtk_get_vcc_info(res) \
	ufs_mtk_smc(UFS_MTK_SIP_GET_VCC_INFO, res)

#define ufs_mtk_device_pwr_ctrl(on, ufs_version, res) \
	ufs_mtk_smc(UFS_MTK_SIP_DEVICE_PWR_CTRL, res, on, ufs_version)

#define ufshcd_eh_in_progress(h) \
	((h)->eh_flags & UFSHCD_EH_IN_PROGRESS)

static struct ufs_dev_fix ufs_mtk_dev_fixups[] = {
	UFS_FIX(UFS_ANY_VENDOR, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_DELAY_BEFORE_LPM | UFS_DEVICE_QUIRK_DELAY_AFTER_LPM),
	UFS_FIX(UFS_VENDOR_SKHYNIX, "H9HQ21AFAMZDAR",
		UFS_DEVICE_QUIRK_SUPPORT_EXTENDED_FEATURES),
	UFS_FIX(UFS_VENDOR_SKHYNIX, "H9HQ15AFAMBDAR",
		UFS_DEVICE_QUIRK_DELAY_BEFORE_LPM | UFS_DEVICE_QUIRK_DELAY_AFTER_LPM),
	END_FIX
};

static const struct of_device_id ufs_mtk_of_match[] = {
	{ .compatible = "mediatek,mt8183-ufshci" },
	{},
};

static bool ufs_mtk_is_boost_crypt_enabled(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	return !!(host->caps & UFS_MTK_CAP_BOOST_CRYPT_ENGINE);
}

static bool ufs_mtk_is_va09_supported(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	return !!(host->caps & UFS_MTK_CAP_VA09_PWR_CTRL);
}

static bool ufs_mtk_is_broken_vcc(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	return !!(host->caps & UFS_MTK_CAP_BROKEN_VCC);
}

static bool ufs_mtk_is_delay_after_vcc_off(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	return !!(host->caps & UFS_MTK_CAP_DEALY_AFTER_VCC_OFF);
}

static bool ufs_mtk_is_force_vsx_lpm(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	return !!(host->caps & UFS_MTK_CAP_FORCE_VSx_LPM);
}

static void ufs_mtk_cfg_unipro_cg(struct ufs_hba *hba, bool enable)
{
	u32 tmp;

	if (enable) {
		ufshcd_dme_get(hba,
			       UIC_ARG_MIB(VS_SAVEPOWERCONTROL), &tmp);
		tmp = tmp |
		      (1 << RX_SYMBOL_CLK_GATE_EN) |
		      (1 << SYS_CLK_GATE_EN) |
		      (1 << TX_CLK_GATE_EN);
		ufshcd_dme_set(hba,
			       UIC_ARG_MIB(VS_SAVEPOWERCONTROL), tmp);

		ufshcd_dme_get(hba,
			       UIC_ARG_MIB(VS_DEBUGCLOCKENABLE), &tmp);
		tmp = tmp & ~(1 << TX_SYMBOL_CLK_REQ_FORCE);
		ufshcd_dme_set(hba,
			       UIC_ARG_MIB(VS_DEBUGCLOCKENABLE), tmp);
	} else {
		ufshcd_dme_get(hba,
			       UIC_ARG_MIB(VS_SAVEPOWERCONTROL), &tmp);
		tmp = tmp & ~((1 << RX_SYMBOL_CLK_GATE_EN) |
			      (1 << SYS_CLK_GATE_EN) |
			      (1 << TX_CLK_GATE_EN));
		ufshcd_dme_set(hba,
			       UIC_ARG_MIB(VS_SAVEPOWERCONTROL), tmp);

		ufshcd_dme_get(hba,
			       UIC_ARG_MIB(VS_DEBUGCLOCKENABLE), &tmp);
		tmp = tmp | (1 << TX_SYMBOL_CLK_REQ_FORCE);
		ufshcd_dme_set(hba,
			       UIC_ARG_MIB(VS_DEBUGCLOCKENABLE), tmp);
	}
}

static void ufs_mtk_crypto_enable(struct ufs_hba *hba)
{
	struct arm_smccc_res res;

	ufs_mtk_crypto_ctrl(res, 1);
	if (res.a0) {
		dev_info(hba->dev, "%s: crypto enable failed, err: %lu\n",
			 __func__, res.a0);
		hba->caps &= ~UFSHCD_CAP_CRYPTO;
	}
}

static void ufs_mtk_host_reset(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	reset_control_assert(host->hci_reset);
	reset_control_assert(host->crypto_reset);
	reset_control_assert(host->unipro_reset);

	usleep_range(100, 110);

	reset_control_deassert(host->unipro_reset);
	reset_control_deassert(host->crypto_reset);
	reset_control_deassert(host->hci_reset);
}

static void ufs_mtk_init_reset_control(struct ufs_hba *hba,
				       struct reset_control **rc,
				       char *str)
{
	*rc = devm_reset_control_get(hba->dev, str);
	if (IS_ERR(*rc)) {
		dev_info(hba->dev, "Failed to get reset control %s: %ld\n",
			 str, PTR_ERR(*rc));
		*rc = NULL;
	}
}

static void ufs_mtk_init_reset(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	ufs_mtk_init_reset_control(hba, &host->hci_reset,
				   "hci_rst");
	ufs_mtk_init_reset_control(hba, &host->unipro_reset,
				   "unipro_rst");
	ufs_mtk_init_reset_control(hba, &host->crypto_reset,
				   "crypto_rst");
}

static int ufs_mtk_hce_enable_notify(struct ufs_hba *hba,
				     enum ufs_notify_change_status status)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	unsigned long flags;

	if (status == PRE_CHANGE) {
		if (host->unipro_lpm) {
			hba->vps->hba_enable_delay_us = 0;
		} else {
			hba->vps->hba_enable_delay_us = 600;
			ufs_mtk_host_reset(hba);
		}

		if (hba->caps & UFSHCD_CAP_CRYPTO)
			ufs_mtk_crypto_enable(hba);

		if (host->caps & UFS_MTK_CAP_DISABLE_AH8) {
			spin_lock_irqsave(hba->host->host_lock, flags);
			ufshcd_writel(hba, 0,
				      REG_AUTO_HIBERNATE_IDLE_TIMER);
			spin_unlock_irqrestore(hba->host->host_lock,
					       flags);

			hba->capabilities &= ~MASK_AUTO_HIBERN8_SUPPORT;
			hba->ahit = 0;
		}

		ufshcd_writel(hba,
			(ufshcd_readl(hba, REG_UFS_XOUFS_CTRL) | 0x80),
			REG_UFS_XOUFS_CTRL);
	}

	return 0;
}

static int ufs_mtk_bind_mphy(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct device *dev = hba->dev;
	struct device_node *np = dev->of_node;
	int err = 0;

	host->mphy = devm_of_phy_get_by_index(dev, np, 0);

	if (host->mphy == ERR_PTR(-EPROBE_DEFER)) {
		/*
		 * UFS driver might be probed before the phy driver does.
		 * In that case we would like to return EPROBE_DEFER code.
		 */
		err = -EPROBE_DEFER;
		dev_info(dev,
			 "%s: required phy hasn't probed yet. err = %d\n",
			__func__, err);
	} else if (IS_ERR(host->mphy)) {
		err = PTR_ERR(host->mphy);
		if (err != -ENODEV) {
			dev_info(dev, "%s: PHY get failed %d\n", __func__,
				 err);
		}
	}

	if (err)
		host->mphy = NULL;
	/*
	 * Allow unbound mphy because not every platform needs specific
	 * mphy control.
	 */
	if (err == -ENODEV)
		err = 0;

	return err;
}

static int ufs_mtk_setup_ref_clk(struct ufs_hba *hba, bool on)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct arm_smccc_res res;
	ktime_t timeout, time_checked;
	u32 value;

	if (host->ref_clk_enabled == on)
		return 0;

	if (on) {
		ufs_mtk_ref_clk_notify(on, res);
		ufshcd_writel(hba, REFCLK_REQUEST, REG_UFS_REFCLK_CTRL);
	} else {
		ufshcd_delay_us(host->ref_clk_gating_wait_us, 10);
		ufshcd_writel(hba, REFCLK_RELEASE, REG_UFS_REFCLK_CTRL);
	}

	/* Wait for ack */
	timeout = ktime_add_us(ktime_get(), REFCLK_REQ_TIMEOUT_US);
	do {
		time_checked = ktime_get();
		value = ufshcd_readl(hba, REG_UFS_REFCLK_CTRL);

		/* Wait until ack bit equals to req bit */
		if (((value & REFCLK_ACK) >> 1) == (value & REFCLK_REQUEST))
			goto out;

		usleep_range(100, 200);
	} while (ktime_before(time_checked, timeout));

	dev_err(hba->dev, "missing ack of refclk req, reg: 0x%x\n", value);

	ufs_mtk_ref_clk_notify(host->ref_clk_enabled, res);

	return -ETIMEDOUT;

out:
	host->ref_clk_enabled = on;
	if (on)
		ufshcd_delay_us(host->ref_clk_ungating_wait_us, 10);
	else
		ufs_mtk_ref_clk_notify(on, res);

	return 0;
}

static void ufs_mtk_setup_ref_clk_wait_us(struct ufs_hba *hba,
					  u16 gating_us, u16 ungating_us)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	if (hba->dev_info.clk_gating_wait_us) {
		host->ref_clk_gating_wait_us =
			hba->dev_info.clk_gating_wait_us;
	} else {
		host->ref_clk_gating_wait_us = gating_us;
	}

	host->ref_clk_ungating_wait_us = ungating_us;
}

static void ufs_mtk_dbg_sel(struct ufs_hba *hba)
{
	static u32 hw_ver;

	if (!hw_ver)
		hw_ver = ufshcd_readl(hba, REG_UFS_MTK_HW_VER);

	if (((hw_ver >> 16) & 0xFF) >= 0x36) {
		ufshcd_writel(hba, 0x820820, REG_UFS_DEBUG_SEL);
		ufshcd_writel(hba, 0x0, REG_UFS_DEBUG_SEL_B0);
		ufshcd_writel(hba, 0x55555555, REG_UFS_DEBUG_SEL_B1);
		ufshcd_writel(hba, 0xaaaaaaaa, REG_UFS_DEBUG_SEL_B2);
		ufshcd_writel(hba, 0xffffffff, REG_UFS_DEBUG_SEL_B3);
	} else {
		ufshcd_writel(hba, 0x20, REG_UFS_DEBUG_SEL);
	}
}

static void ufs_mtk_wait_idle_state(struct ufs_hba *hba,
			    unsigned long retry_ms)
{
	u64 timeout, time_checked;
	u32 val, sm;
	bool wait_idle;

	timeout = sched_clock() + retry_ms * 1000000UL;


	/* wait a specific time after check base */
	udelay(10);
	wait_idle = false;

	do {
		time_checked = sched_clock();
		ufs_mtk_dbg_sel(hba);
		val = ufshcd_readl(hba, REG_UFS_PROBE);

		sm = val & 0x1f;

		/*
		 * if state is in H8 enter and H8 enter confirm
		 * wait until return to idle state.
		 */
		if ((sm >= VS_HIB_ENTER) && (sm <= VS_HIB_EXIT)) {
			wait_idle = true;
			udelay(50);
			continue;
		} else if (!wait_idle)
			break;

		if (wait_idle && (sm == VS_HCE_BASE))
			break;
	} while (time_checked < timeout);

	if (wait_idle && sm != VS_HCE_BASE)
		dev_info(hba->dev, "wait idle tmo: 0x%x\n", val);
}

static int ufs_mtk_wait_link_state(struct ufs_hba *hba, u32 state,
				   unsigned long max_wait_ms)
{
	ktime_t timeout, time_checked;
	u32 val;

	timeout = ktime_add_ms(ktime_get(), max_wait_ms);
	do {
		time_checked = ktime_get();
		ufs_mtk_dbg_sel(hba);
		val = ufshcd_readl(hba, REG_UFS_PROBE);
		val = val >> 28;

		if (val == state)
			return 0;

		/* Sleep for max. 200us */
		usleep_range(100, 200);
	} while (ktime_before(time_checked, timeout));

	if (val == state)
		return 0;

	return -ETIMEDOUT;
}

static void ufs_mtk_pm_qos(struct ufs_hba *hba, bool qos_en)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	if (host && host->pm_qos_init) {
		if (qos_en)
			cpu_latency_qos_update_request(
				&host->pm_qos_req, 0);
		else
			cpu_latency_qos_update_request(
				&host->pm_qos_req,
				PM_QOS_DEFAULT_VALUE);
	}
}

static int ufs_mtk_mphy_power_on(struct ufs_hba *hba, bool on)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct phy *mphy = host->mphy;
	struct arm_smccc_res res;
	int ret = 0;

	if (!mphy || !(on ^ host->mphy_powered_on))
		return 0;

	if (on) {
		if (ufs_mtk_is_va09_supported(hba)) {
			ret = regulator_enable(host->reg_va09);
			if (ret < 0)
				goto out;
			/* wait 200 us to stablize VA09 */
			usleep_range(200, 210);
			ufs_mtk_va09_pwr_ctrl(res, 1);
		}
		phy_power_on(mphy);
	} else {
		phy_power_off(mphy);
		if (ufs_mtk_is_va09_supported(hba)) {
			ufs_mtk_va09_pwr_ctrl(res, 0);
			ret = regulator_disable(host->reg_va09);
			if (ret < 0)
				goto out;
		}
	}
out:
	if (ret) {
		dev_info(hba->dev,
			 "failed to %s va09: %d\n",
			 on ? "enable" : "disable",
			 ret);
	} else {
		host->mphy_powered_on = on;
	}

	return ret;
}

static int ufs_mtk_get_host_clk(struct device *dev, const char *name,
				struct clk **clk_out)
{
	struct clk *clk;
	int err = 0;

	clk = devm_clk_get(dev, name);
	if (IS_ERR(clk))
		err = PTR_ERR(clk);
	else
		*clk_out = clk;

	return err;
}

static void ufs_mtk_boost_crypt(struct ufs_hba *hba, bool boost)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct ufs_mtk_crypt_cfg *cfg;
	struct regulator *reg;
	int volt, ret;

	if (!ufs_mtk_is_boost_crypt_enabled(hba))
		return;

	cfg = host->crypt;
	volt = cfg->vcore_volt;
	reg = cfg->reg_vcore;

	ret = clk_prepare_enable(cfg->clk_crypt_mux);
	if (ret) {
		dev_info(hba->dev, "clk_prepare_enable(): %d\n",
			 ret);
		return;
	}

	if (boost) {
		ret = regulator_set_voltage(reg, volt, INT_MAX);
		if (ret) {
			dev_info(hba->dev,
				 "failed to set vcore to %d\n", volt);
			goto out;
		}

		ret = clk_set_parent(cfg->clk_crypt_mux,
				     cfg->clk_crypt_perf);
		if (ret) {
			dev_info(hba->dev,
				 "failed to set clk_crypt_perf\n");
			regulator_set_voltage(reg, 0, INT_MAX);
			goto out;
		}
	} else {
		ret = clk_set_parent(cfg->clk_crypt_mux,
				     cfg->clk_crypt_lp);
		if (ret) {
			dev_info(hba->dev,
				 "failed to set clk_crypt_lp\n");
			goto out;
		}

		ret = regulator_set_voltage(reg, 0, INT_MAX);
		if (ret) {
			dev_info(hba->dev,
				 "failed to set vcore to MIN\n");
		}
	}
out:
	clk_disable_unprepare(cfg->clk_crypt_mux);
}

static int ufs_mtk_init_host_clk(struct ufs_hba *hba, const char *name,
				 struct clk **clk)
{
	int ret;

	ret = ufs_mtk_get_host_clk(hba->dev, name, clk);
	if (ret) {
		dev_info(hba->dev, "%s: failed to get %s: %d", __func__,
			 name, ret);
	}

	return ret;
}

static void ufs_mtk_init_boost_crypt(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct ufs_mtk_crypt_cfg *cfg;
	struct device *dev = hba->dev;
	struct regulator *reg;
	u32 volt;

	host->crypt = devm_kzalloc(dev, sizeof(*(host->crypt)),
				   GFP_KERNEL);
	if (!host->crypt)
		goto disable_caps;

	reg = devm_regulator_get_optional(dev, "dvfsrc-vcore");
	if (IS_ERR(reg)) {
		dev_info(dev, "failed to get dvfsrc-vcore: %ld",
			 PTR_ERR(reg));
		goto disable_caps;
	}

	if (of_property_read_u32(dev->of_node, "boost-crypt-vcore-min",
				 &volt)) {
		dev_info(dev, "failed to get boost-crypt-vcore-min");
		goto disable_caps;
	}

	cfg = host->crypt;
	if (ufs_mtk_init_host_clk(hba, "crypt_mux",
				  &cfg->clk_crypt_mux))
		goto disable_caps;

	if (ufs_mtk_init_host_clk(hba, "crypt_lp",
				  &cfg->clk_crypt_lp))
		goto disable_caps;

	if (ufs_mtk_init_host_clk(hba, "crypt_perf",
				  &cfg->clk_crypt_perf))
		goto disable_caps;

	cfg->reg_vcore = reg;
	cfg->vcore_volt = volt;
	host->caps |= UFS_MTK_CAP_BOOST_CRYPT_ENGINE;

disable_caps:
	return;
}

static void ufs_mtk_init_va09_pwr_ctrl(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	host->reg_va09 = regulator_get(hba->dev, "va09");
	if (IS_ERR(host->reg_va09))
		dev_info(hba->dev, "failed to get va09");
	else
		host->caps |= UFS_MTK_CAP_VA09_PWR_CTRL;
}

static void ufs_mtk_init_host_caps(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct device_node *np = hba->dev->of_node;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;

	if (of_property_read_bool(np, "mediatek,ufs-boost-crypt"))
		ufs_mtk_init_boost_crypt(hba);

	if (of_property_read_bool(np, "mediatek,ufs-support-va09"))
		ufs_mtk_init_va09_pwr_ctrl(hba);

	if (of_property_read_bool(np, "mediatek,ufs-disable-ah8"))
		host->caps |= UFS_MTK_CAP_DISABLE_AH8;

	if (of_property_read_bool(np, "mediatek,ufs-qos"))
		host->qos_enabled = host->qos_allowed = true;

	if (of_property_read_bool(np, "mediatek,ufs-broken-vcc"))
		host->caps |= UFS_MTK_CAP_BROKEN_VCC;

	if (of_property_read_bool(np, "mediatek,ufs-delay-after-vcc-off"))
		host->caps |= UFS_MTK_CAP_DEALY_AFTER_VCC_OFF;

	dev_info(hba->dev, "caps: 0x%x", host->caps);

	boot_node = of_parse_phandle(np, "bootmode", 0);

	if (!boot_node)
		dev_info(hba->dev, "failed to get bootmode phandle\n");
	else {
		tag = (struct tag_bootmode *)of_get_property(boot_node,
							"atag,boot", NULL);
		if (!tag)
			dev_info(hba->dev, "failed to get atag,boot\n");
		else if (tag->boottype == BOOTDEV_UFS)
			host->boot_device = true;
	}
}

/**
 * ufs_mtk_setup_clocks - enables/disable clocks
 * @hba: host controller instance
 * @on: If true, enable clocks else disable them.
 * @status: PRE_CHANGE or POST_CHANGE notify
 *
 * Returns 0 on success, non-zero on failure.
 */
static int ufs_mtk_setup_clocks(struct ufs_hba *hba, bool on,
				enum ufs_notify_change_status status)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	bool clk_pwr_off = false;
	int ret = 0;

	/*
	 * In case ufs_mtk_init() is not yet done, simply ignore.
	 * This ufs_mtk_setup_clocks() shall be called from
	 * ufs_mtk_init() after init is done.
	 */
	if (!host)
		return 0;

	if (!on && status == PRE_CHANGE) {
		if (ufshcd_is_link_off(hba)) {
			clk_pwr_off = true;
		} else if (ufshcd_is_link_hibern8(hba) ||
			 (!ufshcd_can_hibern8_during_gating(hba) &&
			 ufshcd_is_auto_hibern8_enabled(hba))) {
			/*
			 * Gate ref-clk and poweroff mphy if link state is in
			 * OFF or Hibern8 by either Auto-Hibern8 or
			 * ufshcd_link_state_transition().
			 */
			ret = ufs_mtk_wait_link_state(hba,
						      VS_LINK_HIBERN8,
						      15);
			if (!ret)
				clk_pwr_off = true;
		}

		if (clk_pwr_off) {
			ufs_mtk_pm_qos(hba, on);
			ufs_mtk_boost_crypt(hba, on);
			ufs_mtk_setup_ref_clk(hba, on);
			phy_power_off(host->mphy);
			if (host->qos_enabled)
				ufs_mtk_biolog_clk_gating(on);
		}
	} else if (on && status == POST_CHANGE) {
		phy_power_on(host->mphy);
		ufs_mtk_setup_ref_clk(hba, on);
		ufs_mtk_boost_crypt(hba, on);
		ufs_mtk_pm_qos(hba, on);
		if (host->qos_enabled)
			ufs_mtk_biolog_clk_gating(on);
	}

	return ret;
}

static bool ufs_mtk_is_data_cmd(struct scsi_cmnd *cmd)
{
	char cmd_op = cmd->cmnd[0];

	if (cmd_op == WRITE_10 || cmd_op == READ_10 ||
	    cmd_op == WRITE_16 || cmd_op == READ_16 ||
	    cmd_op == WRITE_6 || cmd_op == READ_6)
		return true;

	return false;
}

/*feature-devinfo-v001-2-begin*/
static void ufshcd_lrb_scsicmd_time_statistics(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	if (lrbp->cmd->cmnd[0] == WRITE_10 || lrbp->cmd->cmnd[0] == WRITE_16) {
		if (hba->pwr_info.gear_tx == 1) {
			ufs_transmission_status.gear_min_write_sec += blk_rq_sectors(lrbp->cmd->request);
			ufs_transmission_status.gear_min_write_us +=
				ktime_us_delta(lrbp->compl_time_stamp, lrbp->issue_time_stamp);
		}

		if (hba->pwr_info.gear_tx == 3 || hba->pwr_info.gear_tx == 4) {
			ufs_transmission_status.gear_max_write_sec += blk_rq_sectors(lrbp->cmd->request);
			ufs_transmission_status.gear_max_write_us +=
				ktime_us_delta(lrbp->compl_time_stamp, lrbp->issue_time_stamp);
		}
	} else if (lrbp->cmd->cmnd[0] == READ_10 || lrbp->cmd->cmnd[0] == READ_16) {
		if (hba->pwr_info.gear_rx == 1) {
			ufs_transmission_status.gear_min_read_sec += blk_rq_sectors(lrbp->cmd->request);
			ufs_transmission_status.gear_min_read_us +=
				ktime_us_delta(lrbp->compl_time_stamp, lrbp->issue_time_stamp);
		}

		if (hba->pwr_info.gear_rx == 3 || hba->pwr_info.gear_rx == 4) {
			ufs_transmission_status.gear_max_read_sec += blk_rq_sectors(lrbp->cmd->request);
			ufs_transmission_status.gear_max_read_us +=
				ktime_us_delta(lrbp->compl_time_stamp, lrbp->issue_time_stamp);
		}
	} else {
		if (hba->pwr_info.gear_rx == 1) {
			ufs_transmission_status.gear_min_other_sec += blk_rq_sectors(lrbp->cmd->request);
			ufs_transmission_status.gear_min_other_us += ktime_us_delta(lrbp->compl_time_stamp, lrbp->issue_time_stamp);
		}

		if (hba->pwr_info.gear_rx == 3 || hba->pwr_info.gear_rx == 4) {
			ufs_transmission_status.gear_max_other_sec += blk_rq_sectors(lrbp->cmd->request);
			ufs_transmission_status.gear_max_other_us += ktime_us_delta(lrbp->compl_time_stamp, lrbp->issue_time_stamp);
		}
	}

	return;
}

static void ufshcd_lrb_devcmd_time_statistics(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	if (hba->pwr_info.gear_tx == 1) {
		ufs_transmission_status.gear_min_dev_us +=
			ktime_us_delta(lrbp->compl_time_stamp, lrbp->issue_time_stamp);
	}

	if (hba->pwr_info.gear_tx == 3 || hba->pwr_info.gear_tx == 4) {
		ufs_transmission_status.gear_max_dev_us +=
			ktime_us_delta(lrbp->compl_time_stamp, lrbp->issue_time_stamp);
	}
}

int get_rtc_time(struct rtc_time *tm)
{
	struct rtc_device *rtc;
	int rc = 0;

	rtc = rtc_class_open("rtc0");
	if (rtc == NULL)
		return -1;

	rc = rtc_read_time(rtc, tm);
	if (rc)
		goto close_time;

	rc = rtc_valid_tm(tm);
	if (rc)
		goto close_time;

close_time:
	rtc_class_close(rtc);

	return rc;
}

void ufs_active_time_get(struct ufs_hba *hba)
{
	struct rtc_time tm;
	int rc = 0;
	ufs_transmission_status.active_count++;
	rc = get_rtc_time(&tm);
	if (rc != 0) {
		dev_err(hba->dev,"ufs_transmission_status: get_rtc_time failed\n");
		return;
	}
	ufs_transmission_status.resume_timing = (tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec);
	if (ufs_transmission_status.resume_timing < ufs_transmission_status.suspend_timing) {
		ufs_transmission_status.sleep_time += ((ufs_transmission_status.resume_timing
			+ ONE_DAY_SEC) - ufs_transmission_status.suspend_timing);
		return;
	}
	if(ufs_transmission_status.suspend_timing == 0)
		return;

	ufs_transmission_status.sleep_time += (ufs_transmission_status.resume_timing
		- ufs_transmission_status.suspend_timing);
	return;
}

void ufs_sleep_time_get(struct ufs_hba *hba)
{
	struct rtc_time tm;
	int rc = 0;
	ufs_transmission_status.sleep_count++;
	rc = get_rtc_time(&tm);
	if (rc != 0) {
		dev_err(hba->dev,"ufs_transmission_status: get_rtc_time failed\n");
		return;
	}
	ufs_transmission_status.suspend_timing = (tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec);
	if (ufs_transmission_status.suspend_timing < ufs_transmission_status.resume_timing) {
		ufs_transmission_status.active_time += ((ufs_transmission_status.suspend_timing
			+ ONE_DAY_SEC) - ufs_transmission_status.resume_timing);
		return;
	}
	if(ufs_transmission_status.resume_timing == 0)
		return;

	ufs_transmission_status.active_time += (ufs_transmission_status.suspend_timing
		- ufs_transmission_status.resume_timing);
	return;
}
/*feature-devinfo-v001-2-end*/

#define UFS_VEND_SAMSUNG  (1 << 0)

struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool init;
	unsigned int vend;
};

#if defined(CONFIG_UFSFEATURE)
static void ufs_mtk_trace_vh_prepare_command_vend_ss(void *data,
				struct ufs_hba *hba, struct request *rq,
				struct ufshcd_lrb *lrbp, int *err)
{
	ufsf_change_lun(ufs_mtk_get_ufsf(hba), lrbp);
	*err = ufsf_prep_fn(ufs_mtk_get_ufsf(hba), lrbp);
}

static void ufs_mtk_trace_vh_compl_command_vend_ss(struct ufs_hba *hba,
				struct ufshcd_lrb *lrbp,
				unsigned long out_reqs,
				unsigned long out_tasks)
{
	struct scsi_cmnd *cmd = lrbp->cmd;
	struct utp_upiu_header *header = &lrbp->ucd_rsp_ptr->header;
	int result = 0;
	int scsi_status;
	int ocs;
	struct ufsf_feature *ufsf = ufs_mtk_get_ufsf(hba);

	if (!cmd)
		return;

#if defined(CONFIG_UFSSHPB) && defined(CONFIG_HPB_DEBUG)
	trace_printk("%llu + %u cmd 0x%X comp tag[%d] out %lX\n",
		     (unsigned long long) blk_rq_pos(cmd->request),
		     (unsigned int) blk_rq_sectors(cmd->request),
		     cmd->cmnd[0], lrbp->task_tag, hba->outstanding_reqs);
#endif
	ocs = le32_to_cpu(header->dword_2) & MASK_OCS;

	if (ocs == OCS_SUCCESS) {
		result = be32_to_cpu(header->dword_0) >> 24;
		if (result == UPIU_TRANSACTION_RESPONSE) {
			scsi_status = be32_to_cpu(header->dword_1) &
				MASK_SCSI_STATUS;
			if (scsi_status == SAM_STAT_GOOD) {
				ufsf_hpb_noti_rb(ufsf, lrbp);
				if (ufsf_upiu_check_for_ccd(lrbp)) {
					ufsf_copy_sense_data(lrbp);
					/*
					 * this applies to "GOOD" case
					 * in scsi_decide_disposition()
					 * and will pass normally.
					 * if result is 0x00, sense will not
					 * be able to copy in sg_scsi_ioctl()
					 * Finally, ioctl tool in userspace will
					 * receive the error as 1.
					 */
					result |= GOOD_CCD;
				}
			}
		}
	}

/*feature-devinfo-v001-3-begin*/
	if (lrbp->cmd) {
		if (ufs_transmission_status.transmission_status_enable) {
			lrbp->compl_time_stamp = ktime_get();
			ufshcd_lrb_scsicmd_time_statistics(hba, lrbp);
		}
	} else if (lrbp->command_type == UTP_CMD_TYPE_DEV_MANAGE ||
			lrbp->command_type == UTP_CMD_TYPE_UFS_STORAGE) {
		if (ufs_transmission_status.transmission_status_enable) {
			ufshcd_lrb_devcmd_time_statistics(hba, lrbp);
		}
	}
/*feature-devinfo-v001-3-end*/
}

static void ufs_mtk_trace_vh_update_sdev_vend_ss(void *data, struct scsi_device *sdev)
{
	struct ufs_hba *hba = shost_priv(sdev->host);
	struct ufsf_feature *ufsf = ufs_mtk_get_ufsf(hba);

	if (hba->dev_info.wmanufacturerid == UFS_VENDOR_SAMSUNG)
		ufsf_slave_configure(ufsf, sdev);
}
#endif

static void ufs_mtk_trace_vh_send_command(void *data, struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct scsi_cmnd *cmd = lrbp->cmd;

	if (!cmd)
		return;

	if (lrbp && lrbp->cmd && lrbp->cmd->cmnd[0]) {
		struct request *rq = blk_mq_rq_from_pdu(lrbp->cmd);
		if (rq)
			rq->cmd_flags |= REQ_HIPRI;
	}
	if (ufs_mtk_is_data_cmd(cmd)) {
		ufs_mtk_biolog_send_command(lrbp->task_tag, cmd);
		ufs_mtk_biolog_check(1);
	}
}

static void ufs_mtk_trace_vh_compl_command(void *data, struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct scsi_cmnd *cmd = lrbp->cmd;
	int tag = lrbp->task_tag;
	unsigned long req_mask;
	unsigned long outstanding_reqs;
	unsigned long outstanding_tasks;
	unsigned long flags;
#if defined(CONFIG_UFSFEATURE)
	struct ufsf_feature *ufsf;
#endif

	if (!cmd)
		return;

	spin_lock_irqsave(hba->host->host_lock, flags);
	outstanding_reqs = hba->outstanding_reqs;
	outstanding_tasks = hba->outstanding_tasks;
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ufs_mtk_is_data_cmd(cmd)) {
		req_mask = outstanding_reqs & ~(1 << tag);
		ufs_mtk_biolog_transfer_req_compl(tag, req_mask);
		ufs_mtk_biolog_check(req_mask);
	}

#if defined(CONFIG_UFSFEATURE)
	ufsf = ufs_mtk_get_ufsf(hba);

	if (ufsf->hba)
		ufs_mtk_trace_vh_compl_command_vend_ss(hba, lrbp,
			outstanding_reqs, outstanding_tasks);
#endif
        /*feature-devinfo-v001-4-begin*/
	if (ufs_transmission_status.transmission_status_enable) {
		if(lrbp->cmd) {
			ufs_transmission_status.scsi_send_count++;
		} else {
			ufs_transmission_status.dev_cmd_count++;
		}
	}
        /*feature-devinfo-v001-4-end*/
}

static struct tracepoints_table interests[] = {
	{
		.name = "android_vh_ufs_send_command",
		.func = ufs_mtk_trace_vh_send_command
	},
	{
		.name = "android_vh_ufs_compl_command",
		.func = ufs_mtk_trace_vh_compl_command
	},
#if defined(CONFIG_UFSFEATURE)
	{
		.name = "android_vh_ufs_prepare_command",
		.func = ufs_mtk_trace_vh_prepare_command_vend_ss,
		.vend = UFS_VEND_SAMSUNG
	},
	{
		.name = "android_vh_ufs_update_sdev",
		.func = ufs_mtk_trace_vh_update_sdev_vend_ss,
		.vend = UFS_VEND_SAMSUNG
	},
#endif
};

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(interests) / sizeof(struct tracepoints_table); \
	i++)

static void ufs_mtk_lookup_tracepoints(struct tracepoint *tp,
				       void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(interests[i].name, tp->name) == 0)
			interests[i].tp = tp;
	}
}

static void ufs_mtk_uninstall_tracepoints(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (interests[i].init) {
			tracepoint_probe_unregister(interests[i].tp,
						    interests[i].func,
						    NULL);
		}
	}
}

static int ufs_mtk_install_tracepoints(struct ufs_hba *hba)
{
	int i;
	unsigned int vend;

	/* Install the tracepoints */
	for_each_kernel_tracepoint(ufs_mtk_lookup_tracepoints, NULL);

	FOR_EACH_INTEREST(i) {
		if (interests[i].tp == NULL) {
			dev_info(hba->dev, "Error: tracepoint %s not found\n",
				interests[i].name);
			continue;
		}

		vend = interests[i].vend;
		if (vend & UFS_VEND_SAMSUNG) {
			if (hba->dev_info.wmanufacturerid != UFS_VENDOR_SAMSUNG)
				continue;
		}

		tracepoint_probe_register(interests[i].tp,
					  interests[i].func,
					  NULL);
		interests[i].init = true;
	}

	return 0;
}

static void ufs_mtk_get_controller_version(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	int ret, ver = 0;

	if (host->hw_ver.major)
		return;

	/* Set default (minimum) version anyway */
	host->hw_ver.major = 2;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_LOCALVERINFO), &ver);
	if (!ret) {
		if (ver >= UFS_UNIPRO_VER_1_8)
			host->hw_ver.major = 3;
	}
}

static u32 ufs_mtk_get_ufs_hci_version(struct ufs_hba *hba)
{
	return hba->ufs_version;
}


#if IS_ENABLED(CONFIG_RPMB)

#define SEC_PROTOCOL_UFS  0xEC
#define SEC_SPECIFIC_UFS_RPMB 0x0001

#define SEC_PROTOCOL_CMD_SIZE 12
#define SEC_PROTOCOL_RETRIES 3
#define SEC_PROTOCOL_RETRIES_ON_RESET 10
#define SEC_PROTOCOL_TIMEOUT msecs_to_jiffies(30000)

int ufs_mtk_rpmb_security_out(struct scsi_device *sdev,
			 struct rpmb_frame *frames, u32 cnt)
{
	struct scsi_sense_hdr sshdr = {0};
	u32 trans_len = cnt * sizeof(struct rpmb_frame);
	int reset_retries = SEC_PROTOCOL_RETRIES_ON_RESET;
	int ret;
	u8 cmd[SEC_PROTOCOL_CMD_SIZE];

	memset(cmd, 0, SEC_PROTOCOL_CMD_SIZE);
	cmd[0] = SECURITY_PROTOCOL_OUT;
	cmd[1] = SEC_PROTOCOL_UFS;
	put_unaligned_be16(SEC_SPECIFIC_UFS_RPMB, cmd + 2);
	cmd[4] = 0;                              /* inc_512 bit 7 set to 0 */
	put_unaligned_be32(trans_len, cmd + 6);  /* transfer length */

retry:
	ret = scsi_execute_req(sdev, cmd, DMA_TO_DEVICE,
				     frames, trans_len, &sshdr,
				     SEC_PROTOCOL_TIMEOUT, SEC_PROTOCOL_RETRIES,
				     NULL);

	if (ret && scsi_sense_valid(&sshdr) &&
	    sshdr.sense_key == UNIT_ATTENTION)
		/*
		 * Device reset might occur several times,
		 * give it one more chance
		 */
		if (--reset_retries > 0)
			goto retry;

	if (ret)
		dev_err(&sdev->sdev_gendev, "%s: failed with err %0x\n",
			__func__, ret);

	if (driver_byte(ret) & DRIVER_SENSE)
		scsi_print_sense_hdr(sdev, "rpmb: security out", &sshdr);

	return ret;
}

int ufs_mtk_rpmb_security_in(struct scsi_device *sdev,
			struct rpmb_frame *frames, u32 cnt)
{
	struct scsi_sense_hdr sshdr = {0};
	u32 alloc_len = cnt * sizeof(struct rpmb_frame);
	int reset_retries = SEC_PROTOCOL_RETRIES_ON_RESET;
	int ret;
	u8 cmd[SEC_PROTOCOL_CMD_SIZE];

	memset(cmd, 0, SEC_PROTOCOL_CMD_SIZE);
	cmd[0] = SECURITY_PROTOCOL_IN;
	cmd[1] = SEC_PROTOCOL_UFS;
	put_unaligned_be16(SEC_SPECIFIC_UFS_RPMB, cmd + 2);
	cmd[4] = 0;                             /* inc_512 bit 7 set to 0 */
	put_unaligned_be32(alloc_len, cmd + 6); /* allocation length */

retry:
	ret = scsi_execute_req(sdev, cmd, DMA_FROM_DEVICE,
				     frames, alloc_len, &sshdr,
				     SEC_PROTOCOL_TIMEOUT, SEC_PROTOCOL_RETRIES,
				     NULL);

	if (ret && scsi_sense_valid(&sshdr) &&
	    sshdr.sense_key == UNIT_ATTENTION)
		/*
		 * Device reset might occur several times,
		 * give it one more chance
		 */
		if (--reset_retries > 0)
			goto retry;

	if (ret)
		dev_err(&sdev->sdev_gendev, "%s: failed with err %0x\n",
			__func__, ret);

	if (driver_byte(ret) & DRIVER_SENSE)
		scsi_print_sense_hdr(sdev, "rpmb: security in", &sshdr);

	return ret;
}

static int ufs_mtk_rpmb_cmd_seq(struct device *dev,
			       struct rpmb_cmd *cmds, u32 ncmds)
{
	unsigned long flags;
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host;
	struct scsi_device *sdev;
	struct rpmb_cmd *cmd;
	int i;
	int ret;

	host = ufshcd_get_variant(hba);

	spin_lock_irqsave(hba->host->host_lock, flags);
	sdev = hba->sdev_rpmb;
	if (sdev) {
		ret = scsi_device_get(sdev);
		if (!ret && !scsi_device_online(sdev)) {
			ret = -ENODEV;
			scsi_device_put(sdev);
		}
	} else {
		ret = -ENODEV;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	if (ret)
		return ret;

	/*
	 * Send all command one by one.
	 * Use rpmb lock to prevent other rpmb read/write threads cut in line.
	 * Use mutex not spin lock because in/out function might sleep.
	 */
	down(&host->rpmb_sem);
	/* Ensure device is resumed before RPMB operation */
	scsi_autopm_get_device(sdev);

	for (ret = 0, i = 0; i < ncmds && !ret; i++) {
		cmd = &cmds[i];
		if (cmd->flags & RPMB_F_WRITE)
			ret = ufs_mtk_rpmb_security_out(sdev, cmd->frames,
						       cmd->nframes);
		else
			ret = ufs_mtk_rpmb_security_in(sdev, cmd->frames,
						      cmd->nframes);
	}

	/* Allow device to be runtime suspended */
	scsi_autopm_put_device(sdev);
	up(&host->rpmb_sem);

	scsi_device_put(sdev);
	return ret;
}

static struct rpmb_ops ufs_mtk_rpmb_dev_ops = {
	.cmd_seq = ufs_mtk_rpmb_cmd_seq,
	.type = RPMB_TYPE_UFS,
};
static struct rpmb_dev *rawdev_ufs_rpmb;

/**
 * ufs_mtk_rpmb_ddd - add mtk rpmb cdev
 * @data: host controller instance (hba)
 *
 * Read max ufs device read/write rpmb size support and
 * set to reliable_wr_cnt for rpmb cdev read/write reference.
 *
 * Register raw cdve device in rawdev_ufs_rpmb
 */
static void ufs_mtk_rpmb_add(void *data, async_cookie_t cookie)
{
	int err;
	u8 *desc_buf;
	struct rpmb_dev *rdev;
	u8 rw_size;
	int retry = 10;
	struct ufs_mtk_host *host;
	struct ufs_hba *hba = (struct ufs_hba *)data;

	host = ufshcd_get_variant(hba);

	/* wait ufshcd_scsi_add_wlus add sdev_rpmb  */
	while (hba->sdev_rpmb == NULL) {
		if (retry) {
			retry--;
			msleep(1000);
		} else {
			dev_err(hba->dev,
				"scsi rpmb device cannot found\n");
			goto out;
		}
	}

	desc_buf = kmalloc(QUERY_DESC_MAX_SIZE, GFP_KERNEL);
	if (!desc_buf)
		goto out;

	err = ufshcd_read_desc_param(hba, QUERY_DESC_IDN_GEOMETRY, 0, 0,
				     desc_buf, QUERY_DESC_MAX_SIZE);
	if (err) {
		dev_warn(hba->dev, "%s: cannot get rpmb rw limit %d\n",
			 dev_name(hba->dev), err);
		/* fallback to singel frame write */
		rw_size = 1;
	} else {
		rw_size = desc_buf[GEOMETRY_DESC_PARAM_RPMB_RW_SIZE];
	}

	kfree(desc_buf);
	dev_info(hba->dev, "rpmb rw_size: %d\n", rw_size);

	ufs_mtk_rpmb_dev_ops.reliable_wr_cnt = rw_size;

	if (unlikely(scsi_device_get(hba->sdev_rpmb)))
		goto out;

	rdev = rpmb_dev_register(hba->dev, &ufs_mtk_rpmb_dev_ops);
	if (IS_ERR(rdev)) {
		dev_warn(hba->dev, "%s: cannot register to rpmb %ld\n",
			 dev_name(hba->dev), PTR_ERR(rdev));
		goto out_put_dev;
	}

	/*
	 * Preserve rpmb_dev to globals for connection of legacy
	 * rpmb ioctl solution.
	 */
	rawdev_ufs_rpmb = rdev;

	/*
	 * Initialize rpmb semaphore.
	 */
	sema_init(&host->rpmb_sem, 1);

out_put_dev:
	scsi_device_put(hba->sdev_rpmb);

out:
	return;
}

struct rpmb_dev *ufs_mtk_rpmb_get_raw_dev(void)
{
	return rawdev_ufs_rpmb;
}
EXPORT_SYMBOL_GPL(ufs_mtk_rpmb_get_raw_dev);
#endif
/*feature-memorymonitor-v001-1-begin*/
static int monitor_verify_command(unsigned char *cmd)
{
    if (cmd[0] != 0x3B && cmd[0] != 0x3C && cmd[0] != 0xC0)
        return false;

    return true;
}

/**
 * ufs_ioctl_monitor - special cmd for memory monitor
 * @hba: per-adapter instance
 * @buf_user: user space buffer for ioctl data
 * @return: 0 for success negative error code otherwise
 *
 */
int ufs_ioctl_monitor(struct scsi_device *dev, void __user *buf_user)
{
	struct scsi_disk *sdp = (struct scsi_disk *)dev_get_drvdata(&dev->sdev_gendev);
	struct request_queue *q = dev->request_queue;
	struct gendisk *disk = sdp->disk;
	struct request *rq;
	struct scsi_request *req;
	struct scsi_ioctl_command __user *sic = (struct scsi_ioctl_command __user *)buf_user;
	int err;
	unsigned int in_len, out_len, bytes, opcode, cmdlen;
	char *buffer = NULL;

	/*
	 * get in an out lengths, verify they don't exceed a page worth of data
	 */
	if (get_user(in_len, &sic->inlen))
		return -EFAULT;
	if (get_user(out_len, &sic->outlen))
		return -EFAULT;
	if (in_len > PAGE_SIZE || out_len > PAGE_SIZE)
		return -EINVAL;
	if (get_user(opcode, sic->data))
		return -EFAULT;

	bytes = max(in_len, out_len);
	if (bytes) {
		buffer = kzalloc(bytes, q->bounce_gfp | GFP_USER| __GFP_NOWARN);
		if (!buffer)
			return -ENOMEM;

	}

	rq = blk_get_request(q, in_len ? REQ_OP_SCSI_OUT : REQ_OP_SCSI_IN,
			__GFP_RECLAIM);
	if (IS_ERR(rq)) {
		err = PTR_ERR(rq);
		goto error_free_buffer;
	}
	req = scsi_req(rq);

	cmdlen = COMMAND_SIZE(opcode);
	if ((VENDOR_SPECIFIC_CDB == opcode) &&(0 == strncmp(dev->vendor, "SAMSUNG ", 8)))
		cmdlen = 16;

	/*
	 * get command and data to send to device, if any
	 */
	err = -EFAULT;
	req->cmd_len = cmdlen;
	if (copy_from_user(req->cmd, sic->data, cmdlen))
		goto error;

	if (in_len && copy_from_user(buffer, sic->data + cmdlen, in_len))
		goto error;

	if (!monitor_verify_command(req->cmd))
		goto error;

	/* default.  possible overriden later */
	req->retries = 5;

	if (bytes && blk_rq_map_kern(q, rq, buffer, bytes, __GFP_RECLAIM)) {
		err = DRIVER_ERROR << 24;
		goto error;
	}

	blk_execute_rq(q, disk, rq, 0);

#define OMAX_SB_LEN 16          /* For backward compatibility */
	err = req->result & 0xff;	/* only 8 bit SCSI status */
	if (err) {
		if (req->sense_len && req->sense) {
			bytes = (OMAX_SB_LEN > req->sense_len) ?
				req->sense_len : OMAX_SB_LEN;
			if (copy_to_user(sic->data, req->sense, bytes))
				err = -EFAULT;
		}
	} else {
		if (copy_to_user(sic->data, buffer, out_len))
			err = -EFAULT;
	}

error:
	blk_put_request(rq);

error_free_buffer:
	kfree(buffer);

	return err;
}
/*feature-memorymonitor-v001-1-end*/

/**
 * ufs_mtk_query_ioctl - perform user read queries
 * @hba: per-adapter instance
 * @lun: used for lun specific queries
 * @buffer: user space buffer for reading and submitting query data and params
 * @return: 0 for success negative error code otherwise
 *
 * Expected/Submitted buffer structure is struct ufs_ioctl_query_data.
 * It will read the opcode, idn and buf_length parameters, and, put the
 * response in the buffer field while updating the used size in buf_length.
 */
static int
ufs_mtk_query_ioctl(struct ufs_hba *hba, u8 lun, void __user *buffer)
{
	struct ufs_ioctl_query_data *ioctl_data;
	int err = 0;
	int length = 0;
	void *data_ptr;
	bool flag;
	u32 att;
	u8 index = 0;
	u8 *desc = NULL;

	ioctl_data = kzalloc(sizeof(*ioctl_data), GFP_KERNEL);
	if (!ioctl_data) {
		err = -ENOMEM;
		goto out;
	}

	/* extract params from user buffer */
	err = copy_from_user(ioctl_data, buffer,
			     sizeof(struct ufs_ioctl_query_data));
	if (err) {
		dev_err(hba->dev,
			"%s: Failed copying buffer from user, err %d\n",
			__func__, err);
		goto out_release_mem;
	}

#if defined(CONFIG_UFSFEATURE)
	if (ufsf_check_query(ioctl_data->opcode)) {
		err = ufsf_query_ioctl(ufs_mtk_get_ufsf(hba), lun, buffer,
				       ioctl_data, UFSFEATURE_SELECTOR);
		goto out_release_mem;
	}
#endif

	/* verify legal parameters & send query */
	switch (ioctl_data->opcode) {
	case UPIU_QUERY_OPCODE_READ_DESC:
		switch (ioctl_data->idn) {
		case QUERY_DESC_IDN_DEVICE:
		case QUERY_DESC_IDN_CONFIGURATION:
		case QUERY_DESC_IDN_INTERCONNECT:
		case QUERY_DESC_IDN_GEOMETRY:
		case QUERY_DESC_IDN_POWER:
                /*feature-memorymonitor-v001-2-begin*/
		case QUERY_DESC_IDN_HEALTH:
                /*feature-memorymonitor-v001-2-end*/
			index = 0;
			break;
		case QUERY_DESC_IDN_UNIT:
			if (!ufs_is_valid_unit_desc_lun(&hba->dev_info, lun, 0)) {
				dev_err(hba->dev,
					"%s: No unit descriptor for lun 0x%x\n",
					__func__, lun);
				err = -EINVAL;
				goto out_release_mem;
			}
			index = lun;
			break;
		default:
			goto out_einval;
		}
		length = min_t(int, QUERY_DESC_MAX_SIZE,
			       ioctl_data->buf_size);
		desc = kzalloc(length, GFP_KERNEL);
		if (!desc) {
			dev_err(hba->dev, "%s: Failed allocating %d bytes\n",
				__func__, length);
			err = -ENOMEM;
			goto out_release_mem;
		}
		err = ufshcd_query_descriptor_retry(hba, ioctl_data->opcode,
						    ioctl_data->idn, index, 0,
						    desc, &length);
		break;
	case UPIU_QUERY_OPCODE_READ_ATTR:
		switch (ioctl_data->idn) {
		case QUERY_ATTR_IDN_BOOT_LU_EN:
		case QUERY_ATTR_IDN_POWER_MODE:
		case QUERY_ATTR_IDN_ACTIVE_ICC_LVL:
		case QUERY_ATTR_IDN_OOO_DATA_EN:
		case QUERY_ATTR_IDN_BKOPS_STATUS:
		case QUERY_ATTR_IDN_PURGE_STATUS:
		case QUERY_ATTR_IDN_MAX_DATA_IN:
		case QUERY_ATTR_IDN_MAX_DATA_OUT:
		case QUERY_ATTR_IDN_REF_CLK_FREQ:
		case QUERY_ATTR_IDN_CONF_DESC_LOCK:
		case QUERY_ATTR_IDN_MAX_NUM_OF_RTT:
		case QUERY_ATTR_IDN_EE_CONTROL:
		case QUERY_ATTR_IDN_EE_STATUS:
		case QUERY_ATTR_IDN_SECONDS_PASSED:
			index = 0;
			break;
		case QUERY_ATTR_IDN_DYN_CAP_NEEDED:
		case QUERY_ATTR_IDN_CORR_PRG_BLK_NUM:
			index = lun;
			break;
		default:
			goto out_einval;
		}
		err = ufshcd_query_attr(hba, ioctl_data->opcode,
					ioctl_data->idn, index, 0, &att);
		break;

	case UPIU_QUERY_OPCODE_WRITE_ATTR:
		err = copy_from_user(&att,
				     buffer +
				     sizeof(struct ufs_ioctl_query_data),
				     sizeof(u32));
		if (err) {
			dev_err(hba->dev,
				"%s: Failed copying buffer from user, err %d\n",
				__func__, err);
			goto out_release_mem;
		}

		switch (ioctl_data->idn) {
		case QUERY_ATTR_IDN_BOOT_LU_EN:
			index = 0;
			if (!att) {
				dev_err(hba->dev,
					"%s: Illegal ufs query ioctl data, opcode 0x%x, idn 0x%x, att 0x%x\n",
					__func__, ioctl_data->opcode,
					(unsigned int)ioctl_data->idn, att);
				err = -EINVAL;
				goto out_release_mem;
			}
			break;
		default:
			goto out_einval;
		}
		err = ufshcd_query_attr(hba, ioctl_data->opcode,
					ioctl_data->idn, index, 0, &att);
		break;

	case UPIU_QUERY_OPCODE_READ_FLAG:
		switch (ioctl_data->idn) {
		case QUERY_FLAG_IDN_FDEVICEINIT:
		case QUERY_FLAG_IDN_PERMANENT_WPE:
		case QUERY_FLAG_IDN_PWR_ON_WPE:
		case QUERY_FLAG_IDN_BKOPS_EN:
		case QUERY_FLAG_IDN_PURGE_ENABLE:
		case QUERY_FLAG_IDN_FPHYRESOURCEREMOVAL:
		case QUERY_FLAG_IDN_BUSY_RTC:
			break;
		default:
			goto out_einval;
		}
		err = ufshcd_query_flag(hba, ioctl_data->opcode,
					ioctl_data->idn, 0, &flag);
		break;
	default:
		goto out_einval;
	}

	if (err) {
		dev_err(hba->dev, "%s: Query for idn %d failed\n", __func__,
			ioctl_data->idn);
		goto out_release_mem;
	}

	/*
	 * copy response data
	 * As we might end up reading less data than what is specified in
	 * "ioctl_data->buf_size". So we are updating "ioctl_data->
	 * buf_size" to what exactly we have read.
	 */
	switch (ioctl_data->opcode) {
	case UPIU_QUERY_OPCODE_READ_DESC:
		ioctl_data->buf_size = min_t(int, ioctl_data->buf_size, length);
		data_ptr = desc;
		break;
	case UPIU_QUERY_OPCODE_READ_ATTR:
		ioctl_data->buf_size = sizeof(u32);
		data_ptr = &att;
		break;
	case UPIU_QUERY_OPCODE_READ_FLAG:
		ioctl_data->buf_size = 1;
		data_ptr = &flag;
		break;
	case UPIU_QUERY_OPCODE_WRITE_ATTR:
		goto out_release_mem;
	default:
		goto out_einval;
	}

	/* copy to user */
	err = copy_to_user(buffer, ioctl_data,
			   sizeof(struct ufs_ioctl_query_data));
	if (err)
		dev_err(hba->dev, "%s: Failed copying back to user.\n",
			__func__);
	err = copy_to_user(buffer + sizeof(struct ufs_ioctl_query_data),
			   data_ptr, ioctl_data->buf_size);
	if (err)
		dev_err(hba->dev, "%s: err %d copying back to user.\n",
			__func__, err);
	goto out_release_mem;

out_einval:
	dev_err(hba->dev,
		"%s: illegal ufs query ioctl data, opcode 0x%x, idn 0x%x\n",
		__func__, ioctl_data->opcode, (unsigned int)ioctl_data->idn);
	err = -EINVAL;
out_release_mem:
	kfree(ioctl_data);
	kfree(desc);
out:
	return err;
}

/**
 * ufs_mtk_ioctl - ufs ioctl callback registered in scsi_host
 * @dev: scsi device required for per LUN queries
 * @cmd: command opcode
 * @buffer: user space buffer for transferring data
 *
 * Supported commands:
 * UFS_IOCTL_QUERY
 */
static int
ufs_mtk_ioctl(struct scsi_device *dev, unsigned int cmd, void __user *buffer)
{
	struct ufs_hba *hba = shost_priv(dev->host);
	int err = 0;

	BUG_ON(!hba);
	if (!buffer) {
		dev_err(hba->dev, "%s: User buffer is NULL!\n", __func__);
		return -EINVAL;
	}

	switch (cmd) {
	case UFS_IOCTL_QUERY:
		pm_runtime_get_sync(hba->dev);
		err = ufs_mtk_query_ioctl(hba,
					  ufshcd_scsi_to_upiu_lun(dev->lun),
					  buffer);
		pm_runtime_put_sync(hba->dev);
		break;
        /*feature-memorymonitor-v001-3-begin*/
	case UFS_IOCTL_MONITOR:
		pm_runtime_get_sync(hba->dev);
		err = ufs_ioctl_monitor(dev, buffer);
		pm_runtime_put_sync(hba->dev);
		break;
        /*feature-memorymonitor-v001-3-end*/
	default:
		err = -ENOIOCTLCMD;
		dev_dbg(hba->dev, "%s: Unsupported ioctl cmd %d\n", __func__,
			cmd);
		break;
	}

	return err;
}

/*feature-devinfo-v001-5-begin*/
static ssize_t ufshcd_transmission_status_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
					"transmission_status_enable:%u\n"
					"gear_min_write_sec:%llu\n"
					"gear_max_write_sec:%llu\n"
					"gear_min_read_sec:%llu\n"
					"gear_max_read_sec:%llu\n"
					"gear_min_write_us:%llu\n"
					"gear_max_write_us:%llu\n"
					"gear_min_read_us:%llu\n"
					"gear_max_read_us:%llu\n"
					"gear_min_dev_us:%llu\n"
					"gear_max_dev_us:%llu\n"
					"gear_min_other_sec:%llu\n"
					"gear_max_other_sec:%llu\n"
					"gear_min_other_us:%llu\n"
					"gear_max_other_us:%llu\n"
					"scsi_send_count:%llu\n"
					"dev_cmd_count:%llu\n"
					"active_count:%llu\n"
					"active_time:%llu\n"
					"sleep_count:%llu\n"
					"sleep_time:%llu\n"
					"powerdown_count:%llu\n"
					"powerdown_time:%llu\n"
					"power_total_count:%llu\n"
					"current_pwr_mode:%u\n",
					ufs_transmission_status.transmission_status_enable,
					ufs_transmission_status.gear_min_write_sec,
					ufs_transmission_status.gear_max_write_sec,
					ufs_transmission_status.gear_min_read_sec,
					ufs_transmission_status.gear_max_read_sec,
					ufs_transmission_status.gear_min_write_us,
					ufs_transmission_status.gear_max_write_us,
					ufs_transmission_status.gear_min_read_us,
					ufs_transmission_status.gear_max_read_us,
					ufs_transmission_status.gear_min_dev_us,
					ufs_transmission_status.gear_max_dev_us,
					ufs_transmission_status.gear_min_other_sec,
					ufs_transmission_status.gear_max_other_sec,
					ufs_transmission_status.gear_min_other_us,
					ufs_transmission_status.gear_max_other_us,
					ufs_transmission_status.scsi_send_count,
					ufs_transmission_status.dev_cmd_count,
					ufs_transmission_status.active_count,
					ufs_transmission_status.active_time,
					ufs_transmission_status.sleep_count,
					ufs_transmission_status.sleep_time,
					ufs_transmission_status.powerdown_count,
					ufs_transmission_status.powerdown_time,
					ufs_transmission_status.power_total_count,
					ufs_transmission_status.current_pwr_mode);
}

static ssize_t ufshcd_transmission_status_data_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 value;

	if (kstrtou32(buf, 0, &value))
		return -EINVAL;

	value = !!value;

	if (value) {
		ufs_transmission_status.transmission_status_enable = 1;
	} else {
		ufs_transmission_status.transmission_status_enable = 0;
		memset(&ufs_transmission_status, 0, sizeof(struct ufs_transmission_status_t));
	}

	return count;
}

static void ufshcd_transmission_status_init_sysfs(struct ufs_hba *hba)
{
	printk("tianwen: ufshcd_transmission_status_init_sysfs start\n");
	ufs_transmission_status_attr.show = ufshcd_transmission_status_data_show;
	ufs_transmission_status_attr.store = ufshcd_transmission_status_data_store;
	sysfs_attr_init(&ufs_transmission_status_attr.attr);
	ufs_transmission_status_attr.attr.name = "ufs_transmission_status";
	ufs_transmission_status_attr.attr.mode = 0644;
	if (device_create_file(hba->dev, &ufs_transmission_status_attr))
		dev_err(hba->dev, "Failed to create sysfs for ufs_transmission_status_attr\n");

	/*init the struct ufs_transmission_status*/
	memset(&ufs_transmission_status, 0, sizeof(struct ufs_transmission_status_t));
	ufs_transmission_status.transmission_status_enable = 1;
}
/*feature-devinfo-v001-5-end*/

/* Same with ufshcd_populate_vreg, should EXPORT for upstream */
#define MAX_PROP_SIZE 32
static int ufs_mtk_populate_vreg(struct device *dev, const char *name,
		struct ufs_vreg **out_vreg)
{
	int ret = 0;
	char prop_name[MAX_PROP_SIZE];
	struct ufs_vreg *vreg = NULL;
	struct device_node *np = dev->of_node;

	if (!np)
		goto out;

	snprintf(prop_name, MAX_PROP_SIZE, "%s-supply", name);
	if (!of_parse_phandle(np, prop_name, 0)) {
		dev_info(dev, "%s: Unable to find %s regulator, assuming enabled\n",
				__func__, prop_name);
		goto out;
	}

	vreg = devm_kzalloc(dev, sizeof(*vreg), GFP_KERNEL);
	if (!vreg)
		return -ENOMEM;

	vreg->name = kstrdup(name, GFP_KERNEL);

	snprintf(prop_name, MAX_PROP_SIZE, "%s-max-microamp", name);
	if (of_property_read_u32(np, prop_name, &vreg->max_uA)) {
		dev_info(dev, "%s: unable to find %s\n", __func__, prop_name);
		vreg->max_uA = 0;
	}
out:
	if (!ret)
		*out_vreg = vreg;
	return ret;
}

/* Same with ufshcd_get_vreg, should EXPORT for upstream */
static int ufs_mtk_get_vreg(struct device *dev, struct ufs_vreg *vreg)
{
	int ret = 0;

	if (!vreg)
		goto out;

	vreg->reg = devm_regulator_get(dev, vreg->name);
	if (IS_ERR(vreg->reg)) {
		ret = PTR_ERR(vreg->reg);
		dev_err(dev, "%s: %s get failed, err=%d\n",
				__func__, vreg->name, ret);
	}
out:
	return ret;
}

static void ufs_mtk_ctrl_dev_pwr(struct ufs_hba *hba, bool vcc_on, bool is_init)
{
	struct arm_smccc_res res;
	u64 ufs_version;

	/* prevent entering lpm when device is still active */
	if (!vcc_on && ufshcd_is_ufs_dev_active(hba))
		return;

	/* Transfer the ufs version to tf-a */
	ufs_version = (u64)hba->dev_info.wspecversion;

	/*
	 * If VCC kept always-on, we do not use smc call to avoid
	 * non-essential time consumption.
	 *
	 * We don't need to control VS buck (the upper layer of VCCQ/VCCQ2)
	 * to enter LPM, because UFS device may be active when VCC
	 * is always-on. We also introduce UFS_MTK_CAP_FORCE_VSx_LPM to
	 * allow overriding such protection to save power.
	 */
	if (ufs_mtk_is_force_vsx_lpm(hba) || !ufs_mtk_is_broken_vcc(hba) || is_init)
		ufs_mtk_device_pwr_ctrl(vcc_on, ufs_version, res);
}
/*feature-flashaging806-v001-2-begin*/
//#ifdef OPLUS_UFS_SIGNAL_QUALITY
void recordUniproErr(
	struct unipro_signal_quality_ctrl *signalCtrl,
	u32 reg,
	enum ufs_event_type type
) {
	unsigned long err_bits;
	int ec;
	struct signal_quality *rec = &signalCtrl->record;
	switch (type)
	{
	case UFS_EVT_FATAL_ERR:
		if (DEVICE_FATAL_ERROR & reg)
			rec->ufs_device_err_cnt++;
		if (CONTROLLER_FATAL_ERROR & reg)
			rec->ufs_host_err_cnt++;
		if (SYSTEM_BUS_FATAL_ERROR & reg)
			rec->ufs_bus_err_cnt++;
		if (CRYPTO_ENGINE_FATAL_ERROR & reg)
			rec->ufs_crypto_err_cnt++;
		break;
	case UFS_EVT_LINK_STARTUP_FAIL:
		if (UIC_LINK_LOST & reg)
			rec->ufs_link_lost_cnt++;
		break;
	case UFS_EVT_PA_ERR:
		err_bits = reg & UIC_PHY_ADAPTER_LAYER_ERROR_CODE_MASK;
		for_each_set_bit(ec, &err_bits, UNIPRO_PA_ERR_MAX) {
			rec->unipro_PA_err_total_cnt++;
			rec->unipro_PA_err_cnt[ec]++;
		}
		break;
	case UFS_EVT_DL_ERR:
		err_bits = reg & UIC_DATA_LINK_LAYER_ERROR_CODE_MASK;
		for_each_set_bit(ec, &err_bits, UNIPRO_DL_ERR_MAX) {
			rec->unipro_DL_err_total_cnt++;
			rec->unipro_DL_err_cnt[ec]++;
		}
		break;
	case UFS_EVT_NL_ERR:
		err_bits = reg & UIC_NETWORK_LAYER_ERROR_CODE_MASK;
		for_each_set_bit(ec, &err_bits, UNIPRO_NL_ERR_MAX) {
			rec->unipro_NL_err_total_cnt++;
			rec->unipro_NL_err_cnt[ec]++;
		}
		break;
	case UFS_EVT_TL_ERR:
		err_bits = reg & UIC_TRANSPORT_LAYER_ERROR_CODE_MASK;
		for_each_set_bit(ec, &err_bits, UNIPRO_TL_ERR_MAX) {
			rec->unipro_TL_err_total_cnt++;
			rec->unipro_TL_err_cnt[ec]++;
		}
		break;
	case UFS_EVT_DME_ERR:
		err_bits = reg & UIC_DME_ERROR_CODE_MASK;
		for_each_set_bit(ec, &err_bits, UNIPRO_DME_ERR_MAX) {
			rec->unipro_DME_err_total_cnt++;
			rec->unipro_DME_err_cnt[ec]++;
		}
		break;
	default:
		break;
	}
}

#define SEQ_EASY_PRINT(x)   seq_printf(s, #x"\t%d\n", signalCtrl->record.x)
#define SEQ_PA_PRINT(x)     \
	seq_printf(s, #x"\t%d\n", signalCtrl->record.unipro_PA_err_cnt[x])
#define SEQ_DL_PRINT(x)     \
	seq_printf(s, #x"\t%d\n", signalCtrl->record.unipro_DL_err_cnt[x])
#define SEQ_NL_PRINT(x)     \
	seq_printf(s, #x"\t%d\n", signalCtrl->record.unipro_NL_err_cnt[x])
#define SEQ_TL_PRINT(x)     \
	seq_printf(s, #x"\t%d\n", signalCtrl->record.unipro_TL_err_cnt[x])
#define SEQ_DME_PRINT(x)    \
	seq_printf(s, #x"\t%d\n", signalCtrl->record.unipro_DME_err_cnt[x])
#define SEQ_GEAR_PRINT(x)  \
	seq_printf(s, #x"\t%d\n", signalCtrl->record.gear_err_cnt[x])

static int record_read_func(struct seq_file *s, void *v)
{
	struct unipro_signal_quality_ctrl *signalCtrl =
		(struct unipro_signal_quality_ctrl *)(s->private);
	if (!signalCtrl)
		return -EINVAL;
	SEQ_EASY_PRINT(ufs_device_err_cnt);
	SEQ_EASY_PRINT(ufs_host_err_cnt);
	SEQ_EASY_PRINT(ufs_bus_err_cnt);
	SEQ_EASY_PRINT(ufs_crypto_err_cnt);
	SEQ_EASY_PRINT(ufs_link_lost_cnt);
	SEQ_EASY_PRINT(unipro_PA_err_total_cnt);
	SEQ_PA_PRINT(UNIPRO_PA_LANE0_ERR_CNT);
	SEQ_PA_PRINT(UNIPRO_PA_LANE1_ERR_CNT);
	SEQ_PA_PRINT(UNIPRO_PA_LANE2_ERR_CNT);
	SEQ_PA_PRINT(UNIPRO_PA_LANE3_ERR_CNT);
	SEQ_PA_PRINT(UNIPRO_PA_LINE_RESET);
	SEQ_EASY_PRINT(unipro_DL_err_total_cnt);
	SEQ_DL_PRINT(UNIPRO_DL_NAC_RECEIVED);
	SEQ_DL_PRINT(UNIPRO_DL_TCX_REPLAY_TIMER_EXPIRED);
	SEQ_DL_PRINT(UNIPRO_DL_AFCX_REQUEST_TIMER_EXPIRED);
	SEQ_DL_PRINT(UNIPRO_DL_FCX_PROTECTION_TIMER_EXPIRED);
	SEQ_DL_PRINT(UNIPRO_DL_CRC_ERROR);
	SEQ_DL_PRINT(UNIPRO_DL_RX_BUFFER_OVERFLOW);
	SEQ_DL_PRINT(UNIPRO_DL_MAX_FRAME_LENGTH_EXCEEDED);
	SEQ_DL_PRINT(UNIPRO_DL_WRONG_SEQUENCE_NUMBER);
	SEQ_DL_PRINT(UNIPRO_DL_AFC_FRAME_SYNTAX_ERROR);
	SEQ_DL_PRINT(UNIPRO_DL_NAC_FRAME_SYNTAX_ERROR);
	SEQ_DL_PRINT(UNIPRO_DL_EOF_SYNTAX_ERROR);
	SEQ_DL_PRINT(UNIPRO_DL_FRAME_SYNTAX_ERROR);
	SEQ_DL_PRINT(UNIPRO_DL_BAD_CTRL_SYMBOL_TYPE);
	SEQ_DL_PRINT(UNIPRO_DL_PA_INIT_ERROR);
	SEQ_DL_PRINT(UNIPRO_DL_PA_ERROR_IND_RECEIVED);
	SEQ_DL_PRINT(UNIPRO_DL_PA_INIT);
	SEQ_EASY_PRINT(unipro_NL_err_total_cnt);
	SEQ_NL_PRINT(UNIPRO_NL_UNSUPPORTED_HEADER_TYPE);
	SEQ_NL_PRINT(UNIPRO_NL_BAD_DEVICEID_ENC);
	SEQ_NL_PRINT(UNIPRO_NL_LHDR_TRAP_PACKET_DROPPING);
	SEQ_EASY_PRINT(unipro_TL_err_total_cnt);
	SEQ_TL_PRINT(UNIPRO_TL_UNSUPPORTED_HEADER_TYPE);
	SEQ_TL_PRINT(UNIPRO_TL_UNKNOWN_CPORTID);
	SEQ_TL_PRINT(UNIPRO_TL_NO_CONNECTION_RX);
	SEQ_TL_PRINT(UNIPRO_TL_CONTROLLED_SEGMENT_DROPPING);
	SEQ_TL_PRINT(UNIPRO_TL_BAD_TC);
	SEQ_TL_PRINT(UNIPRO_TL_E2E_CREDIT_OVERFLOW);
	SEQ_TL_PRINT(UNIPRO_TL_SAFETY_VALVE_DROPPING);
	SEQ_EASY_PRINT(unipro_DME_err_total_cnt);
	SEQ_DME_PRINT(UNIPRO_DME_GENERIC);
	SEQ_DME_PRINT(UNIPRO_DME_TX_QOS);
	SEQ_DME_PRINT(UNIPRO_DME_RX_QOS);
	SEQ_DME_PRINT(UNIPRO_DME_PA_INIT_QOS);
	SEQ_GEAR_PRINT(UFS_HS_G1);
	SEQ_GEAR_PRINT(UFS_HS_G2);
	SEQ_GEAR_PRINT(UFS_HS_G3);
	SEQ_GEAR_PRINT(UFS_HS_G4);
	return 0;
}

static int record_open(struct inode *inode, struct file *file)
{
	return single_open(file, record_read_func, PDE_DATA(inode));
}

static const struct proc_ops record_fops = {
	.proc_open = record_open,
	.proc_read = seq_read,
	.proc_release = single_release,
};

#define SEQ_UPLOAD_PRINT(x) \
	seq_printf(s, #x": %d\n", signalCtrl->record.x \
		-signalCtrl->record_upload.x);\
	signalCtrl->record_upload.x = signalCtrl->record.x;

#define SEQ_PA_UPLOAD_PRINT(x) \
	seq_printf(s, #x": %d\n", signalCtrl->record.unipro_PA_err_cnt[x] \
		-signalCtrl->record_upload.unipro_PA_err_cnt[x]);\
	signalCtrl->record_upload.unipro_PA_err_cnt[x] = signalCtrl->record.unipro_PA_err_cnt[x];

#define SEQ_DL_UPLOAD_PRINT(x) \
		seq_printf(s, #x": %d\n", signalCtrl->record.unipro_DL_err_cnt[x] \
			-signalCtrl->record_upload.unipro_DL_err_cnt[x]);\
		signalCtrl->record_upload.unipro_DL_err_cnt[x] = signalCtrl->record.unipro_DL_err_cnt[x];

#define SEQ_DL_UPLOAD_PRINT(x) \
			seq_printf(s, #x": %d\n", signalCtrl->record.unipro_DL_err_cnt[x] \
				-signalCtrl->record_upload.unipro_DL_err_cnt[x]);\
			signalCtrl->record_upload.unipro_DL_err_cnt[x] = signalCtrl->record.unipro_DL_err_cnt[x];

#define SEQ_NL_UPLOAD_PRINT(x) \
				seq_printf(s, #x": %d\n", signalCtrl->record.unipro_NL_err_cnt[x] \
					-signalCtrl->record_upload.unipro_NL_err_cnt[x]);\
				signalCtrl->record_upload.unipro_NL_err_cnt[x] = signalCtrl->record.unipro_NL_err_cnt[x];

#define SEQ_TL_UPLOAD_PRINT(x) \
					seq_printf(s, #x": %d\n", signalCtrl->record.unipro_TL_err_cnt[x] \
						-signalCtrl->record_upload.unipro_TL_err_cnt[x]);\
					signalCtrl->record_upload.unipro_TL_err_cnt[x] = signalCtrl->record.unipro_TL_err_cnt[x];

#define SEQ_DME_UPLOAD_PRINT(x) \
						seq_printf(s, #x": %d\n", signalCtrl->record.unipro_DME_err_cnt[x] \
							-signalCtrl->record_upload.unipro_DME_err_cnt[x]);\
						signalCtrl->record_upload.unipro_DME_err_cnt[x] = signalCtrl->record.unipro_DME_err_cnt[x];

#define SEQ_GEAR_UPLOAD_PRINT(x) \
						seq_printf(s, #x": %d\n", signalCtrl->record.gear_err_cnt[x] \
							-signalCtrl->record_upload.gear_err_cnt[x]);\
						signalCtrl->record_upload.gear_err_cnt[x] = signalCtrl->record.gear_err_cnt[x];


static int record_upload_read_func(struct seq_file *s, void *v)
{
	struct unipro_signal_quality_ctrl *signalCtrl =
		(struct unipro_signal_quality_ctrl *)(s->private);
	if (!signalCtrl)
		return -EINVAL;
	SEQ_UPLOAD_PRINT(ufs_device_err_cnt);
	SEQ_UPLOAD_PRINT(ufs_host_err_cnt);
	SEQ_UPLOAD_PRINT(ufs_bus_err_cnt);
	SEQ_UPLOAD_PRINT(ufs_crypto_err_cnt);
	SEQ_UPLOAD_PRINT(ufs_link_lost_cnt);
	SEQ_UPLOAD_PRINT(unipro_PA_err_total_cnt);
	SEQ_UPLOAD_PRINT(unipro_DL_err_total_cnt);
	SEQ_UPLOAD_PRINT(unipro_NL_err_total_cnt);
	SEQ_UPLOAD_PRINT(unipro_TL_err_total_cnt);
	SEQ_UPLOAD_PRINT(unipro_DME_err_total_cnt);
	SEQ_PA_UPLOAD_PRINT(UNIPRO_PA_LANE0_ERR_CNT);
	SEQ_PA_UPLOAD_PRINT(UNIPRO_PA_LANE1_ERR_CNT);
	SEQ_PA_UPLOAD_PRINT(UNIPRO_PA_LANE2_ERR_CNT);
	SEQ_PA_UPLOAD_PRINT(UNIPRO_PA_LANE3_ERR_CNT);
	SEQ_PA_UPLOAD_PRINT(UNIPRO_PA_LINE_RESET);

	SEQ_DL_UPLOAD_PRINT(UNIPRO_DL_NAC_RECEIVED);
	SEQ_DL_UPLOAD_PRINT(UNIPRO_DL_TCX_REPLAY_TIMER_EXPIRED);
	SEQ_DL_UPLOAD_PRINT(UNIPRO_DL_AFCX_REQUEST_TIMER_EXPIRED);
	SEQ_DL_UPLOAD_PRINT(UNIPRO_DL_FCX_PROTECTION_TIMER_EXPIRED);
	SEQ_DL_UPLOAD_PRINT(UNIPRO_DL_CRC_ERROR);
	SEQ_DL_UPLOAD_PRINT(UNIPRO_DL_RX_BUFFER_OVERFLOW);
	SEQ_DL_UPLOAD_PRINT(UNIPRO_DL_MAX_FRAME_LENGTH_EXCEEDED);
	SEQ_DL_UPLOAD_PRINT(UNIPRO_DL_WRONG_SEQUENCE_NUMBER);
	SEQ_DL_UPLOAD_PRINT(UNIPRO_DL_AFC_FRAME_SYNTAX_ERROR);
	SEQ_DL_UPLOAD_PRINT(UNIPRO_DL_NAC_FRAME_SYNTAX_ERROR);
	SEQ_DL_UPLOAD_PRINT(UNIPRO_DL_EOF_SYNTAX_ERROR);
	SEQ_DL_UPLOAD_PRINT(UNIPRO_DL_FRAME_SYNTAX_ERROR);
	SEQ_DL_UPLOAD_PRINT(UNIPRO_DL_BAD_CTRL_SYMBOL_TYPE);
	SEQ_DL_UPLOAD_PRINT(UNIPRO_DL_PA_INIT_ERROR);
	SEQ_DL_UPLOAD_PRINT(UNIPRO_DL_PA_ERROR_IND_RECEIVED);
	SEQ_DL_UPLOAD_PRINT(UNIPRO_DL_PA_INIT);

	SEQ_NL_UPLOAD_PRINT(UNIPRO_NL_UNSUPPORTED_HEADER_TYPE);
	SEQ_NL_UPLOAD_PRINT(UNIPRO_NL_BAD_DEVICEID_ENC);
	SEQ_NL_UPLOAD_PRINT(UNIPRO_NL_LHDR_TRAP_PACKET_DROPPING);

	SEQ_TL_UPLOAD_PRINT(UNIPRO_TL_UNSUPPORTED_HEADER_TYPE);
	SEQ_TL_UPLOAD_PRINT(UNIPRO_TL_UNKNOWN_CPORTID);
	SEQ_TL_UPLOAD_PRINT(UNIPRO_TL_NO_CONNECTION_RX);
	SEQ_TL_UPLOAD_PRINT(UNIPRO_TL_CONTROLLED_SEGMENT_DROPPING);
	SEQ_TL_UPLOAD_PRINT(UNIPRO_TL_BAD_TC);
	SEQ_TL_UPLOAD_PRINT(UNIPRO_TL_E2E_CREDIT_OVERFLOW);
	SEQ_TL_UPLOAD_PRINT(UNIPRO_TL_SAFETY_VALVE_DROPPING);

	SEQ_DME_UPLOAD_PRINT(UNIPRO_DME_GENERIC);
	SEQ_DME_UPLOAD_PRINT(UNIPRO_DME_TX_QOS);
	SEQ_DME_UPLOAD_PRINT(UNIPRO_DME_RX_QOS);
	SEQ_DME_UPLOAD_PRINT(UNIPRO_DME_PA_INIT_QOS);

	SEQ_GEAR_UPLOAD_PRINT(UFS_HS_G1);
	SEQ_GEAR_UPLOAD_PRINT(UFS_HS_G2);
	SEQ_GEAR_UPLOAD_PRINT(UFS_HS_G3);
	SEQ_GEAR_UPLOAD_PRINT(UFS_HS_G4);
	return 0;
}

static int record_upload_open(struct inode *inode, struct file *file)
{
	return single_open(file, record_upload_read_func, PDE_DATA(inode));
}

static const struct proc_ops record_upload_fops = {
	.proc_open = record_upload_open,
	.proc_read = seq_read,
	.proc_release = single_release,
};

int create_signal_quality_proc(struct unipro_signal_quality_ctrl *signalCtrl)
{
	struct proc_dir_entry *d_entry;
	signalCtrl->ctrl_dir = proc_mkdir("ufs_signalShow", NULL);
	if (!signalCtrl->ctrl_dir)
		return -ENOMEM;
	d_entry = proc_create_data("record", S_IRUGO, signalCtrl->ctrl_dir,
			&record_fops, signalCtrl);
	if (!d_entry)
		return -ENOMEM;
	d_entry = proc_create_data("record_upload", S_IRUGO, signalCtrl->ctrl_dir,
			&record_upload_fops, signalCtrl);
	if (!d_entry)
		return -ENOMEM;
	return 0;
}

void remove_signal_quality_proc(struct unipro_signal_quality_ctrl *signalCtrl)
{
	if (signalCtrl->ctrl_dir) {
		remove_proc_entry("record", signalCtrl->ctrl_dir);
		remove_proc_entry("record_upload", signalCtrl->ctrl_dir);
	}
	return;
}
//#endif /*OPLUS_UFS_SIGNAL_QUALITY*/
/*feature-flashaging806-v001-2-end*/

/*feature-iostack-v001-begin*/
#define IOSTACK_WORK_DELAY  (10 * HZ)
static void iostack_monitor_work(struct work_struct *work)
{
	struct ufs_mtk_host *host = container_of(to_delayed_work(work),
							struct ufs_mtk_host,
							iostack_work);
	struct ufs_hba *hba = host->hba;
	unsigned int hba_irqs = 0;
	unsigned int self_block = hba->host->host_self_blocked;

	hba_irqs = kstat_irqs_usr(hba->irq);
	pr_err("iostack:hba_irqs = %d, self-block = %d\n", hba_irqs, self_block);
	schedule_delayed_work(&host->iostack_work, IOSTACK_WORK_DELAY);
}

static void ufs_iostack_init(struct ufs_mtk_host *host)
{
	INIT_DELAYED_WORK(&host->iostack_work, iostack_monitor_work);
	schedule_delayed_work(&host->iostack_work, IOSTACK_WORK_DELAY);
}
/*feature-iostack-v001-end*/


/**
 * ufs_mtk_init - find other essential mmio bases
 * @hba: host controller instance
 *
 * Binds PHY with controller and powers up PHY enabling clocks
 * and regulators.
 *
 * Returns -EPROBE_DEFER if binding fails, returns negative error
 * on phy power up failure and returns zero on success.
 */
static int ufs_mtk_init(struct ufs_hba *hba)
{
	const struct of_device_id *id;
	struct device *dev = hba->dev;
	struct ufs_mtk_host *host;
	struct arm_smccc_res res;
	int err = 0;

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
	if (!host) {
		err = -ENOMEM;
		dev_info(dev, "%s: no memory for mtk ufs host\n", __func__);
		goto out;
	}

	host->hba = hba;
	ufshcd_set_variant(hba, host);

	id = of_match_device(ufs_mtk_of_match, dev);
	if (!id) {
		err = -EINVAL;
		goto out;
	}

	/* Initialize host capability */
	ufs_mtk_init_host_caps(hba);

	err = ufs_mtk_bind_mphy(hba);
	if (err)
		goto out_variant_clear;

	ufs_mtk_init_reset(hba);

	/* Enable runtime autosuspend */
	hba->caps |= UFSHCD_CAP_RPM_AUTOSUSPEND;

	/* Enable clock-gating */
	hba->caps |= UFSHCD_CAP_CLK_GATING;

	/* Enable inline encryption */
	hba->caps |= UFSHCD_CAP_CRYPTO;

	/* Enable WriteBooster */
	hba->caps |= UFSHCD_CAP_WB_EN;

	hba->quirks |= UFSHCI_QUIRK_SKIP_MANUAL_WB_FLUSH_CTRL;
	hba->vps->wb_flush_threshold = UFS_WB_BUF_REMAIN_PERCENT(80);

	if (host->caps & UFS_MTK_CAP_DISABLE_AH8)
		hba->caps |= UFSHCD_CAP_HIBERN8_WITH_CLK_GATING;

	/*
	 * ufshcd_vops_init() is invoked after
	 * ufshcd_setup_clock(true) in ufshcd_hba_init() thus
	 * phy clock setup is skipped.
	 *
	 * Enable phy power and clocks specifically here.
	 */
	ufs_mtk_mphy_power_on(hba, true);
	ufs_mtk_setup_clocks(hba, true, POST_CHANGE);

	/* Get vcc-opt */
	ufs_mtk_get_vcc_info(res);
	if (res.a1 == VCC_1)
		err = ufs_mtk_populate_vreg(dev, "vcc-opt1", &host->vcc);
	else if (res.a1 == VCC_2)
		err = ufs_mtk_populate_vreg(dev, "vcc-opt2", &host->vcc);
	else
		goto skip_vcc;
	if (err)
		goto out_variant_clear;
	err = ufs_mtk_get_vreg(dev, host->vcc);
	if (err)
		goto out_variant_clear;
skip_vcc:

	cpu_latency_qos_add_request(&host->pm_qos_req,
	     	   PM_QOS_DEFAULT_VALUE);
	host->pm_qos_init = true;

	ufs_mtk_biolog_init(host->qos_allowed, host->boot_device);

#if IS_ENABLED(CONFIG_SCSI_UFS_MEDIATEK_DBG)
	ufs_mtk_dbg_register(hba);
#endif

	/*feature-devinfo-v001-6-begin*/
	ufshcd_transmission_status_init_sysfs(hba);
	/*feature-devinfo-v001-6-end*/
        /*feature-flashaging806-v001-3-begin*/
	create_signal_quality_proc(&signalCtrl);
        /*feature-flashaging806-v001-3-end*/

#if IS_ENABLED(CONFIG_RPMB)
	async_schedule(ufs_mtk_rpmb_add, hba);
#endif

	/* Provide SCSI host ioctl API */
	hba->host->hostt->ioctl = (int (*)(struct scsi_device *, unsigned int,
				   void __user *))ufs_mtk_ioctl;
#ifdef CONFIG_COMPAT
	hba->host->hostt->compat_ioctl = (int (*)(struct scsi_device *,
					  unsigned int,
					  void __user *))ufs_mtk_ioctl;
#endif
	ufs_iostack_init(host);
	goto out;

out_variant_clear:
	ufshcd_set_variant(hba, NULL);
out:
	return err;
}

static int ufs_mtk_pre_pwr_change(struct ufs_hba *hba,
				  struct ufs_pa_layer_attr *dev_max_params,
				  struct ufs_pa_layer_attr *dev_req_params)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct ufs_dev_params host_cap;
	u32 adapt_val;
	int ret;

	host_cap.tx_lanes = UFS_MTK_LIMIT_NUM_LANES_TX;
	host_cap.rx_lanes = UFS_MTK_LIMIT_NUM_LANES_RX;
	host_cap.hs_rx_gear = UFS_MTK_LIMIT_HSGEAR_RX;
	host_cap.hs_tx_gear = UFS_MTK_LIMIT_HSGEAR_TX;
	host_cap.pwm_rx_gear = UFS_MTK_LIMIT_PWMGEAR_RX;
	host_cap.pwm_tx_gear = UFS_MTK_LIMIT_PWMGEAR_TX;
	host_cap.rx_pwr_pwm = UFS_MTK_LIMIT_RX_PWR_PWM;
	host_cap.tx_pwr_pwm = UFS_MTK_LIMIT_TX_PWR_PWM;
	host_cap.rx_pwr_hs = UFS_MTK_LIMIT_RX_PWR_HS;
	host_cap.tx_pwr_hs = UFS_MTK_LIMIT_TX_PWR_HS;
	host_cap.hs_rate = UFS_MTK_LIMIT_HS_RATE;
	host_cap.desired_working_mode =
				UFS_MTK_LIMIT_DESIRED_MODE;

	ret = ufshcd_get_pwr_dev_param(&host_cap,
				       dev_max_params,
				       dev_req_params);
	if (ret) {
		pr_info("%s: failed to determine capabilities\n",
			__func__);
	}

	if (host->hw_ver.major >= 3) {
		if (dev_req_params->gear_tx == UFS_HS_G4)
			adapt_val = PA_INITIAL_ADAPT;
		else
			adapt_val = PA_NO_ADAPT;
		ufshcd_dme_set(hba,
			       UIC_ARG_MIB(PA_TXHSADAPTTYPE),
			       adapt_val);
	}

	return ret;
}

static int ufs_mtk_pwr_change_notify(struct ufs_hba *hba,
				     enum ufs_notify_change_status stage,
				     struct ufs_pa_layer_attr *dev_max_params,
				     struct ufs_pa_layer_attr *dev_req_params)
{
	int ret = 0;

	switch (stage) {
	case PRE_CHANGE:
		ret = ufs_mtk_pre_pwr_change(hba, dev_max_params,
					     dev_req_params);
		break;
	case POST_CHANGE:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ufs_mtk_unipro_set_lpm(struct ufs_hba *hba, bool lpm)
{
	int ret;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	ret = ufshcd_dme_set(hba,
			     UIC_ARG_MIB_SEL(VS_UNIPROPOWERDOWNCONTROL, 0),
			     lpm ? 1 : 0);
	if (!ret || !lpm) {
		/*
		 * Forcibly set as non-LPM mode if UIC commands is failed
		 * to use default hba_enable_delay_us value for re-enabling
		 * the host.
		 */
		host->unipro_lpm = lpm;
	}

	return ret;
}

static int ufs_mtk_pre_link(struct ufs_hba *hba)
{
	int ret;
	u32 tmp;

	ufs_mtk_get_controller_version(hba);

	ret = ufs_mtk_unipro_set_lpm(hba, false);
	if (ret)
		return ret;

	/*
	 * Setting PA_Local_TX_LCC_Enable to 0 before link startup
	 * to make sure that both host and device TX LCC are disabled
	 * once link startup is completed.
	 */
	ret = ufshcd_disable_host_tx_lcc(hba);
	if (ret)
		return ret;

	/* disable deep stall */
	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(VS_SAVEPOWERCONTROL), &tmp);
	if (ret)
		return ret;

	tmp &= ~(1 << 6);

	ret = ufshcd_dme_set(hba, UIC_ARG_MIB(VS_SAVEPOWERCONTROL), tmp);

	return ret;
}

static void ufs_mtk_setup_clk_gating(struct ufs_hba *hba)
{
	unsigned long flags;
	u32 ah_ms;

	if (ufshcd_is_clkgating_allowed(hba)) {
		if (ufshcd_is_auto_hibern8_supported(hba) && hba->ahit)
			ah_ms = FIELD_GET(UFSHCI_AHIBERN8_TIMER_MASK,
					  hba->ahit);
		else
			ah_ms = 10;
		spin_lock_irqsave(hba->host->host_lock, flags);
		hba->clk_gating.delay_ms = ah_ms + 5;
		spin_unlock_irqrestore(hba->host->host_lock, flags);
	}
}

static int ufs_mtk_post_link(struct ufs_hba *hba)
{
	/* enable unipro clock gating feature */
	ufs_mtk_cfg_unipro_cg(hba, true);

	/* configure auto-hibern8 timer to 10ms */
	if (ufshcd_is_auto_hibern8_supported(hba)) {
		hba->ahit = FIELD_PREP(UFSHCI_AHIBERN8_TIMER_MASK, 5) |
			    FIELD_PREP(UFSHCI_AHIBERN8_SCALE_MASK, 3);
	}

	ufs_mtk_setup_clk_gating(hba);

	device_enable_async_suspend(hba->dev);

	return 0;
}

static int ufs_mtk_link_startup_notify(struct ufs_hba *hba,
				       enum ufs_notify_change_status stage)
{
	int ret = 0;

	switch (stage) {
	case PRE_CHANGE:
		ret = ufs_mtk_pre_link(hba);
		break;
	case POST_CHANGE:
		ret = ufs_mtk_post_link(hba);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ufs_mtk_device_reset(struct ufs_hba *hba)
{
	struct arm_smccc_res res;

#if defined(CONFIG_UFSFEATURE)
	struct ufsf_feature *ufsf = ufs_mtk_get_ufsf(hba);

	if (ufsf->hba)
		ufsf_reset_host(ufs_mtk_get_ufsf(hba));
#endif

	/* disable hba before device reset */
	ufshcd_hba_stop(hba);

	ufs_mtk_device_reset_ctrl(0, res);

	/*
	 * The reset signal is active low. UFS devices shall detect
	 * more than or equal to 1us of positive or negative RST_n
	 * pulse width.
	 *
	 * To be on safe side, keep the reset low for at least 10us.
	 */
	usleep_range(10, 15);

	ufs_mtk_device_reset_ctrl(1, res);

	/* Some devices may need time to respond to rst_n */
	usleep_range(10000, 15000);

	dev_info(hba->dev, "device reset done\n");

	return 0;
}

static int ufs_mtk_link_set_hpm(struct ufs_hba *hba)
{
	int err;
	u32 val;

	err = ufshcd_hba_enable(hba);
	if (err)
		return err;

	err = ufs_mtk_unipro_set_lpm(hba, false);
	if (err) {
		ufs_mtk_dbg_sel(hba);
		val = ufshcd_readl(hba, REG_UFS_PROBE);
		ufshcd_update_evt_hist(hba, UFS_EVT_RESUME_ERR, (u32)val);
		val = ufshcd_readl(hba, REG_INTERRUPT_STATUS);
		ufshcd_update_evt_hist(hba, UFS_EVT_RESUME_ERR, (u32)val);
		return err;
	}

	err = ufshcd_uic_hibern8_exit(hba);
	if (err)
		return err;

	/* Check link state to make sure exit h8 success */
	ufs_mtk_wait_idle_state(hba, 5);
	err = ufs_mtk_wait_link_state(hba, VS_LINK_UP, 100);
	if (err) {
		dev_warn(hba->dev, "exit h8 state fail, err=%d\n", err);
		return err;
	}
	ufshcd_set_link_active(hba);

	err = ufshcd_make_hba_operational(hba);
	if (err)
		return err;

	return 0;
}

static int ufs_mtk_link_set_lpm(struct ufs_hba *hba)
{
	int err;

	/* not wait unipro resetCnf */
	ufshcd_writel(hba,
		(ufshcd_readl(hba, REG_UFS_XOUFS_CTRL) & ~0x100),
		REG_UFS_XOUFS_CTRL);

	err = ufs_mtk_unipro_set_lpm(hba, true);
	if (err) {
		/* Resume UniPro state for following error recovery */
		ufs_mtk_unipro_set_lpm(hba, false);
		return err;
	}

	return 0;
}

static void ufs_mtk_vreg_set_lpm(struct ufs_hba *hba, bool lpm)
{
	if (!hba->vreg_info.vccq2 || !hba->vreg_info.vcc)
		return;

	/* prevent entering lpm when device is still active */
	if (lpm && ufshcd_is_ufs_dev_active(hba))
		return;

	if (lpm && !hba->vreg_info.vcc->enabled)
		regulator_set_mode(hba->vreg_info.vccq2->reg,
				   REGULATOR_MODE_IDLE);
	else if (!lpm)
		regulator_set_mode(hba->vreg_info.vccq2->reg,
				   REGULATOR_MODE_NORMAL);
}

static int ufs_mtk_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	int err;
	struct arm_smccc_res res;
	ufs_sleep_time_get(hba);
	if (ufshcd_is_link_hibern8(hba)) {
		err = ufs_mtk_link_set_lpm(hba);
		if (err)
			goto fail;
	}

	if (!ufshcd_is_link_active(hba)) {
		/*
		 * Make sure no error will be returned to prevent
		 * ufshcd_suspend() re-enabling regulators while vreg is still
		 * in low-power mode.
		 */
		ufs_mtk_vreg_set_lpm(hba, true);
		err = ufs_mtk_mphy_power_on(hba, false);
		if (err)
			goto fail;
	}

	if (ufshcd_is_link_off(hba))
		ufs_mtk_device_reset_ctrl(0, res);

	ufs_mtk_host_pwr_ctrl(false, res);

	return 0;
fail:
	/*
	 * Set link as off state enforcedly to trigger
	 * ufshcd_host_reset_and_restore() in ufshcd_suspend()
	 * for completed host reset.
	 */
	ufshcd_set_link_off(hba);
	return -EAGAIN;
}

static int ufs_mtk_resume(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	int err;
	struct arm_smccc_res res;

	ufs_active_time_get(hba);
	if (ufshcd_eh_in_progress(hba))
		ufs_mtk_ctrl_dev_pwr(hba, true, false);

	ufs_mtk_host_pwr_ctrl(true, res);

	err = ufs_mtk_mphy_power_on(hba, true);
	if (err)
		goto fail;

	ufs_mtk_vreg_set_lpm(hba, false);

	if (ufshcd_is_link_hibern8(hba)) {
		err = ufs_mtk_link_set_hpm(hba);
		if (err)
			goto fail;
	}

	return 0;
fail:
	return ufshcd_link_recovery(hba);
}

static void ufs_mtk_dbg_register_dump(struct ufs_hba *hba)
{
	ufshcd_dump_regs(hba, REG_UFS_REFCLK_CTRL, 0x4, "Ref-Clk Ctrl ");

	ufshcd_dump_regs(hba, REG_UFS_EXTREG, 0x4, "Ext Reg ");

	ufshcd_dump_regs(hba, REG_UFS_MPHYCTRL,
			 REG_UFS_REJECT_MON - REG_UFS_MPHYCTRL + 4,
			 "MPHY Ctrl ");

	/* Direct debugging information to REG_MTK_PROBE */
	ufs_mtk_dbg_sel(hba);
	ufshcd_dump_regs(hba, REG_UFS_PROBE, 0x4, "Debug Probe ");
	ufs_mtk_dbg_dump(100);
}

static int ufs_mtk_setup_regulators(struct ufs_hba *hba, bool on)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct ufs_vreg_info *vreg_info = &hba->vreg_info;
	int ret = 0;

	if (host->vcc) {
		vreg_info->vcc = host->vcc;
		if (on)
			ret = regulator_enable(host->vcc->reg);
		else
			ret = regulator_disable(host->vcc->reg);
		if (!ret)
			host->vcc->enabled = on;
	}

	return ret;
}

static void ufs_mtk_fix_regulators(struct ufs_hba *hba)
{
	struct ufs_dev_info *dev_info = &hba->dev_info;
	struct ufs_vreg_info *vreg_info = &hba->vreg_info;

	if (dev_info->wspecversion >= 0x0300) {
		if (vreg_info->vccq2) {
			regulator_disable(vreg_info->vccq2->reg);
			devm_kfree(hba->dev, vreg_info->vccq2->name);
			devm_kfree(hba->dev, vreg_info->vccq2);
			vreg_info->vccq2 = NULL;
		}
		if (vreg_info->vccq)
			vreg_info->vccq->always_on = true;
	} else {
		if (vreg_info->vccq) {
			regulator_disable(vreg_info->vccq->reg);
			devm_kfree(hba->dev, vreg_info->vccq->name);
			devm_kfree(hba->dev, vreg_info->vccq);
			vreg_info->vccq = NULL;
		}
		if (vreg_info->vccq2)
			vreg_info->vccq2->always_on = true;
	}
}

static int ufs_mtk_fixup_vcc_regulator(struct ufs_hba *hba)
{
	int err = 0;
	u16 ufs_ver;
	char vcc_name[MAX_PROP_SIZE];
	struct device *dev = hba->dev;
	struct ufs_vreg_info *vcc_info = &hba->vreg_info;

	/* Get UFS version to setting VCC */
	ufs_ver = (hba->dev_info.wspecversion & 0xF00) >> 8;
	snprintf(vcc_name, MAX_PROP_SIZE, "vcc_ufs%u", ufs_ver);

	err = ufs_mtk_populate_vreg(dev, vcc_name, &vcc_info->vcc);
	if (err || !vcc_info->vcc) {
		dev_info(dev, "%s: Unable to find %s\n", __func__, vcc_name);
		goto out;
	}

	err = ufs_mtk_get_vreg(dev, vcc_info->vcc);
	if (err)
		goto out;

	err = regulator_enable(vcc_info->vcc->reg);
	if (!err)
		vcc_info->vcc->enabled = true;
	else
		dev_info(dev, "%s: %s enable failed, err=%d\n",
					__func__, vcc_info->vcc->name, err);

out:
	if (err)
		dev_info(hba->dev, "%s: No combo ufs pmic setting\n", __func__);
	return err;
}

static int ufs_mtk_apply_dev_quirks(struct ufs_hba *hba)
{
	struct ufs_dev_info *dev_info = &hba->dev_info;
	u16 mid = dev_info->wmanufacturerid;

	if (mid == UFS_VENDOR_SAMSUNG) {
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TACTIVATE), 6);
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_HIBERN8TIME), 10);
	}

	/*
	 * Decide waiting time before gating reference clock and
	 * after ungating reference clock according to vendors'
	 * requirements. Default 30, 30.
	 */
	if (mid == UFS_VENDOR_SAMSUNG)
		ufs_mtk_setup_ref_clk_wait_us(hba, 1, 32);
	else if (mid == UFS_VENDOR_SKHYNIX)
		ufs_mtk_setup_ref_clk_wait_us(hba, 30, 30);
	else if (mid == UFS_VENDOR_TOSHIBA)
		ufs_mtk_setup_ref_clk_wait_us(hba, 100, 32);
	else
		ufs_mtk_setup_ref_clk_wait_us(hba, 30, 30);

	ufs_mtk_fix_regulators(hba);

	return 0;
}

static void ufs_mtk_fixup_dev_quirks(struct ufs_hba *hba)
{
	struct ufs_dev_info *dev_info = &hba->dev_info;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	/*
	 * If VCC setting is no yet enable,
	 * setting VCC by ufs_mtk_fixup_vcc_regulator.
	 */
	if (!hba->vreg_info.vcc)
		ufs_mtk_fixup_vcc_regulator(hba);

	ufshcd_fixup_dev_quirks(hba, ufs_mtk_dev_fixups);

	if (dev_info->wmanufacturerid != UFS_VENDOR_PHISON_CUST)
		dev_info->hpb_enabled = false;
	else
		pr_info("ufs wmanufacturerid = %x, enable hpb",dev_info->wmanufacturerid);

	if (dev_info->wmanufacturerid == UFS_VENDOR_MICRON)
		host->caps |= UFS_MTK_CAP_BROKEN_VCC;

	if (STR_PRFX_EQUAL("H9HQ15AFAMBDAR", dev_info->model))
		host->caps |= UFS_MTK_CAP_BROKEN_VCC | UFS_MTK_CAP_FORCE_VSx_LPM;

	if (ufs_mtk_is_delay_after_vcc_off(hba) && hba->vreg_info.vcc) {
		/*
		 * Always delay 5ms after VCC off.
		 */
		hba->dev_quirks |= UFS_DEVICE_QUIRK_DELAY_AFTER_LPM;
	}

	if (ufs_mtk_is_broken_vcc(hba) && hba->vreg_info.vcc &&
	    (hba->dev_quirks & UFS_DEVICE_QUIRK_DELAY_AFTER_LPM)) {
		hba->vreg_info.vcc->always_on = true;
		/*
		 * VCC will be kept always-on thus we don't
		 * need any delay during regulator operations
		 */
		hba->dev_quirks &= ~(UFS_DEVICE_QUIRK_DELAY_BEFORE_LPM |
			UFS_DEVICE_QUIRK_DELAY_AFTER_LPM);
	}

	ufs_mtk_install_tracepoints(hba);

	/* give more time for H8 */
	if (STR_PRFX_EQUAL("KLUFG4LHGC-B0E1", dev_info->model)) {
		hba->rpm_lvl = UFS_PM_LVL_1;
		hba->spm_lvl = UFS_PM_LVL_1;
	}

#if defined(CONFIG_UFSFEATURE)
	if (hba->dev_info.wmanufacturerid == UFS_VENDOR_SAMSUNG) {
		host->ufsf.hba = hba;
		ufsf_set_init_state(ufs_mtk_get_ufsf(hba));
	}
#endif
}

void recordGearErr(struct unipro_signal_quality_ctrl *signalCtrl, struct ufs_hba *hba)
{
	struct ufs_pa_layer_attr *pwr_info = &hba->pwr_info;
	u32 dev_gear = min_t(u32, pwr_info->gear_rx, pwr_info->gear_tx);

	if (dev_gear > UFS_HS_G4)
		return;

	signalCtrl->record.gear_err_cnt[dev_gear]++;
}

static void ufs_mtk_event_notify(struct ufs_hba *hba,
				 enum ufs_event_type evt, void *data)
{
	unsigned int val = *(u32 *)data;
        /*feature-flashaging806-v001-4-begin*/
	recordUniproErr(&signalCtrl, val, evt);
	recordGearErr(&signalCtrl, hba);
        /*feature-flashaging806-v001-4-end*/
	trace_ufs_mtk_event(evt, val);

	/*
	 * After error handling of ufshcd_host_reset_and_restore
	 * Bypass clear ua to send scsi command request sense, else
	 * deadlock hang because scsi is waiting error handling done.
	 */
	if (evt == UFS_EVT_HOST_RESET)
		hba->wlun_dev_clr_ua = false;

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	if (evt == UFS_EVT_ABORT && !ufs_abort_aee_count) {
		cmd_hist_disable();
		ufs_abort_aee_count++;
		aee_kernel_warning_api(__FILE__,
			__LINE__, DB_OPT_FS_IO_LOG,
			"ufshcd_abort", "timeout at tag %d", val);
	}
#endif
}

static void ufs_mtk_auto_hibern8_disable(struct ufs_hba *hba)
{
	unsigned long flags;
	int ret;

	/* disable auto-hibern8 */
	spin_lock_irqsave(hba->host->host_lock, flags);
	ufshcd_writel(hba, 0, REG_AUTO_HIBERNATE_IDLE_TIMER);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	/* wait host return to idle state when auto-hibern8 off */
	ufs_mtk_wait_idle_state(hba, 5);

	ret = ufs_mtk_wait_link_state(hba, VS_LINK_UP, 100);
	if (ret)
		dev_warn(hba->dev, "exit h8 state fail, ret=%d\n", ret);
}

static void ufs_mtk_hibern8_notify(struct ufs_hba *hba, enum uic_cmd_dme cmd,
				    enum ufs_notify_change_status status)
{
	if (!ufshcd_is_auto_hibern8_supported(hba))
		return;

	if (status == PRE_CHANGE && cmd == UIC_CMD_DME_HIBER_ENTER)
		ufs_mtk_auto_hibern8_disable(hba);

	if (status == POST_CHANGE && cmd == UIC_CMD_DME_HIBER_ENTER &&
		hba->dev_info.wmanufacturerid == UFS_VENDOR_MICRON)
		usleep_range(5000, 5100);
}

/*
 * struct ufs_hba_mtk_vops - UFS MTK specific variant operations
 *
 * The variant operations configure the necessary controller and PHY
 * handshake during initialization.
 */
static const struct ufs_hba_variant_ops ufs_hba_mtk_vops = {
	.name                = "mediatek.ufshci",
	.init                = ufs_mtk_init,
	.get_ufs_hci_version = ufs_mtk_get_ufs_hci_version,
	.setup_clocks        = ufs_mtk_setup_clocks,
	.setup_regulators    = ufs_mtk_setup_regulators,
	.hce_enable_notify   = ufs_mtk_hce_enable_notify,
	.link_startup_notify = ufs_mtk_link_startup_notify,
	.pwr_change_notify   = ufs_mtk_pwr_change_notify,
	.hibern8_notify      = ufs_mtk_hibern8_notify,
	.apply_dev_quirks    = ufs_mtk_apply_dev_quirks,
	.fixup_dev_quirks    = ufs_mtk_fixup_dev_quirks,
	.suspend             = ufs_mtk_suspend,
	.resume              = ufs_mtk_resume,
	.dbg_register_dump   = ufs_mtk_dbg_register_dump,
	.device_reset        = ufs_mtk_device_reset,
	.event_notify        = ufs_mtk_event_notify,
};

/**
 * ufs_mtk_probe - probe routine of the driver
 * @pdev: pointer to Platform device handle
 *
 * Return zero for success and non-zero for failure
 */
static int ufs_mtk_probe(struct platform_device *pdev)
{
	int err;
	struct device *dev = &pdev->dev;
	struct device_node *reset_node;
	struct platform_device *reset_pdev;
	struct device_link *link;
	struct ufs_hba *hba;
	struct cpumask dstp;

	reset_node = of_find_compatible_node(NULL, NULL,
					     "ti,syscon-reset");
	if (!reset_node) {
		dev_notice(dev, "find ti,syscon-reset fail\n");
		goto skip_reset;
	}
	reset_pdev = of_find_device_by_node(reset_node);
	if (!reset_pdev) {
		dev_notice(dev, "find reset_pdev fail\n");
		goto skip_reset;
	}
	link = device_link_add(dev, &reset_pdev->dev,
		DL_FLAG_AUTOPROBE_CONSUMER);
	if (!link) {
		dev_notice(dev, "add reset device_link fail\n");
		goto skip_reset;
	}
	/* supplier is not probed */
	if (link->status == DL_STATE_DORMANT) {
		err = -EPROBE_DEFER;
		goto out;
	}

skip_reset:
	/* perform generic probe */
	err = ufshcd_pltfrm_init(pdev, &ufs_hba_mtk_vops);
	if (err) {
		dev_info(dev, "probe failed %d\n", err);
		goto out;
	}

	/* set affinity to cpu3 */
	hba = platform_get_drvdata(pdev);
	cpumask_clear(&dstp);
	cpumask_set_cpu(7, &dstp);
	cpumask_set_cpu(6, &dstp);
	cpumask_set_cpu(5, &dstp);
	irq_modify_status(hba->irq, 0, IRQ_NO_BALANCING);
	if (hba && hba->irq) {
		err = irq_set_affinity_hint(hba->irq, &dstp);
		if (err < 0)
			dev_err(dev, "affinity set err=%d\n", err);
	}

	/*
	 * Because the default power setting of VSx (the upper layer of
	 * VCCQ/VCCQ2) is HWLP, we need to prevent VCCQ/VCCQ2 from
	 * entering LPM.
	 */
	ufs_mtk_ctrl_dev_pwr(hba, true, true);

out:

	of_node_put(reset_node);
	return err;
}

#if defined(CONFIG_UFSFEATURE)
static void ufs_mtk_remove_ufsf(struct ufs_hba *hba)
{
	struct ufsf_feature *ufsf = ufs_mtk_get_ufsf(hba);

	if (ufsf->hba)
		ufsf_remove(ufsf);
}
#endif

/**
 * ufs_mtk_remove - set driver_data of the device to NULL
 * @pdev: pointer to platform device handle
 *
 * Always return 0
 */
static int ufs_mtk_remove(struct platform_device *pdev)
{
	struct ufs_hba *hba = platform_get_drvdata(pdev);

	pm_runtime_get_sync(&(pdev)->dev);

#if defined(CONFIG_UFSFEATURE)
	ufs_mtk_remove_ufsf(hba);
#endif
        /*feature-flashaging806-v001-5-begin*/
	remove_signal_quality_proc(&signalCtrl);
        /*feature-flashaging806-v001-5-end*/
	ufshcd_remove(hba);
	ufs_mtk_biolog_exit();
	ufs_mtk_uninstall_tracepoints();
	return 0;
}

int ufs_mtk_pltfrm_suspend(struct device *dev)
{
	int ret;
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host;
#if defined(CONFIG_UFSFEATURE)
	struct ufsf_feature *ufsf = ufs_mtk_get_ufsf(hba);
#endif

	host = ufshcd_get_variant(hba);
	if (down_trylock(&host->rpmb_sem))
		return -EBUSY;

#if defined(CONFIG_UFSFEATURE)
	if (ufsf->hba)
		ufsf_suspend(ufsf, true);
#endif

	/* Check if shutting down */
	if (!ufshcd_is_user_access_allowed(hba)) {
		ret = -EBUSY;
		goto out;
	}

	if (ufshcd_is_auto_hibern8_supported(hba)) {
		ufshcd_hold(hba, false);
		ufs_mtk_auto_hibern8_disable(hba);
		ufshcd_release(hba);
	}

	ret = ufshcd_pltfrm_suspend(dev);

	if (!ret)
		ufs_mtk_ctrl_dev_pwr(hba, false, false);
out:

#if defined(CONFIG_UFSFEATURE)
	/* We assume link is off */
	if (ret && ufsf)
		ufsf_resume(ufsf, true);
#endif
	if (ret)
		up(&host->rpmb_sem);

	return ret;
}

int ufs_mtk_pltfrm_resume(struct device *dev)
{
	int ret;
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host;
#if defined(CONFIG_UFSFEATURE)
	struct ufsf_feature *ufsf = ufs_mtk_get_ufsf(hba);
	bool is_link_off = ufshcd_is_link_off(hba);
#endif

	ufs_mtk_ctrl_dev_pwr(hba, true, false);

	ret = ufshcd_pltfrm_resume(dev);

	if (ret)
		ufs_mtk_ctrl_dev_pwr(hba, false, false);

#if defined(CONFIG_UFSFEATURE)
	if (!ret && ufsf->hba)
		ufsf_resume(ufsf, is_link_off);
#endif

	host = ufshcd_get_variant(hba);
	if (!ret)
		up(&host->rpmb_sem);

	return ret;
}

int ufs_mtk_pltfrm_runtime_suspend(struct device *dev)
{
	int ret;
#if defined(CONFIG_UFSFEATURE)
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufsf_feature *ufsf = ufs_mtk_get_ufsf(hba);

	if (ufsf->hba)
		ufsf_suspend(ufsf, false);
#endif

	if (ufshcd_is_auto_hibern8_supported(hba)) {
		ufshcd_hold(hba, false);
		ufs_mtk_auto_hibern8_disable(hba);
		ufshcd_release(hba);
	}

	ret = ufshcd_pltfrm_runtime_suspend(dev);

	if (!ret)
		ufs_mtk_ctrl_dev_pwr(hba, false, false);

#if defined(CONFIG_UFSFEATURE)
	/* We assume link is off */
	if (ret && ufsf->hba)
		ufsf_resume(ufsf, true);
#endif

	return ret;
}

int ufs_mtk_pltfrm_runtime_resume(struct device *dev)
{
	int ret;
#if defined(CONFIG_UFSFEATURE)
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufsf_feature *ufsf = ufs_mtk_get_ufsf(hba);
	bool is_link_off = ufshcd_is_link_off(hba);
#endif

	ufs_mtk_ctrl_dev_pwr(hba, true, false);

	ret = ufshcd_pltfrm_runtime_resume(dev);

	if (ret)
		ufs_mtk_ctrl_dev_pwr(hba, false, false);

#if defined(CONFIG_UFSFEATURE)
	if (!ret && ufsf->hba)
		ufsf_resume(ufsf, is_link_off);
#endif

	return ret;
}

void ufs_mtk_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct scsi_device *sdev;

#if defined(CONFIG_UFSFEATURE)
	struct ufsf_feature *ufsf = ufs_mtk_get_ufsf(hba);

	if (ufsf->hba)
		ufsf_suspend(ufsf, false);
#endif

	/*
	 * Quiesce all SCSI devices to prevent any non-PM requests sending
	 * from block layer during and after shutdown.
	 *
	 * Note. Using scsi_autopm_get_device() instead of pm_runtime_disable()
	 * is to prevent noisy message by below checking,
	 * WARN_ON_ONCE(sdev->quiesced_by && sdev->quiesced_by != current);
	 * This warning shows up if we try to quiesce a runtime-suspended
	 * SCSI device. This is possible during our new shutdown flow.
	 * Using scsi_autopm_get_device() to resume all SCSI devices first
	 * can prevent it.
	 */
	shost_for_each_device(sdev, hba->host)
		scsi_device_quiesce(sdev);


	/* Alaways shutdown even in case of error */
	ufshcd_pltfrm_shutdown(pdev);
}

static const struct dev_pm_ops ufs_mtk_pm_ops = {
	.suspend         = ufs_mtk_pltfrm_suspend,
	.resume          = ufs_mtk_pltfrm_resume,
	.runtime_suspend = ufs_mtk_pltfrm_runtime_suspend,
	.runtime_resume  = ufs_mtk_pltfrm_runtime_resume,
	.runtime_idle    = ufshcd_pltfrm_runtime_idle,
};

static struct platform_driver ufs_mtk_pltform = {
	.probe      = ufs_mtk_probe,
	.remove     = ufs_mtk_remove,
	.shutdown   = ufs_mtk_shutdown,
	.driver = {
		.name   = "ufshcd-mtk",
		.pm     = &ufs_mtk_pm_ops,
		.of_match_table = ufs_mtk_of_match,
	},
};

MODULE_AUTHOR("Stanley Chu <stanley.chu@mediatek.com>");
MODULE_AUTHOR("Peter Wang <peter.wang@mediatek.com>");
MODULE_DESCRIPTION("MediaTek UFS Host Driver");
MODULE_LICENSE("GPL v2");

module_platform_driver(ufs_mtk_pltform);
