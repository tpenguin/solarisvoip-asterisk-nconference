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

#ifndef _NCONFERENCE_COMMON_H
#define NCONFERENCE_COMMON_H

/* asterisk includes */
#include "asterisk.h"
#include "asterisk/lock.h"
#include "asterisk/file.h"
#include "asterisk/logger.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/config.h"
#include "asterisk/app.h"
#include "asterisk/dsp.h"
#include "asterisk/musiconhold.h"
#include "asterisk/manager.h"
#include "asterisk/options.h"
#include "asterisk/cli.h"
#include "asterisk/say.h"
#include "asterisk/utils.h"
#include "asterisk/translate.h"
#include "asterisk/frame.h"
#include "asterisk/features.h"
#include "asterisk/monitor.h"

/* standard includes */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <pthread.h>


extern ast_mutex_t conflist_lock;


// debug logging level
#define APP_NCONFERENCE_DEBUG	0

#if (APP_NCONFERENCE_DEBUG == 0)
#define AST_CONF_DEBUG 	LOG_DEBUG
#else
#define AST_CONF_DEBUG 	LOG_NOTICE
#endif

#define APP_CONFERENCE_CNAM     "DigiDial"

#define APP_CONFERENCE_NAME     "NConference"
#define APP_CONFERENCE_MANID	"NConference-"

//
// feature defines
//
#define ENABLE_VAD		1
#define ENABLE AUTOGAIN		0	// Not used yet

#define AST_CONF_DTMF_TIMEOUT 2

// sample information for AST_FORMAT_SLINEAR format
#define AST_CONF_SAMPLE_RATE_8K 	8000
#define AST_CONF_SAMPLE_RATE_16K 	16000
#define AST_CONF_SAMPLE_RATE 		AST_CONF_SAMPLE_RATE_8K

// Time to wait while reading a channel
#define AST_CONF_WAITFOR_TIME 40 

// Time to destroy empty conferences (seconds)
#define AST_CONF_DESTROY_TIME 300

// -----------------------------------------------
#define AST_CONF_SKIP_MS_AFTER_VOICE_DETECTION 	210
#define AST_CONF_SKIP_MS_WHEN_SILENT     		90

#define AST_CONF_CBUFFER_8K_SIZE 3072


// Timelog functions


#if 1

#define TIMELOG(func,min,message) \
	do { \
		struct timeval t1, t2; \
		int diff; \
		gettimeofday(&t1,NULL); \
		func; \
		gettimeofday(&t2,NULL); \
		if((diff = usecdiff(&t2, &t1)) > min) \
			ast_log( AST_CONF_DEBUG, "TimeLog: %s: %d ms\n", message, diff); \
	} while (0)

#else

#define TIMELOG(func,min,message) func

#endif


#define ast_generator_activate(chan,gen,params) ast_activate_generator(chan,gen,params)
#define ast_generator_deactivate(chan) ast_deactivate_generator(chan)
#define ast_generator_is_active(chan) \
    (chan->generatordata ? 1 : 0)

#endif // _NCONFERENCE_COMMON_H
