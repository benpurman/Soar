/////////////////////////////////////////////////////////////////
// EpMem command file.
//
// Author: Jonathan Voigt, voigtjr@gmail.com,
//         Nate Derbinsky, nlderbin@umich.edu
// Date  : 2007
//
/////////////////////////////////////////////////////////////////

#include <portability.h>

#include "cli_CommandLineInterface.h"
#include "cli_Commands.h"

#include "sml_Names.h"
#include "sml_AgentSML.h"

#include "episodic_memory.h"
#include "agent.h"
#include "misc.h"

using namespace cli;
using namespace sml;

bool CommandLineInterface::DoEpMem( const char pOp, const std::string* pAttr, const std::string* pVal, epmem_time_id memory_id )
{
    agent* thisAgent = m_pAgentSML->GetSoarAgent();
   	std::ostringstream tempString;

   	if ( !pOp )
    {   // Print Epmem Settings
        PrintCLIMessage_Header("Episodic Memory Settings", 40);
        PrintCLIMessage_Item("learning:", thisAgent->epmem_params->learning, 40);
        PrintCLIMessage_Section("Encoding", 40);
        PrintCLIMessage_Item("phase:", thisAgent->epmem_params->phase, 40);
        PrintCLIMessage_Item("trigger:", thisAgent->epmem_params->trigger, 40);
        PrintCLIMessage_Item("force:", thisAgent->epmem_params->force, 40);
        PrintCLIMessage_Item("exclusions:", thisAgent->epmem_params->exclusions, 40);
        PrintCLIMessage_Section("Storage", 40);
        PrintCLIMessage_Item("database:", thisAgent->epmem_params->database, 40);
        PrintCLIMessage_Item("append:", thisAgent->epmem_params->append_db, 40);
        PrintCLIMessage_Item("path:", thisAgent->epmem_params->path, 40);
        PrintCLIMessage_Item("lazy-commit:", thisAgent->epmem_params->lazy_commit, 40);
        PrintCLIMessage_Section("Retrieval", 40);
        PrintCLIMessage_Item("balance:", thisAgent->epmem_params->balance, 40);
        PrintCLIMessage_Item("graph-match:", thisAgent->epmem_params->graph_match, 40);
        PrintCLIMessage_Item("graph-match-ordering:", thisAgent->epmem_params->gm_ordering, 40);
        PrintCLIMessage_Section("Performance", 40);
        PrintCLIMessage_Item("page-size:", thisAgent->epmem_params->page_size, 40);
        PrintCLIMessage_Item("cache-size:", thisAgent->epmem_params->cache_size, 40);
        PrintCLIMessage_Item("optimization:", thisAgent->epmem_params->opt, 40);
        PrintCLIMessage_Item("timers:", thisAgent->epmem_params->timers, 40);
        PrintCLIMessage_Section("Experimental", 40);
        PrintCLIMessage_Item("merge:", thisAgent->epmem_params->merge, 40);
        PrintCLIMessage("");

        return true;
    }
	else if ( pOp == 'b' )
    {
        std::string err;

        bool result = epmem_backup_db( thisAgent, pAttr->c_str(), &( err ) );

        if ( !result )
            SetError( "Error while backing up database: " + err );
        else {
           	tempString << "Episodic memory database backed up to " << pAttr->c_str();
           	PrintCLIMessage(&tempString);
        }

        return result;
    }
  else if ( pOp == 'c' )
  {
      const char *msg = "Episodic memory database closed.";
      const char *tag_type = sml_Names::kTypeString;

      epmem_close( thisAgent );
      PrintCLIMessage(msg);
      return true;
  }
  else if ( pOp == 'e' )
    {
       bool result = thisAgent->epmem_params->learning->set_string("on");

        if ( !result )
            SetError( "This parameter is protected while the episodic memory database is open." );
        else
     	   PrintCLIMessage( "Episodic memory enabled.");

        return result;
    }
    else if ( pOp == 'd' )
    {
       bool result = thisAgent->epmem_params->learning->set_string("off");

       if ( !result )
           SetError( "This parameter is protected while the episodic memory database is open." );
       else
    	   PrintCLIMessage( "Episodic memory disabled.");

        return result;
    }
    else if ( pOp == 'g' )
    {
        soar_module::param *my_param = thisAgent->epmem_params->get( pAttr->c_str() );
        if ( !my_param )
            return SetError( "Invalid epmem parameter." );

        PrintCLIMessage_Item("", my_param, 0);
        return true;
    }
    else if ( pOp == 'i' )
    {
    	epmem_reinit_cmd( thisAgent );
    	PrintCLIMessage( "Episodic memory system re-initialized.");
        if ((thisAgent->epmem_params->database->get_value() != epmem_param_container::memory) &&
           (thisAgent->epmem_params->append_db->get_value() == on))
        {
            PrintCLIMessage( "Note: There was no effective change to episodic memory contents \n"
                             "      because Soar is storing episodic memory to a database file and append \n"
                             "      mode is on.");
        }
    	return true;
    }
	else if ( pOp == 'p' )
    {
        std::string viz;

        epmem_print_episode( thisAgent, memory_id, &( viz ) );
        if ( viz.empty() )
        {
            return SetError( "Invalid episode." );
        }
        tempString << "Episode " << memory_id;
        PrintCLIMessage_Header(tempString.str().c_str(), 40);
        PrintCLIMessage(&viz);
        return true;
    }
    else if ( pOp == 's' )
    {
        soar_module::param *my_param = thisAgent->epmem_params->get( pAttr->c_str() );
        if ( !my_param )
            return SetError( "Invalid epmem parameter." );

        if ( !my_param->validate_string( pVal->c_str() ) )
            return SetError( "Invalid setting for epmem parameter." );

        epmem_param_container::db_choices last_db_mode = thisAgent->epmem_params->database->get_value();
        bool result = my_param->set_string( pVal->c_str() );

        if ( !result )
        {
            SetError( "This parameter is protected while the episodic memory database is open." );
        } else {
            tempString << pAttr->c_str() << " = " << pVal->c_str();
            PrintCLIMessage(&tempString);
            if ( thisAgent->epmem_db->get_status() == soar_module::connected )
            {
                if (((!strcmp(pAttr->c_str(),"database")) && (thisAgent->epmem_params->database->get_value() != last_db_mode)) ||
                     (!strcmp(pAttr->c_str(),"path")))
                {
                    PrintCLIMessage( "To finalize episodic memory database switch, issue an epmem --init command.\n");
                }
            }
            if (!strcmp(pAttr->c_str(),"append"))
            {
                if (thisAgent->epmem_params->append_db->get_value() == off)
                {
                    PrintCLIMessage( "Warning: Since append mode is off, starting/reinitializing,\n"
                                     "         Soar will erase the episodic memory database.\n");
                }

            }
        }
        return result;
    }
    else if ( pOp == 'S' )
    {
        epmem_attach(thisAgent);
        if ( !pAttr )
        {   // Print SMem Settings
            PrintCLIMessage_Header("Episodic Memory Statistics", 40);
            PrintCLIMessage_Item("Time:", thisAgent->epmem_stats->time, 40);
            PrintCLIMessage_Item("SQLite Version:", thisAgent->epmem_stats->db_lib_version, 40);
            PrintCLIMessage_Item("Memory Usage:", thisAgent->epmem_stats->mem_usage, 40);
            PrintCLIMessage_Item("Memory Highwater:", thisAgent->epmem_stats->mem_high, 40);
            PrintCLIMessage_Item("Retrievals:", thisAgent->epmem_stats->ncbr, 40);
            PrintCLIMessage_Item("Queries:", thisAgent->epmem_stats->cbr, 40);
            PrintCLIMessage_Item("Nexts:", thisAgent->epmem_stats->nexts, 40);
            PrintCLIMessage_Item("Prevs:", thisAgent->epmem_stats->prevs, 40);
            PrintCLIMessage_Item("Last Retrieval WMEs:", thisAgent->epmem_stats->ncb_wmes, 40);
            PrintCLIMessage_Item("Last Query Positive:", thisAgent->epmem_stats->qry_pos, 40);
            PrintCLIMessage_Item("Last Query Negative:", thisAgent->epmem_stats->qry_neg, 40);
            PrintCLIMessage_Item("Last Query Retrieved:", thisAgent->epmem_stats->qry_ret, 40);
            PrintCLIMessage_Item("Last Query Cardinality:", thisAgent->epmem_stats->qry_card, 40);
            PrintCLIMessage_Item("Last Query Literals:", thisAgent->epmem_stats->qry_lits, 40);
        }
        else
        {
            // check attribute name
            soar_module::statistic *my_stat = thisAgent->epmem_stats->get( pAttr->c_str() );
            if ( !my_stat )
                return SetError( "Invalid statistic." );

            PrintCLIMessage_Item("", my_stat, 0);
        }

        return true;
    }
    else if ( pOp == 't' )
    {
        if ( !pAttr )
        {
            struct foo: public soar_module::accumulator< soar_module::timer * >
            {
            private:
                bool raw;
                cli::CommandLineInterface *this_cli;
                std::ostringstream& m_Result;

                foo& operator=(const foo&) { return *this; }

            public:
                foo( bool m_RawOutput, cli::CommandLineInterface *new_cli, std::ostringstream& m_Result ): raw( m_RawOutput ), this_cli( new_cli ), m_Result( m_Result ) {};


                void operator() ( soar_module::timer *t )
                {
                    std::string output( t->get_name() );
                    output += ":";
                    this_cli->PrintCLIMessage_Item(output.c_str(), t, 40);
                }
            } bar( m_RawOutput, this, m_Result );

            PrintCLIMessage_Header("Episodic Memory Timers", 40);
            thisAgent->epmem_timers->for_each( bar );
        }
        else
        {
            // check attribute name
            soar_module::timer *my_timer = thisAgent->epmem_timers->get( pAttr->c_str() );
            if ( !my_timer )
                return SetError( "Invalid timer." );

            PrintCLIMessage_Item("", my_timer, 0);
        }

        return true;
    }
    else if ( pOp == 'v' )
    {
        std::string viz;

        epmem_visualize_episode( thisAgent, memory_id, &( viz ) );

        if ( viz.empty() ) return SetError( "Invalid episode." );
        PrintCLIMessage(&viz);

        return true;
    }

    return SetError( "Unknown option." );
}
