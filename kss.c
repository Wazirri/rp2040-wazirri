#define	kss_c

#include	"kss.h"

#include	<string.h>
#include	<stdlib.h>

#include	"hardware/gpio.h"
#include	"hardware/timer.h"

//Timing Debug
	uint64_t sys_time_s = 0;
//

void	prbits(int x) {
	for (int i = 0; i < 8; i++) {
		if (x & (1 << i))
			printf("%d", i);
		else
			printf("_");
	}
}

int	kss_loop(uint64_t semi_period, shiftr *sp) {
	uint64_t	us_now;
	static	int	a = 0;
	
	if (!sp) {
	//	printf("NULL SHIFTR!! \r\n");
		return 0;
	}
	us_now  = time_us_64();
	
	if (!sp->ts) {
		sp->ts = us_now;
		if (!sp->vals) {
			sp->vals = calloc(sizeof(int), sp->maxv);
			sp->oldvals = calloc(sizeof(int), sp->maxv);
			for (int i = 0; i < sp->maxv; i++) 
				sp->oldvals[i] = -1;
		}
		gpio_init(sp->clk);
		gpio_init(sp->inh);
		gpio_init(sp->ld);
		gpio_init(sp->qh);

		gpio_set_dir(sp->clk, sp->clkv = 1);
		gpio_set_dir(sp->inh, sp->inhv = 1);
		gpio_set_dir(sp->ld, sp->ldv = 1);
		gpio_set_dir(sp->qh, 0);

		gpio_put(sp->clk, sp->clkv = 1);
		gpio_put(sp->inh, sp->inhv = 1);
		gpio_put(sp->ld, sp->ldv = 1);
		sp->qhv = -1;
		sp->state = 0;
		sp->tick = 0;
		sp->valc = 0;
		sp->vals[sp->valc = 0] = 0;
	}
	if (us_now <= (sp->ts + semi_period))
		return -1;
	sp->ts = us_now;
	if (kss_debug)
		printf("%4d > ", sp->tick);
	sp->clkv = sp->tick & 1;
	gpio_put(sp->clk, sp->clkv);
	
	switch (sp->state) {
	case 0:
		switch (sp->tick) {
		case 2:
			gpio_put(sp->ld, sp->ldv = 0);
			break;
		case 4:
			gpio_put(sp->ld, sp->ldv = 1);
			break;
		case 7:
			gpio_put(sp->inh, sp->inhv = 0);
			sp->state = 1;
			sp->tick = 0;
			sp->vals[sp->valc = 0] = 0;

			break;
		}
		break;
	}
	sp->qhv = gpio_get(sp->qh) ? 1 : 0;
	if (kss_debug) {
		printf("C[%c]", sp->clkv ? '#' : '_');
		printf("I[%c]", sp->inhv ? '1' : '0');
		printf("L[%c]", sp->ldv ? '1' : '0');
	}

	if (sp->state) {
		if (kss_debug)
			printf(" Q[%c]", sp->qhv ? '1' : '0');
		if (sp->tick & 1) {
			sp->vals[sp->valc] |= sp->qhv << (sp->state - 1); 
			if (kss_debug) {
				printf(" %d %d ", sp->vals[sp->valc], sp->state);
				prbits(sp->vals[sp->valc]);
			}
			sp->state++;
		} 
		if (sp->state == 9) {
			if (sp->vals[sp->valc] == 0) {
				// NO DEVICE
				sp->ts = 0;
				sp->valc = 0;
				return 0;
			} else if ((sp->vals[sp->valc] == 255) || (sp->valc + 1 >= sp->maxv)) {
				sp->ts = 0;
				if (kss_debug)
					printf("\r\n");						
				return sp->valc;
			} 
			sp->valc++;
			sp->vals[sp->valc] = 0;
			sp->state = 1;
		}
	}
	sp->tick++;

	if (kss_debug)
		printf("\r\n");	
	return -1;
}

int	kss_checkchanges(shiftr *sp) {
	int32_t	rv;
	if (!sp)
		return 0;
	if (!sp->vals || !sp->oldvals)
		return 0;
	rv = memcmp(sp->vals, sp->oldvals, sizeof(int) * sp->valc);
	memcpy(sp->oldvals, sp->vals, sizeof(int) * sp->valc);
	return rv;
}

int kss_count = -1;
int is_kss_changed = 0;
int unsent_kss_change = 0;
int k_count = 0;

void side_core() {

	sleep_ms(2000);
	printf("side_core()\n");
    int kss_count = -1;
    int is_kss_changed = 0;
    int unsent_kss_change = 0;
    int k_count = 0;
	while (true) {
		if ((k_count = kss_loop_v1((uint64_t) SEMI_CLK_PERIOD_US, &serpar)) == -1) {
			continue;
		}
		sleep_ms(5);
		is_kss_changed = 0;
        if (k_count != kss_count) 
            is_kss_changed = 1;
        is_kss_changed |= kss_checkchanges(&serpar);		
        if (is_kss_changed) {
            kss_count = k_count;
            unsent_kss_change = 1;
			prepKssStr(kss_count, kss_debug);
            last_kss_change_tm = time_us_64();
			continue;	// nn
        }
        if (unsent_kss_change) {
            if(time_us_64() > (last_kss_change_tm + (WAIT_FOR_SETTLE_MS * 1000))) {
                printf(kss_str);
                last_kss_send_tm = time_us_64();
                unsent_kss_change = 0;
            }
        }
        else if (time_us_64() > (last_kss_send_tm + RESEND_WHEN_UNCHANGED_S * 1000 * 1000)) {
            printf(kss_str);
            last_kss_send_tm = time_us_64();
        }

    }
	
}

void printKSS(int kss_count){
	if (!kss_count) {
		printf("KSS()\r\n");
		return;
	} else if (kss_count > 0) {

		// printf("KSS [ %2d ] : ", kss_count);
		// for (int i = 0; i < kss_count; i++) {
		// 	printf("%d", serpar.vals[i] & 1);
		// 	printf("%d", (serpar.vals[i] >> 1) & 1);
		// 	printf((i != kss_count - 1 ?"-" : ""));
		// }
		printf("KSS(");
		for (int i = 0; i < kss_count; i++) {
			printf("%d", serpar.vals[i] & 1);
			printf("%d", (serpar.vals[i] >> 1) & 1);
		}
		printf(")\r\n");
	}

	printf( "verb: ");
	for (int i = 0; i < kss_count; i++) {
		for (int j = 0 ; j < 8 ; j++) {
			printf("%d", (serpar.vals[i] >> j) & 1);
		}
		printf(",");
	}
	printf("\r\n");

	
}

#define KSS_STR_START_LEN 4
void prepKssStr(int kss_count, int deb) {
	if (kss_count < 0) {
		return;
	}
	if (!kss_count) {
		sprintf(kss_str + KSS_STR_START_LEN, ")\r\n");
		return;
	}

	for ( int i = 0 ; i < kss_count; i++) {
		kss_str[KSS_STR_START_LEN + i * 2] = ((serpar.vals[i] & 1) ? '0' : '1');
		kss_str[KSS_STR_START_LEN + i * 2 + 1] = (((serpar.vals[i] >> 1) & 1) ? '0' : '1');
	}
	sprintf(kss_str + KSS_STR_START_LEN + kss_count * 2, ")\r\n");
	return;
}

int	kss_loop_v1(uint64_t semi_period, shiftr *sp) {
	uint64_t	us_now;
	static	int	a = 0;
	
	if (!sp) {
	//	printf("NULL SHIFTR!! \r\n");
		return 0;
	}
	us_now  = time_us_64();
	
	if (!sp->ts) {
		sp->ts = us_now;
		if (!sp->vals) {
			sp->vals = calloc(sizeof(int), sp->maxv);
			sp->oldvals = calloc(sizeof(int), sp->maxv);
			for (int i = 0; i < sp->maxv; i++) 
				sp->oldvals[i] = -1;
			
			gpio_init(sp->clk);
			gpio_init(sp->inh);
			gpio_init(sp->ld);
			gpio_init(sp->qh);

			gpio_pull_up(sp->clk);
			gpio_pull_up(sp->inh);
			gpio_pull_up(sp->ld);

			gpio_set_drive_strength(sp->clk, GPIO_DRIVE_STRENGTH_2MA);
			gpio_set_drive_strength(sp->inh, GPIO_DRIVE_STRENGTH_2MA);
			gpio_set_drive_strength(sp->ld, GPIO_DRIVE_STRENGTH_2MA);

			gpio_set_dir(sp->clk, sp->clkv = 1);
			gpio_set_dir(sp->inh, sp->inhv = 1);
			gpio_set_dir(sp->ld, sp->ldv = 1);
			gpio_set_dir(sp->qh, 0);

		}

		gpio_put(sp->clk, sp->clkv = 1);
		gpio_put(sp->inh, sp->inhv = 1);
		gpio_put(sp->ld, sp->ldv = 1);
		sp->qhv = -1;
		sp->state = 0;
		sp->tick = 0;
		sp->valc = 0;
		sp->vals[sp->valc = 0] = 0;
	}
	if (us_now <= (sp->ts + semi_period))
		return -1;
	sp->ts = us_now;
	// if (kss_debug)
	// 	printf("%4d > ", sp->tick);
	
	// sp->clkv = sp->tick & 1;
	// gpio_put(sp->clk, sp->clkv);
	
	switch (sp->state) {
	case 0:
		switch (sp->tick) {
		case WAIT_BEFORE_LOAD_CYCLES:
			gpio_put(sp->ld, sp->ldv = 0);
			if (kss_debug) {
				printf("%4d > ", sp->tick);
				printf("C[%c]", sp->clkv ? '#' : '_');
				printf("I[%c]", sp->inhv ? '1' : '0');
				printf("L[%c]", sp->ldv ? '1' : '0');
				printf("\r\n");
			}
			break;
		case (WAIT_LOAD_CYCLES + WAIT_BEFORE_LOAD_CYCLES):
			gpio_put(sp->ld, sp->ldv = 1);
			if (kss_debug) {
				printf("%4d > ", sp->tick);
				printf("C[%c]", sp->clkv ? '#' : '_');
				printf("I[%c]", sp->inhv ? '1' : '0');
				printf("L[%c]", sp->ldv ? '1' : '0');
				printf("\r\n");
			}
			break;
		case (WAIT_LOAD_CYCLES + WAIT_BEFORE_LOAD_CYCLES + WAIT_AFTER_LOAD_CYCLES):
			gpio_put(sp->inh, sp->inhv = 0);
			sp->state = 1;
			sp->vals[sp->valc = 0] = 0;
			if (kss_debug) {
				printf("%4d > ", sp->tick);
				printf("C[%c]", sp->clkv ? '#' : '_');
				printf("I[%c]", sp->inhv ? '1' : '0');
				printf("L[%c]", sp->ldv ? '1' : '0');
				printf("\r\n");
			}
			sp->tick = 0;
			break;
		}
		break;
	}
	if (sp->state) {

		int rmn = sp->tick % (CLK_HIGH_CYCLES + CLK_LOW_CYCLES);
		
		switch (rmn)
		{
		case CLK_HIGH_CYCLES - 1:
			sp->qhv = gpio_get(sp->qh) ? 1 : 0;
			if (kss_debug)
				printf(" C[%c]Q[%c]\r\n",sp->clkv ? '#' : '_', sp->qhv ? '1' : '0');
			break;
		
		case CLK_HIGH_CYCLES:
			gpio_put(sp->clk, sp->clkv = 0);
			sp->vals[sp->valc] |= sp->qhv << (sp->state - 1); 
			if (kss_debug) {
				printf(" %d %d ", sp->vals[sp->valc], sp->state);
				prbits(sp->vals[sp->valc]);
				printf("\r\n");
			}
			sp->state++;
			break;

		case 0:
			gpio_put(sp->clk, sp->clkv = 1);
			break;

		default:
			break;
		}
		// if (sp->tick % (CLK_HIGH_CYCLES + CLK_LOW_CYCLES) == CLK_HIGH_CYCLES) {
		// 	sp->qhv = gpio_get(sp->qh) ? 1 : 0;
		// 	gpio_put(sp->clk, sp->clkv = 0);
		// 	sp->vals[sp->valc] |= sp->qhv << (sp->state - 1); 
		// 	if (kss_debug) {
		// 		printf(" %d %d ", sp->vals[sp->valc], sp->state);
		// 		prbits(sp->vals[sp->valc]);
		// 		printf("\r\n");
		// 	}
		// 	sp->state++;
		// }
		// else if (sp->tick % (CLK_HIGH_CYCLES + CLK_LOW_CYCLES) == 0) {
		// 	gpio_put(sp->clk, sp->clkv = 1);
		// }
		
		if (sp->state == 9) {
			if (sp->vals[sp->valc] == 0) {
				// NO DEVICE
				sp->ts = 0;
				sp->valc = 0;
				return 0;
			} else if ((sp->vals[sp->valc] == 255) || (sp->valc + 1 >= sp->maxv)) {
				// sp->ts = 0;
				sp->ts = 0;
				if (kss_debug) {
					printf("%4d > ", sp->tick);
					printf("C[%c]", sp->clkv ? '#' : '_');
					printf("I[%c]", sp->inhv ? '1' : '0');
					printf("L[%c]", sp->ldv ? '1' : '0');
					printf("\r\n");	
				}							
				return sp->valc;
			} 
			sp->valc++;
			sp->vals[sp->valc] = 0;
			sp->state = 1;
		}
	}
	sp->tick++;

	// if (kss_debug)
	// 	printf("\r\n");	
	return -1;
}