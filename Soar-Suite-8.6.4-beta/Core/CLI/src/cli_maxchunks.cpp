/////////////////////////////////////////////////////////////////
// max-chunks command file.
//
// Author: Jonathan Voigt, voigtjr@gmail.com
// Date  : 2004
//
/////////////////////////////////////////////////////////////////

#include <portability.h>

#include "cli_CommandLineInterface.h"

#include "cli_Commands.h"
#include "sml_Names.h"
#include "sml_StringOps.h"

#include "gSKI_Agent.h"

using namespace cli;
using namespace sml;

bool CommandLineInterface::ParseMaxChunks(gSKI::Agent* pAgent, std::vector<std::string>& argv) {

	// n defaults to 0 (print current value)
	int n = 0;

	if (argv.size() > 2) return SetError(CLIError::kTooManyArgs);

	// one argument, figure out if it is a positive integer
	if (argv.size() == 2) {
		n = atoi(argv[1].c_str());
		if (n <= 0) return SetError(CLIError::kIntegerMustBePositive);
	}

	return DoMaxChunks(pAgent, n);
}

bool CommandLineInterface::DoMaxChunks(gSKI::Agent* pAgent, const int n) {

	if (!RequireAgent(pAgent)) return false;

	if (!n) {
		// query
		if (m_RawOutput) {
			m_Result << pAgent->GetMaxChunks();
		} else {
			char buf[kMinBufferSize];
			AppendArgTagFast(sml_Names::kParamValue, sml_Names::kTypeInt, Int2String(pAgent->GetMaxChunks(), buf, kMinBufferSize));
		}
		return true;
	}

	pAgent->SetMaxChunks(n);
	return true;
}

