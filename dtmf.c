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
#include "sounds.h"
#include "member.h"
#include "frame.h"
#include "dtmf.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 2308 $");

int parse_dtmf_option( struct ast_conf_member *member, int subclass ) {


    if ( !member->dtmf_admin_mode && !member->dtmf_long_insert )

	    // *************************************************************** DTMF NORMAL MODE

	switch (subclass) {
	    case '*':
		if ( member->type != MEMBERTYPE_MASTER )
		    break;
		member->dtmf_ts = time(NULL);
		member->dtmf_admin_mode=1;
		member->dtmf_buffer[0]='\0';
		ast_log(AST_CONF_DEBUG,"Dialplan admin mode activated\n" );
		conference_queue_sound( member, "conf-sysop" );
		break;
	    case '1': 
		conference_queue_sound( member, "beep" );
		member->talk_volume = (member->talk_volume > -5) ? member->talk_volume-1 : member->talk_volume;
		ast_log(AST_CONF_DEBUG,"TALK Volume DOWN to %d\n",member->talk_volume);
		if (member->talk_volume) set_talk_volume(member, NULL, 1);
		break;
	    case '2': 
		member->talk_mute = (member->talk_mute == 0 ) ? 1 : 0;
		queue_incoming_silent_frame(member,3);
		if ( member->talk_mute == 1) {
    		    ast_moh_start(member->chan,"");
		    if ( member->is_speaking == 1 ) { 
#if ENABLE_VAD
			member->is_speaking = 0;
			send_state_change_notifications(member);
#endif
		    }
		} 
		else {
			ast_moh_stop(member->chan);
			member->is_speaking = 1;
			send_state_change_notifications(member);
			ast_generator_activate(member->chan,&membergen,member);
		}
		ast_log(AST_CONF_DEBUG,"Volume MUTE (muted: %d)\n",member->talk_mute);
		break;
	    case '3': 
		conference_queue_sound( member, "beep" );
		member->talk_volume = (member->talk_volume < 5) ? member->talk_volume+1 : member->talk_volume;
		ast_log(AST_CONF_DEBUG,"TALK Volume UP %d\n",member->talk_volume);
		if (member->talk_volume) set_talk_volume(member, NULL, 1);
		break;
	    case '4': 
#if ENABLE_VAD
		if (member->enable_vad_allowed) {
		    member->enable_vad = ( member->enable_vad ==0 ) ? 1 : 0;
		    // if we disable VAD, Then the user is always speaking 
		    if (!member->enable_vad) {
			member->is_speaking = 1;
			conference_queue_sound( member, "disabled" );
		    } else 
			conference_queue_sound( member, "enabled" );
		    ast_log(AST_CONF_DEBUG,"Member VAD set to %d\n",member->enable_vad);
		}
		else
#endif
		    ast_log(AST_CONF_DEBUG,"Member not enabled for VAD\n");
		break;
	    case '5':
		member->talk_mute = (member->talk_mute == 0 ) ? 1 : 0;
		queue_incoming_silent_frame(member,3);
		if ( member->talk_mute == 1 ) {
			if ( member->is_speaking == 1 ) {
				member->is_speaking = 0;
				send_state_change_notifications(member);
			}
			conference_queue_sound( member, "conf-muted" );
		} else {
			queue_incoming_silent_frame(member,3);
			member->is_speaking = 1;
			send_state_change_notifications(member);
			conference_queue_sound( member, "conf-unmuted" );
		}
		ast_log(AST_CONF_DEBUG,"Member Talk MUTE set to %d\n", member->talk_mute);
		break;
	    case '6':
		member->dont_play_any_sound =  !(member->dont_play_any_sound);
		ast_log(AST_CONF_DEBUG,"Member dont_play_any_sound set to %d\n",member->dont_play_any_sound);
		if (!member->dont_play_any_sound)
		    conference_queue_sound(member,"beep");
		break;
		case '8':
			member->dtmf_buffer[0]='\0';
			member->dtmf_long_insert=1;
			member->dtmf_help_mode=1;
			member->dtmf_ts = time(NULL);
		break;
	    case '9':
		    conference_queue_sound(member,"conf-getpin");
		    member->dtmf_buffer[0]='\0';
		    member->dtmf_long_insert=1;
			member->dtmf_ts = time(NULL);
		break;
	    case '0': {
		    char buf[10];
		    snprintf(buf, sizeof(buf), "%d", member->conf->membercount);
		    conference_queue_sound(member,"conf-thereare");
		    conference_queue_number(member, buf );
		    conference_queue_sound(member,"conf-peopleinconf");
		    }
		break;

	    default:
		//ast_log(AST_CONF_DEBUG, "DTMF received - Key %c\n",f->subclass);
		ast_log(AST_CONF_DEBUG,"Don't know how to manage %c DTMF\n",subclass);
		break;
	}

    else if ( !member->dtmf_admin_mode && member->dtmf_long_insert && !member->dtmf_help_mode ) {
	    switch (subclass) {
		case '*':
		    member->dtmf_long_insert=0;
			member->dtmf_ts = 0;
		    break;
		case '#':
		    member->dtmf_long_insert=0;
			member->dtmf_ts = 0;
		    ast_log(AST_CONF_DEBUG,"Pin entered %s does match ?\n",member->dtmf_buffer);
		    if ( strcmp( member->dtmf_buffer, member->conf->pin ) )
			conference_queue_sound(member,"conf-invalidpin");
		    else {
			conference_queue_sound(member,"beep");
			member->type = MEMBERTYPE_MASTER;
		    }
		    member->dtmf_buffer[0]='\0';
		    break;
		default: {
			member->dtmf_ts = time(NULL);
		    char t[2];
		    t[0] = subclass;
		    t[1] = '\0';
		    if ( strlen(member->dtmf_buffer)+1 < sizeof(member->dtmf_buffer)  ) {
			strcat(member->dtmf_buffer,t);
		    }
		    ast_log(AST_CONF_DEBUG,"DTMF Buffer: %s \n",member->dtmf_buffer);
		    }
		    break;
	    }
    }

    else if ( !member->dtmf_admin_mode && member->dtmf_long_insert && member->dtmf_help_mode ) {
		ast_log(AST_CONF_DEBUG, "User help for key '%c'.\n", subclass);
	    switch (subclass) {
		case '1':
			conference_queue_sound(member,"conf-help-volume-lower");
			break;
		case '2':
			conference_queue_sound(member,"conf-help-mute-wmusic");
			break;
		case '3':
			conference_queue_sound(member,"conf-help-volume-higher");
			break;
		case '5':
			conference_queue_sound(member,"conf-help-mute");
			break;
		case '6':
			conference_queue_sound(member,"conf-help-toggle-announce");
			break;
		case '8':
			conference_queue_sound(member,"conf-help-help");
			break;
		case '9':
			conference_queue_sound(member,"conf-help-enter-moderator-pin");
			break;
		case '0':
			conference_queue_sound(member,"conf-help-list-members");
			break;
		case '*':
			conference_queue_sound(member,"conf-help-admin-functions");
			break;
		}
		member->dtmf_long_insert=0;
		member->dtmf_buffer[0]='\0';
		member->dtmf_help_mode=0;
		member->dtmf_ts = 0;
    }

	else if ( member->dtmf_admin_mode && member->dtmf_help_mode ) {
		ast_log(AST_CONF_DEBUG, "Admin help for key '%c'.\n", subclass);
		switch (subclass) {
		case '1':
			conference_queue_sound(member,"conf-help-admin-outcall");
			break;
		case '2':
			conference_queue_sound(member,"conf-help-admin-private-consult");
			break;
		case '4':
			conference_queue_sound(member,"conf-help-admin-toggle-sounds");
			break;
		case '5':
			conference_queue_sound(member,"conf-help-admin-mute-all");
			break;
		case '6':
			conference_queue_sound(member,"conf-help-admin-mute-wmusic");
			break;
		case '7':
			conference_queue_sound(member,"conf-help-admin-toggle-lock");
			break;
		case '9':
			conference_queue_sound(member,"conf-help-admin-set-pin");
			break;
		case '0':
			conference_queue_sound(member,"conf-help-admin-hangup-options");
			break;
		}
		member->dtmf_long_insert=0;
		member->dtmf_buffer[0]='\0';
		member->dtmf_help_mode=0;
		member->dtmf_admin_mode=0;
		member->dtmf_ts = 0;
	}

    else if (member->dtmf_admin_mode && !member->dtmf_help_mode) {
	
	    // *************************************************************** DTMF ADMIN MODE

	    if ( subclass == '*' ) { 
		member->dtmf_admin_mode=0;
		member->dtmf_ts = 0;
		ast_log(AST_CONF_DEBUG,"Dialplan admin mode deactivated\n" );
	    }
		else if ( subclass == '8' ) {
			ast_log(LOG_NOTICE,"Enter help menu.\n");
			member->dtmf_help_mode = 1;
		}
	    else if ( subclass == '#' ) { 
		member->dtmf_admin_mode=0;
		if ( strlen(member->dtmf_buffer) >= 1 ) {
			member->dtmf_ts = 0;
		    ast_log(AST_CONF_DEBUG,"Admin mode. Trying to parse command %s\n",member->dtmf_buffer );
		    conference_parse_admin_command(member);
		}
		else
		    ast_log(AST_CONF_DEBUG,"Admin mode. Invalid empty command (%s)\n",member->dtmf_buffer );
	    } 
	    else {
		char t[2];
		t[0] = subclass;
		t[1] = '\0';
		if ( strlen(member->dtmf_buffer)+1 < sizeof(member->dtmf_buffer)  ) {
		    strcat(member->dtmf_buffer,t);
		}
		ast_log(AST_CONF_DEBUG,"DTMF Buffer: %s \n",member->dtmf_buffer);
		member->dtmf_ts = time(NULL);
	    }
    }

    return 0;
}
