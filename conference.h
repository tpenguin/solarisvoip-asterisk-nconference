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

extern ast_mutex_t conflist_lock;

extern struct ast_conference *conflist;
extern int conference_count;


enum conf_actions {
    CONF_ACTION_HANGUP,
    CONF_ACTION_ENABLE_SOUNDS,
    CONF_ACTION_MUTE_ALL,
    CONF_ACTION_QUEUE_SOUND,
    CONF_ACTION_QUEUE_NUMBER,
    CONF_ACTION_PLAYMOH,
	CONF_ACTION_CONSULT_PLAYMOH
};

struct ast_conf_command_queue 
{
    // The command issued
    int  command;

    // An optional integer  value passed 
    int  param_number;

    // An optional string value passed
    char param_text[80];

    // The member who ordered this command
    struct ast_conf_member *issuedby;

    // The next command in the list
    struct ast_conf_command_queue *next;
};

struct ast_conference 
{
	// conference name
	char name[128]; 
	// The pin to gain conference admin priileges	
	char pin[20];
	// Auto destroy when empty
	short auto_destroy;
	// Is it locked ? (No more members)
	short is_locked;

    // AGI ?
    short agi ;

	//Command queue to be executed on all members
	struct ast_conf_command_queue *command_queue;
	// single-linked list of members in conference
	struct ast_conf_member* memberlist ;
	int membercount ;
	
	// conference thread id
	pthread_t conference_thread ;
	// conference data mutex
	ast_mutex_t lock ;
	
	// pointer to next conference in single-linked list
	struct ast_conference* next ;

} ;

void init_conference( void ) ;

int add_command_to_queue ( 
	struct ast_conference *conf, struct ast_conf_member *member ,
	int command, int param_number, char *param_text 
	) ;


struct ast_conference* start_conference( struct ast_conf_member* member ) ;
void remove_conf( struct ast_conference *conf );
struct ast_conf_member *find_member( struct ast_conference *conf, const char* name ) ;
struct ast_conference *find_conf( const char* name );
int conference_parse_admin_command(struct ast_conf_member *member);
void add_member( struct ast_conference *conf, struct ast_conf_member *member );
int remove_member(struct ast_conference* conf, struct ast_conf_member* member );
void conference_exec( struct ast_conference *conf );
struct ast_conference* create_conf( char* name, struct ast_conf_member* member);
int conf_do_originate(struct ast_conf_member *member, char *ext);
int conference_set_pin(struct ast_conf_member *member, char *pin);
void handle_conf_agi_end( const char* name, struct ast_conf_member *member );
