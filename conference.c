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

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 2308 $");

// mutex for synchronizing access to conflist
AST_MUTEX_DEFINE_EXPORTED(conflist_lock);

// mutex for synchronizing calls to start_conference() and remove_conf()
AST_MUTEX_DEFINE_STATIC(start_stop_conf_lock);


// single-linked list of current conferences
struct ast_conference *conflist = NULL ;
int conference_count = 0 ;


// called by app_conference.c:load_module()
void init_conference( void ) 
{
    ast_mutex_init( &conflist_lock ) ;
    ast_mutex_init( &start_stop_conf_lock ) ;
}

/* ********************************************************************************************** 
     command queue related functions
   *********************************************************************************************/

int add_command_to_queue ( 
	struct ast_conference *conf, struct ast_conf_member *member ,
	int command, int param_number, char *param_text 
	) 
{

    struct ast_conf_command_queue *cq, *cq_last;

    cq = calloc(1, sizeof( struct ast_conf_command_queue ) ) ;
	
    if ( cq == NULL ) 
    {
    	ast_log( LOG_ERROR, "unable to malloc ast_conf_command_queue\n" ) ;
    	return 1 ;
    }

    cq->next = NULL;
    cq->command = command;
    cq->issuedby = member;
    cq->param_number = param_number;
    strncpy(cq->param_text, param_text, sizeof(cq->param_text) );

    ast_mutex_lock( &conf->lock ) ;

    if ( conf->command_queue == NULL ) {
	conf->command_queue = cq;
    } else {
	cq_last = conf->command_queue;
	while ( cq_last->next != NULL ) 
	    cq_last = cq_last->next;
	cq_last->next = cq;
    }

    ast_log( AST_CONF_DEBUG, "Conference, name => %s - Added command %d params: '%d|%s'\n", 
	      conf->name, cq->command, cq->param_number, cq->param_text );

    ast_mutex_unlock( &conf->lock ) ;

    return 1;
}

static struct ast_conf_command_queue *get_command_from_queue ( struct ast_conference *conf ) 
{
    struct ast_conf_command_queue *cq;

    ast_mutex_lock( &conf->lock ) ;

    cq = conf->command_queue;

    if (cq != NULL)
	conf->command_queue = cq->next;

    ast_mutex_unlock( &conf->lock ) ;

    return cq;    
}

static void ast_conf_command_execute( struct ast_conference *conf ) {
    struct ast_conf_command_queue 	*cq;
    struct ast_conf_member *member 	= NULL ;

    cq = get_command_from_queue(conf);

    if ( cq == NULL ) return;

    ast_log(AST_CONF_DEBUG,"Parsing Command Queue for conference %s\n",conf->name);

    switch (cq->command) {
	case CONF_ACTION_MUTE_ALL: 
	    // get list of conference members
	    member = conf->memberlist ;
	    // loop over member list to retrieve queued frames
	    while ( member != NULL )
	    {
		if (member != cq->issuedby) {
		    ast_mutex_lock( &member->lock ) ;
		    queue_incoming_silent_frame(member,2);
		    member->talk_mute = cq->param_number;
		    ast_log(AST_CONF_DEBUG,"(CQ) Member Talk MUTE set to %d\n",member->talk_mute);
		    if (cq->param_number) {
				if (member->is_speaking == 1) {
					member->is_speaking = 0;
				}
				conference_queue_sound( member, "conf-muted" );
			} else {
				member->is_speaking = 1;
				conference_queue_sound( member, "conf-unmuted" );
			}
		    ast_mutex_unlock( &member->lock ) ;
		}
		// adjust our pointer to the next inline
		member = member->next ;
	    } 
	    break;
	case CONF_ACTION_ENABLE_SOUNDS: 
	    // get list of conference members
	    member = conf->memberlist ;
	    // loop over member list to retrieve queued frames
	    while ( member != NULL )
	    {
		ast_mutex_lock( &member->lock ) ;
		queue_incoming_silent_frame(member,2);
		member->dont_play_any_sound =  cq->param_number;
		ast_log(AST_CONF_DEBUG,"(CQ) Member Talk Disable sounds set to %d\n",member->dont_play_any_sound);
		ast_mutex_unlock( &member->lock ) ;
		// adjust our pointer to the next inline
		member = member->next ;
	    } 
	    break;
	case CONF_ACTION_QUEUE_MODERATOR_SOUND:
		// get list of conference members
		member = conf->memberlist ;
		// loop over member list to retrieve queued frames
		while ( member != NULL )
		{
		if (member != cq->issuedby && member->type == MEMBERTYPE_MASTER) {
			ast_mutex_lock( &member->lock ) ;
			queue_incoming_silent_frame(member,2);
			if ( !(member->quiet_mode == 1 && cq->param_number) )
			conference_queue_sound( member, cq->param_text );
			ast_mutex_unlock( &member->lock ) ;
			// adjust our pointer to the next inline
		}
			member = member->next ;
		} 
		break;
	case CONF_ACTION_QUEUE_SOUND: 
	    // get list of conference members
	    member = conf->memberlist ;
	    // loop over member list to retrieve queued frames
	    while ( member != NULL )
	    {
		if (member != cq->issuedby) {
		    ast_mutex_lock( &member->lock ) ;
		    queue_incoming_silent_frame(member,2);
		    if ( !(member->quiet_mode == 1 && cq->param_number) )
			conference_queue_sound( member, cq->param_text );
		    ast_mutex_unlock( &member->lock ) ;
		    // adjust our pointer to the next inline
		}
		    member = member->next ;
	    } 
	    break;
	case CONF_ACTION_QUEUE_NUMBER: 
	    // get list of conference members
	    member = conf->memberlist ;
	    // loop over member list to retrieve queued frames
	    while ( member != NULL )
	    {
		if (member != cq->issuedby) {
		    ast_mutex_lock( &member->lock ) ;
		    queue_incoming_silent_frame(member,2);
		    if ( !(member->quiet_mode == 1 && cq->param_number) )
			conference_queue_number( member, cq->param_text );
		    ast_mutex_unlock( &member->lock ) ;
		    // adjust our pointer to the next inline
		}
		member = member->next ;
	    } 
	    break;
	case CONF_ACTION_CONSULT_PLAYMOH: 
	    // get list of conference members
	    member = conf->memberlist ;
	    // loop over member list to retrieve queued frames
	    while ( member != NULL )
	    {
		if (member->type != MEMBERTYPE_CONSULTANT && member->type != MEMBERTYPE_MASTER) {;
		    ast_mutex_lock( &member->lock ) ;
		    if ( cq->param_number == 1) {
			member->force_on_hold =  1;
			if (member->is_speaking == 1) {
				member->is_speaking = 0;
			}
		    } 
		    else {
			member->force_on_hold = -1;
			member->is_speaking = 1;
		    }
		    ast_mutex_unlock( &member->lock ) ;
		    ast_log(AST_CONF_DEBUG,"(CQ) Member: consult playing moh set to %d\n",cq->param_number);
		    // adjust our pointer to the next inline
		}
		member = member->next ;
	    } 
	    break;
	case CONF_ACTION_PLAYMOH: 
	    // get list of conference members
	    member = conf->memberlist ;
	    // loop over member list to retrieve queued frames
	    while ( member != NULL )
	    {
		if (member != cq->issuedby) {;
		    ast_mutex_lock( &member->lock ) ;
		    if ( cq->param_number == 1) {
			member->force_on_hold =  1;
			if (member->is_speaking == 1) {
				member->is_speaking = 0;
			}
		    } 
		    else {
			member->force_on_hold = -1;
			member->is_speaking = 1;
		    }
		    ast_mutex_unlock( &member->lock ) ;
		    ast_log(AST_CONF_DEBUG,"(CQ) Member: playing moh set to %d\n",cq->param_number);
		    // adjust our pointer to the next inline
		}
		member = member->next ;
	    } 
	    break;
	case CONF_ACTION_HANGUP: 
	    member = conf->memberlist ;
	    // Scan all the members
	    while ( member != NULL )
	    {
		// If it's not me and we don't have to kick all members
		if (member != cq->issuedby) {
		    ast_mutex_lock( &member->lock ) ;
		    queue_incoming_silent_frame(member,2);
		    if (cq->param_number == 0) 
			conference_queue_sound( member, "goodbye" );
		    else
			conference_queue_sound( member, "conf-kicked" );
		    member->force_remove_flag = 1;
		    ast_log(AST_CONF_DEBUG,"(CQ) Conf %s Member Kicked: %s\n",conf->name, member->chan->name);
		    ast_mutex_unlock( &member->lock ) ;
		    if (cq->param_number == 1) 
			break;
		}
		// adjust our pointer to the next inline
		member 	= member->next ;
	    } 
	    break;
	default:
	    ast_log( LOG_WARNING, "Conference %s : don't know how to execute command %d\n", conf->name, cq->command) ;	
	    break;
    }

    // Free the struct we got
    free(cq);
}

/* ********************************************************************************************** 
     member-related functions
   *********************************************************************************************/

void add_member( struct ast_conference *conf, struct ast_conf_member *member ) 
{
	short announce = 1;

    if ( conf == NULL ) 
    {
    	ast_log( LOG_ERROR, "unable to add member to NULL conference\n" ) ;
    	return ;
    }

    // acquire the conference lock
    ast_mutex_lock( &conf->lock ) ;

    member->next = conf->memberlist ; // next is now list
    conf->memberlist = member ; // member is now at head of list
    conf->membercount ++;


    // release the conference lock
    ast_mutex_unlock( &conf->lock ) ;	

    memset(member->clid, 0, sizeof(member->clid));

    if ( (member->chan != NULL) && (member->chan->cid.cid_num != NULL) )
	strncpy( member->clid, member->chan->cid.cid_num, sizeof(member->clid) );
    else
	strncpy( member->clid, "", sizeof(member->clid) );

	/* We have a slight bug here.  Because of the architecture, we always 
	 * add_member which may not be a valid idea if the conference was locked
	 * and we are going to immediate remove the caller.
	 */
	if (conf->is_locked && (member->type != MEMBERTYPE_MASTER) ) {
	    if (strlen(conf->pin) == 0 || strncmp(conf->pin,member->pin,sizeof(conf->pin)) ) {
			/* dont play file */
			announce = 0;
		}
	}

	if (announce) {
		queue_incoming_silent_frame(member,2);
		if (ast_strlen_zero(member->entry_sounds)) {
			add_command_to_queue( conf, member, CONF_ACTION_QUEUE_NUMBER , 1, member->clid );
			add_command_to_queue( conf, member, CONF_ACTION_QUEUE_SOUND  , 1, "conf-hasjoin" );
		} else {
			char argstr[128];
			char *stringp, *token;

			strncpy(argstr, member->entry_sounds, sizeof(argstr) - 1);
			stringp = argstr;

			while ((token = strsep(&stringp, "&")) != NULL) {
				if (member->type == MEMBERTYPE_CONSULTANT) {
					add_command_to_queue(conf, member, CONF_ACTION_QUEUE_MODERATOR_SOUND, 1, token);
				} else {
					add_command_to_queue(conf, member, CONF_ACTION_QUEUE_SOUND, 1, token);
				}
				ast_log(AST_CONF_DEBUG, "Playing entry sound: %s\n", token);
			}
		}
		if (!ast_strlen_zero(member->intro_sounds)) {
			char argstr[128];
			char *stringp, *token;

			strncpy(argstr, member->intro_sounds, sizeof(argstr) - 1);
			stringp = argstr;

			while ((token = strsep(&stringp, "&")) != NULL) {
				conference_queue_sound(member, token);
				ast_log(AST_CONF_DEBUG, "Playing intro sound: %s\n", token);
			}
		}

		ast_log( AST_CONF_DEBUG, "member added to conference, name => %s\n", conf->name ) ;	
	
		manager_event(
		EVENT_FLAG_CALL, 
		APP_CONFERENCE_MANID"Join", 
		"Channel: %s\r\n",
		member->chan->name
		) ;
	}

	return ;
}


int remove_member(struct ast_conference* conf, struct ast_conf_member* member ) 
{
    // check for member
    if ( member == NULL )
    {
	ast_log( LOG_WARNING, "unable to remove NULL member\n" ) ;
	return -1 ;
    }

    // check for conference
    if ( conf == NULL )
    {
    	ast_log( LOG_WARNING, "unable to remove member from NULL conference\n" ) ;
    	return -1 ;
    }

    //
    // loop through the member list looking for the requested member
    //

    struct ast_conf_member *member_list = conf->memberlist ;
    struct ast_conf_member *member_temp = NULL ;

    while ( member_list != NULL ) 
    {
        if ( member_list == member ) 
        {
        // Check our flag to kick everyone on the conference if
        // MODERATOR leaves.
        if ( member->auto_destroy_on_exit == 1 )
            add_command_to_queue(conf, member, CONF_ACTION_HANGUP, 0, "");

	    //
	    // if this is the first member in the linked-list,
	    // skip over the first member in the list, else
	    //
	    // point the previous 'next' to the current 'next',
	    // thus skipping the current member in the list	
	    //	
	    if ( member_temp == NULL )
		conf->memberlist = member->next ;
	    else 
		member_temp->next = member->next ;

	    manager_event(
		EVENT_FLAG_CALL, 
		APP_CONFERENCE_MANID"Leave", 
		"Channel: %s\r\n",
		member->chan->name
	    ) ;

	    // delete the member
	    delete_member( member ) ;
			
	    conf->membercount --;
	    ast_log( AST_CONF_DEBUG, "removed member from conference, name => %s\n", conf->name) ;
			
	    break ;
	}
		
	// save a pointer to the current member,
	// and then point to the next member in the list
	member_temp = member_list ;
	member_list = member_list->next ;
    }
	
    // return -1 on error, or the number of members 
    // remaining if the requested member was deleted
    return conf->membercount ;
}

/* ********************************************************************************************** 
     conference-related functions
   *********************************************************************************************/

void conference_exec( struct ast_conference *conf ) 
{

    struct ast_conf_member *member, *temp_member ;
    struct timeval empty_start = {0,0}, tv = {0,0} ;
	short mcnt = 0;

    ast_log( AST_CONF_DEBUG, "Entered conference_exec, name => %s\n", conf->name ) ;
	
    //
    // main conference thread loop
    //

    while ( 1 )
    {

	//Acquire the mutex
	ast_mutex_lock( &conf->lock );
	
	// get list of conference members
	member = conf->memberlist ;

	// loop over member list to retrieve queued frames
	while ( member != NULL )
	{
		mcnt++;
	    ast_mutex_lock( &member->lock ) ;
	    // check for dead members
	    if ( member->remove_flag == 1 ) 
	    {
			if (mcnt == 1 && member->next == NULL) {
				// Run AGI script
				if (member->agi) {
					char * agi = pbx_builtin_getvar_helper(member->chan, "AGI_CONF_END");
					if (agi) {
						struct ast_app * app = pbx_findapp("agi");
						if (app) {
							pbx_exec(member->chan, app, agi, 1);
						}
					} else {
						ast_log(LOG_WARNING, "AGI requested, but AGI_CONF_END missing.\n");
					}
				}
			}
	    	ast_log( AST_CONF_DEBUG, "found member slated for removal, channel => %s\n", member->channel_name ) ;
	    	temp_member = member->next ;
			queue_incoming_silent_frame(member,2);
			if (ast_strlen_zero(member->exit_sounds)) {
				add_command_to_queue( conf, NULL, CONF_ACTION_QUEUE_NUMBER , 1, member->clid );
				add_command_to_queue( conf, NULL, CONF_ACTION_QUEUE_SOUND , 1, "conf-hasleft" );
			} else {
				char argstr[128];
				char *stringp, *token;
	
				strncpy(argstr, member->exit_sounds, sizeof(argstr) - 1);
				stringp = argstr;
	
				while ((token = strsep(&stringp, "&")) != NULL) {
					if (member->type == MEMBERTYPE_CONSULTANT) {
						add_command_to_queue(conf, member, CONF_ACTION_QUEUE_MODERATOR_SOUND, 1, token);
					} else {
						add_command_to_queue(conf, member, CONF_ACTION_QUEUE_SOUND, 1, token);
					}
					ast_log(AST_CONF_DEBUG, "Playing exit sound: %s\n", token);
				}
			}
	    	remove_member( conf, member ) ;
	    	member = temp_member ;
	    	continue ;
	    }

		// check for DTMF timeout condition
		if ( member->dtmf_admin_mode || member->dtmf_help_mode || member->dtmf_long_insert ) {
			// test the time of last key entry
			if ( member->dtmf_ts < (time(NULL) - AST_CONF_DTMF_TIMEOUT)) {
				// kick out of DTMF mode
				ast_log(LOG_NOTICE, "DTMF timeout occurred for user.\n");
				member->dtmf_admin_mode=0;
				member->dtmf_help_mode=0;
				member->dtmf_long_insert=0;
			}
		}

	    ast_mutex_unlock( &member->lock ) ;
	    // adjust our pointer to the next inline
	    member = member->next ;

	} 
	//
	// break, if we have no more members
	//

	if ( conf->memberlist == NULL ) {
	    if ( empty_start.tv_sec == 0 ) 
 		gettimeofday( &empty_start, NULL );

	    if (conf->auto_destroy) {
		ast_log( AST_CONF_DEBUG, "removing conference, count => %d, name => %s\n", conf->membercount, conf->name ) ;
		remove_conf( conf ) ; // stop the conference
		break ; // break from main processing loop
	    } else {
		//ast_log( AST_CONF_DEBUG, "Delaying conference removal. Auto destroy is not set.\n" ) ;
 		gettimeofday( &tv, NULL ) ;
		if ( tv.tv_sec - empty_start.tv_sec > AST_CONF_DESTROY_TIME ) 
		    conf->auto_destroy=1;
	    }
	} else {
	    empty_start.tv_sec = 0;
	}


	//
	// Check for the commands to be executed by the conference
	//
	if (conf->command_queue) 
	    ast_conf_command_execute( conf );

	//---------//
	// CLEANUP //
	//---------//

	// release conference mutex
	ast_mutex_unlock( &conf->lock ) ;

	//Sleep for 1ms (relax)
#ifdef SOLARIS
    struct timespec rqtp;
    rqtp.tv_sec = 0;
    rqtp.tv_nsec = 1000000;
    if (nanosleep(&rqtp, (struct timespec *) NULL) == -1) {
        ast_log(LOG_NOTICE, "Nanosleep timer errored.\n");
    }
#else
	usleep(1000);
#endif
    } // end while ( 1 )

    //
    // exit the conference thread
    // 
	
    ast_log( AST_CONF_DEBUG, "exit conference_exec\n" ) ;

    // exit the thread
    pthread_exit( NULL ) ;
    return ;
}

struct ast_conf_member *find_member( struct ast_conference *conf, const char* name ) 
{
    // check for member
    if ( name == NULL )
	return NULL ;

    // check for conference
    if ( conf == NULL )
    	return NULL ;
    //
    // loop through the member list looking for the requested member
    //
    struct ast_conf_member *member_list = conf->memberlist ;

    while ( member_list != NULL ) 
    {
    	if ( !strcmp(member_list->chan->name,name) ) 
	    return member_list;
	member_list = member_list->next ;
    }
	
    // return -1 on error, or the number of members 
    // remaining if the requested member was deleted
    return NULL ;
}

struct ast_conference *find_conf( const char* name ) 
{	
    struct ast_conference *conf = conflist ;
	
    // no conferences exist
    if ( conflist == NULL ) 
    {
    	ast_log( AST_CONF_DEBUG, "conflist has not yet been initialized, name => %s\n", name ) ;
    	return NULL ;
    }
	
    // acquire mutex
    ast_mutex_lock( &conflist_lock ) ;

    // loop through conf list
    while ( conf != NULL ) 
    {
	if ( strncasecmp( (char*)&(conf->name), name, sizeof(conf->name) ) == 0 )
	{
	    // found conf name match 
	    ast_log( AST_CONF_DEBUG, "found conference in conflist, name => %s\n", name ) ;
	    break ;
	}
	conf = conf->next ;
    }

    // release mutex
    ast_mutex_unlock( &conflist_lock ) ;

    if ( conf == NULL )
    {
	ast_log( AST_CONF_DEBUG, "unable to find conference in conflist, name => %s\n", name ) ;
    }

    return conf ;

}

struct ast_conference* create_conf( char* name, struct ast_conf_member* member )
{
    ast_log( AST_CONF_DEBUG, "entered create_conf, name => %s\n", name ) ;	


    // allocate memory for conference
    struct ast_conference *conf = calloc(1, sizeof( struct ast_conference ) ) ;
	
    if ( conf == NULL ) 
    {
    	ast_log( LOG_ERROR, "unable to malloc ast_conference\n" ) ;
    	return NULL ;
    }

    //
    // initialize conference
    //
	
    conf->next = NULL ;
    conf->memberlist = NULL ;
    conf->membercount = 0 ;
    conf->conference_thread = AST_PTHREADT_NULL ;
    conf->is_locked = 0;
    conf->command_queue = NULL ;

    // copy name to conference
    strncpy( (char*)&(conf->name), name, sizeof(conf->name) - 1 ) ;
    // initialize mutexes
    ast_mutex_init( &conf->lock ) ;
	
    // add the initial member
    add_member( conf, member) ;
	
    //
    // prepend new conference to conflist
    //

    // acquire mutex
    ast_mutex_lock( &conflist_lock ) ;

    conf->next = conflist ;
    conflist = conf ;
    ast_log( AST_CONF_DEBUG, "added new conference to conflist, name => %s\n", name ) ;

    //
    // spawn thread for new conference, using conference_exec( conf )
    //
    // acquire conference mutexes
    ast_mutex_lock( &conf->lock ) ;
	
    if ( ast_pthread_create( &conf->conference_thread, NULL, (void*)conference_exec, conf ) == 0 ) 
    {
	// detach the thread so it doesn't leak
	pthread_detach( conf->conference_thread ) ;
	// release conference mutexes
	ast_mutex_unlock( &conf->lock ) ;
	ast_log( AST_CONF_DEBUG, "started conference thread for conference, name => %s\n", conf->name ) ;
	manager_event(
	    EVENT_FLAG_CALL, 
	    APP_CONFERENCE_MANID"ConfCreate", 
	    "Channel: %s\r\n"
	    "ConfNo: %s\r\n",
	    member->chan->name,
	    name
	) ;
    }
    else
    {
    	ast_log( LOG_ERROR, "unable to start conference thread for conference %s\n", conf->name ) ;
	conf->conference_thread = AST_PTHREADT_NULL ;

	// release conference mutexes
	ast_mutex_unlock( &conf->lock ) ;

	// clean up conference
	free( conf ) ;
	conf = NULL ;
    }

    // count new conference 
    if ( conf != NULL )
    	++conference_count ;

    conf->auto_destroy = 1;

    // release mutex
    ast_mutex_unlock( &conflist_lock ) ;

    return conf ;
}


void remove_conf( struct ast_conference *conf )
{
    struct ast_conference *conf_current = conflist ;
    struct ast_conference *conf_temp = NULL ;

    ast_log( AST_CONF_DEBUG, "attempting to remove conference, name => %s\n", conf->name ) ;

    // acquire mutex
    ast_mutex_lock( &start_stop_conf_lock ) ;

    // acquire mutex
    ast_mutex_lock( &conflist_lock ) ;

    // loop through list of conferences
    while ( conf_current != NULL ) 
    {
	// if conf_current point to the passed conf,
	if ( conf_current == conf ) 
	{
	    if ( conf_temp == NULL ) 
	    {
		// this is the first conf in the list, so we just point 
		// conflist past the current conf to the next
		conflist = conf_current->next ;
	    }
	    else 
	    {
		// this is not the first conf in the list, so we need to
		// point the preceeding conf to the next conf in the list
		conf_temp->next = conf_current->next ;
	    }

	    // calculate time in conference
	    ast_log( AST_CONF_DEBUG, "removed conference, name => %s\n", conf_current->name ) ;
	    manager_event(
		EVENT_FLAG_CALL, 
		APP_CONFERENCE_MANID"ConfRemove",
		"ConfNo: %s\r\n",
		conf_current->name
	    ) ;
	    ast_mutex_unlock( &conf_current->lock ) ;

	    //Free all the command queue
	    struct ast_conf_command_queue *cqd, *cq = conf_current->command_queue;
	    while ( cq != NULL) {
		cqd=cq;
		cq=cq->next;
		free(cqd);
	    }

	    free( conf_current ) ;
	    conf_current = NULL ;
			
	    break ;
	}

	// save a refence to the soon to be previous conf
	conf_temp = conf_current ;
		
	// move conf_current to the next in the list
	conf_current = conf_current->next ;
    }
	
    // count new conference 
    --conference_count ;

    // release mutex
    ast_mutex_unlock( &conflist_lock ) ;
	
    // release mutex
    ast_mutex_unlock( &start_stop_conf_lock ) ;

    return ;
}


struct ast_conference* start_conference( struct ast_conf_member* member ) 
{
    struct ast_conference* conf = NULL ;

    // check input
    if ( member == NULL )
    {
    	ast_log( LOG_WARNING, "unable to handle null member\n" ) ;
    	return NULL ;
    }

    // look for an existing conference
    ast_log( AST_CONF_DEBUG, "attempting to find requested conference\n" ) ;

    // acquire mutex
    ast_mutex_lock( &start_stop_conf_lock ) ;

    conf = find_conf( member->id ) ;
	
    // unable to find an existing conference, try to create one
    if ( conf == NULL )
    {
	// create a new conference
	ast_log( AST_CONF_DEBUG, "attempting to create requested conference\n" ) ;
	
	// create the new conference with one member
	conf = create_conf( member->id, member ) ;

	// return an error if create_conf() failed
	if ( conf == NULL ) 
	{
	    ast_log( LOG_ERROR, "unable to find or create requested conference\n" ) ;
	    ast_mutex_unlock( &start_stop_conf_lock ) ;
	    return NULL ;
	}

    // Run AGI script
    if (member->agi) {
		conf->agi = 1 ;
        char * agi = pbx_builtin_getvar_helper(member->chan, "AGI_CONF_START");
        if (agi) {
            struct ast_app * app = pbx_findapp("agi");
            if (app) {
                pbx_exec(member->chan, app, agi, 1);
            }
        } else {
            ast_log(LOG_WARNING, "AGI requested, but AGI_CONF_START missing.\n");
        }
    }
    }
    else
    {
	// existing conference found, add new member to the conference
	// once we call add_member(), this thread
	// is responsible for calling delete_member()
	//
	add_member( conf, member) ;
    }	

    // release mutex
    ast_mutex_unlock( &start_stop_conf_lock ) ;
    
    return conf ;
}

/* ********************************************************************************************** 
     admin mode-related functions
   *********************************************************************************************/

struct fast_originate_helper {
	char tech[256];
	char data[256];
	int timeout;
	char app[256];
	char appdata[256];
	char cid_name[256];
	char cid_num[256];
	char context[256];
	char exten[256];
	char idtext[256];
	int priority;
	struct ast_variable *vars;
	struct ast_conf_member *frommember;
};

static void *fast_originate(void *data)
{
    int res = 0,
	reason;
    struct fast_originate_helper *in = data;
    struct ast_channel *chan = NULL;

    ast_indicate(in->frommember->chan, AST_CONTROL_RINGING);


    if (1) {
		res = ast_pbx_outgoing_app(
			in->tech, AST_FORMAT_SLINEAR, 
			in->data, in->timeout, 
			in->app, in->appdata, 
			&reason, 1, 
			!ast_strlen_zero(in->cid_num) ? in->cid_num : NULL, 
			!ast_strlen_zero(in->cid_name) ? in->cid_name : NULL,
			in->vars, NULL, &chan );
    } else {
		res = ast_pbx_outgoing_exten(
			in->tech, AST_FORMAT_SLINEAR, 
			in->data, in->timeout, 
			in->context, in->exten, in->priority, 
			&reason, 1, 
			!ast_strlen_zero(in->cid_num) ? in->cid_num : NULL, 
			!ast_strlen_zero(in->cid_name) ? in->cid_name : NULL,
			in->vars, NULL, NULL );
    }   

    ast_log(AST_CONF_DEBUG,"Originate returned %d \n",reason);
    ast_indicate(in->frommember->chan, -1);

    if ( reason == AST_CONTROL_ANSWER ) {
	conference_queue_sound( in->frommember, "beep" );
    } 
    else {
	conference_queue_sound( in->frommember, "beeperr" );
    }

    if (chan)
	ast_mutex_unlock(&chan->lock);
    free(in);
    return NULL;
}


int conf_do_originate(struct ast_conf_member *member, char *ext) {
    int res;

    pthread_t th;
    pthread_attr_t attr;
    struct fast_originate_helper *fast = malloc(sizeof(struct fast_originate_helper));

    char dst[80]="";
    char appdata[80]="";
    char *var;
    
    if (!fast) {
	res = -1;
    } else {
	memset(fast, 0, sizeof(struct fast_originate_helper));

	if (  (var = pbx_builtin_getvar_helper(member->chan, "NCONF_OUTBOUND_TIMEOUT")) ) {
	    fast->timeout = atoi(var) * 1000;
	} else
	    fast->timeout = 30000;

	strcat(dst,ext);
	strcat(dst,"@");
	if ( (var = pbx_builtin_getvar_helper(member->chan, "NCONF_OUTBOUND_CONTEXT")) )
	    strcat(dst,var);
	else
	    strcat(dst,member->chan->context);


	strcat(appdata,member->id);
	strcat(appdata,"|");
	if ( (var = pbx_builtin_getvar_helper(member->chan, "NCONF_OUTBOUND_PARAMS")) )
	    strcat(appdata,var);
	else {
	    strcat(appdata,"Sdq");
#if ENABLE_VAD
	    strcat(appdata,"V");
#endif
	}

	ast_copy_string( fast->tech, 	 "Local", sizeof(fast->tech) );
	ast_copy_string( fast->data, 	 dst, sizeof(fast->data) );
	ast_copy_string( fast->app, 	 APP_CONFERENCE_NAME, sizeof(fast->app) );
	ast_copy_string( fast->appdata, appdata, sizeof(fast->appdata) );

	if ( (var = pbx_builtin_getvar_helper(member->chan, "NCONF_OUTBOUND_CID_NAME")) )
	    ast_copy_string( fast->cid_name, var, sizeof(fast->cid_name) );
	else
	    ast_copy_string( fast->cid_name,APP_CONFERENCE_CNAM " Conf",sizeof(fast->cid_name) );

	if ( (var = pbx_builtin_getvar_helper(member->chan, "NCONF_OUTBOUND_CID_NUM")) )
	    ast_copy_string( fast->cid_num, var,sizeof(fast->cid_num) );
	else
	    ast_copy_string( fast->cid_num, member->id,sizeof(fast->cid_num) );

	ast_copy_string( fast->context, "internal", sizeof(fast->context) );
	ast_copy_string( fast->exten, ext, sizeof(fast->exten) );
	fast->priority = 1;
	fast->vars=NULL;
/**/

	fast->frommember=member;

        pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (ast_pthread_create(&th, &attr, fast_originate, fast)) {
    	    free(fast);
	    res = -1;
	} else {
	    res = 0;
	}

    }

    ast_mutex_unlock(&member->chan->lock);
    ast_conf_member_genactivate( member ) ;

    return res;
}


// **********************************************************************************

int conference_set_pin(struct ast_conf_member *member, char *pin) {

    ast_copy_string(member->conf->pin, pin, sizeof(member->conf->pin));
    ast_log( AST_CONF_DEBUG, "Conference %s: PIN Set to %s\n",member->conf->name, member->conf->pin ) ;
    conference_queue_number( member, member->conf->pin );

    manager_event(
	EVENT_FLAG_CALL, 
	APP_CONFERENCE_MANID"SetPIN",
	"Channel: %s\r\n"
	"PIN: %s\r\n",
	member->chan->name, 
	member->conf->pin
    ) ;

    return 1;
}


// **********************************************************************************


int conference_parse_admin_command(struct ast_conf_member *member) {
    char action;
    char *parameters;
    int res = 0;

    action=member->dtmf_buffer[0];
    parameters=member->dtmf_buffer+1;

    switch (action) {
	case '1':
	    queue_incoming_silent_frame( member, 2 );
	    res = conf_do_originate(member,parameters);
	    break;
	case '2':
		if      ( parameters[0] == '0' ) {
				add_command_to_queue( member->conf, member, CONF_ACTION_CONSULT_PLAYMOH , 0, "" );
				conference_queue_sound( member, "conf-consultation-off" );
		}
		else if ( parameters[0] == '1' ) {
				add_command_to_queue( member->conf, member, CONF_ACTION_CONSULT_PLAYMOH , 1, "" );
				conference_queue_sound( member, "conf-consultation-on" );
		}
		else
		conference_queue_sound( member, "beeperr" );
		break;
	case '4':
	    if      ( parameters[0] == '0' ) {
		conference_queue_sound(member,"conf-all-sounds-off");
	        add_command_to_queue( member->conf, member, CONF_ACTION_ENABLE_SOUNDS , 1, "" );
	    }
	    else if ( parameters[0] == '1' ) {
		conference_queue_sound(member,"conf-all-sounds-on");

	        add_command_to_queue( member->conf, member, CONF_ACTION_ENABLE_SOUNDS , 0, "" );
	    }
	    else
		conference_queue_sound( member, "beeperr" );
	    break;
	case '5':
	    if      ( parameters[0] == '0' )
	        add_command_to_queue( member->conf, member, CONF_ACTION_MUTE_ALL , 0, "" );
	    else if ( parameters[0] == '1' )
	        add_command_to_queue( member->conf, member, CONF_ACTION_MUTE_ALL , 1, "" );
	    else
		conference_queue_sound( member, "beeperr" );
	    break;
	case '6':
	    if      ( parameters[0] == '0' )
	        add_command_to_queue( member->conf, member, CONF_ACTION_PLAYMOH , 0, "" );
	    else if ( parameters[0] == '1' )
	        add_command_to_queue( member->conf, member, CONF_ACTION_PLAYMOH , 1, "" );
	    else
		conference_queue_sound( member, "beeperr" );
	    break;
	case '7':
	    if      ( parameters[0] == '0' ) {
		member->conf->is_locked = 0;
		conference_queue_sound( member, "conf-unlockednow" );
	    }
	    else if ( parameters[0] == '1' ) {
		member->conf->is_locked = 1;
		conference_queue_sound( member, "conf-lockednow" );
	    }
	    else {
		conference_queue_sound( member, "beep" );
	    }
	    break;
	case '9':
	    res = conference_set_pin(member,parameters);
	    conference_queue_sound( member, "beep" );
	    break;
	case '0':
	    if      ( parameters[0] == '0' )
	        add_command_to_queue( member->conf, member, CONF_ACTION_HANGUP , 0, "" );
	    else if ( parameters[0] == '1' )
	        add_command_to_queue( member->conf, member, CONF_ACTION_HANGUP , 1, "" );
	    else if ( parameters[0] == '2' )
	        add_command_to_queue( member->conf, member, CONF_ACTION_HANGUP , 2, "" );
	    else
		conference_queue_sound( member, "beeperr" );
	    break;
	default:
	    ast_log(AST_CONF_DEBUG,"Admin mode: Action: %c parameters: %s. Invalid or unknown\n",
		action, parameters);
	    conference_queue_sound(member,"beeperr");
	    break;
    }

    return 1;
}

void handle_conf_agi_end( const char* name, struct ast_conf_member *member ) 
{	
    struct ast_conference *conf = conflist ;
	short run_agi = 0 ;

    // no conferences exist
    if ( conflist == NULL ) 
    {
    	return ;
    }
	
    // acquire mutex
    ast_mutex_lock( &conflist_lock ) ;

    // loop through conf list
    while ( conf != NULL ) 
    {
	if ( strncasecmp( (char*)&(conf->name), name, sizeof(conf->name) ) == 0 )
	{
	    // found conf name match 
	    ast_log( AST_CONF_DEBUG, "found conference in conflist, name => %s\n", name ) ;
		if (conf->membercount <= 1) {
			run_agi = 1 ;
		}
	    break ;
	}
	conf = conf->next ;
    }

    // release mutex
    ast_mutex_unlock( &conflist_lock ) ;

    if ( conf == NULL )
    {
		run_agi = 1;
    }

	// Run AGI script
	if (run_agi) {
		char * agi = pbx_builtin_getvar_helper(member->chan, "AGI_CONF_END");
		if (agi) {
			struct ast_app * app = pbx_findapp("agi");
			if (app) {
				pbx_exec(member->chan, app, agi, 1);
			}
		} else {
			ast_log(LOG_WARNING, "AGI requested, but AGI_CONF_END missing.\n");
		}
	}
}

