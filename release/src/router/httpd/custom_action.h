#ifndef _custom_action_h
#define _custom_action_h
#endif

#define CONFIG_FILE_BASE_PATH "/jffs/addons/"
#define AMNG_CUSTOM_TXT "custom_settings.txt"
#define APP_CENTRE_TXT "app_centre.txt"
#define APP_CONFIG_TXT "configs.txt"

enum FILE_OPS_RET {
	FILE_OPS_RET_OK,
	FILE_OPS_RET_NOFILE,
	FILE_OPS_RET_WRITE_FAIL
};

enum CUSTOM_OPS_RET {
	CUSTOM_OPS_RET_OK,
	CUSTOM_OPS_RET_INVALID_REQUEST,
	CUSOTM_OPS_RET_INVALID_CONFIGS,
	CUSTOM_OPS_RET_FILE_NOFILE,
	CUSTOM_OPS_RET_FILE_WRITE_FAIL
};

void custom_action(webs_t wp, struct json_object *root);