#ifndef __AMIGATERM_SERIAL_H__
#define __AMIGATERM_SERIAL_H__

extern int serial_init(int baud);
extern void serial_read_start(char *l);
extern void serial_read_start_buf(char *buf, int len);
extern int serial_get_char(char *ch);
extern unsigned int serial_get_signal_bitmask(void);
extern void serial_write_char(char c);
extern void serial_read_abort(void);
extern void serial_set_baud(int baud);
extern void serial_close(void);
extern int serial_read_is_ready(void);
extern int serial_read_wait(void);
extern int serial_read_ready(void);

#endif
