/*
 ============================================================================
 Name        : usbauth_configparser.c
 Author      : Stefan Koch <skoch@suse.de>
 Version     : 1.0
 Copyright   : 2015 SUSE Linux GmbH
 Description : library for USB Firewall including flex/bison parser
 ============================================================================
 */

#include "generic.h"
#include "usbauth_configparser.h"
#include "usbauth_lang.tab.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libudev.h>
#include <dbus/dbus.h>

#define CONFIG_FILE "/etc/usbauth.conf"

uint8_t gen_length;
struct Auth *gen_auths;

extern FILE *usbauth_yyin;

const char* parameter_strings[] = {"INVALID", "busnum", "devpath", "idVendor", "idProduct", "bDeviceClass", "bDeviceSubClass", "bDeviceProtocol", "bConfigurationValue", "bInterfaceNumber", "bInterfaceClass", "bInterfaceSubClass", "bInterfaceProtocol", "devnum", "serial", "intfcount", "devcount", "PARAM_NUM_ITEMS"};
const char* operator_strings[] = {"==", "!=", "<=", ">=", "<", ">", "OP_NUM_ITEMS"};

bool usbauth_dbus_no_error_check(DBusError *error) {
	bool ret = true;

	if(dbus_error_is_set(error)) {
		ret = false;
		printf("error %s", error->message);
		dbus_error_free(error);
	}

	return ret;
}

const char* usbauth_get_param_valStr(enum Parameter param, struct udev_device *udevdev) {
	struct udev_device *parent = NULL;
	const char* paramStr = usbauth_param_to_str(param);
	const char* valStr = NULL;

	if(udevdev)
		valStr = udev_device_get_sysattr_value(udevdev, paramStr);

	if(!valStr) {
		parent = udev_device_get_parent(udevdev);
		valStr = udev_device_get_sysattr_value(parent, paramStr);
	}

	return valStr;
}

int usbauth_get_param_val(enum Parameter param, struct udev_device *udevdev) {
	int val = -1;
	const char* valStr = usbauth_get_param_valStr(param, udevdev);
	char* end = NULL;

	if(valStr)
		val = strtol(valStr, &end, 16);

	if(end && *end != 0)
		val = -1;

	return val;
}

int usbauth_str_to_enum(const char *string, const char** string_array, unsigned array_len) {
	enum Parameter ret = INVALID;

	unsigned i;
	for (i = 0; i < array_len; i++) {
		if (strcmp(string, string_array[i]) == 0) {
			ret = i;
			break;
		}
	}

	return ret;
}

const char* usbauth_enum_to_str(int val, const char** string_array, unsigned array_len) {
	const char* ret = string_array[0];

	if (val < array_len)
		ret = string_array[val];

	return ret;
}

enum Parameter usbauth_str_to_param(const char *string) {
	return usbauth_str_to_enum(string, parameter_strings, sizeof(parameter_strings)/sizeof(const char*));
}

const char* usbauth_param_to_str(enum Parameter param) {
	return usbauth_enum_to_str(param, parameter_strings, sizeof(parameter_strings)/sizeof(const char*));
}

enum Operator usbauth_str_to_op(const char *string) {
	return usbauth_str_to_enum(string, operator_strings, sizeof(operator_strings)/sizeof(const char*));
}

const char* usbauth_op_to_str(enum Operator op) {
	return usbauth_enum_to_str(op, operator_strings, sizeof(operator_strings)/sizeof(const char*));
}

bool usbauth_convert_str_to_data(struct Data *d, const char *paramStr, const char* opStr, const char *valStr) {
	bool ret = true;

	if(!d || !paramStr || !valStr)
		return false;

	d->param = usbauth_str_to_param(paramStr);
	if (d->param == INVALID)
		ret = false;

	d->op = usbauth_str_to_op(opStr);

	d->val = valStr;

	return ret;
}

const char* usbauth_auth_to_str(const struct Auth *auth) {
	char v[16];
	const unsigned str_len = 512;
	char *str = calloc(str_len, sizeof(char));

	strcpy(str, "");
	strcpy(v, "");

	if (auth->type == COND)
		strncat(str, "condition", str_len);
	else if (auth->type == ALLOW)
		strncat(str, "allow", str_len);
	else if (auth->type == DENY)
		strncat(str, "deny", str_len);

	if (auth->type != COMMENT)
		strcat(str, " ");

	struct Data* cond_array = auth->cond_array;
	if (auth->type == COND) {
		int k;
		for (k = 0; k < auth->cond_len; k++) {
			strncat(str, parameter_strings[cond_array[k].param], str_len);
			strncat(str, operator_strings[cond_array[k].op], str_len);
			sprintf(v, "%s", cond_array[k].val);
			strncat(str, v, str_len);
			strncat(str, " ", str_len);
		}

		strncat(str, "case ", str_len);
	}
	struct Data* attr_array = auth->attr_array;
	int j;
	for (j = 0; j < auth->attr_len; j++) {
		strncat(str, attr_array[j].anyChild ? "anyChild " : "", str_len);
		strncat(str, parameter_strings[attr_array[j].param], str_len);
		strncat(str, operator_strings[attr_array[j].op], str_len);
		sprintf(v, "%s", attr_array[j].val);
		strncat(str, v, str_len);
		strncat(str, " ", str_len);
	}

	if ((auth->type == ALLOW || auth->type == DENY) && auth->attr_len == 0)
		strncat(str, "all", str_len);

	if(auth->comment)
		strncat(str, auth->comment, str_len);

	return str;
}

void usbauth_allocate_and_copy(struct Auth** destination, const struct Auth* source, unsigned length) {
	struct Auth *arr = NULL;

	if (length)
		arr = calloc(length, sizeof(struct Auth));

	if (arr) {
		memcpy(arr, source, length * sizeof(struct Auth));

		arr->attr_array = NULL;
		if (arr->attr_len)
			arr->attr_array = calloc(arr->attr_len, sizeof(struct Data));
		if (arr->attr_array)
			memcpy(arr->attr_array, source->attr_array,
					arr->attr_len * sizeof(struct Data));

		arr->cond_array = NULL;
		if (arr->cond_len)
			arr->cond_array = calloc(arr->cond_len, sizeof(struct Data));
		if (arr->cond_array)
			memcpy(arr->cond_array, source->cond_array,
					arr->cond_len * sizeof(struct Data));
	}

	*destination = arr;
}

int usbauth_config_free() {
	int ret = -1;

	if(gen_auths) {
		usbauth_config_free_auths(gen_auths, gen_length);
		gen_auths = NULL;
		gen_length = 0;
		ret = 0;
	}

	return ret;
}

int usbauth_config_read() {
	usbauth_yyin = fopen(CONFIG_FILE, "r");

	if(!usbauth_yyin)
		return -1;

	usbauth_config_free();

	int ret = usbauth_yyparse();

	fclose(usbauth_yyin);

	return ret;
}

int usbauth_config_write() {
	FILE* fout = fopen(CONFIG_FILE "1", "w");

	if(!fout)
		return -1;

	int i = 0;
	for (i = 0; i < gen_length; i++) {
		const char *str = usbauth_auth_to_str(&gen_auths[i]);
		fprintf(fout, "%s\n", str);
		free((char*)str);
		str = NULL;
	}
	fclose(fout);

	return 0;
}

void usbauth_config_free_auths(struct Auth* auths, unsigned length) {
	unsigned i;
	for (i = 0; i < length; i++) {
		free(auths[i].attr_array);
	}
	free(auths);
}

void usbauth_config_get_auths(struct Auth** auths, unsigned *length) {
	usbauth_allocate_and_copy(auths, gen_auths, gen_length);
	*length = gen_length;
}

void usbauth_config_set_auths(struct Auth* auths, unsigned length) {
	usbauth_config_free_auths(gen_auths, gen_length);
	usbauth_allocate_and_copy(&gen_auths, auths, length);
	gen_length = length;
}

