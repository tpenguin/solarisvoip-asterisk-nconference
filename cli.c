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
#include "conference.h"
#include "member.h"
#include "frame.h"
#include "sounds.h"

#include "cli.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 2308 $");

STANDARD_LOCAL_USER ;
LOCAL_USER_DECL;

/* ************************************************************************
       RELATED APPLICATIONS - NCONFERENCEADMIN - TO BE FINISHED, IF NEEDED
 * ************************************************************************/
/*

static char *app_admin = "NConferenceAdmin";
static char *descr_admin =
"  NConference(confno[|var]): Plays back the number of users in the specified\n"
"conference. If var is specified, playback will be skipped and the value\n"
"will be returned in the variable. Returns 0.\n";

// TODO - Do it if someone needs it

static char *synopsis_admin = "NConference participant count";

static int app_admin_exec(struct ast_channel *chan, void *data)
{
	struct localuser *u;
	struct ast_conference *conf;

	if (data && !ast_strlen_zero(data)) {		
		params = ast_strdupa((char *) data);
		confno = strsep(&params, "|");
		command = strsep(&params, "|");
		caller = strsep(&params, "|");

		if (!command) {
			ast_log(LOG_WARNING, "requires a command!\n");
			return -1;
		}

		// Find the right conference 
		if (!(conf = find_conf(argv[2]))) {
			return -1;
		}

//			ast_mutex_unlock(&conflock);
	}

	LOCAL_USER_ADD(u);
	LOCAL_USER_REMOVE(u);
	
}
*/

/* ***************************************************************
       RELATED APPLICATIONS - NCONFERENCECOUNT
 * ***************************************************************/


static char *app_count = "NConferenceCount";
static char *descr_count =
"  NConference(confno[|var]): Plays back the number of users in the specified\n"
"conference to the conference members. \n"
"If var is specified, playback will be skipped and the value\n"
"will be returned in the variable. Returns 0.\n";

static char *synopsis_count = "NConference members count";

static int app_count_exec(struct ast_channel *chan, void *data)
{
	struct localuser *u;
	
	int res = 0;
	struct ast_conference *conf;
	int count;
	char *confnum, *localdata;
	char val[80] = "0"; 

	if (!data || ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "NConferenceCount requires an argument (conference number)\n");
		return -1;
	}

	localdata = ast_strdupa(data);

	LOCAL_USER_ADD(u);

	confnum = strsep(&localdata, "|");       

	conf = find_conf(confnum);

	if (conf) {
		ast_mutex_lock(&conf->lock);
		count = conf->membercount;
		ast_mutex_unlock(&conf->lock);
	}
	else
		count = 0;

	if (localdata && !ast_strlen_zero(localdata)){
		pbx_builtin_setvar_helper(chan, localdata, "");
		/* have var so load it and exit */
		snprintf(val, sizeof(val), "%i", count);
		pbx_builtin_setvar_helper(chan, localdata, val);
	} else if (conf != NULL) {
		char buf[10];
		snprintf(buf, sizeof(buf), "%d", count);
	        add_command_to_queue( conf, NULL, CONF_ACTION_QUEUE_SOUND,  0, "conf-thereare" );
	        add_command_to_queue( conf, NULL, CONF_ACTION_QUEUE_NUMBER, 0, buf );
	        add_command_to_queue( conf, NULL, CONF_ACTION_QUEUE_SOUND,  0, "conf-peopleinconf" );
	}
	LOCAL_USER_REMOVE(u);
	return res;
}

/* ***************************************************************
       CLI FUNCTIONS
 * ***************************************************************/


int nconference_admin_exec( int fd, int argc, char *argv[] );
static char *nconference_admin_complete(char *line, char *word, int pos, int state);

static char nconference_admin_usage[] = 
	"usage: NConference <command>  <conference_name> <usernumber>\n"
	"       Admin commands for conference\n"
	"       <command> can be: kick, list, lock, mute, show, unlock, unmute\n"
;

static struct ast_cli_entry nconference_admin_cli = { 
	{ "NConference", NULL, NULL },
	nconference_admin_exec,
	"Administration Tool for NConference",
	nconference_admin_usage,
	nconference_admin_complete
} ;

int nconference_admin_exec( int fd, int argc, char *argv[] )
{
	struct ast_conference *conf 	= NULL;
	struct ast_conf_member *member	= NULL;
	char cmdline [512];
	int i = 0;
	int total = 0;

	if ( argc < 2 ) 
		return RESULT_SHOWUSAGE ;

	if (argc > 4)
		ast_cli(fd, "Invalid Arguments.\n");

	// Check for length so no buffer will overflow... 
	for (i = 0; i < argc; i++) {
		if (strlen(argv[i]) > 100)
			ast_cli(fd, "Invalid Arguments.\n");
	}

	if (argc == 2 && strstr(argv[1], "show") ) {
		// Show all the conferences 
		conf = conflist;
		if (!conf) {
			ast_cli(fd, "No active conferences.\n");
			return RESULT_SUCCESS;
		}
		ast_cli(fd, " %-s    %7s\n", "Conf. Num", "mEMBERS");
		while(conf) {
			if (conf->membercount == 0)
				ast_copy_string(cmdline, "N/A ", sizeof(cmdline) );
			else 
				snprintf(cmdline, sizeof(cmdline), "%4d", conf->membercount);
			ast_cli(fd, " %-9s    %7d\n", conf->name, conf->membercount );
			total += conf->membercount; 	
			conf = conf->next;
		}
		ast_cli(fd, "*Total number of members: %d\n", total);
		return RESULT_SUCCESS;
	}

	// The other commands require more arguments
	if (argc < 3)
		return RESULT_SHOWUSAGE;


	/* Find the right conference */
	if (!(conf = find_conf(argv[2]))) {
		ast_cli(fd, "No such conference: %s.\n", argv[2]);
			return RESULT_SUCCESS;
	}

	// Find the right member
	if (argc > 3 ) {
	    member = find_member(conf,argv[3] );
	    if ( strcmp( argv[3],"all" ) && ( member == NULL ) ) {
		ast_cli(fd, "No such member: %s in conference %s.\n", argv[3], argv[2]);
			return RESULT_SUCCESS;
	    }
	}



	if      ( !strcmp(argv[1], "list") ) {
		member = conf->memberlist;
		total = 0;
		ast_cli(fd, " %-14s  %-14s  %9s %6s %3s\n", "Channel", "Type","Speaking","Muted","VAD");
		while ( member != NULL ) 
		{
		    ast_cli(fd, " %-14s  %-14s  %9d %6d %3d\n", 
			member->chan->name ,
			membertypetostring( member->type ),
			member->is_speaking,
			member->talk_mute,
			member->enable_vad
		    );
		    total ++;
		    member = member->next ;
		}
		ast_cli(fd, "*Total members: %d \n", total );
	}
	else if ( !strcmp(argv[1], "unlock") ) {
	    if ( conf->is_locked == 0 )
		ast_cli(fd, "Conference: %s is already unlocked.\n", conf->name);
 	    else { 
		conf->is_locked = 0;
	        add_command_to_queue( conf, NULL, CONF_ACTION_QUEUE_SOUND, 0, "conf-unlockednow" );
	    }
	}
	else if ( !strcmp(argv[1], "lock") ) {
	    if ( conf->is_locked == 1 )
		ast_cli(fd, "Conference: %s is already locked.\n", conf->name);
 	    else { 
		conf->is_locked = 1;
	        add_command_to_queue( conf, NULL, CONF_ACTION_QUEUE_SOUND, 0, "conf-lockednow" );
	    }
	}


	else if ( !strcmp(argv[1], "mute") ) {
	    if ( member == NULL ) {
	    	    add_command_to_queue( conf, NULL, CONF_ACTION_MUTE_ALL, 1, "" );
	    } 
	    else 
	    {
		member->talk_mute = 1;
		conference_queue_sound( member, "conf-muted" );
		ast_cli(fd, "Conference: %s - Member %s is now muted.\n", conf->name, member->chan->name);
	    }
	}
	else if ( !strcmp(argv[1], "unmute") ) {
	    if ( member == NULL ) {
	    	    add_command_to_queue( conf, NULL, CONF_ACTION_MUTE_ALL, 0, "" );
	    }
	    else 
	    {
		member->talk_mute = 0;
		conference_queue_sound( member, "conf-unmuted" );
		ast_cli(fd, "Conference: %s - Member %s is now unmuted.\n", conf->name, member->chan->name);
	    }
	}

	else if ( !strcmp(argv[1], "kick") ) {
	    if ( member == NULL ) {
		    ast_cli(fd, "Conference: %s - Member is not correct.\n", conf->name);
	    }
	    else 
	    {
		queue_incoming_silent_frame(member,5);
		conference_queue_sound( member, "conf-kicked" );
		member->force_remove_flag = 1;
		ast_cli(fd, "Conference: %s - Member %s has been kicked.\n", conf->name, member->chan->name);
	    }
	}


    return RESULT_SUCCESS;
}




static char *nconference_admin_complete(char *line, char *word, int pos, int state) {
#define CONF_COMMANDS 7
	int which = 0, x = 0;
	struct ast_conference *cnf 	= NULL;
	struct ast_conf_member *usr 	= NULL;
	char *confno 			= NULL;
	char usrno[50] 			= "";
	char cmds[CONF_COMMANDS][20] 	= {"lock", "unlock", "mute", "unmute", "kick", "list", "show"};
	char *myline			= NULL;
	
	if (pos == 1) {
		/* Command */
		for (x = 0;x < CONF_COMMANDS; x++) {
			if (!strncasecmp(cmds[x], word, strlen(word))) {
				if (++which > state) {
					return strdup(cmds[x]);
				}
			}
		}
	} 
	else if (pos == 2) {
		// Conference Number 
		ast_mutex_lock(&conflist_lock);
		cnf = conflist;
		while(cnf) {
			if (!strncasecmp(word, cnf->name, strlen(word))) {
				if (++which > state)
					break;
			}
			cnf = cnf->next;
		}
		ast_mutex_unlock(&conflist_lock);
		return cnf ? strdup(cnf->name) : NULL;
	} 
	else if (pos == 3) {
		// User Number || Conf Command option
		if ( strstr(line, "mute") || strstr(line, "kick") || strstr(line, "lock") ) {
			if ( (state == 0) && 
			     !(strncasecmp(word, "all", strlen(word)))) {
				return strdup("all");
			}
			which++;
			ast_mutex_lock(&conflist_lock);
			cnf = conflist;

			myline = ast_strdupa(line);
			if ( strsep(&myline, " ") && strsep(&myline, " ") && !confno) {
				while( (confno = strsep(&myline, " ") ) && ( strcmp(confno, " ") == 0 ) )
					;
			}	

			while(cnf) {
				if (strcmp(confno, cnf->name) == 0) {
					break;
				}
				cnf = cnf->next;
			}

			if (cnf) {
				// Search for the user 
				usr = cnf->memberlist;
				while(usr) {
					snprintf(usrno, sizeof(usrno), "%s", usr->chan->name);
					if (!strncasecmp(word, usrno, strlen(word))) {
						if (++which > state)
							break;
					}
					usr = usr->next;
				}
			}
			ast_mutex_unlock(&conflist_lock);
			return usr ? strdup(usrno) : NULL;
		}
	}

	return NULL;
}

/* ***************************************************************
       cli initialization function
 * ***************************************************************/

void register_conference_cli( void ) 
{
	ast_cli_register( &nconference_admin_cli ) ;
	ast_register_application(app_count, app_count_exec, synopsis_count, descr_count);
	//ast_register_application(app_admin, app_admin_exec, synopsis_admin, descr_admin);
}

void unregister_conference_cli( void )
{
	ast_cli_unregister( &nconference_admin_cli ) ;
	ast_unregister_application(app_count);

	//ast_unregister_application(app_admin);
}


