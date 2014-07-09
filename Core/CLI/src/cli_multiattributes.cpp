/////////////////////////////////////////////////////////////////
// multi-attributes command file.
//
// Author: Jonathan Voigt, voigtjr@gmail.com
// Date  : 2004
//
/////////////////////////////////////////////////////////////////

#include <portability.h>

#include "cli_CommandLineInterface.h"

#include "cli_Commands.h"

#include "sml_Names.h"
#include "sml_AgentSML.h"

#include "agent.h"
#include "production.h"
#include "print.h"
#include "symtab.h"

using namespace cli;
using namespace sml;

bool CommandLineInterface::DoMultiAttributes(const std::string* pAttribute, int n) {
    agent* thisAgent = m_pAgentSML->GetSoarAgent();
    multi_attribute* maList = thisAgent->multi_attributes;

    if (!pAttribute && !n) {

        // No args, print current setting
        int count = 0;

        if ( !maList )
        {
            m_Result << "No multi-attributes found.";
        }

        std::stringstream buffer;

        if ( m_RawOutput ) m_Result << "Value\tSymbol";

        while( maList )
        {
            // Arbitrary buffer and size
            char attributeName[1024];
            symbol_to_string(thisAgent, maList->symbol, true, attributeName, 1024);

            if (m_RawOutput) {
                m_Result  << maList->value << "\t" << symbol_to_string(thisAgent, maList->symbol, true, attributeName, 1024)<< "\n";

            } else {
                buffer << maList->value;
                // Value
                AppendArgTagFast( sml_Names::kParamValue, sml_Names::kTypeInt, buffer.str() );
                buffer.clear();

                // Symbol
                AppendArgTagFast( sml_Names::kParamName, sml_Names::kTypeString, attributeName );
            }

            ++count;

            maList = maList->next;
        }

        buffer << count;
        if (!m_RawOutput) {
            PrependArgTagFast(sml_Names::kParamCount, sml_Names::kTypeInt, buffer.str() );
        }
        return true;
    }

    // Setting defaults to 10
    if (!n) n = 10;

    // Set it
    Symbol* s = make_str_constant( thisAgent, pAttribute->c_str() );

    while (maList)
    {
        if (maList->symbol == s)
        {
            maList->value = n;
            symbol_remove_ref(thisAgent, s);
            return true;
        }

        maList = maList->next;
    }

    /* sym wasn't in the table if we get here, so add it */
    maList = static_cast<multi_attribute *>(allocate_memory(thisAgent, sizeof(multi_attribute), MISCELLANEOUS_MEM_USAGE));
    assert(maList);

    maList->value = n;
    maList->symbol = s;
    maList->next = thisAgent->multi_attributes;
    thisAgent->multi_attributes = maList;

     return true;
}

