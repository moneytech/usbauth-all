/*
 * generic.h
 *
 *  Created on: 22.01.2015
 *      Author: stefan
 */

#ifndef GENERIC_H_
#define GENERIC_H_


#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>

enum Parameters {
	INVALID, busnum, devpath, idVendor, idProduct, bDeviceClass, bDeviceSubClass, bDeviceProtocol, bConfigurationValue, bInterfaceNumber, bInterfaceClass, bInterfaceSubClass, bInterfaceProtocol, count
};

enum Operator { eq, neq, lt, gt, l, g };

struct Data {
	int param;
	enum Operator op;
	unsigned val;
};

enum Type { COMMENT, DENY, ALLOW, COND };

struct Auth {
	bool valid;
	enum Type type;
	const char *comment;
	uint8_t count;
	uint8_t attr_len;
	struct Data *attr_array;
	uint8_t cond_len;
	struct Data *cond_array;
};

struct match_ret {
	bool match_attrs:1;
	bool match_cond:1;
};

struct auth_ret {
	bool match;
	bool allowed;
};

#endif /* GENERIC_H_ */
