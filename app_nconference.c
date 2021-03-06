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
#include "app_nconference.h"
#include "conference.h"
#include "member.h"
#include "cli.h"

ASTERISK_FILE_VERSION( __FILE__, "$Revision: 2308 $");


/************************************************************
 *        Text Descriptions
 ***********************************************************/

static char *tdesc = APP_CONFERENCE_CNAM " Channel Independent Conference Application (" CONF_VERSION ")" ;

static char *app = APP_CONFERENCE_NAME ;

static char *synopsis = APP_CONFERENCE_CNAM " Channel Independent Conference (" CONF_VERSION ")" ;

static char *descrip = APP_CONFERENCE_NAME "(confno|[options]|[pin]|[introsound1[&introsound2]]|[entrysound1[&entrysound2]]|[exitsound[&exitsound2]]):\n"
"\n"
"The options string may contain zero or more of the following:\n"
"   'M'      : Caller is Moderator (can do everything).\n"
"   'S'      : Caller is Speaker.\n"
"   'L'      : Caller is Listener (cannot talk).\n"
"   'T'      : Caller is Talker (cannot listen but can only speak).\n"
"   'C'      : Caller is Consultant (can talk only to moderator).\n"
#if ENABLE_VAD
"   'V'      : Do VAD (Voice Activity Detection).\n"
#endif
"   'd'      : Disable DTMF handling (keypad functions).\n"
"   'X'      : Destroy conference when Moderator quits.\n"
"   'x'      : Don't auto destroy conference when all users quit.\n"
"              (it's destroyed after 300 seconds of inactivity)\n"
"	    Note: works only with M option set.\n"
"   'q'      : Quiet mode. Don't play enter sounds when entering.\n"
"   'm'      : Disable music on hold while the conference has a single caller.\n"
"   '0'      : Disable non-moderator user menu '0' key.\n"
"   'r'      : Record conference (records as ${MONITOR_FILENAME})\n"
"   'R'      : Mix the recorded files.\n"
"   'A'      : Enable AGI scripts.\n"
"              ${AGI_CONF_START} will execute on begining of conference\n"
"              $(AGI_CONF_END} will execute on end of conference\n"
"              ${AGI_CONF_JOIN} will execute on user join\n"
"              ${AGI_CONF_LEAVE} will execute on user leave\n"
"   'g'      : When NConference hangs up, exit to execute more commands\n"
"              in the current context.\n"
"   'Q'      : Mute all incoming annoucements for the caller and members.\n"
"   'a(CLID)': Use CLID for the caller id used in annoucements, instead of\n"
"             the actual CLID.\n"
"\n"
" If 'pin' is set, then if the member is a Moderator that pin is inherited \n"
"by the conference (otherwise pin is empty), if the member is not a Moderator \n"
"and the conference is locked, that pin is used to gain access to the conference.\n"
"\n"
" If introsounds, entrysounds, or exitsounds are used, then NConference will not \n"
"perform the default sounds. When a Consultant enters or exits, only the Moderator \n"
"will hear the entrysounds and exitsounds; otherwise, all members hear the entrysounds \n"
"and exitsounds. \n"
"\n"
"Returns 0 if the user exits with the '#' key, or -1 if the user hangs up.\n" ;

struct ast_custom_function acf_nconfcount = {
	.name = "NCONFCOUNT",
	.synopsis = "Retrieves number of members in an NConference",
	.syntax = "NCONFCOUNT(confno|[options])",
	.desc =
	"The options string may contain zero or more of the following:\n"
	"   'M': Caller is Moderator (can do everything).\n"
	"   'S': Caller is Speaker.\n"
	"   'L': Caller is Listener (cannot talk).\n"
	"   'T': Caller is Talker (cannot listen but can only speak).\n"
	"   'C': Caller is Consultant (can talk only to moderator).\n",
	.read = acf_nconfcount_exec,
};

STANDARD_LOCAL_USER ;
LOCAL_USER_DECL;

int unload_module( void ) {
	ast_log( LOG_NOTICE, "unloading " APP_CONFERENCE_NAME " module\n" );
	STANDARD_HANGUP_LOCALUSERS;
	unregister_conference_cli();
	ast_custom_function_unregister(&acf_nconfcount);
	return ast_unregister_application( app ) ;
}

int load_module( void ) {
	ast_log( LOG_NOTICE, "Loading " APP_CONFERENCE_NAME " module\n" );
	init_conference() ;
	register_conference_cli();
	ast_custom_function_register(&acf_nconfcount);
	return ast_register_application( app, app_conference_main, synopsis, descrip ) ;
}

char *description( void ) {
	return tdesc ;
}

int usecount( void ) {
	int res;
	STANDARD_USECOUNT( res ) ;
	return res;
}

char *key()
{
        return ASTERISK_GPL_KEY;
}

/************************************************************
 *        Main Conference function
 ***********************************************************/

int app_conference_main( struct ast_channel* chan, void* data ) {
	int res = 0 ;
	struct localuser *u ;
	LOCAL_USER_ADD( u ) ; 
	res = member_exec( chan, data ) ;
	if (res == -2) {
		/* Conference was locked and user did not have valid PIN - Jump to n+101 */
		ast_log(LOG_NOTICE, "Conference is locked so I am going to try and jump n + 101\n");
		ast_goto_if_exists(chan, chan->context, chan->exten, chan->priority + 101);
		res = 0;
	}
	LOCAL_USER_REMOVE( u ) ;
	return res ;
}

char *acf_nconfcount_exec(struct ast_channel *chan, char *cmd, char *data, char *buf, size_t len) {
	struct localuser *u;
	char *info, *options=NULL, *confno;
	char tmpbuf[16];

	*buf = '\0';
	
	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "NCONFCOUNT requires an argument (confno)\n");
		return buf;
	}

	LOCAL_USER_ACF_ADD(u);

	info = ast_strdupa(data);
	if (!info) {
		ast_log(LOG_ERROR, "Out of memory\n");
		LOCAL_USER_REMOVE(u);
		return buf;
	}
	
	confno = strsep(&info, "|");
	options = info;

	struct ast_conference *conf = find_conf(confno);
	if (conf == NULL) {
		LOCAL_USER_REMOVE(u);
		return buf;
	}

	if (ast_strlen_zero(options)) {
		/* Count All Members */
		snprintf(tmpbuf, 15, "%d", conf->membercount);
		ast_copy_string(buf, tmpbuf, len);
	} else {
		/* Count Certain types of Members */
		int i;
		short counter = 0;
		struct ast_conf_member *member_list = conf->memberlist;
		while (member_list) {

			for ( i = 0 ; i < strlen(options) ; ++i ) {
				switch (options[i]) {
				case 'M':
					if (member_list->type == MEMBERTYPE_MASTER) {
						++counter;
					}
					break ;
				case 'S':
					if (member_list->type == MEMBERTYPE_SPEAKER) {
						++counter;
					}
					break ;
				case 'L':
					if (member_list->type == MEMBERTYPE_LISTENER) {
						++counter;
					}
					break ;
				case 'T':
					if (member_list->type == MEMBERTYPE_TALKER) {
						++counter;
					}
					break ;
				case 'C':
					if (member_list->type == MEMBERTYPE_CONSULTANT) {
						++counter;
					}
					break ;
				default:
					ast_log(LOG_WARNING, "Unknown option '%c'.\n", options[i]);
				}
			}

			member_list = member_list->next;
		}
		snprintf(tmpbuf, 15, "%d", counter);
		ast_copy_string(buf, tmpbuf, len);
	}

	LOCAL_USER_REMOVE(u);
	return buf;
}

