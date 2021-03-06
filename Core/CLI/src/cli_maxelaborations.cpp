/////////////////////////////////////////////////////////////////
// max-elaborations command file.
//
// Author: Jonathan Voigt, voigtjr@gmail.com
// Date  : 2004
//
/////////////////////////////////////////////////////////////////

#include "portability.h"

#include "cli_CommandLineInterface.h"

#include "cli_Commands.h"
#include "sml_Names.h"
#include "sml_AgentSML.h"

#include "agent.h"

using namespace cli;
using namespace sml;

bool CommandLineInterface::DoMaxElaborations(const int n)
{
    agent* thisAgent = m_pAgentSML->GetSoarAgent();
    if (!n)
    {
        // Query
        if (m_RawOutput)
        {
            m_Result << thisAgent->sysparams[MAX_ELABORATIONS_SYSPARAM];
        }
        else
        {
            std::string temp;
            AppendArgTagFast(sml_Names::kParamValue, sml_Names::kTypeInt, to_string(thisAgent->sysparams[MAX_ELABORATIONS_SYSPARAM], temp));
        }
        return true;
    }
    
    thisAgent->sysparams[MAX_ELABORATIONS_SYSPARAM] = n;
    return true;
}

