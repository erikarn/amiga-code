#ifndef __AMIGATERM_SERIAL_H__
#define __AMIGATERM_SERIAL_H__

extern int serial_init(void);
extern void serial_read_start(void);
extern int serial_get_char(void);
extern unsigned int serial_get_signal_bitmask(void);
extern void serial_write_char(char c);
extern void serial_read_abort(void);
extern void serial_set_baud(int baud);
extern void serial_close(void);
extern int serial_read_is_ready(void);

#endif
