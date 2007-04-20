/*
 * app_nconference
 *
 * NConference
 * A channel independent conference application for Openpbx
 *
 * Copyright (C) 2002, 2003 Navynet SRL
 * http://www.navynet.it
 *
 * Massimo "CtRiX" Cetra - ctrix (at) navynet.it
 *
 * This program may be modified and distributed under the 
 * terms of the GNU Public License V2.
 *
 */

#ifdef HAVE_CONFIG_H 
#include "confdefs.h" 
#endif 

#include <stdio.h>
#include "common.h"
#include "vad.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 2308 $");

#define THRESH	200

static int detect_silence(char *buf, int len, int threshold)
{
    int i, totover = 0;
    int16_t value=0;
    void *tmp = buf;
    int16_t *bptr = (int16_t *) tmp;

    for (i=0; i< len; i++){
	value =  abs( bptr[i] );
	if ( value > threshold ) { 
	    totover++;
	}
    }
    //ast_log(LOG_WARNING,"THR: %d %d\n", (max-min), threshold );
    if( totover > len % 5)
	return 0;

    return 1;
}


/*
 *  Silence (Voice Activity Detection )
 * Take data (samples) and length as input and check if it's a silence packet
 * return 1 if silence, 0 if no silence, -1 if error
 */

int vad_is_talk(char *buf, int len, int *silence_nr, int silence_threshold)
{
    int retval;

    retval = detect_silence(buf, len, THRESH);

    if (retval == 1)
	(*silence_nr)++;
    else
	*silence_nr = 0;

    if (*silence_nr > silence_threshold)
	return 0; /* really silent */
    return 1;
}

