#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json.h>
#include <httpd.h>
#include <common.h>
#include "custom_action.h"

// Dont know why, but have to use this to get json_value instead of json-c API
extern char *get_cgi_json(char *name, json_object *root);

// Read Config file
static enum FILE_OPS_RET read_custom_config(struct json_object *settings, char *file_path) {
	FILE *fp = NULL;
	char line[3040];
	char name[30];
	char value[3000];
	enum FILE_OPS_RET ret;

	fp = fopen(file_path, "r");

	if (!fp) {
		ret = FILE_OPS_RET_NOFILE;
		goto CLOSE;
	}

	// Mostly copied from RMerlin's Code
	while (fgets(line, sizeof(line), fp)) {
		if (sscanf(line, "%29s%*[ ]%2999s%*[ \n]", name, value) == 2) {
			json_object_object_add(settings, name, json_object_new_string(value));
		}
	}

	ret = FILE_OPS_RET_OK;

CLOSE:
	if (fp)
		fclose(fp);

	return ret;
}

// Write config File
static enum FILE_OPS_RET write_custom_config(struct json_object *settings, char *file_path) {
	FILE *fp = NULL;
	char line[3040];
	enum FILE_OPS_RET ret;

	fp = fopen(file_path, "w");

	if (!fp) {
		ret = FILE_OPS_RET_WRITE_FAIL;
		goto CLOSE;
	}

	// Mostly copied from RMerlin's Code
	json_object_object_foreach(settings, key, val) {
		snprintf(line, sizeof(line), "%s %s\n", key, json_object_get_string(val));
		fwrite(line, sizeof(char), strlen(line), fp);
	}

	ret = FILE_OPS_RET_OK;

CLOSE:
	if (fp)
		fclose(fp);
	
	return ret;
}

// Config file read/write resp state
static void file_ops_resp(enum FILE_OPS_RET ret, struct json_object *resp) {
	switch (ret) {
		case FILE_OPS_RET_OK:
			json_object_object_add(resp, "state", json_object_new_int(CUSTOM_OPS_RET_OK));
			break;
		case FILE_OPS_RET_NOFILE:
			json_object_object_add(resp, "state", json_object_new_int(CUSTOM_OPS_RET_FILE_NOFILE));
			break;
		case FILE_OPS_RET_WRITE_FAIL:
			json_object_object_add(resp, "state", json_object_new_int(CUSTOM_OPS_RET_FILE_WRITE_FAIL));
			break;
	}
}

// read config file
static void custom_get(char *config_type, struct json_object *resp) {
	char config_path[100];
	struct json_object *config_file = json_object_new_object();
	enum FILE_OPS_RET ret;
	
	if (!strcmp(config_type, "amng_custom")) {
		// ASUSWRT Merlin's addon config
		snprintf(config_path, sizeof(config_path), "%s%s", CONFIG_FILE_BASE_PATH, AMNG_CUSTOM_TXT);
		HTTPD_DBG("get_amng_custom: %s\n", config_path);
		ret = read_custom_config(config_file, config_path);
		if (ret == FILE_OPS_RET_OK) {
			json_object_object_add(resp, "configs", config_file);
		}
		goto DONE;
			
	} else if (!strcmp(config_type, "app_centre")) {
		// App centre config
		snprintf(config_path, sizeof(config_path), "%s%s", CONFIG_FILE_BASE_PATH, APP_CENTRE_TXT);
		HTTPD_DBG("get_app_centre: %s\n", config_path);
		ret = read_custom_config(config_file, config_path);
		if (ret == FILE_OPS_RET_OK) {
			json_object_object_add(resp, "configs", config_file);
		}
		goto DONE;
			
	} else {
		// app centre apps config
		snprintf(config_path, sizeof(config_path), "%s%s/%s", CONFIG_FILE_BASE_PATH, config_type, 
		APP_CONFIG_TXT);
		HTTPD_DBG("get_app_centre_apps: %s\n", config_path);
		ret = read_custom_config(config_file, config_path);
		if (ret == FILE_OPS_RET_OK) {
			json_object_object_add(resp, "configs", config_file);
		}
		goto DONE;
	}

DONE:
	// set resp state with file ops return
	file_ops_resp(ret, resp);
	if (config_file)
		json_object_put(config_file);
}

// remove key: val in config file
static void custom_set_remove(struct json_object *config_file, struct json_object *set_vals, struct 
json_object *deleted_keys) {
	char *key2del = NULL;
	struct json_object *val2del_obj = NULL;
	
	for (int i = 0; i < json_object_array_length(set_vals); i++) {
		key2del = json_object_get_string(json_object_array_get_idx(set_vals, i));
		// found the key
		if (key2del != NULL && json_object_object_get_ex(config_file, key2del, &val2del_obj)) {
			json_object_object_del(config_file, key2del);
			json_object_array_add(deleted_keys, json_object_new_string(key2del));
		}
	}
}

// add or overwite config_file
static void custom_set_add(struct json_object *config_file, struct json_object *set_vals) {
	json_object_object_foreach(set_vals, key, val) {
		json_object_object_add(config_file, key, val);
	}
}

static void custom_set(char *config_type, struct json_object *set_vals, struct json_object *resp) {
	char config_path[100];
	struct json_object *config_file = json_object_new_object();
	struct json_object *deleted_keys = json_object_new_array();
	enum FILE_OPS_RET ret;
	enum json_type set_vals_type = json_object_get_type(set_vals);

	// array is remove, object is set, other is invalid
	if (set_vals_type != json_type_array || set_vals_type != json_type_object)
		goto INVALID_CONFIGS;

	// if setval is an array, then it is remove action
	// check setval is valid
	if (set_vals_type == json_type_array) {			
		// check if the array contain string only
		for (int i = 0; i < json_object_array_length(set_vals); i++) {
			if (json_object_get_type(json_object_array_get_idx(set_vals, i)) != json_type_string)
				goto INVALID_CONFIGS;
		}
	}
			
	// if setval is an object, then it is set action
	// check setval is valid
	if (set_vals_type == json_type_object) {
		// check object key is string only
		json_object_object_foreach(set_vals, key, val) {
			if (json_object_get_type(val) != json_type_string)
				goto INVALID_CONFIGS;
		}
	}

	if (!strcmp(config_type, "amng_custom")) {
		// ASUSWRT Merlin's addon config
		snprintf(config_path, sizeof(config_path), "%s%s", CONFIG_FILE_BASE_PATH, AMNG_CUSTOM_TXT);
		HTTPD_DBG("set_amng_custom: %s\n", config_path);
		ret = read_custom_config(config_file, config_path);
		if (ret == FILE_OPS_RET_OK) {
			// remove action
			if (set_vals_type == json_type_array) {
				HTTPD_DBG("keys to del: %s\n", json_object_to_json_string(set_vals));
				custom_set_remove(config_file, set_vals, deleted_keys);
				json_object_object_add(resp, "deleted_keys", deleted_keys);
			}

			// set or overwrite
			if (set_vals_type == json_type_object) {
				HTTPD_DBG("keys to add: %s\n", json_object_to_json_string(set_vals));
				custom_set_add(config_file, set_vals);
			}
		}
		goto CONFIG_OK;
		
	} else if (!strcmp(config_type, "app_centre")) {
		// App centre config
		snprintf(config_path, sizeof(config_path), "%s%s", CONFIG_FILE_BASE_PATH, APP_CENTRE_TXT);
		HTTPD_DBG("set_app_centre: %s\n", config_path);
		ret = read_custom_config(config_file, config_path);
		if (ret == FILE_OPS_RET_OK) {
			// remove action
			if (set_vals_type == json_type_array) {
				HTTPD_DBG("keys to del: %s\n", json_object_to_json_string(set_vals));
				custom_set_remove(config_file, set_vals, deleted_keys);
				json_object_object_add(resp, "deleted_keys", deleted_keys);
			}

			// set or overwrite
			if (set_vals_type == json_type_object) {
				HTTPD_DBG("keys to add: %s\n", json_object_to_json_string(set_vals));
				custom_set_add(config_file, set_vals);
			}
		}
		goto CONFIG_OK;

	} else {
		// app centre apps config
		snprintf(config_path, sizeof(config_path), "%s%s/%s", CONFIG_FILE_BASE_PATH, config_type, 
		APP_CONFIG_TXT);
		HTTPD_DBG("set_app_centre_apps: %s\n", config_path);
		ret = read_custom_config(config_file, config_path);
		if (ret == FILE_OPS_RET_OK) {
			// remove action
			if (set_vals_type == json_type_array) {
				HTTPD_DBG("keys to del: %s\n", json_object_to_json_string(set_vals));
				custom_set_remove(config_file, set_vals, deleted_keys);
				json_object_object_add(resp, "deleted_keys", deleted_keys);
			}

			// set or overwrite
			if (set_vals_type == json_type_object) {
				HTTPD_DBG("keys to add: %s\n", json_object_to_json_string(set_vals));
				custom_set_add(config_file, set_vals);
			}
		}
		goto CONFIG_OK;

	}

INVALID_CONFIGS:
	json_object_object_add(resp, "state", json_object_new_int(CUSOTM_OPS_RET_INVALID_CONFIGS));
	goto DONE;

CONFIG_OK:
	// config format check passed, but file ops may fail
	ret = write_custom_config(config_file, config_path);
	file_ops_resp(ret, resp);

DONE:
	if (config_file)
		json_object_put(config_file);
	if (deleted_keys)
		json_object_put(deleted_keys);
}

void custom_action(webs_t wp, struct json_object *root) {
	struct json_object *resp = json_object_new_object();
	struct json_object *set_vals_obj = NULL;
	char *set_vals = NULL, *action_mode = NULL, *config_type = NULL;

	action_mode = get_cgi_json("action_mode", root);
	config_type = get_cgi_json("config_type", root);

	if (action_mode == NULL || config_type == NULL)
		goto INVALID_REQUEST;

	HTTPD_DBG("action_mode: %s, config_type: %s\n", action_mode, config_type);

	if (!strcmp(action_mode, "getAll")) {
		// Get all settings from a setting file, the process it with js on browser
		custom_get(config_type, resp);
		goto DONE;
			
	} else if (!strcmp(action_mode, "set")) {
		// Set single or multiple value, accept an object of string or an array for delete
		// check if config object in request

		// have to use get_cgi_json
		// So no direct nested object, have to pass configs as json_string then convert to json obj
		set_vals = get_cgi_json("configs", root);
		
		if (set_vals == NULL)
			goto INVALID_REQUEST;

		set_vals_obj = json_tokener_parse(set_vals);

		if (set_vals_obj == NULL)
			goto INVALID_REQUEST;
			
		custom_set(config_type, set_vals_obj, resp);
		goto DONE;
			
	}  else {
		goto INVALID_REQUEST;
	}

	
INVALID_REQUEST:
	json_object_object_add(resp, "state", json_object_new_int(CUSTOM_OPS_RET_INVALID_REQUEST));
	
DONE:
	websWrite(wp, "%s\n", json_object_to_json_string(resp));
	if (resp)
		json_object_put(resp);
	if (set_vals_obj)
		json_object_put(set_vals_obj);
}
