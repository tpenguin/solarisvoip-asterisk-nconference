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

#define AST_CONF_8K_MEMBER
#define AST_CONF_16K_MEMBER

// Circular buffer
struct member_cbuffer {
    short buffer8k[AST_CONF_CBUFFER_8K_SIZE];

    int	 index8k;	// The index of the last valid frame in the circular buffer

    int	 my_position8k; // The index if the last frame sent to this member
			// This is not related to this cbuf
};



struct ast_frame* get_outgoing_frame( struct ast_conference *conf, struct ast_conf_member* member, int samples ) ;
int queue_incoming_frame( struct ast_conf_member* member, struct ast_frame* fr ) ;
int queue_incoming_silent_frame( struct ast_conf_member *member, int count);

long usecdiff( struct timeval* timeA, struct timeval* timeB );
void add_milliseconds( struct timeval* tv, long ms );
int set_talk_volume(struct ast_conf_member *member, struct ast_frame *f, int is_talk);
void mix_slinear_frames( short *dst, const short *src, int samples, int startindex, int cbuffersize );
void copy_frame_content( struct member_cbuffer *cbuf, struct ast_frame *sfr );

