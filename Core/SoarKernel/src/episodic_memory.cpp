#include <portability.h>

/*************************************************************************
 * PLEASE SEE THE FILE "COPYING" (INCLUDED WITH THIS SOFTWARE PACKAGE)
 * FOR LICENSE AND COPYRIGHT INFORMATION. 
 *************************************************************************/

/*************************************************************************
 *
 *  file:  episodic_memory.cpp
 *
 * =======================================================================
 * Description  :  Various functions for EpMem
 * =======================================================================
 */

#include <stdlib.h>

#include <iostream>
#include <string>
#include <map>
#include <vector>

#include "symtab.h"
#include "io_soar.h"
#include "wmem.h"

#include "xmlTraceNames.h"
#include "gski_event_system_functions.h"
#include "print.h"

#include "episodic_memory.h"

#include "sqlite3.h"

using namespace std;

/***************************************************************************
 * Function     : epmem_clean_parameters
 **************************************************************************/
void epmem_clean_parameters( agent *my_agent )
{
	for ( int i=0; i<EPMEM_PARAMS; i++ )
	{
		if ( my_agent->epmem_params[ i ]->type == epmem_param_string )
			delete my_agent->epmem_params[ i ]->param->string_param.value;
			
		delete my_agent->epmem_params[ i ];
	}
}

/***************************************************************************
 * Function     : epmem_add_parameter
 **************************************************************************/
epmem_parameter *epmem_add_parameter( const char *name, double value, bool (*val_func)( double ) )
{
	// new parameter entry
	epmem_parameter *newbie = new epmem_parameter;
	newbie->param = new epmem_parameter_union;
	newbie->param->number_param.value = value;
	newbie->param->number_param.val_func = val_func;
	newbie->type = epmem_param_number;
	newbie->name = name;
	
	return newbie;
}

epmem_parameter *epmem_add_parameter( const char *name, const long value, bool (*val_func)( const long ), const char *(*to_str)( long ), const long (*from_str)( const char * ) )
{
	// new parameter entry
	epmem_parameter *newbie = new epmem_parameter;
	newbie->param = new epmem_parameter_union;
	newbie->param->constant_param.val_func = val_func;
	newbie->param->constant_param.to_str = to_str;
	newbie->param->constant_param.from_str = from_str;
	newbie->param->constant_param.value = value;
	newbie->type = epmem_param_constant;
	newbie->name = name;
	
	return newbie;
}

epmem_parameter *epmem_add_parameter( const char *name, const char *value, bool (*val_func)( const char * ) )
{
	// new parameter entry
	epmem_parameter *newbie = new epmem_parameter;
	newbie->param = new epmem_parameter_union;
	newbie->param->string_param.value = new string( value );
	newbie->param->string_param.val_func = val_func;
	newbie->type = epmem_param_string;
	newbie->name = name;
	
	return newbie;
}

/***************************************************************************
 * Function     : epmem_convert_parameter
 **************************************************************************/
const char *epmem_convert_parameter( agent *my_agent, const long param )
{
	if ( ( param < 0 ) || ( param >= EPMEM_PARAMS ) )
		return NULL;

	return my_agent->epmem_params[ param ]->name;
}

const long epmem_convert_parameter( agent *my_agent, const char *name )
{
	for ( int i=0; i<EPMEM_PARAMS; i++ )
		if ( !strcmp( name, my_agent->epmem_params[ i ]->name ) )
			return i;

	return EPMEM_PARAMS;
}

/***************************************************************************
 * Function     : epmem_valid_parameter
 **************************************************************************/
bool epmem_valid_parameter( agent *my_agent, const char *name )
{
	return ( epmem_convert_parameter( my_agent, name ) != EPMEM_PARAMS );
}

bool epmem_valid_parameter( agent *my_agent, const long param )
{
	return ( epmem_convert_parameter( my_agent, param ) != NULL );
}

/***************************************************************************
 * Function     : epmem_get_parameter_type
 **************************************************************************/
epmem_param_type epmem_get_parameter_type( agent *my_agent, const char *name )
{
	const long param = epmem_convert_parameter( my_agent, name );
	if ( param == EPMEM_PARAMS )
		return epmem_param_invalid;
	
	return my_agent->epmem_params[ param ]->type;
}

epmem_param_type epmem_get_parameter_type( agent *my_agent, const long param )
{
	if ( !epmem_valid_parameter( my_agent, param ) )
		return epmem_param_invalid;

	return my_agent->epmem_params[ param ]->type;
}

/***************************************************************************
 * Function     : epmem_get_parameter
 **************************************************************************/
const long epmem_get_parameter( agent *my_agent, const char *name, const double test )
{
	const long param = epmem_convert_parameter( my_agent, name );
	if ( param == EPMEM_PARAMS )
		return NULL;
	
	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_constant )
		return NULL;
	
	return my_agent->epmem_params[ param ]->param->constant_param.value;
}

const char *epmem_get_parameter( agent *my_agent, const char *name, const char *test )
{
	const long param = epmem_convert_parameter( my_agent, name );
	if ( param == EPMEM_PARAMS )
		return NULL;
	
	if ( epmem_get_parameter_type( my_agent, param ) == epmem_param_string )
		return my_agent->epmem_params[ param ]->param->string_param.value->c_str();
	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_constant )
		return NULL;
	
	return my_agent->epmem_params[ param ]->param->constant_param.to_str( my_agent->epmem_params[ param ]->param->constant_param.value );
}

double epmem_get_parameter( agent *my_agent, const char *name )
{
	const long param = epmem_convert_parameter( my_agent, name );
	if ( param == EPMEM_PARAMS )
		return NULL;
	
	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_number )
		return NULL;
	
	return my_agent->epmem_params[ param ]->param->number_param.value;
}

//

const long epmem_get_parameter( agent *my_agent, const long param, const double test )
{
	if ( !epmem_valid_parameter( my_agent, param ) )
		return NULL;

	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_constant )
		return NULL;
	
	return my_agent->epmem_params[ param ]->param->constant_param.value;
}

const char *epmem_get_parameter( agent *my_agent, const long param, const char *test )
{
	if ( !epmem_valid_parameter( my_agent, param ) )
		return NULL;
	
	if ( epmem_get_parameter_type( my_agent, param ) == epmem_param_string )
		return my_agent->epmem_params[ param ]->param->string_param.value->c_str();
	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_constant )
		return NULL;
	
	return my_agent->epmem_params[ param ]->param->constant_param.to_str( my_agent->epmem_params[ param ]->param->constant_param.value );
}

double epmem_get_parameter( agent *my_agent, const long param )
{
	if ( !epmem_valid_parameter( my_agent, param ) )
		return NULL;
	
	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_number )
		return NULL;
	
	return my_agent->epmem_params[ param ]->param->number_param.value;
}

/***************************************************************************
 * Function     : epmem_valid_parameter_value
 **************************************************************************/
bool epmem_valid_parameter_value( agent *my_agent, const char *name, double new_val )
{
	const long param = epmem_convert_parameter( my_agent, name );
	if ( param == EPMEM_PARAMS )
		return false;
	
	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_number )
		return false;
	
	return my_agent->epmem_params[ param ]->param->number_param.val_func( new_val );
}

bool epmem_valid_parameter_value( agent *my_agent, const char *name, const char *new_val )
{
	const long param = epmem_convert_parameter( my_agent, name );
	if ( param == EPMEM_PARAMS )
		return false;
	
	if ( epmem_get_parameter_type( my_agent, param ) == epmem_param_string )
		return my_agent->epmem_params[ param ]->param->string_param.val_func( new_val );
	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_constant )
		return false;
	
	return my_agent->epmem_params[ param ]->param->constant_param.val_func( my_agent->epmem_params[ param ]->param->constant_param.from_str( new_val ) );
}

bool epmem_valid_parameter_value( agent *my_agent, const char *name, const long new_val )
{
	const long param = epmem_convert_parameter( my_agent, name );
	if ( param == EPMEM_PARAMS )
		return false;
	
	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_constant )
		return false;
	
	return my_agent->epmem_params[ param ]->param->constant_param.val_func( new_val );
}

//

bool epmem_valid_parameter_value( agent *my_agent, const long param, double new_val )
{
	if ( !epmem_valid_parameter( my_agent, param ) )
		return false;
	
	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_number )
		return false;
	
	return my_agent->epmem_params[ param ]->param->number_param.val_func( new_val );
}

bool epmem_valid_parameter_value( agent *my_agent, const long param, const char *new_val )
{
	if ( !epmem_valid_parameter( my_agent, param ) )
		return false;
	
	if ( epmem_get_parameter_type( my_agent, param ) == epmem_param_string )
		return my_agent->epmem_params[ param ]->param->string_param.val_func( new_val );
	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_constant )
		return false;
	
	return my_agent->epmem_params[ param ]->param->constant_param.val_func( my_agent->epmem_params[ param ]->param->constant_param.from_str( new_val ) );
}

bool epmem_valid_parameter_value( agent *my_agent, const long param, const long new_val )
{
	if ( !epmem_valid_parameter( my_agent, param ) )
		return false;
	
	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_constant )
		return false;
	
	return my_agent->epmem_params[ param ]->param->constant_param.val_func( new_val );
}

/***************************************************************************
 * Function     : epmem_set_parameter
 **************************************************************************/
bool epmem_set_parameter( agent *my_agent, const char *name, double new_val )
{
	const long param = epmem_convert_parameter( my_agent, name );
	if ( param == EPMEM_PARAMS )
		return false;
	
	if ( !epmem_valid_parameter_value( my_agent, param, new_val ) )
		return false;
	
	my_agent->epmem_params[ param ]->param->number_param.value = new_val;

	return true;
}

bool epmem_set_parameter( agent *my_agent, const char *name, const char *new_val )
{	
	const long param = epmem_convert_parameter( my_agent, name );
	if ( param == EPMEM_PARAMS )
		return false;
	
	if ( !epmem_valid_parameter_value( my_agent, param, new_val ) )
		return false;

	if ( epmem_get_parameter_type( my_agent, param ) == epmem_param_string )
	{
		(*my_agent->epmem_params[ param ]->param->string_param.value) = new_val;
		return true;
	}
	
	const long converted_val = my_agent->epmem_params[ param ]->param->constant_param.from_str( new_val );

	// learning special case
	if ( param == EPMEM_PARAM_LEARNING )
		set_sysparam( my_agent, EPMEM_ENABLED, converted_val );
	
	my_agent->epmem_params[ param ]->param->constant_param.value = converted_val;

	return true;
}

bool epmem_set_parameter( agent *my_agent, const char *name, const long new_val )
{
	const long param = epmem_convert_parameter( my_agent, name );
	if ( param == EPMEM_PARAMS )
		return false;
	
	if ( !epmem_valid_parameter_value( my_agent, param, new_val ) )
		return false;

	// learning special case
	if ( param == EPMEM_PARAM_LEARNING )
		set_sysparam( my_agent, EPMEM_ENABLED, new_val );
	
	my_agent->epmem_params[ param ]->param->constant_param.value = new_val;

	return true;
}

//

bool epmem_set_parameter( agent *my_agent, const long param, double new_val )
{
	if ( !epmem_valid_parameter_value( my_agent, param, new_val ) )
		return false;
	
	my_agent->epmem_params[ param ]->param->number_param.value = new_val;

	return true;
}

bool epmem_set_parameter( agent *my_agent, const long param, const char *new_val )
{
	if ( !epmem_valid_parameter_value( my_agent, param, new_val ) )
		return false;

	if ( epmem_get_parameter_type( my_agent, param ) == epmem_param_string )
	{
		(*my_agent->epmem_params[ param ]->param->string_param.value) = new_val;
		return true;
	}
	
	const long converted_val = my_agent->epmem_params[ param ]->param->constant_param.from_str( new_val );

	// learning special case
	if ( param == EPMEM_PARAM_LEARNING )
		set_sysparam( my_agent, EPMEM_ENABLED, converted_val );
	
	my_agent->epmem_params[ param ]->param->constant_param.value = converted_val;

	return true;
}

bool epmem_set_parameter( agent *my_agent, const long param, const long new_val )
{	
	if ( !epmem_valid_parameter_value( my_agent, param, new_val ) )
		return false;

	// learning special case
	if ( param == EPMEM_PARAM_LEARNING )
		set_sysparam( my_agent, EPMEM_ENABLED, new_val );
	
	my_agent->epmem_params[ param ]->param->constant_param.value = new_val;

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// learning
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_validate_learning
 **************************************************************************/
bool epmem_validate_learning( const long new_val )
{
	return ( ( new_val == EPMEM_LEARNING_ON ) || ( new_val == EPMEM_LEARNING_OFF ) );
}

/***************************************************************************
 * Function     : epmem_convert_learning
 **************************************************************************/
const char *epmem_convert_learning( const long val )
{
	const char *return_val = NULL;
	
	switch ( val )
	{
		case EPMEM_LEARNING_ON:
			return_val = "on";
			break;
			
		case EPMEM_LEARNING_OFF:
			return_val = "off";
			break;
	}
	
	return return_val;
}

const long epmem_convert_learning( const char *val )
{
	long return_val = NULL;
	
	if ( !strcmp( val, "on" ) )
		return_val = EPMEM_LEARNING_ON;
	else if ( !strcmp( val, "off" ) )
		return_val = EPMEM_LEARNING_OFF;
	
	return return_val;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// database
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_validate_database
 **************************************************************************/
bool epmem_validate_database( const long new_val )
{
	return ( ( new_val == EPMEM_DB_MEM ) || ( new_val == EPMEM_DB_FILE ) );
}

/***************************************************************************
 * Function     : epmem_convert_database
 **************************************************************************/
const char *epmem_convert_database( const long val )
{
	const char *return_val = NULL;
	
	switch ( val )
	{
		case EPMEM_DB_MEM:
			return_val = "memory";
			break;
			
		case EPMEM_DB_FILE:
			return_val = "file";
			break;
	}
	
	return return_val;
}

const long epmem_convert_database( const char *val )
{
	long return_val = NULL;
	
	if ( !strcmp( val, "memory" ) )
		return_val = EPMEM_DB_MEM;
	else if ( !strcmp( val, "file" ) )
		return_val = EPMEM_DB_FILE;
	
	return return_val;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// path
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_validate_path
 **************************************************************************/
bool epmem_validate_path( const char *new_val )
{
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// indexing
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_validate_indexing
 **************************************************************************/
bool epmem_validate_indexing( const long new_val )
{
	return ( ( new_val > 0 ) && ( new_val <= EPMEM_INDEXING_BIGTREE_INSTANCE ) );
}

/***************************************************************************
 * Function     : epmem_convert_indexing
 **************************************************************************/
const char *epmem_convert_indexing( const long val )
{
	const char *return_val = NULL;
	
	switch ( val )
	{
		case EPMEM_INDEXING_BIGTREE_INSTANCE:
			return_val = "bigtree_instance";
			break;
	}
	
	return return_val;
}

const long epmem_convert_indexing( const char *val )
{
	long return_val = NULL;
	
	if ( !strcmp( val, "bigtree_instance" ) )
		return_val = EPMEM_INDEXING_BIGTREE_INSTANCE;
	
	return return_val;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// provenance
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_validate_provenance
 **************************************************************************/
bool epmem_validate_provenance( const long new_val )
{
	return ( ( new_val == EPMEM_PROVENANCE_ON ) || ( new_val == EPMEM_PROVENANCE_OFF ) );
}

/***************************************************************************
 * Function     : epmem_convert_provenance
 **************************************************************************/
const char *epmem_convert_provenance( const long val )
{
	const char *return_val = NULL;
	
	switch ( val )
	{
		case EPMEM_PROVENANCE_ON:
			return_val = "on";
			break;
			
		case EPMEM_PROVENANCE_OFF:
			return_val = "off";
			break;
	}
	
	return return_val;
}

const long epmem_convert_provenance( const char *val )
{
	long return_val = NULL;
	
	if ( !strcmp( val, "on" ) )
		return_val = EPMEM_PROVENANCE_ON;
	else if ( !strcmp( val, "off" ) )
		return_val = EPMEM_PROVENANCE_OFF;
	
	return return_val;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// trigger
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_validate_trigger
 **************************************************************************/
bool epmem_validate_trigger( const long new_val )
{
	return ( ( new_val > 0 ) && ( new_val <= EPMEM_TRIGGER_OUTPUT ) );
}

/***************************************************************************
 * Function     : epmem_convert_trigger
 **************************************************************************/
const char *epmem_convert_trigger( const long val )
{
	const char *return_val = NULL;
	
	switch ( val )
	{
		case EPMEM_TRIGGER_OUTPUT:
			return_val = "output";
			break;
	}
	
	return return_val;
}

const long epmem_convert_trigger( const char *val )
{
	long return_val = NULL;
	
	if ( !strcmp( val, "output" ) )
		return_val = EPMEM_TRIGGER_OUTPUT;	
	
	return return_val;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_enabled
 **************************************************************************/
bool epmem_enabled( agent *my_agent )
{
	return ( my_agent->sysparams[ EPMEM_ENABLED ] == EPMEM_LEARNING_ON );
}

/***************************************************************************
 * Function     : epmem_clean_stats
 **************************************************************************/
void epmem_clean_stats( agent *my_agent )
{
	for ( int i=0; i<EPMEM_STATS; i++ )
	  delete my_agent->epmem_stats[ i ];
}

/***************************************************************************
 * Function     : epmem_reset_stats
 **************************************************************************/
void epmem_reset_stats( agent *my_agent )
{
	for ( int i=0; i<EPMEM_STATS; i++ )
		my_agent->epmem_stats[ i ]->value = 0;
}

/***************************************************************************
 * Function     : epmem_add_stat
 **************************************************************************/
epmem_stat *epmem_add_stat( const char *name )
{
	// new stat entry
	epmem_stat *newbie = new epmem_stat;
	newbie->name = name;
	newbie->value = 0;
	
	return newbie;
}

/***************************************************************************
 * Function     : epmem_convert_stat
 **************************************************************************/
const long epmem_convert_stat( agent *my_agent, const char *name )
{
	for ( int i=0; i<EPMEM_STATS; i++ )
		if ( !strcmp( name, my_agent->epmem_stats[ i ]->name ) )
			return i;

	return EPMEM_STATS;
}

const char *epmem_convert_stat( agent *my_agent, const long stat )
{
	if ( ( stat < 0 ) || ( stat >= EPMEM_STATS ) )
		return NULL;

	return my_agent->epmem_stats[ stat ]->name;
}

/***************************************************************************
 * Function     : epmem_valid_stat
 **************************************************************************/
bool epmem_valid_stat( agent *my_agent, const char *name )
{
	return ( epmem_convert_stat( my_agent, name ) != EPMEM_STATS );
}

bool epmem_valid_stat( agent *my_agent, const long stat )
{
	return ( epmem_convert_stat( my_agent, stat ) != NULL );
}

/***************************************************************************
 * Function     : epmem_get_stat
 **************************************************************************/
double epmem_get_stat( agent *my_agent, const char *name )
{
	const long stat = epmem_convert_stat( my_agent, name );
	if ( stat == EPMEM_STATS )
		return 0;

	return my_agent->epmem_stats[ stat ]->value;
}

double epmem_get_stat( agent *my_agent, const long stat )
{
	if ( !epmem_valid_stat( my_agent, stat ) )
		return 0;

	return my_agent->epmem_stats[ stat ]->value;
}

/***************************************************************************
 * Function     : epmem_set_stat
 **************************************************************************/
bool epmem_set_stat( agent *my_agent, const char *name, double new_val )
{
	const long stat = epmem_convert_stat( my_agent, name );
	if ( stat == EPMEM_STATS )
		return false;
	
	my_agent->epmem_stats[ stat ]->value = new_val;
	
	return true;
}

bool epmem_set_stat( agent *my_agent, const long stat, double new_val )
{
	if ( !epmem_valid_stat( my_agent, stat ) )
		return false;
	
	my_agent->epmem_stats[ stat ]->value = new_val;
	
	return true;
}

//

/***************************************************************************
 * Function     : epmem_reset
 **************************************************************************/
void epmem_reset( agent *my_agent )
{
	Symbol *goal = my_agent->top_goal;
	while( goal )
	{
		epmem_data *data = goal->id.epmem_info;
				
		data->last_tag = 0;
		
		goal = goal->id.lower_goal;
	}
}

/***************************************************************************
 * Function     : epmem_consider_new_episode
 **************************************************************************/
void epmem_consider_new_episode( agent *my_agent )
{
	const long trigger = epmem_get_parameter( my_agent, EPMEM_PARAM_TRIGGER, EPMEM_RETURN_LONG );
	bool new_memory = false;
	
	if ( trigger == EPMEM_TRIGGER_OUTPUT )
	{
		slot *s;
		wme *w;
		Symbol *ol = my_agent->io_header_output;
		
			
		// examine all commands on the output-link for any
		// that appeared since last memory was recorded
		for ( s = ol->id.slots; s != NIL; s = s->next )
		{
			for ( w = s->wmes; w != NIL; w = w->next )
			{
				if ( w->timetag > my_agent->bottom_goal->id.epmem_info->last_tag )
				{
					new_memory = true;
					my_agent->bottom_goal->id.epmem_info->last_tag = w->timetag; 
				}
			}
		}
	}
	
	if ( new_memory )
		epmem_new_episode( my_agent );
}

/* ===================================================================
   epmem_get_augs_of_id()

   This routine works just like the one defined in utilities.h.
   Except this one does not use C++ templates because I have an
   irrational dislike for them borne from the years when the STL
   highly un-portable.  I'm told this is no longer true but I'm still
   bitter. 
   
   Created (sort of): 25 Jan 2006
   =================================================================== */
wme **epmem_get_augs_of_id(agent* thisAgent, Symbol * id, tc_number tc, int *num_attr)
{
   slot *s;
   wme *w;


   wme **list;                 /* array of WME pointers, AGR 652 */
   int attr;                   /* attribute index, AGR 652 */
   int n;


/* AGR 652  The plan is to go through the list of WMEs and find out how
  many there are.  Then we malloc an array of that many pointers.
  Then we go through the list again and copy all the pointers to that array.
  Then we qsort the array and print it out.  94.12.13 */


   if (id->common.symbol_type != IDENTIFIER_SYMBOL_TYPE)
       return NULL;
   if (id->id.tc_num == tc)
       return NULL;
   id->id.tc_num = tc;


   /* --- first, count all direct augmentations of this id --- */
   n = 0;
   for (w = id->id.impasse_wmes; w != NIL; w = w->next)
       n++;
   for (w = id->id.input_wmes; w != NIL; w = w->next)
       n++;
   for (s = id->id.slots; s != NIL; s = s->next) {
       for (w = s->wmes; w != NIL; w = w->next)
           n++;
       for (w = s->acceptable_preference_wmes; w != NIL; w = w->next)
           n++;
   }


   /* --- next, construct the array of wme pointers and sort them --- */
   list = static_cast<wme**>(allocate_memory(thisAgent, n * sizeof(wme *), MISCELLANEOUS_MEM_USAGE));
   attr = 0;
   for (w = id->id.impasse_wmes; w != NIL; w = w->next)
       list[attr++] = w;
   for (w = id->id.input_wmes; w != NIL; w = w->next)
       list[attr++] = w;
   for (s = id->id.slots; s != NIL; s = s->next) {
       for (w = s->wmes; w != NIL; w = w->next)
           list[attr++] = w;
       for (w = s->acceptable_preference_wmes; w != NIL; w = w->next)
           list[attr++] = w;
   }


   *num_attr = n;
   return list;
}

/***************************************************************************
 * Function     : epmem_new_episode
 **************************************************************************/
void epmem_new_episode( agent *my_agent )
{	
	// provide trace output
	if ( my_agent->sysparams[ TRACE_EPMEM_SYSPARAM ] )
	{
		char buf[256];
		SNPRINTF( buf, 254, "NEW EPISODE: (%c%d)", my_agent->bottom_goal->id.name_letter, my_agent->bottom_goal->id.name_number );
		
		print( my_agent, buf );
		
		gSKI_MakeAgentCallbackXML( my_agent, kFunctionBeginTag, kTagWarning );
		gSKI_MakeAgentCallbackXML( my_agent, kFunctionAddAttribute, kTypeString, buf );
		gSKI_MakeAgentCallbackXML( my_agent, kFunctionEndTag, kTagWarning );
	}
	
	// if this is the first episode, initialize db components	
	if ( my_agent->epmem_db_status == -1 )
	{			
		const char *db_path;
		if ( epmem_get_parameter( my_agent, EPMEM_PARAM_DB, EPMEM_RETURN_LONG ) == EPMEM_DB_MEM )
			db_path = ":memory:";
		else
			db_path = epmem_get_parameter( my_agent, EPMEM_PARAM_PATH, EPMEM_RETURN_STRING );
		
		// attempt connection
		my_agent->epmem_db_status = sqlite3_open_v2( db_path, &(my_agent->epmem_db), SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL );
		if ( my_agent->epmem_db_status )
		{
			char buf[256];
			SNPRINTF( buf, 254, "DB ERROR: %s", sqlite3_errmsg( my_agent->epmem_db ) );
			
			print( my_agent, buf );
					
			gSKI_MakeAgentCallbackXML( my_agent, kFunctionBeginTag, kTagWarning );
			gSKI_MakeAgentCallbackXML( my_agent, kFunctionAddAttribute, kTypeString, buf );
			gSKI_MakeAgentCallbackXML( my_agent, kFunctionEndTag, kTagWarning );
		}
		else
		{
			const char *tail;
			sqlite3_stmt *create;
			int rc;
						
			// create vars table (needed before var queries)
			sqlite3_prepare_v2( my_agent->epmem_db, "CREATE TABLE IF NOT EXISTS vars (id INT PRIMARY KEY,value NONE)", -1, &create, &tail );
			sqlite3_step( create );
			sqlite3_finalize( create );
			
			// common queries
			sqlite3_prepare_v2( my_agent->epmem_db, "BEGIN", -1, &( my_agent->epmem_statements[ EPMEM_STMT_BEGIN ] ), &tail );
			sqlite3_prepare_v2( my_agent->epmem_db, "COMMIT", -1, &( my_agent->epmem_statements[ EPMEM_STMT_COMMIT ] ), &tail );
			sqlite3_prepare_v2( my_agent->epmem_db, "ROLLBACK", -1, &( my_agent->epmem_statements[ EPMEM_STMT_ROLLBACK ] ), &tail );			
			sqlite3_prepare_v2( my_agent->epmem_db, "SELECT value FROM vars WHERE id=?", -1, &( my_agent->epmem_statements[ EPMEM_STMT_VAR_GET ] ), &tail );
			sqlite3_prepare_v2( my_agent->epmem_db, "REPLACE INTO vars (id,value) VALUES (?,?)", -1, &( my_agent->epmem_statements[ EPMEM_STMT_VAR_SET ] ), &tail );
			
			// further statement preparation depends upon representation options
			const long indexing = epmem_get_parameter( my_agent, EPMEM_PARAM_INDEXING, EPMEM_RETURN_LONG );
			const long provenance = epmem_get_parameter( my_agent, EPMEM_PARAM_PROVENANCE, EPMEM_RETURN_LONG );
						
			// at this point initialize the database for receipt of episodes
			sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BEGIN ] );
			sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BEGIN ] );
			switch ( indexing )
			{
				case EPMEM_INDEXING_BIGTREE_INSTANCE:					
					// episodes table
					sqlite3_prepare_v2( my_agent->epmem_db, "CREATE TABLE IF NOT EXISTS episodes (id INT,time INT,weight REAL)", -1, &create, &tail );
					sqlite3_step( create );					
					sqlite3_finalize( create );
					
					// id index
					sqlite3_prepare_v2( my_agent->epmem_db, "CREATE INDEX IF NOT EXISTS id ON episodes (id)", -1, &create, &tail );
					sqlite3_step( create );					
					sqlite3_finalize( create );
					
					// weight index (for sorting)
					sqlite3_prepare_v2( my_agent->epmem_db, "CREATE INDEX IF NOT EXISTS weight ON episodes (weight)", -1, &create, &tail );
					sqlite3_step( create );					
					sqlite3_finalize( create );
					
					// time index (for next)
					sqlite3_prepare_v2( my_agent->epmem_db, "CREATE INDEX IF NOT EXISTS time ON episodes (time)", -1, &create, &tail );
					sqlite3_step( create );					
					sqlite3_finalize( create );

					// custom statement for inserting episodes
					sqlite3_prepare_v2( my_agent->epmem_db, "INSERT INTO episodes (id,time,weight) VALUES (?,?,?)", -1, &( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_ADD_EPISODE ] ), &tail );
					
					// first id table
					sqlite3_prepare_v2( my_agent->epmem_db, "CREATE TABLE IF NOT EXISTS id_0 (id INT PRIMARY KEY,name TEXT,value NONE)", -1, &create, &tail );
					sqlite3_step( create );
					sqlite3_finalize( create );
					
					// main index
					sqlite3_prepare_v2( my_agent->epmem_db, "CREATE INDEX IF NOT EXISTS name_value ON id_0 (name,value)", -1, &create, &tail );
					sqlite3_step( create );					
					sqlite3_finalize( create );
					
					// create queries for any id tables
					sqlite3_prepare_v2( my_agent->epmem_db, "SELECT DISTINCT ltrim(tbl_name, 'id_') AS my_id FROM sqlite_master WHERE tbl_name LIKE 'id_%' AND type='table' ORDER BY tbl_name ASC", -1, &create, &tail );
					while ( sqlite3_step( create ) == SQLITE_ROW )
					{
						// get id
						rc = sqlite3_column_int( create, 0 );						
						
						char temp_sql[256];
						sqlite3_stmt *temp_stmt;
						
						// create insert query for the id
						SNPRINTF( temp_sql, 254, "INSERT INTO id_%d (id,name,value) VALUES (?,?,?)", rc );
						sqlite3_prepare_v2( my_agent->epmem_db, temp_sql, -1, &temp_stmt, &tail );
						(*my_agent->epmem_dyn_statements)[ ( ( rc * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_INSERT ) ] = temp_stmt;
						temp_stmt = NULL;
						
						// create select query for the id
						SNPRINTF( temp_sql, 254, "SELECT id FROM id_%d WHERE name=? AND value=?", rc );
						sqlite3_prepare_v2( my_agent->epmem_db, temp_sql, -1, &temp_stmt, &tail );
						(*my_agent->epmem_dyn_statements)[ ( ( rc * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_SELECT ) ] = temp_stmt;
						temp_stmt = NULL;
						
						// create null query for the id
						SNPRINTF( temp_sql, 254, "SELECT id FROM id_%d WHERE name=? AND value IS NULL", rc );
						sqlite3_prepare_v2( my_agent->epmem_db, temp_sql, -1, &temp_stmt, &tail );
						(*my_agent->epmem_dyn_statements)[ ( ( rc * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_NULL ) ] = temp_stmt;
						temp_stmt = NULL;
					}
					sqlite3_finalize( create );
					
					// get max id
					sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_VAR_GET ], 1, EPMEM_VAR_BIGTREE_MAX_ID );
					if ( sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_VAR_GET ] ) == SQLITE_ROW )
						my_agent->epmem_id_counter = sqlite3_column_int( my_agent->epmem_statements[ EPMEM_STMT_VAR_GET ], 0 );					
					sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_VAR_GET ] );
					
					// get max time
					sqlite3_prepare_v2( my_agent->epmem_db, "SELECT MAX(time) FROM episodes", -1, &create, &tail );
					if ( sqlite3_step( create ) == SQLITE_ROW )						
						my_agent->epmem_time_counter = ( sqlite3_column_int( create, 0 ) + 1 );
					sqlite3_finalize( create );			
					
					break;
			}
			
			switch ( provenance )
			{
				case EPMEM_PROVENANCE_ON:
					break;
					
				case EPMEM_PROVENANCE_OFF:
					break;
			}
			sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_COMMIT ] );
			sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_COMMIT ] );
		}
	}
	
	// add the episode only if db is properly initialized
	if ( my_agent->epmem_db_status != SQLITE_OK )
		return;
	
	if ( epmem_get_parameter( my_agent, EPMEM_PARAM_INDEXING, EPMEM_RETURN_LONG ) == EPMEM_INDEXING_BIGTREE_INSTANCE )
	{
		// for now we are only recording episodes at the top state
		Symbol *parent_sym;
		wme **wmes = NULL;
		int len = 0;
		int pos = 0;
		int i;
		int parent_id;
		int child_id;
		int tc = my_agent->top_goal->id.tc_num + 3;
		vector<Symbol *> syms;
		vector<int> ids;

		syms.push_back( my_agent->top_goal );
		ids.push_back( 0 );	
		
		sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BEGIN ] );
		sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BEGIN ] );
		while ( pos != syms.size() )
		{		
			parent_sym = syms[ pos ];
			parent_id = ids[ pos ];
			pos++;				
			wmes = epmem_get_augs_of_id( my_agent, parent_sym, tc, &len );

			if ( wmes != NULL )
			{
				for ( i=0; i<len; i++ )
				{
					// find wme id
					child_id = -1;
					if ( wmes[i]->value->common.symbol_type != IDENTIFIER_SYMBOL_TYPE )
					{
						sqlite3_bind_text( (*my_agent->epmem_dyn_statements)[ ( ( parent_id * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_SELECT ) ], 1, (const char *) wmes[i]->attr->sc.name, -1, SQLITE_STATIC );
						switch( wmes[i]->value->common.symbol_type )
						{
							case SYM_CONSTANT_SYMBOL_TYPE:
								sqlite3_bind_text( (*my_agent->epmem_dyn_statements)[ ( ( parent_id * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_SELECT ) ], 2, (const char *) wmes[i]->value->sc.name, -1, SQLITE_STATIC );
								break;
					            
							case INT_CONSTANT_SYMBOL_TYPE:
			        			sqlite3_bind_int( (*my_agent->epmem_dyn_statements)[ ( ( parent_id * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_SELECT ) ], 2, wmes[i]->value->ic.value );
								break;
				
							case FLOAT_CONSTANT_SYMBOL_TYPE:
			        			sqlite3_bind_double( (*my_agent->epmem_dyn_statements)[ ( ( parent_id * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_SELECT ) ], 2, wmes[i]->value->fc.value );
								break;
						}
						
						if ( sqlite3_step( (*my_agent->epmem_dyn_statements)[ ( ( parent_id * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_SELECT ) ] ) == SQLITE_ROW )
							child_id = sqlite3_column_int( (*my_agent->epmem_dyn_statements)[ ( ( parent_id * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_SELECT ) ], 0 );
						
						sqlite3_reset( (*my_agent->epmem_dyn_statements)[ ( ( parent_id * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_SELECT ) ] );
					}
					else
					{
						sqlite3_bind_text( (*my_agent->epmem_dyn_statements)[ ( ( parent_id * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_NULL ) ], 1, (const char *) wmes[i]->attr->sc.name, -1, SQLITE_STATIC );
						if ( sqlite3_step( (*my_agent->epmem_dyn_statements)[ ( ( parent_id * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_NULL ) ] ) == SQLITE_ROW )
							child_id = sqlite3_column_int( (*my_agent->epmem_dyn_statements)[ ( ( parent_id * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_NULL ) ], 0 );
						
						sqlite3_reset( (*my_agent->epmem_dyn_statements)[ ( ( parent_id * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_NULL ) ] );
					}
					
					// insert on no id
					if ( child_id == -1 )
					{
						child_id = (my_agent->epmem_id_counter++);
							
						// insert
						sqlite3_bind_int( (*my_agent->epmem_dyn_statements)[ ( ( parent_id * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_INSERT ) ], 1, child_id );
						sqlite3_bind_text( (*my_agent->epmem_dyn_statements)[ ( ( parent_id * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_INSERT ) ], 2, (const char *) wmes[i]->attr->sc.name, -1, SQLITE_STATIC );				
						switch ( wmes[i]->value->common.symbol_type )
						{
							case SYM_CONSTANT_SYMBOL_TYPE:
								sqlite3_bind_text( (*my_agent->epmem_dyn_statements)[ ( ( parent_id * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_INSERT ) ], 3, (const char *) wmes[i]->value->sc.name, -1, SQLITE_STATIC );
								break;
								
							case INT_CONSTANT_SYMBOL_TYPE:
								sqlite3_bind_int( (*my_agent->epmem_dyn_statements)[ ( ( parent_id * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_INSERT ) ], 3, wmes[i]->value->ic.value );
								break;
								
							case FLOAT_CONSTANT_SYMBOL_TYPE:
								sqlite3_bind_double( (*my_agent->epmem_dyn_statements)[ ( ( parent_id * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_INSERT ) ], 3, wmes[i]->value->fc.value );
								break;
								
							case IDENTIFIER_SYMBOL_TYPE:
								sqlite3_bind_null( (*my_agent->epmem_dyn_statements)[ ( ( parent_id * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_INSERT ) ], 3 );
								break;
						}
						sqlite3_step( (*my_agent->epmem_dyn_statements)[ ( ( parent_id * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_INSERT ) ] );
						sqlite3_reset( (*my_agent->epmem_dyn_statements)[ ( ( parent_id * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_INSERT ) ] );
						
						// also create new id table
						if ( wmes[i]->value->common.symbol_type == IDENTIFIER_SYMBOL_TYPE )
						{
							sqlite3_stmt *create;
							const char *tail;
							char temp_sql[256];				
							
							// id table
							SNPRINTF( temp_sql, 254, "CREATE TABLE IF NOT EXISTS id_%d (id INT PRIMARY KEY,name TEXT,value NONE)", child_id );
							sqlite3_prepare_v2( my_agent->epmem_db, temp_sql, -1, &create, &tail );
							sqlite3_step( create );
							sqlite3_finalize( create );
							
							// main index
							SNPRINTF( temp_sql, 254, "CREATE INDEX IF NOT EXISTS name_value ON id_%d (name,value)", child_id );
							sqlite3_prepare_v2( my_agent->epmem_db, temp_sql, -1, &create, &tail );
							sqlite3_step( create );					
							sqlite3_finalize( create );
							
							// create insert query for the id
							SNPRINTF( temp_sql, 254, "INSERT INTO id_%d (id,name,value) VALUES (?,?,?)", child_id );
							sqlite3_prepare_v2( my_agent->epmem_db, temp_sql, -1, &create, &tail );
							(*my_agent->epmem_dyn_statements)[ ( ( child_id * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_INSERT ) ] = create;
							create = NULL;
							
							// create select query for the id
							SNPRINTF( temp_sql, 254, "SELECT id FROM id_%d WHERE name=? AND value=?", child_id );
							sqlite3_prepare_v2( my_agent->epmem_db, temp_sql, -1, &create, &tail );
							(*my_agent->epmem_dyn_statements)[ ( ( child_id * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_SELECT ) ] = create;
							create = NULL;
							
							// create null query for the id
							SNPRINTF( temp_sql, 254, "SELECT id FROM id_%d WHERE name=? AND value IS NULL", child_id );
							sqlite3_prepare_v2( my_agent->epmem_db, temp_sql, -1, &create, &tail );
							(*my_agent->epmem_dyn_statements)[ ( ( child_id * EPMEM_BIGTREE_QUERIES ) + EPMEM_BIGTREE_NULL ) ] = create;
							create = NULL;
						}
					}
					
					// keep track of identifiers (for further study)
					if ( wmes[i]->value->common.symbol_type == IDENTIFIER_SYMBOL_TYPE )
					{
						syms.push_back( wmes[i]->value );
						ids.push_back( child_id );
					}
					else
					{
						// add leaf nodes to the episodic store
						sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_ADD_EPISODE ], 1, child_id );
						sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_ADD_EPISODE ], 2, my_agent->epmem_time_counter );
						sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_ADD_EPISODE ], 3, 0 );
						sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_ADD_EPISODE ] );
						sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_ADD_EPISODE ] );
					}
				}
			}
		}
		sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_COMMIT ] );
		sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_COMMIT ] );	
		my_agent->epmem_time_counter++;
	}
}
