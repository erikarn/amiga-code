#ifndef	__AMIGATERM_SERIAL_READ_H__
#define	__AMIGATERM_SERIAL_READ_H__

extern	serial_retval_t readchar_sched(int schedule_next, int timeout_ms,
	    unsigned char *ch);
extern	serial_retval_t readchar_flush(int timeout_ms);
extern	serial_retval_t readchar_buf(char *buf, int len);
extern	serial_retval_t readchar(unsigned char *ch);

#endif	/* __AMIGATERM_SERIAL_READ_H__ */
