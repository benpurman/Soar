/*************************************************************************
 * PLEASE SEE THE FILE "license.txt" (INCLUDED WITH THIS SOFTWARE PACKAGE)
 * FOR LICENSE AND COPYRIGHT INFORMATION.
 *************************************************************************/

/* =======================================================================
                                decide.h

   Decide.cpp contains the decider as well as routine for managing working
   memory, preference memory, slots, and the garbage collection of
   disconnected WMEs.

   Whenever a link is added from one identifier to another (i.e.,
   (I37 ^x R26)), we call post_link_addition().  This records the link
   addition and buffers it for later processing.  Similarly, whenever a
   link is removed, we call post_link_removal(), which buffers the
   removal for later processing.  At the end of the phase, we call
   do_buffered_link_changes() to update the goal stack level of all
   identifiers, and garbage collect anything that's now disconnected.

   Whenever some acceptable or require preference for a context slot
   changes, we call mark_context_slot_as_acceptable_preference_changed().

   see decide.cpp for more information in the comments.
======================================================================= */

#ifndef DECIDE_H
#define DECIDE_H

#include "kernel.h"

typedef unsigned char byte;
typedef struct symbol_struct Symbol;
typedef struct wme_struct wme;
typedef struct slot_struct slot;
typedef struct instantiation_struct instantiation;
typedef struct preference_struct preference;
typedef struct agent_struct agent;
typedef struct cons_struct cons;
typedef cons list;
typedef struct dl_cons_struct dl_cons;

#ifdef USE_MEM_POOL_ALLOCATORS
#include "soar_module.h"
template <class T>
class SoarMemoryPoolAllocator;

typedef std::set< wme*, std::less< wme* >, soar_module::soar_memory_pool_allocator< wme* > > wma_pooled_wme_set;
typedef std::map< Symbol*, uint64_t, std::less< Symbol* >, soar_module::soar_memory_pool_allocator< std::pair< Symbol*, uint64_t > > > wma_sym_reference_map;
#else
typedef std::set< wme* > wma_pooled_wme_set;
typedef std::map< Symbol*, uint64_t > wma_sym_reference_map;
#endif

extern void post_link_addition (agent* thisAgent, Symbol *from, Symbol *to);
extern void post_link_removal (agent* thisAgent, Symbol *from, Symbol *to);

extern void mark_context_slot_as_acceptable_preference_changed (agent* thisAgent, slot *s);

void remove_existing_attribute_impasse_for_slot (agent* thisAgent, slot *s);
void post_link_addition (agent* thisAgent, Symbol *from, Symbol *to);
void post_link_removal (agent* thisAgent, Symbol *from, Symbol *to);

/* ------------------------------------------------------------------------
                 Goal Dependency Set

   The Goal Dependency Set is a data structure used in Operand2 to maintain
   the integrity of a subgoal with respect to changes in supergoal WMEs.
   Whenever a WME in the goal's dependency set changes, the goal is immediately
   removed.  The routines for maintaining the GDS and determining if a goal
   should be retracted are in decide.c

   Fields in a goal dependency set:

      goal:  points to the goal for which this dependency set was created.
             The goal also has a pointer back to the GDS.

      wmes_in_gds:  A DLL of WMEs in the goal dependency set

   The GDS is created only when necessary; that is, when an o-supported WME
   is created in some subgoal and that subgoal has no GDS already.  The
   instantiations that led to the creation of the o-supported WME are
   examined; any supergoal WMEs in these instantiations are added to the
   wmes_in_gds DLL.  The GDS for each goal is examined for every WM change;
   if a WME changes that is on a GDS, the goal that the GDS points to is
   immediately removed.

   When a goal is removed, the GDS is not immediately removed.  Instead,
   whenever a WME is removed (or when it is added to another GDS), we check
   to also make certain that its GDS has other WMEs on the wmes_in_gds DLL.
   If not, then we remove the GDS then.  This delay avoids having to scan
   over all the WMEs in the GDS in addition to removing the goal (i.e., the
   maintenance cost is amortized over a number of WM phases).

   */

typedef struct gds_struct {
  Symbol *goal;                /* pointer to the goal for the dependency set */
  wme *wmes_in_gds;            /* pointer to the dll of WMEs in GDS of goal */
} goal_dependency_set;

void elaborate_gds (agent* thisAgent);
void gds_invalid_so_remove_goal (agent* thisAgent, wme *w);
void free_parent_list(agent* thisAgent);
void uniquely_add_to_head_of_dll(agent* thisAgent, instantiation *inst);
void create_gds_for_goal( agent* thisAgent, Symbol *goal );
extern void remove_operator_if_necessary(agent* thisAgent, slot *s, wme *w);

extern int GDS_PrintCmd (/****ClientData****/ int clientData,
                         /****Tcl_Interp****/ void * interp,
                         int argc, char *argv[]);

/* ------------------------------------------------------------------------
                                Slots

   Fields in a slot:

      next, prev:  used for a doubly-linked list of all slots for a certain
        identifier.

      id, attr:   identifier and attribute of the slot

      wmes:  header of a doubly-linked list of all wmes in the slot

      acceptable_preference_wmes:  header of doubly-linked list of all
        acceptable preference wmes in the slot.  (This is only used for
        context slots.)

      all_preferences:  header of a doubly-linked list of all preferences
        currently in the slot

      preferences[NUM_PREFERENCE_TYPES]: array of headers of doubly-linked
        lists, one for each possible type of preference.  These store
        all the preferences, sorted into lists according to their types.
        Within each list, the preferences are sorted according to their
        match goal, with the pref. supported by the highest goal at the
        head of the list.

      CDPS: a dll of preferences in the context-dependent preference set,
        which is the set of all preferences that contributed to an operator's
        selection.  This is used to allow Soar to backtrace through evaluation
        rules in substates.  The rules that determine which preferences are
        in the CPSD are outlined in run_preference_semantics().

      impasse_id:  points to the identifier of the attribute impasse object
        for this slot.  (NIL if the slot isn't impassed.)

      isa_context_slot:  true iff this is a context slot

      impasse_type:  indicates the type of the impasse for this slot.  This
        is one of NONE_IMPASSE_TYPE, CONSTRAINT_FAILURE_IMPASSE_TYPE, etc.

      marked_for_possible_removal:  true iff this slot is on the list of
        slots that might be deallocated at the end of the current top-level
        phase.

      changed:  indicates whether the preferences for this slot have changed.
        For non-context slots, this is either NIL or a pointer to the
        corresponding dl_cons in changed_slots (see decide.c); for context
        slots, it's just a zero/nonzero flag.

      acceptable_preference_changed:  for context slots only; this is zero
        if no acceptable or require preference in this slot has changed;
        if one has changed, it points to a dl_cons.
------------------------------------------------------------------------ */

typedef struct slot_struct {
  struct slot_struct *next, *prev;  /* dll of slots for this id */
  Symbol *id;                       /* id, attr of the slot */
  Symbol *attr;
  wme *wmes;                        /* dll of wmes in the slot */
  wme *acceptable_preference_wmes;  /* dll of acceptable pref. wmes */
  preference *all_preferences;      /* dll of all pref's in the slot */
  preference *preferences[NUM_PREFERENCE_TYPES]; /* dlls for each type */
  ::list *CDPS;                               /* list of prefs in the CDPS to backtrace through */
  Symbol *impasse_id;               /* NIL if slot is not impassed */
  bool isa_context_slot;
  byte impasse_type;
  bool marked_for_possible_removal;
  dl_cons *changed;   /* for non-context slots: points to the corresponding
                         dl_cons in changed_slots;  for context slots: just
                         zero/nonzero flag indicating slot changed */
  dl_cons *acceptable_preference_changed; /* for context slots: either zero,
                                             or points to dl_cons if the slot
                                             has changed + or ! pref's */

  wma_sym_reference_map* wma_val_references;

} slot;

/* MMA 8-2012 */
void add_to_CDPS (agent* thisAgent, slot *s, preference *pref, bool unique_value = true);
void rl_update_for_one_candidate(agent* thisAgent, slot *s, bool consistency, preference *candidates);
extern byte run_preference_semantics(agent* thisAgent, slot *s, preference **result_candidates, bool consistency = false, bool predict = false);
/* MMA end */

/* ---------------------------------------------------------------------
                      Top-Level Decider Routines

   Init_decider() should be called at startup time to initialize this
   module.

   Do_buffered_wm_and_ownership_changes() does the end-of-phase processing
   of WM changes, ownership calculations, garbage collection, etc.

   Do_working_memory_phase() and do_decision_phase() are called from
   the top level to run those phases.

   Create_top_goal() creates the top goal in the goal stack.
   Clear_goal_stack() wipes out the whole goal stack--this is called
   during an init-soar.

   Print_lowest_slot_in_context_stack() is used for the watch 0 trace
   to print the context slot that was just decided.
--------------------------------------------------------------------- */

extern void remove_wmes_for_context_slot (agent* thisAgent, slot *s); /* added this prototype -ajc (5/1/02) */
extern void init_decider (agent* thisAgent);
extern void do_buffered_wm_and_ownership_changes (agent* thisAgent);
extern void do_working_memory_phase (agent* thisAgent);
extern void do_decision_phase (agent* thisAgent,bool predict = false);
extern void create_top_goal (agent* thisAgent);
extern void clear_goal_stack (agent* thisAgent);
extern void print_lowest_slot_in_context_stack (agent* thisAgent);

/* These prototypes moved here from consistency.cpp -ajc (5/3/02) */
extern void remove_existing_context_and_descendents (agent* thisAgent, Symbol *goal);
extern byte type_of_existing_impasse (agent* thisAgent, Symbol *goal);
extern Symbol *attribute_of_existing_impasse (agent* thisAgent, Symbol *goal);

/* These prototypes moved here from chunk.cpp -ajc (5/3/02) */
extern byte type_of_existing_impasse (agent* thisAgent, Symbol *goal);

// SBW 5/07 added prototype
unsigned int count_candidates(preference * candidates);

#endif
