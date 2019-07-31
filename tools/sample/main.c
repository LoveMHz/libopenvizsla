/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ftdi.h>

#include <signal.h>

#include <cha.h>
#include <chb.h>

#define OV_VENDOR  0x1d50
#define OV_PRODUCT 0x607c

#define PORTB_DONE_BIT     (1 << 2)  // GPIOH2
#define PORTB_INIT_BIT     (1 << 5)  // GPIOH5

static void packet_handler(struct ov_packet* packet, void* data) {
	printf("[%04x] Received %d bytes at %d:", packet->flags, packet->size, packet->timestamp);
	for (int i = 0; i < packet->size; ++i)
		printf(" %02x", packet->data[i]);
	printf("\n");
}

static void sighandler(int signum) {
	printf("Caught signal %d\n", signum);
}

int main(int argc, char** argv) {
	unsigned char buf[] = {0x55, 0x04, 0x01, 0x00, 0x5a};
	unsigned char inp_buf[4096];
	int ret;
	enum ov_usb_speed speed = OV_LOW_SPEED;

	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);

	struct cha cha;
	struct chb chb;

	struct ftdi_version_info version;

	ret = cha_init(&cha, "");
	if (ret == -1) {
		fprintf(stderr, cha_get_error_string(&cha));
		return 1;
	}

	ret = chb_init(&chb);
	if (ret == -1) {
		fprintf(stderr, chb_get_error_string(&chb));
		return 1;
	}

	ret = cha_open(&cha);
	if (ret == -1) {
		fprintf(stderr, cha_get_error_string(&cha));
		return 1;
	}

	ret = chb_open(&chb);
	if (ret == -1) {
		fprintf(stderr, chb_get_error_string(&chb));
		return 1;
	}

	uint8_t status;
	ret = chb_get_status(&chb, &status);
	if (ret == -1) {
		fprintf(stderr, chb_get_error_string(&chb));
		return 1;
	}

	printf("%x %d %d\n", status, status & PORTB_INIT_BIT, status & PORTB_DONE_BIT);

	ret = cha_switch_fifo_mode(&cha);
	if (ret == -1) {
		fprintf(stderr, "cha_switch_fifo_mode %s\n", cha_get_error_string(&cha));
		return 1;
	}

	ret = cha_stop_stream(&cha);
	if (ret == -1) {
		fprintf(stderr, "cha_stop_stream %s\n", cha_get_error_string(&cha));
		return 1;
	}

	ret = cha_set_usb_speed(&cha, speed);
	if (ret == -1) {
		fprintf(stderr, "cha_set_usb_speed %s\n", cha_get_error_string(&cha));
		return 1;
	}

	ret = cha_get_usb_speed(&cha, &speed);
	if (ret == -1) {
		fprintf(stderr, "cha_get_usb_speed %s\n", cha_get_error_string(&cha));
		return 1;
	}
	printf("USB speed: %x\n", speed);

	ret = cha_start_stream(&cha);
	if (ret == -1) {
		fprintf(stderr, "cha_start_stream %s\n", cha_get_error_string(&cha));
		return 1;
	}

	union {
		struct ov_packet packet;
		char buf[1024];
	} p;
	struct cha_loop cha_loop;

	ret = cha_loop_init(&cha_loop, &cha, &p.packet, sizeof(p), &packet_handler, NULL);
	if (ret == -1) {
		fprintf(stderr, "cha_loop_init %s\n", cha_get_error_string(&cha));
		return 1;
	}

	printf("Start looping\n");

	ret = cha_loop_run(&cha_loop, 100);
	if (ret == -1) {
		fprintf(stderr, "cha_loop_run %s\n", cha_get_error_string(&cha));
		return 1;
	}

	printf("Stop looping\n");

	ret = cha_stop_stream(&cha);
	if (ret == -1) {
		fprintf(stderr, "cha_stop_stream %s\n", cha_get_error_string(&cha));
		return 1;
	}

	chb_destroy(&chb);
	cha_destroy(&cha);

	return 0;
}
