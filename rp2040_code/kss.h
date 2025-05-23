#ifndef	kss_h
#define	kss_h


#include	<stdint.h>
#include	<stdio.h>
#include 	"pico/stdlib.h"

// For kss_loop_v1
#define WAIT_BEFORE_LOAD_CYCLES 11000
#define WAIT_LOAD_CYCLES 1000
#define WAIT_AFTER_LOAD_CYCLES 1000
#define CLK_HIGH_CYCLES 3
#define CLK_LOW_CYCLES 9


#define SEMI_CLK_PERIOD_US 15 //Which means one read is around SEMI_CLK_PERIOD_US * 13000
#define WAIT_FOR_SETTLE_MS 300
#define RESEND_WHEN_UNCHANGED_S 10

typedef	struct {
	uint64_t	ts;
	int		maxv;
	int		*vals;
	int		clk;
	int		clk_inv;
	int		inh;
	int		inh_inv;
	int		ld;
	int		ld_inv;
	int		qh;
	int		qh_inv;
	int		clkv, inhv, ldv, qhv;

	int		valc;
	int		state;
	int		tick;
	int		*oldvals;
	int		*averVals;
	int 	*sumVals;
} shiftr;

#ifdef	kss_c
	shiftr	serpar = {
		ts:0, 
		maxv:16, 
		vals:NULL,
		oldvals:NULL,
		clk:11, clk_inv:0,			// CLK 11
		inh:4, inh_inv:0,			// INH 4
		ld:10, ld_inv:0,			// LD 10
		qh:5, qh_inv:0,			// QH 5
	};
	int	kss_debug = 0;
	uint64_t last_kss_change_tm = 0;
	uint64_t last_kss_send_tm = 0;
	char kss_str[64] = "KSS()\r\n";
#else
extern	shiftr	serpar;
extern	int		kss_debug;
#endif
int	kss_loop(uint64_t semi_period, shiftr *sp);
int	kss_loop_v1(uint64_t semi_period, shiftr *sp);
int	kss_checkchanges(shiftr *sp);
void	prbits(int x);


void prepKssStr(int kss_count, int deb);
void side_core();
void printKSS(int kss_count);
#endif