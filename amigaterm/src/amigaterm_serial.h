#ifndef __AMIGATERM_SERIAL_H__
#define __AMIGATERM_SERIAL_H__

typedef enum {
	SERIAL_RET_OK = 0,
	SERIAL_RET_TIMEOUT = 1,
	SERIAL_RET_ABORT = 2,
} serial_retval_t;

/* Control routines */
extern int serial_init(int baud, int enable_hwflow);
extern void serial_set_baud(int baud);
extern void serial_close(void);

/* Read routines */
extern void serial_read_start(void);
extern void serial_read_start_buf(char *buf, int len);
extern int serial_get_char(char *ch);
extern unsigned int serial_get_read_signal_bitmask(void);
extern void serial_read_abort(void);
extern int serial_read_is_ready(void);
extern int serial_read_wait(void);
extern int serial_read_ready(void);

/* Write routines */
extern unsigned int serial_get_write_signal_bitmask(void);
extern void serial_write_abort(void);
extern void serial_write_char(char c);
extern int serial_write_is_ready(void);
extern int serial_write_wait(void);
extern int serial_write_ready(void);
extern void serial_write_start_buf(char *buf, int len);
#endif
