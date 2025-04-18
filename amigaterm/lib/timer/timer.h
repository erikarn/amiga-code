#ifndef __TIMER_H__
#define __TIMER_H__

extern int timer_init(void);
extern void timer_close(void);
extern void timer_timeout_set(int ms);
extern unsigned int timer_get_signal_bitmask(void);
extern void timer_timeout_abort(void);
extern int timer_timeout_fired(void);
extern int timer_timeout_complete(void);

#endif
