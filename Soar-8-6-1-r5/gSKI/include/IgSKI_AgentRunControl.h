/********************************************************************
* @file igski_agentruncontrol.h 
*********************************************************************
* @remarks Copyright (C) 2002 Soar Technology, All rights reserved. 
* The U.S. government has non-exclusive license to this software 
* for government purposes. 
*********************************************************************
* created:	   6/13/2002   14:50
*
* purpose: 
*********************************************************************/
#ifndef IGSKI_AGENTRUNCONTROL_H
#define IGSKI_AGENTRUNCONTROL_H

#include "gSKI_Enumerations.h"

namespace gSKI {

   // Forward declarations
   struct Error;

   /** 
   * @brief Interface for controlling agent execution
   *
   * This class provides an abstract interface for objects that
   *  control agent execution.  There are two such objects:
   *  IAgentManager and IAgentThreadGroup.
   *
   */
   class IAgentRunControl {
   public:

      /** 
      * @brief Destructor (does nothing)
      */ 
      virtual ~IAgentRunControl() {}

      /** 
      * @brief Initializes all of the agents managed by this object
      *
      * This method calls the IAgent::ReinitializeWithOldSettings method on
      *  all of the agents managed by this object.  It will not return until
      *  all agents in this thread group have been reinitialized.
      *
      * This call will stop the entire thread group (if it is running)
      *  at the next valid stop time (see IAgentRunControl::Stop).  
      *
      * @see IAgent::ReinitializeWithOldSettings
      *
      * Possible Errors:
      *   @li Any errors that can be generated by ReinitializeWithOldSettings
      *
      * @param  err Pointer to client-owned error structure.  If the pointer
      *               is not 0 this structure is filled with extended error
      *               information.  If it is 0 (the default) extended error
      *               information is not returned.
      *
      * @return true if all agents were reinitialized successfully, false 
      *           if there was a problem reinitializing one or more agents.
      */
      virtual bool ReinitializeAll(Error* err = 0) = 0;

      /**
      * @brief Runs all agents currently owned by this object
      *
      * This method returns immediately after starting the agents running.
      *  It does not block until run completion.
      *
      * If one or more the agents managed by this object are already running
      *  this method fails and the currently running agent(s) continue to
      *  run as they did before this method call.  
      *
      * Possible Errors:
      *   @li gSKIERR_AGENT_RUNNING if one or more agents is alread running
      *   @li gSKIERR_NO_AGENTS if there are no agents to run.
      *
      * @see egSKIRunType
      *
      * @param runLength How long to run the system.  Choices are       
      *          gSKI_RUN_ELABORATION_PHASE, gSKI_RUN_DECISION_PHASE,
      *          gSKI_RUN_DECISION_CYCLE, gSKI_RUN_UNTIL_OUTPUT, and
      *          gSKI_RUN_FOREVER.  See egSKIRunType for details.
      * @param  count For gSKI_RUN_ELABORATION_PHASE, gSKI_RUN_DECISION_PHASE,
      *          and gSKI_RUN_DECISION_CYCLE this parameter tells the method
      *          how many elaboration phases, decision phase or decision cycles
      *          to run before the thread groups return. For other run types
      *          this parameter is ignored.
      * @param runInterleave How to interleave agent execution.  Choices are
      *          gSKI_INTERLEAVE_ELAB_PHASE, gSKI_INTERLEAVE_DECISION_PHASE
      *          gSKI_INTERLEAVE_DECISION_CYCLE, and gSKI_INTERLEAVE_OUTPUT.
      *          See egSKIInterleaveType for more details.
      * @param  err Pointer to client-owned error structure.  If the pointer
      *               is not 0 this structure is filled with extended error
      *               information.  If it is 0 (the default) extended error
      *               information is not returned.
      *
      * @returns true if the run executed successfully.  false if an
      *           error occured.  See value in err for more detailed 
      *           error information
      */
      virtual bool RunInSeparateThread(egSKIRunType        runLength     = gSKI_RUN_FOREVER, 
                                       unsigned long       count         = 1,
                                       egSKIInterleaveType runInterleave = gSKI_INTERLEAVE_SMALLEST_STEP,
                                       Error*              err           = 0) = 0;

      /** 
      * @brief Runs all of the agents managed by this object in a client owned thread.
      *
      * Call this method to execute agents in a client-owned thread.  Typical
      *  reasons for doing this include running agents in a single threaded
      *  application, and managing threading and agent scheduling in the client.
      *  
      * This method will not return until the the given run length has elapsed
      *  (unless there is an error or Stop is called). 
      *
      * After this method returns, you may call this method again to continue
      *  a run at the point the agents left off.
      *
      * Possible Errors:
      *   @li gSKIERR_AGENT_RUNNING if this method is called before the last
      *         instance of Run returns.
      *   @li gSKIERR_NO_AGENTS if there are no agents to run.
      *
      * @see egSKIRunType
      *
      * @param runLength How long to run the system.  Choices are       
      *          gSKI_RUN_ELABORATION_PHASE, gSKI_RUN_DECISION_PHASE,
      *          gSKI_RUN_DECISION_CYCLE, gSKI_RUN_UNTIL_OUTPUT, and
      *          gSKI_RUN_FOREVER.  See egSKIRunType for details.
      * @param  count For gSKI_RUN_ELABORATION_PHASE, gSKI_RUN_DECISION_PHASE,
      *          and gSKI_RUN_DECISION_CYCLE this parameter tells the method
      *          how many elaboration phases, decision phase or decision cycles
      *          to run before returning. For other run types this parameter
      *          is ignored.
      * @param runInterleave How to interleave agent execution.  Choices are
      *          gSKI_INTERLEAVE_ELAB_PHASE, gSKI_INTERLEAVE_DECISION_PHASE
s      *          gSKI_INTERLEAVE_DECISION_CYCLE, and gSKI_INTERLEAVE_OUTPUT.
      *          See egSKIInterleaveType for more details.
      * @param  err Pointer to client-owned error structure.  If the pointer
      *               is not 0 this structure is filled with extended error
      *               information.  If it is 0 (the default) extended error
      *               information is not returned.
      *
      * @returns true if the run executed successfully.  false if an
      *           error occured.  See value in err for more detailed 
      *           error information
      */
      virtual bool RunInClientThread(egSKIRunType        runLength     = gSKI_RUN_FOREVER, 
                                     unsigned long       count         = 1,
                                     egSKIInterleaveType runInterleave = gSKI_INTERLEAVE_SMALLEST_STEP,
                                     Error*              err           = 0) = 0;

      /**
      * @brief Stops all agents this object manages
      *
      * Call this method to stop all running agents managed by this object. 
      *  This includes agents running in client threads.
      *
      * If no agents are running, nothing happens.
      *
      * Agents do not stop immediately upon being notified to stop. They stop
      *  at one of the safe stopping points listed in the egSKIStopLocation 
      *  enumeration.  Essentially this Stop method is a request to the agents
      *  to stop processing.  They will stop when they get a chance.
      *
      * Be careful when calling Stop from a single threaded application.   If
      *  you call stop in a single threaded app with stopType = gSKI_STOP_BY_SUSPEND
      *  you will put your whole process to sleep with no chance of waking.
      *  If the operating system allows(e.g. Windows and Linux), this method
      *  will check to see if the agents were run in a client-owned thread
      *  and if that thread is the same thread asking to stop.  If it is,
      *  the Stop method will not stop the agents and will return an error.
      *
      * Possible Errors:
      *   @li gSKIERR_CANNOT_PUT_TO_SLEEP if you specify gSKI_STOP_BY_SLEEPING
      *         from the client-owned thread that created the agents. (Only
      *         applicable when running agents using IAgentManager::RunInClientThread
      *         or IAgentThreadGroup::RunInClientThread).
      *   @li gSKIERR_CANNOT_STOP_FOR_CALLBACKS if you specify 
      *           gSKI_STOP_ON_CALLBACK_RETURN or gSKI_STOP_AFTER_ALL_CALLBACKS_RETURN
      *           together with gSKI_STOP_BY_RETURNING.  These settings are 
      *           incompatible because the system cannot safely return after
      *           callbacks (it can sleep, however).
      *   @li gSKIERR_NO_AGENTS if there are no agents to stop.
      * 
      * @see egSKIStopLocation
      * @see egSKIStopType
      *
      * @param stopLoc Where to stop the agent execution.  Possible locations are
      *         gSKI_STOP_ON_CALLBACK_RETURN, gSKI_STOP_AFTER_ALL_CALLBACKS_RETURN,
      *         gSKI_STOP_AFTER_CURRENT_PHASE, gSKI_STOP_NEXT_ELABORATION_PHASE,
      *         gSKI_STOP_NEXT_DECISION_PHASE, and gSKI_STOP_NEXT_DECISION_CYCLE.
      *         See egSKIStopLocation for more details.
      * @param stopType How to stop the agents.  This can be one of
      *               gSKI_STOP_BY_RETURNING or gSKI_STOP_BY_SLEEPING.
      *               See egSKIStopType for more details.
      * @param block Pass true to have the Stop function block until all of the
      *               agents actually stop.  Pass false to have the Stop method
      *               return immediately after notifying the agents that they
      *               should stop.
      * @param  err Pointer to client-owned error structure.  If the pointer
      *               is not 0 this structure is filled with extended error
      *               information.  If it is 0 (the default) extended error
      *               information is not returned.
      *
      * @returns true if the agents could be stopped.  false if there was an
      *           error preventing them from being stopped.  See err for 
      *           detailed error information.
      */
      virtual bool Stop(egSKIStopLocation    stopLoc, 
                        egSKIStopType        stopType,
                        bool                 block = true, 
                        Error*               err   = 0) = 0;
   };


}

#endif
