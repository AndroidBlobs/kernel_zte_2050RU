/************************************************************************
*
* File Name: himax_common_interface.c
*
*  *   Version: v1.0
*
************************************************************************/
#include "himax_platform.h"
#include "himax_common.h"
#include "himax_ic_core.h"
#ifdef HX_USB_DETECT_GLOBAL
#include <linux/power_supply.h>
#endif

extern struct himax_ts_data *private_ts;
extern struct himax_ic_data *ic_data;
extern struct himax_core_fp g_core_fp;
extern uint8_t HX_SMWP_EN;
extern bool fw_update_complete;
#if defined(HX_AUTO_UPDATE_FW) || defined(HX_ZERO_FLASH)
extern char *i_CTPM_firmware_name;
#endif
#ifdef HX_USB_DETECT_GLOBAL
extern bool USB_detect_flag;
#endif

int himax_vendor_id = 0;
int himax_test_faied_buffer_length = 0;
int himax_test_failed_count = 0;
int himax_tptest_result = 0;
char CTPM_firmware_name[50] = {0};
char hx_criteria_csv_name[30] = {0};
char g_hx_save_file_path[20] = {0};
char g_hx_save_file_name[50] = {0};

char *himax_test_failed_node_buffer = NULL;
char *himax_test_temp_buffer = NULL;
u8 *himax_test_failed_node = NULL;
bool	fw_updating = false;

#define TEST_RESULT_LENGTH (8 * 1200)
#define TEST_TEMP_LENGTH 8
#define HIMAX_PINCTRL_INIT_STATE "pmx_ts_init"
#ifdef CONFIG_TS_HIMAX_MTK_INTERFACE
#define HIMAX_PINCTRL_IRQ_STATE "eint_as_int"
#define HIMAX_PINCTRL_RST_0_STATE "rst_output0"
#define HIMAX_PINCTRL_RST_1_STATE "rst_output1"
#endif
#define TP_TEST_INIT		1
#define TP_TEST_START	2
#define TP_TEST_END		3

#ifdef HIMAX_GET_FW_BY_LCM
struct tpvendor_t himax_vendor_l[] = {
	{HX_VENDOR_ID_0, HXTS_VENDOR_0_NAME},
	{HX_VENDOR_ID_1, HXTS_VENDOR_1_NAME},
	{HX_VENDOR_ID_2, HXTS_VENDOR_2_NAME},
	{HX_VENDOR_ID_3, HXTS_VENDOR_2_NAME},
	{VENDOR_END, "Unknown"},
};
#endif

int himax_platform_pinctrl_init(struct himax_i2c_platform_data *pdata)
{
	int ret = 0;
	struct i2c_client *client = private_ts->client;

	/* Get pinctrl if target uses pinctrl */
	pdata->ts_pinctrl = devm_pinctrl_get(&(client->dev));
	if (IS_ERR_OR_NULL(pdata->ts_pinctrl)) {
		ret = PTR_ERR(pdata->ts_pinctrl);
		E("Target does not use pinctrl %d\n", ret);
		goto err_pinctrl_get;
	}

	pdata->pinctrl_state_init
	    = pinctrl_lookup_state(pdata->ts_pinctrl, HIMAX_PINCTRL_INIT_STATE);
	if (IS_ERR_OR_NULL(pdata->pinctrl_state_init)) {
		ret = PTR_ERR(pdata->pinctrl_state_init);
		E("Can not lookup %s pinstate %d\n", HIMAX_PINCTRL_INIT_STATE, ret);
		goto err_pinctrl_lookup;
	}

#ifdef CONFIG_TS_HIMAX_MTK_INTERFACE
	pdata->pinctrl_int_input
	    = pinctrl_lookup_state(pdata->ts_pinctrl, HIMAX_PINCTRL_IRQ_STATE);
	if (IS_ERR_OR_NULL(pdata->pinctrl_int_input)) {
		ret = PTR_ERR(pdata->pinctrl_int_input);
		E("Can not lookup %s pinstate %d\n", HIMAX_PINCTRL_IRQ_STATE, ret);
		goto err_pinctrl_lookup;
	}

	pdata->pinctrl_rst_output0
	    = pinctrl_lookup_state(pdata->ts_pinctrl, HIMAX_PINCTRL_RST_0_STATE);
	if (IS_ERR_OR_NULL(pdata->pinctrl_rst_output0)) {
		ret = PTR_ERR(pdata->pinctrl_rst_output0);
		E("Can not lookup %s pinstate %d\n", HIMAX_PINCTRL_RST_0_STATE, ret);
		goto err_pinctrl_lookup;
	}

	pdata->pinctrl_rst_output1
	    = pinctrl_lookup_state(pdata->ts_pinctrl, HIMAX_PINCTRL_RST_1_STATE);
	if (IS_ERR_OR_NULL(pdata->pinctrl_rst_output1)) {
		ret = PTR_ERR(pdata->pinctrl_rst_output1);
		E("Can not lookup %s pinstate %d\n", HIMAX_PINCTRL_RST_1_STATE, ret);
		goto err_pinctrl_lookup;
	}

	himax_pdata = pdata;
#endif

	ret = pinctrl_select_state(pdata->ts_pinctrl, pdata->pinctrl_state_init);
	if (ret < 0) {
		E("failed to select pin to init state");
		goto err_select_init_state;
	}

	return 0;

err_select_init_state:
err_pinctrl_lookup:
	devm_pinctrl_put(pdata->ts_pinctrl);
err_pinctrl_get:
	pdata->ts_pinctrl = NULL;
	return ret;
}

#ifdef HIMAX_GET_FW_BY_LCM
int himax_get_fw_by_lcminfo(void)
{
	int i = 0;

	if (HXTS_MODULE_NUM == 1) {
		himax_vendor_id = HX_VENDOR_ID_0;
		snprintf(CTPM_firmware_name, sizeof(CTPM_firmware_name),
			"Himax_firmware_%s.bin", HXTS_VENDOR_0_NAME);
		i_CTPM_firmware_name = CTPM_firmware_name;
		snprintf(hx_criteria_csv_name, sizeof(hx_criteria_csv_name),
			"hx_criteria_%s.csv", HXTS_VENDOR_0_NAME);
		I("HXTP firmware name :%s, module num is 0.\n", CTPM_firmware_name);
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(himax_vendor_l); i++) {
		if (strnstr(tpd_fw_cdev.lcm_info, himax_vendor_l[i].vendor_name, strlen(tpd_fw_cdev.lcm_info))) {
			himax_vendor_id = himax_vendor_l[i].vendor_id;
			snprintf(CTPM_firmware_name, sizeof(CTPM_firmware_name),
				"Himax_firmware_%s.bin", himax_vendor_l[i].vendor_name);
			i_CTPM_firmware_name = CTPM_firmware_name;
			snprintf(hx_criteria_csv_name, sizeof(hx_criteria_csv_name),
				"hx_criteria_%s.csv", himax_vendor_l[i].vendor_name);
			I("HXTP firmware name :%s\n", CTPM_firmware_name);
			return 0;
		}
	}
	return -EIO;

}
#else
int himax_get_vendor_id_by_lcdid(struct himax_ts_data *ts, struct himax_i2c_platform_data *pdata)
{
	int tp_id0 = 0, tp_id1 = 0;
	struct device_node *dt = ts->client->dev.of_node;

	pdata->gpio_id0 = of_get_named_gpio(dt, "himax,id0-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_id0)) {
		I("gpio_id0 value is not valid\n");
		return -EIO;
	}
	pdata->gpio_id1 = of_get_named_gpio(dt, "himax,id1-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_id1)) {
		I("gpio_id1 value is not valid\n");
		return -EIO;
	}
	tp_id0 = gpio_get_value(pdata->gpio_id0);
	tp_id1 = gpio_get_value(pdata->gpio_id1);
	I("HXTP,lcd_id0=%d, lcd_id1=%d\n", tp_id0, tp_id1);

	if ((tp_id0 == 0) && (tp_id1 == 0)) {
		himax_vendor_id = HX_VENDOR_ID_0;
#if defined(HX_AUTO_UPDATE_FW) || defined(HX_ZERO_FLASH)
		snprintf(CTPM_firmware_name, sizeof(CTPM_firmware_name), HX_UPGRADE_FW0);
		i_CTPM_firmware_name = CTPM_firmware_name;
#endif
		I("HXTP,it's %s\n", HXTS_VENDOR_0_NAME);
	} else if ((tp_id0 == 1) && (tp_id1 == 0)) {
		himax_vendor_id = HX_VENDOR_ID_1;
#if defined(HX_AUTO_UPDATE_FW) || defined(HX_ZERO_FLASH)
		snprintf(CTPM_firmware_name, sizeof(CTPM_firmware_name), HX_UPGRADE_FW1);
		i_CTPM_firmware_name = CTPM_firmware_name;
#endif
		I("HXTP,it's %s\n", HXTS_VENDOR_1_NAME);
	} else if ((tp_id0 == 0) && (tp_id1 == 1)) {
		himax_vendor_id = HX_VENDOR_ID_2;
#if defined(HX_AUTO_UPDATE_FW) || defined(HX_ZERO_FLASH)
		snprintf(CTPM_firmware_name, sizeof(CTPM_firmware_name), HX_UPGRADE_FW2);
		i_CTPM_firmware_name = CTPM_firmware_name;
#endif
		I("HXTP,it's %s\n", HXTS_VENDOR_2_NAME);
	} else {
		himax_vendor_id = HX_VENDOR_ID_3;
#if defined(HX_AUTO_UPDATE_FW) || defined(HX_ZERO_FLASH)
		snprintf(CTPM_firmware_name, sizeof(CTPM_firmware_name), HX_UPGRADE_FW3);
		i_CTPM_firmware_name = CTPM_firmware_name;
#endif
		I("HXTP,it's %s\n", HXTS_VENDOR_3_NAME);
	}
	snprintf(hx_criteria_csv_name, sizeof(hx_criteria_csv_name), "hx_criteria_%02x.csv", himax_vendor_id);
	return 0;
}
#endif

void himax_free_lcdid_gpio(struct himax_i2c_platform_data *pdata)
{
	if (gpio_is_valid(pdata->gpio_id0)) {
		gpio_free(pdata->gpio_id0);
	}
	if (gpio_is_valid(pdata->gpio_id1)) {
		gpio_free(pdata->gpio_id1);
	}
}

static int tpd_init_tpinfo(struct tpd_classdev_t *cdev)
{
	I("tpd_init_tpinfo\n");
	g_core_fp.fp_read_FW_ver();
#ifdef HX_RST_PIN_FUNC
	g_core_fp.fp_ic_reset(false, false);
#endif
	switch (himax_vendor_id) {
	case HX_VENDOR_ID_0:
		strlcpy(cdev->ic_tpinfo.vendor_name, HXTS_VENDOR_0_NAME, sizeof(cdev->ic_tpinfo.vendor_name));
		break;
	case HX_VENDOR_ID_1:
		strlcpy(cdev->ic_tpinfo.vendor_name, HXTS_VENDOR_1_NAME, sizeof(cdev->ic_tpinfo.vendor_name));
		break;
	case HX_VENDOR_ID_2:
		strlcpy(cdev->ic_tpinfo.vendor_name, HXTS_VENDOR_2_NAME, sizeof(cdev->ic_tpinfo.vendor_name));
		break;
	case HX_VENDOR_ID_3:
		strlcpy(cdev->ic_tpinfo.vendor_name, HXTS_VENDOR_3_NAME, sizeof(cdev->ic_tpinfo.vendor_name));
		break;
	default:
		strlcpy(cdev->ic_tpinfo.vendor_name, "Unknown.", sizeof(cdev->ic_tpinfo.vendor_name));
		break;
	}
	snprintf(cdev->ic_tpinfo.tp_name, sizeof(cdev->ic_tpinfo.tp_name), "Himax_%s", private_ts->chip_name);
	cdev->ic_tpinfo.chip_model_id = TS_CHIP_HIMAX;
	cdev->ic_tpinfo.firmware_ver = ic_data->vendor_fw_ver;
	cdev->ic_tpinfo.config_ver = ic_data->vendor_touch_cfg_ver;
	cdev->ic_tpinfo.display_ver = ic_data->vendor_display_cfg_ver;
	cdev->ic_tpinfo.module_id = himax_vendor_id;
	cdev->ic_tpinfo.i2c_addr = 0x48;
	return 0;
}

#ifdef HX_SMART_WAKEUP
static int tpd_get_wakegesture(struct tpd_classdev_t *cdev)
{
	struct himax_ts_data *ts = private_ts;

	I("%s wakeup_gesture_enable val is:%d.\n", __func__, ts->SMWP_enable);
	cdev->b_gesture_enable = ts->SMWP_enable;
	return cdev->b_gesture_enable;
}

static int tpd_enable_wakegesture(struct tpd_classdev_t *cdev, int enable)
{
	struct himax_ts_data *ts = private_ts;

	ts->SMWP_enable = enable;
	ts->gesture_cust_en[0] = ts->SMWP_enable;
	if (!ts->suspended) {
		g_core_fp.fp_set_SMWP_enable(ts->SMWP_enable, ts->suspended);
	} else {
		cdev->tp_suspend_write_gesture = true;
	}
	HX_SMWP_EN = ts->SMWP_enable;
	I("%s: SMART_WAKEUP_enable = %d.\n", __func__, HX_SMWP_EN);

	return ts->SMWP_enable;
}
#endif

static bool himax_suspend_need_awake(struct tpd_classdev_t *cdev)
{
#ifdef HX_SMART_WAKEUP
	struct himax_ts_data *ts = private_ts;

	if (!cdev->tp_suspend_write_gesture &&
		(fw_updating || ts->SMWP_enable)) {
		I("tp suspend need awake.\n");
		return true;
	}
#else
	if (fw_updating) {
		I("tp suspend need awake.\n");
		return true;
	}
#endif
	else {
		cdev->tp_suspend_write_gesture = false;
		I("tp suspend dont need awake.\n");
		return false;
	}
}


#ifdef HX_HIGH_SENSE
static int tpd_hsen_read(struct tpd_classdev_t *cdev)
{
	struct himax_ts_data *ts = private_ts;

	cdev->b_smart_cover_enable = ts->HSEN_enable;
	cdev->b_glove_enable = ts->HSEN_enable;

	return ts->HSEN_enable;
}

static int tpd_hsen_write(struct tpd_classdev_t *cdev, int enable)
{
	struct himax_ts_data *ts = private_ts;

	ts->HSEN_enable = enable;
	if (!ts->suspended)
		g_core_fp.fp_set_HSEN_enable(ts->HSEN_enable, ts->suspended);
	I("%s: HSEN_enable = %d.\n", __func__, ts->HSEN_enable);

	return ts->HSEN_enable;
}
#endif

static int himax_i2c_reg_read(struct tpd_classdev_t *cdev, u32 addr, u8 *data, int len)
{
	u8  address[4] = {0};

	address[0] = (u8)addr;
	address[1] = (u8)(addr >> 8);
	address[2] = (u8)(addr >> 16);
	address[3] = (u8)(addr >> 24);
	g_core_fp.fp_register_read(address, len, data, false);
	return 0;
}

static int himax_i2c_reg_write(struct tpd_classdev_t *cdev, u32 addr, u8 *data, int len)
{
	u8  address[4] = {0};

	address[0] = (u8)addr;
	address[1] = (u8)(addr >> 8);
	address[2] = (u8)(addr >> 16);
	address[3] = (u8)(addr >> 24);
	g_core_fp.fp_register_write(address, len, data, false);
	return 0;
}

static int himax_tp_fw_upgrade(struct tpd_classdev_t *cdev, char *fw_name, int fwname_len)
{
	char fileName[128] = {0};
	int result = 0;
#ifndef HX_ZERO_FLASH
	int fw_type = 0;
	const struct firmware *fw = NULL;
#endif

	memset(fileName, 0, sizeof(fileName));
	snprintf(fileName, sizeof(fileName), "%s", fw_name);
	fileName[fwname_len - 1] = '\0';
	I("%s: upgrade from file(%s) start!\n", __func__, fileName);
	himax_int_enable(0);
#ifdef HX_ZERO_FLASH
	I("NOW Running Zero flash update!\n");
	I("%s: upgrade from file(%s) start!\n", __func__, fileName);
	result = g_core_fp.fp_0f_op_file_dirly(fileName);
	if (result) {
		fw_update_complete = false;
		I("Zero flash update fail!\n");
		goto error_fw_upgrade;
	} else {
		fw_update_complete = true;
		I("Zero flash update complete!\n");
	}
	goto firmware_upgrade_done;
#else
	I("NOW Running common flow update!\n");
	I("%s: upgrade from file(%s) start!\n", __func__, fileName);
	result = request_firmware(&fw, fileName, private_ts->dev);

	if (result < 0) {
		I("fail to request_firmware fwpath: %s (ret:%d)\n", fileName, result);
		goto error_fw_upgrade;
	}
	I("%s: FW image: %02X, %02X, %02X, %02X\n", __func__,
			fw->data[0], fw->data[1], fw->data[2], fw->data[3]);
	fw_type = (fw->size) / 1024;
	I("Now FW size is : %dk\n", fw_type);
	fw_updating = true;
	switch (fw_type) {
	case 32:
		if (g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_32k((unsigned char *)fw->data,
			fw->size, false) == 0) {
			E("%s: TP upgrade error, line: %d\n", __func__, __LINE__);
			fw_update_complete = false;
		} else {
			I("%s: TP upgrade OK, line: %d\n", __func__, __LINE__);
			fw_update_complete = true;
		}
		break;

	case 60:
		if (g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_60k((unsigned char *)fw->data,
			fw->size, false) == 0) {
			E("%s: TP upgrade error, line: %d\n", __func__, __LINE__);
			fw_update_complete = false;
		} else {
			I("%s: TP upgrade OK, line: %d\n", __func__, __LINE__);
			fw_update_complete = true;
		}
		break;

	case 64:
		if (g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_64k((unsigned char *)fw->data,
			fw->size, false) == 0) {
			E("%s: TP upgrade error, line: %d\n", __func__, __LINE__);
			fw_update_complete = false;
		} else {
			I("%s: TP upgrade OK, line: %d\n", __func__, __LINE__);
			fw_update_complete = true;
		}
		break;

	case 124:
		if (g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_124k((unsigned char *)fw->data,
			fw->size, false) == 0) {
			E("%s: TP upgrade error, line: %d\n", __func__, __LINE__);
			fw_update_complete = false;
		} else {
			I("%s: TP upgrade OK, line: %d\n", __func__, __LINE__);
			fw_update_complete = true;
		}
		break;

	case 128:
		if (g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_128k((unsigned char *)fw->data,
			fw->size, false) == 0) {
			E("%s: TP upgrade error, line: %d\n", __func__, __LINE__);
			fw_update_complete = false;
		} else {
			I("%s: TP upgrade OK, line: %d\n", __func__, __LINE__);
			fw_update_complete = true;
		}
		break;

	default:
		E("%s: Flash command fail: %d\n", __func__, __LINE__);
		fw_update_complete = false;
		break;
	}
	release_firmware(fw);
	fw_updating = false;
	goto firmware_upgrade_done;
#endif
firmware_upgrade_done:
	g_core_fp.fp_reload_disable(0);
	g_core_fp.fp_read_FW_ver();
	g_core_fp.fp_touch_information();
#ifdef HX_RST_PIN_FUNC
	g_core_fp.fp_ic_reset(true, false);
#else
	g_core_fp.fp_sense_on(0x00);
#endif

	himax_int_enable(1);
	return 0;
error_fw_upgrade:
	himax_int_enable(1);
	return -EIO;
}

static int himax_gpio_shutdown_config(void)
{
#ifdef HX_RST_PIN_FUNC
	if (gpio_is_valid(private_ts->rst_gpio)) {
		I("%s\n", __func__);
		#ifdef CONFIG_TS_HIMAX_MTK_INTERFACE
		himax_rst_output(0);
		#else
		gpio_set_value(private_ts->rst_gpio, 0);
		#endif
	}
#endif
	return 0;
}
/* himax TP slef test*/

static int himax_test_init(void)
{
	I("%s:enter\n", __func__);
	himax_test_failed_node_buffer = kzalloc(TEST_RESULT_LENGTH, GFP_KERNEL);
	himax_test_temp_buffer = kzalloc(TEST_TEMP_LENGTH, GFP_KERNEL);
	himax_test_failed_node = kzalloc((ic_data->HX_TX_NUM * ic_data->HX_RX_NUM), GFP_KERNEL);
	if (himax_test_failed_node_buffer == NULL || himax_test_temp_buffer == NULL ||
		himax_test_failed_node == NULL) {
		if (himax_test_failed_node_buffer != NULL)
			kfree(himax_test_failed_node_buffer);
		if (himax_test_temp_buffer != NULL)
			kfree(himax_test_temp_buffer);
		if (himax_test_failed_node != NULL)
			kfree(himax_test_failed_node);
		E("%s:alloc memory failde!\n", __func__);
		return -ENOMEM;
	}
	himax_test_faied_buffer_length = 0;
	himax_test_failed_count = 0;
	himax_tptest_result = 0;
	return 0;
}

static void himax_test_buffer_free(void)
{
	I("%s:enter\n", __func__);
	if (himax_test_failed_node_buffer != NULL)
		kfree(himax_test_failed_node_buffer);
	if (himax_test_temp_buffer != NULL)
		kfree(himax_test_temp_buffer);
	if (himax_test_failed_node != NULL)
		kfree(himax_test_failed_node);
}

static int himax_save_failed_node_to_buffer(char *tmp_buffer, int length)
{

	if (himax_test_failed_node_buffer == NULL) {
		E("warning:himax_test_failed_node_buffer is null.");
		return -EPERM;
	}

	snprintf(himax_test_failed_node_buffer + himax_test_faied_buffer_length,
		(TEST_RESULT_LENGTH - himax_test_faied_buffer_length), tmp_buffer);
	himax_test_faied_buffer_length += length;
	himax_test_failed_count++;

	return 0;
}

int himax_save_failed_node(int failed_node)
{
	int i_len = 0;
	int tx = 0;
	int rx = 0;

	tx = failed_node / ic_data->HX_RX_NUM;
	rx = failed_node % ic_data->HX_RX_NUM;
	if (himax_test_failed_node == NULL)
		return -EPERM;
	if (himax_test_failed_node[failed_node] == 0) {
		if (himax_test_temp_buffer != NULL) {
			i_len = snprintf(himax_test_temp_buffer, TEST_TEMP_LENGTH, ",%d,%d", tx, rx);
			himax_save_failed_node_to_buffer(himax_test_temp_buffer, i_len);
			himax_test_failed_node[failed_node] = 1;
			return 0;
		} else {
			return -EPERM;
		}
	} else {
		return 0;
	}
}

static int tpd_test_save_file_path_store(struct tpd_classdev_t *cdev, const char *buf)
{
	memset(g_hx_save_file_path, 0, sizeof(g_hx_save_file_path));
	snprintf(g_hx_save_file_path, sizeof(g_hx_save_file_path), "%s", buf);

	I("save file path:%s.", g_hx_save_file_path);

	return 0;
}

static int tpd_test_save_file_path_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;

	num_read_chars = snprintf(buf, PAGE_SIZE, "%s\n", g_hx_save_file_path);

	return num_read_chars;
}

static int tpd_test_save_file_name_store(struct tpd_classdev_t *cdev, const char *buf)
{
	memset(g_hx_save_file_name, 0, sizeof(g_hx_save_file_name));
	snprintf(g_hx_save_file_name, sizeof(g_hx_save_file_name), "%s", buf);

	I("save file path:%s.", g_hx_save_file_name);

	return 0;
}

static int tpd_test_save_file_name_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;

	num_read_chars = snprintf(buf, PAGE_SIZE, "%s\n", g_hx_save_file_name);

	return num_read_chars;
}

static int tpd_test_cmd_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;
	int i_len = 0;

	I("%s:enter\n", __func__);
	i_len = snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d", himax_tptest_result, ic_data->HX_TX_NUM,
		ic_data->HX_RX_NUM,	 himax_test_failed_count);
	I("tpd test resutl:%d && rawdata node failed count:%d.\n", himax_tptest_result, himax_test_failed_count);

	if (himax_test_failed_node_buffer != NULL) {
		i_len += snprintf(buf + i_len, PAGE_SIZE - i_len, himax_test_failed_node_buffer);
	}
	E("tpd  test:%s.\n", buf);
	num_read_chars = i_len;
	return num_read_chars;
}

static int tpd_test_cmd_store(struct tpd_classdev_t *cdev, const char *buf)
{
	unsigned long  command = 0;
	int retval = 0;

	I("%s:enter\n", __func__);
	retval = kstrtoul(buf, 10, &command);
	if (retval) {
		E("invalid param:%s", buf);
		return -EIO;
	}
	if (command == TP_TEST_INIT) {
		retval = himax_test_init();
		if (retval < 0) {
			E("%s:alloc memory failde!\n", __func__);
			return -ENOMEM;
		}
	} else if (command == TP_TEST_START) {
		I("%s:start TP test.\n", __func__);
		himax_int_enable(0);
		private_ts->in_self_test = 1;
		g_core_fp.fp_chip_self_test();
#ifdef HX_ESD_RECOVERY
		HX_ESD_RESET_ACTIVATE = 1;
#endif
		himax_int_enable(1);
		private_ts->in_self_test = 0;

	} else if (command == TP_TEST_END) {
		himax_test_buffer_free();
	} else {
		E("invalid command %ld", command);
	}
	return 0;
}

static int tpd_test_channel_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;

	num_read_chars =
	    snprintf(buf, PAGE_SIZE, "%d %d", ic_data->HX_TX_NUM, ic_data->HX_RX_NUM);

	return num_read_chars;
}

static int himax_tp_suspend_show(struct tpd_classdev_t *cdev)
{
	struct himax_ts_data *ts = private_ts;

	cdev->tp_suspend = ts->suspended;
	return cdev->tp_suspend;
}

static int himax_set_tp_suspend(struct tpd_classdev_t *cdev, int enable)
{
	struct himax_ts_data *ts = private_ts;

	if (enable) {
		himax_chip_common_suspend(ts);
	} else {
		g_core_fp.fp_ic_reset(false, false);
		himax_chip_common_resume(ts);
	}
	cdev->tp_suspend = ts->suspended;
	return cdev->tp_suspend;
}

#ifdef HEADLINE_MODE
static int himax_headset_state_show(struct tpd_classdev_t *cdev)
{
	struct himax_ts_data *ts = private_ts;

	cdev->headset_state = ts->headset_state;
	return cdev->headset_state;
}

static int himax_set_headset_state(struct tpd_classdev_t *cdev, int enable)
{
	struct himax_ts_data *ts = private_ts;
	bool ret = false;

	ts->headset_state = enable;
	I("%s: headset_state = %d.\n", __func__, ts->headset_state);
	ret = g_core_fp.fp_headset_mode_set(ts->headset_state, ts->suspended);
	if (ret == true) {
		I("%s: headset writ success/n", __func__);
	} else {
		I("%s: headset writ fail/n", __func__);
	}

	return ts->headset_state;
}
#endif

#ifdef HX_USB_DETECT_GLOBAL
static bool himax_get_charger_ststus(void)
{
	static struct power_supply *batt_psy;
	union power_supply_propval val = { 0, };
	bool status = false;

	if (batt_psy == NULL)
		batt_psy = power_supply_get_by_name("battery");
	if (batt_psy) {
		batt_psy->desc->get_property(batt_psy, POWER_SUPPLY_PROP_STATUS, &val);
	}
	if ((val.intval == POWER_SUPPLY_STATUS_CHARGING) ||
		(val.intval == POWER_SUPPLY_STATUS_FULL)) {
		status = true;
	} else {
		status = false;
	}
	I("charger status:%d", status);
	return status;
}

static void himax_work_charger_detect_work(struct work_struct *work)
{
	USB_detect_flag = himax_get_charger_ststus();
}

static int himax_charger_notify_call(struct notifier_block *nb, unsigned long event, void *data)
{
	struct power_supply *psy = data;

	if (event != PSY_EVENT_PROP_CHANGED) {
		return NOTIFY_DONE;
	}

	if ((strcmp(psy->desc->name, "usb") == 0)
	    || (strcmp(psy->desc->name, "ac") == 0)) {
		if (delayed_work_pending(&private_ts->charger_work)) {
			return NOTIFY_DONE;
		}
		queue_delayed_work(private_ts->charger_wq, &private_ts->charger_work, msecs_to_jiffies(50));
	}

	return NOTIFY_DONE;
}

static int himax_init_charger_notifier(void)
{
	int ret = 0;

	I("Init Charger notifier");

	private_ts->charger_notifier.notifier_call = himax_charger_notify_call;
	ret = power_supply_reg_notifier(&private_ts->charger_notifier);
	return ret;
}

#endif

void himax_tpd_register_fw_class(void)
{
	I("tpd_register_fw_class\n");
#ifdef HX_USB_DETECT_GLOBAL
	private_ts->charger_wq = create_singlethread_workqueue("HMX_charger_detect");
	if (!private_ts->charger_wq) {
		E(" allocate charger_wq failed\n");
	} else  {
		USB_detect_flag = himax_get_charger_ststus();
		INIT_DELAYED_WORK(&private_ts->charger_work, himax_work_charger_detect_work);
		himax_init_charger_notifier();
	}
#endif
	tpd_fw_cdev.get_tpinfo = tpd_init_tpinfo;
#ifdef HX_SMART_WAKEUP
	tpd_fw_cdev.get_gesture = tpd_get_wakegesture;
	tpd_fw_cdev.wake_gesture = tpd_enable_wakegesture;
#endif
#ifdef HX_HIGH_SENSE
	tpd_fw_cdev.get_smart_cover = tpd_hsen_read;
	tpd_fw_cdev.set_smart_cover = tpd_hsen_write;
	tpd_fw_cdev.get_glove_mode = tpd_hsen_read;
	tpd_fw_cdev.set_glove_mode = tpd_hsen_write;
#endif
	tpd_fw_cdev.tp_i2c_16bor32b_reg_read = himax_i2c_reg_read;
	tpd_fw_cdev.tp_i2c_16bor32b_reg_write = himax_i2c_reg_write;
	tpd_fw_cdev.reg_char_num = REG_CHAR_NUM_8;
	tpd_fw_cdev.tp_fw_upgrade = himax_tp_fw_upgrade;
	tpd_fw_cdev.tpd_gpio_shutdown = himax_gpio_shutdown_config;
	tpd_fw_cdev.tpd_suspend_need_awake = himax_suspend_need_awake;
	tpd_fw_cdev.tp_suspend_show = himax_tp_suspend_show;
	tpd_fw_cdev.set_tp_suspend = himax_set_tp_suspend;
#ifdef HEADLINE_MODE
	tpd_fw_cdev.headset_state_show = himax_headset_state_show;
	tpd_fw_cdev.set_headset_state = himax_set_headset_state;
#endif

	tpd_fw_cdev.tpd_test_set_save_filepath = tpd_test_save_file_path_store;
	tpd_fw_cdev.tpd_test_get_save_filepath = tpd_test_save_file_path_show;
	tpd_fw_cdev.tpd_test_set_save_filename = tpd_test_save_file_name_store;
	tpd_fw_cdev.tpd_test_get_save_filename = tpd_test_save_file_name_show;
	tpd_fw_cdev.tpd_test_set_cmd = tpd_test_cmd_store;
	tpd_fw_cdev.tpd_test_get_cmd = tpd_test_cmd_show;
	tpd_fw_cdev.tpd_test_get_channel_info = tpd_test_channel_show;
	snprintf(g_hx_save_file_path, sizeof(g_hx_save_file_path), "%s", HX_RSLT_OUT_PATH);
	snprintf(g_hx_save_file_name, sizeof(g_hx_save_file_name), "%s", HX_RSLT_OUT_FILE);
}
