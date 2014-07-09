#include <portability.h>

/*************************************************************************
 * PLEASE SEE THE FILE "license.txt" (INCLUDED WITH THIS SOFTWARE PACKAGE)
 * FOR LICENSE AND COPYRIGHT INFORMATION.
 *************************************************************************/

/*************************************************************************
 *
 *  file:  production.cpp
 *
 * ====================================================================
 *                    Production Utilities for Soar 6
 *
 * This file contains various utility routines for manipulating
 * productions and parts of productions:  tests, conditions, actions,
 * etc.  Also includes the reorderer and compile-time o-support calculations.
 * parser.cpp loads productions.
 * Init_production_utilities() should be called before anything else here.
 * =======================================================================
 */

#include <stdlib.h>

#include "production.h"
#include "mem.h"
#include "kernel.h"
#include "print.h"
#include "agent.h"
#include "rhs.h"
#include "instantiations.h"
#include "reorder.h"
#include "symtab.h"
#include "init_soar.h"
#include "rete.h"
#include "reinforcement_learning.h"
#include "test.h"

#include <ctype.h>


void init_production_utilities (agent* thisAgent) {
  init_memory_pool (thisAgent, &thisAgent->complex_test_pool, sizeof(complex_test), "complex test");
  init_memory_pool (thisAgent, &thisAgent->condition_pool, sizeof(condition), "condition");
  init_memory_pool (thisAgent, &thisAgent->production_pool, sizeof(production), "production");
  init_memory_pool (thisAgent, &thisAgent->action_pool, sizeof(action), "action");
  init_memory_pool (thisAgent, &thisAgent->not_pool, sizeof(not_struct), "not");
  init_reorderer(thisAgent);
}

/* ********************************************************************

           Utility Routines for Various Parts of Productions

******************************************************************** */

/* ====================================================================

              Utilities for Symbols and Lists of Symbols

==================================================================== */

/* -----------------------------------------------------------------
   Find first letter of test, or '*' if nothing appropriate.
   (See comments on first_letter_from_symbol for more explanation.)
----------------------------------------------------------------- */

char first_letter_from_test (test t) {
  complex_test *ct;
  cons *c;
  char ch;

  if (test_is_blank_test (t)) return '*';
  if (test_is_blank_or_equality_test (t))
    return first_letter_from_symbol (referent_of_equality_test(t));

  ct = complex_test_from_test (t);
  switch(ct->type) {
  case GOAL_ID_TEST: return 's';
  case IMPASSE_ID_TEST: return 'i';
  case CONJUNCTIVE_TEST:
    for (c=ct->data.conjunct_list; c!=NIL; c=c->rest) {
      ch = first_letter_from_test (static_cast<char *>(c->first));
      if (ch != '*') return ch;
    }
    return '*';
  default:  /* disjunction tests, and relational tests other than equality */
    return '*';
  }
}


/* =================================================================

                  Utility Routines for Conditions

================================================================= */

/* ----------------------------------------------------------------
   Deallocates a condition list (including any NCC's and tests in it).
---------------------------------------------------------------- */

void deallocate_condition_list (agent* thisAgent,
								condition *cond_list) {
  condition *c;

  while (cond_list) {
    c = cond_list;
    cond_list = cond_list->next;
    if (c->type==CONJUNCTIVE_NEGATION_CONDITION) {
      deallocate_condition_list (thisAgent, c->data.ncc.top);
    } else { /* positive and negative conditions */
      quickly_deallocate_test (thisAgent, c->data.tests.id_test);
      quickly_deallocate_test (thisAgent, c->data.tests.attr_test);
      quickly_deallocate_test (thisAgent, c->data.tests.value_test);
    }
    free_with_pool (&thisAgent->condition_pool, c);
  }
}

/* ----------------------------------------------------------------
   Returns a new copy of the given condition.
---------------------------------------------------------------- */

condition *copy_condition (agent* thisAgent,
						   condition *cond) {
  condition *New;

  if (!cond) return NIL;
  allocate_with_pool (thisAgent, &thisAgent->condition_pool, &New);
  New->type = cond->type;

  switch (cond->type) {
  case POSITIVE_CONDITION:
    New->bt = cond->bt;
    /* ... and fall through to next case */
  case NEGATIVE_CONDITION:
    New->data.tests.id_test = copy_test (thisAgent, cond->data.tests.id_test);
    New->data.tests.attr_test = copy_test (thisAgent, cond->data.tests.attr_test);
    New->data.tests.value_test = copy_test (thisAgent, cond->data.tests.value_test);
    New->test_for_acceptable_preference = cond->test_for_acceptable_preference;
    break;
  case CONJUNCTIVE_NEGATION_CONDITION:
    copy_condition_list (thisAgent, cond->data.ncc.top, &(New->data.ncc.top),
                         &(New->data.ncc.bottom));
    break;
  }
  return New;
}

/* ----------------------------------------------------------------
   Copies the given condition list, returning pointers to the
   top-most and bottom-most conditions in the new copy.
---------------------------------------------------------------- */

void copy_condition_list (agent* thisAgent,
						  condition *top_cond,
                          condition **dest_top,
                          condition **dest_bottom) {
  condition *New, *prev;

  prev = NIL;
  while (top_cond) {
    New = copy_condition (thisAgent, top_cond);
    if (prev) prev->next = New; else *dest_top = New;
    New->prev = prev;
    prev = New;
    top_cond = top_cond->next;
  }
  if (prev) prev->next = NIL; else *dest_top = NIL;
  *dest_bottom = prev;
}

/* ----------------------------------------------------------------
   Returns true iff the two conditions are identical.
---------------------------------------------------------------- */

bool conditions_are_equal (condition *c1, condition *c2) {
  if (c1->type != c2->type) return false;
  bool neg = true;
  switch (c1->type) {
  case POSITIVE_CONDITION:
	  neg = false;
  case NEGATIVE_CONDITION:
    if (! tests_are_equal (c1->data.tests.id_test,
                           c2->data.tests.id_test, neg))
      return false;
    if (! tests_are_equal (c1->data.tests.attr_test,
                           c2->data.tests.attr_test, neg))
      return false;
    if (! tests_are_equal (c1->data.tests.value_test,
                           c2->data.tests.value_test, neg))
      return false;
    if (c1->test_for_acceptable_preference !=
        c2->test_for_acceptable_preference)
      return false;
    return true;

  case CONJUNCTIVE_NEGATION_CONDITION:
    for (c1=c1->data.ncc.top, c2=c2->data.ncc.top;
         ((c1!=NIL)&&(c2!=NIL));
         c1=c1->next, c2=c2->next)
      if (! conditions_are_equal (c1,c2)) return false;
    if (c1==c2) return true;  /* make sure they both hit end-of-list */
    return false;
  }
  return false; /* unreachable, but without it, gcc -Wall warns here */
}

/* ----------------------------------------------------------------
   Returns a hash value for the given condition.
---------------------------------------------------------------- */

uint32_t hash_condition (agent* thisAgent,
							  condition *cond) {
  uint32_t result;
  condition *c;

  switch (cond->type) {
  case POSITIVE_CONDITION:
    result = hash_test (thisAgent, cond->data.tests.id_test);
    result = (result << 24) | (result >>  8);
    result ^= hash_test (thisAgent, cond->data.tests.attr_test);
    result = (result << 24) | (result >>  8);
    result ^= hash_test (thisAgent, cond->data.tests.value_test);
    if (cond->test_for_acceptable_preference) result++;
    break;
  case NEGATIVE_CONDITION:
    result = 1267818;
    result ^= hash_test (thisAgent, cond->data.tests.id_test);
    result = (result << 24) | (result >>  8);
    result ^= hash_test (thisAgent, cond->data.tests.attr_test);
    result = (result << 24) | (result >>  8);
    result ^= hash_test (thisAgent, cond->data.tests.value_test);
    if (cond->test_for_acceptable_preference) result++;
    break;
  case CONJUNCTIVE_NEGATION_CONDITION:
    result = 82348149;
    for (c=cond->data.ncc.top; c!=NIL; c=c->next) {
      result ^= hash_condition (thisAgent, c);
      result = (result << 24) | (result >>  8);
    }
    break;
  default:
    { char msg[BUFFER_MSG_SIZE];
    strncpy (msg, "Internal error: bad cond type in hash_condition\n", BUFFER_MSG_SIZE);
    msg[BUFFER_MSG_SIZE - 1] = 0; /* ensure null termination */
    abort_with_fatal_error(thisAgent, msg);
    }
    result = 0; /* unreachable, but gcc -Wall warns without it */
  }
  return result;
}


/* =================================================================

                    Utility Routines for Nots

================================================================= */

/* ----------------------------------------------------------------
   Deallocates the given (singly-linked) list of Nots.
---------------------------------------------------------------- */

void deallocate_list_of_nots (agent* thisAgent,
							  not_struct *nots) {
  not_struct *temp;

  while (nots) {
    temp = nots;
    nots = nots->next;
    symbol_remove_ref (thisAgent, temp->s1);
    symbol_remove_ref (thisAgent, temp->s2);
    free_with_pool (&thisAgent->not_pool, temp);
  }
}

/* *********************************************************************

                    Transitive Closure Utilities

********************************************************************* */

/* =====================================================================

              Increment TC Counter and Return New TC Number

   Get_new_tc_number() is called from lots of places.  Any time we need
   to mark a set of identifiers and/or variables, we get a new tc_number
   by calling this routine, then proceed to mark various ids or vars
   by setting the sym->tc_num or sym->tc_num fields.

   A global tc number counter is maintained and incremented by this
   routine in order to generate a different tc_number each time.  If
   the counter ever wraps around back to 0, we bump it up to 1 and
   reset the the tc_num fields on all existing identifiers and variables
   to 0.
===================================================================== */

tc_number get_new_tc_number (agent* thisAgent) {
  /* This was originally a global variable. For the present I'll move it here,
     but it probably belongs in kernel_struct. */

  thisAgent->current_tc_number++;
  if (thisAgent->current_tc_number==0) {
    reset_id_and_variable_tc_numbers (thisAgent);
    thisAgent->current_tc_number = 1;
  }
  return thisAgent->current_tc_number;
}

/* =====================================================================

               Marking, Unmarking, and Collecting Symbols

   Sometimes in addition to marking symbols using their tc_num fields,
   we also want to build up a list of the symbols we've marked.  So,
   many routines in this file take an "id_list" or "var_list" argument.
   This argument should be NIL if no such list is desired.  If non-NIL,
   it should point to the header of the linked list being built.

   Mark_identifier_if_unmarked() and mark_variable_if_unmarked() are
   macros for adding id's and var's to the set of symbols.

   Unmark_identifiers_and_free_list() unmarks all the id's in the given
   list, and deallocates the list.  Unmark_variables_and_free_list()
   is similar, only the list should be a list of variables rather than
   identifiers.

   Symbol_is_constant_or_marked_variable() tests whether the given symbol
   is either a constant (non-variable) or a variable marked with the
   given tc number.
===================================================================== */

/*#define mark_identifier_if_unmarked(ident,tc,id_list) { \
  if ((ident)->tc_num != (tc)) { \
    (ident)->tc_num = (tc); \
    if (id_list) push ((ident),(*(id_list))); } }*/
inline void mark_identifier_if_unmarked(agent* thisAgent,
										Symbol * ident, tc_number tc, list ** id_list)
{
  if ((ident)->tc_num != (tc))
  {
    (ident)->tc_num = (tc);
    if (id_list)
		push (thisAgent, (ident),(*(id_list)));
  }
}

/*#define mark_variable_if_unmarked(v,tc,var_list) { \
  if ((v)->tc_num != (tc)) { \
    (v)->tc_num = (tc); \
    if (var_list) push ((v),(*(var_list))); } }*/
inline void mark_variable_if_unmarked(agent* thisAgent, Symbol * v,
									  tc_number tc, list ** var_list)
{
  if ((v)->tc_num != (tc))
  {
    (v)->tc_num = (tc);
    if (var_list) push (thisAgent, (v),(*(var_list)));
  }
}

void unmark_identifiers_and_free_list (agent* thisAgent, list *id_list) {
  cons *next;
  Symbol *sym;

  while (id_list) {
    sym = static_cast<symbol_struct *>(id_list->first);
    next = id_list->rest;
    free_cons (thisAgent, id_list);
    sym->tc_num = 0;
    id_list = next;
  }
}

void unmark_variables_and_free_list (agent* thisAgent, list *var_list) {
  cons *next;
  Symbol *sym;

  while (var_list) {
    sym = static_cast<symbol_struct *>(var_list->first);
    next = var_list->rest;
    free_cons (thisAgent, var_list);
    sym->tc_num = 0;
    var_list = next;
  }
}

/* =====================================================================

   Finding the variables bound in tests, conditions, and condition lists

   These routines collect the variables that are bound in tests, etc.  Their
   "var_list" arguments should either be NIL or else should point to
   the header of the list of marked variables being constructed.
===================================================================== */

void add_bound_variables_in_test (agent* thisAgent, test t,
								  tc_number tc, list **var_list) {
  cons *c;
  Symbol *referent;
  complex_test *ct;

  if (test_is_blank_test(t)) return;

  if (test_is_blank_or_equality_test(t)) {
    referent = referent_of_equality_test(t);
    if (referent->symbol_type==VARIABLE_SYMBOL_TYPE)
      mark_variable_if_unmarked (thisAgent, referent, tc, var_list);
    return;
  }

  ct = complex_test_from_test(t);
  if (ct->type==CONJUNCTIVE_TEST) {
    for (c=ct->data.conjunct_list; c!=NIL; c=c->rest)
      add_bound_variables_in_test (thisAgent, static_cast<char *>(c->first), tc, var_list);
  }
}

void add_bound_variables_in_condition (agent* thisAgent, condition *c, tc_number tc,
                                       list **var_list) {
  if (c->type!=POSITIVE_CONDITION) return;
  add_bound_variables_in_test (thisAgent, c->data.tests.id_test, tc, var_list);
  add_bound_variables_in_test (thisAgent, c->data.tests.attr_test, tc, var_list);
  add_bound_variables_in_test (thisAgent, c->data.tests.value_test, tc, var_list);
}

void add_bound_variables_in_condition_list (agent* thisAgent, condition *cond_list,
                                            tc_number tc, list **var_list) {
  condition *c;

  for (c=cond_list; c!=NIL; c=c->next)
    add_bound_variables_in_condition (thisAgent, c, tc, var_list);
}

/* =====================================================================

   Finding all variables from tests, conditions, and condition lists

   These routines collect all the variables in tests, etc.  Their
   "var_list" arguments should either be NIL or else should point to
   the header of the list of marked variables being constructed.
===================================================================== */

void add_all_variables_in_test (agent* thisAgent, test t,
								tc_number tc, list **var_list) {
  cons *c;
  Symbol *referent;
  complex_test *ct;

  if (test_is_blank_test(t)) return;

  if (test_is_blank_or_equality_test(t)) {
    referent = referent_of_equality_test(t);
    if (referent->symbol_type==VARIABLE_SYMBOL_TYPE)
      mark_variable_if_unmarked (thisAgent, referent, tc, var_list);
    return;
  }

  ct = complex_test_from_test(t);

  switch (ct->type) {
  case GOAL_ID_TEST:
  case IMPASSE_ID_TEST:
  case DISJUNCTION_TEST:
    break;

  case CONJUNCTIVE_TEST:
    for (c=ct->data.conjunct_list; c!=NIL; c=c->rest)
      add_all_variables_in_test (thisAgent, static_cast<char *>(c->first), tc, var_list);
    break;

  default:
    /* --- relational tests other than equality --- */
    referent = ct->data.referent;
    if (referent->symbol_type==VARIABLE_SYMBOL_TYPE)
      mark_variable_if_unmarked (thisAgent, referent, tc, var_list);
    break;
  }
}

void add_all_variables_in_condition_list (agent* thisAgent, condition *cond_list,
                                          tc_number tc, list **var_list);

void add_all_variables_in_condition (agent* thisAgent,
									 condition *c, tc_number tc,
                                     list **var_list) {
  if (c->type==CONJUNCTIVE_NEGATION_CONDITION) {
    add_all_variables_in_condition_list (thisAgent, c->data.ncc.top, tc, var_list);
  } else {
    add_all_variables_in_test (thisAgent, c->data.tests.id_test, tc, var_list);
    add_all_variables_in_test (thisAgent, c->data.tests.attr_test, tc, var_list);
    add_all_variables_in_test (thisAgent, c->data.tests.value_test, tc, var_list);
  }
}

void add_all_variables_in_condition_list (agent* thisAgent, condition *cond_list,
                                          tc_number tc, list **var_list) {
  condition *c;

  for (c=cond_list; c!=NIL; c=c->next)
    add_all_variables_in_condition (thisAgent, c, tc, var_list);
}

/* =====================================================================

   Finding all variables from rhs_value's, actions, and action lists

   These routines collect all the variables in rhs_value's, etc.  Their
   "var_list" arguments should either be NIL or else should point to
   the header of the list of marked variables being constructed.

   Warning: These are part of the reorderer and handle only productions
   in non-reteloc, etc. format.  They don't handle reteloc's or
   RHS unbound variables.
===================================================================== */

void add_all_variables_in_rhs_value (agent* thisAgent,
									 rhs_value rv, tc_number tc,
                                     list **var_list) {
  list *fl;
  cons *c;
  Symbol *sym;

  if (rhs_value_is_symbol(rv)) {
    /* --- ordinary values (i.e., symbols) --- */
    sym = rhs_value_to_symbol(rv);
    if (sym->symbol_type==VARIABLE_SYMBOL_TYPE)
      mark_variable_if_unmarked (thisAgent, sym, tc, var_list);
  } else {
    /* --- function calls --- */
    fl = rhs_value_to_funcall_list(rv);
    for (c=fl->rest; c!=NIL; c=c->rest)
      add_all_variables_in_rhs_value (thisAgent, static_cast<char *>(c->first), tc, var_list);
  }
}

void add_all_variables_in_action (agent* thisAgent, action *a,
								  tc_number tc, list **var_list){
  Symbol *id;

  if (a->type==MAKE_ACTION) {
    /* --- ordinary make actions --- */
    id = rhs_value_to_symbol(a->id);
    if (id->is_variable())
      mark_variable_if_unmarked (thisAgent, id, tc, var_list);
    add_all_variables_in_rhs_value (thisAgent, a->attr, tc, var_list);
    add_all_variables_in_rhs_value (thisAgent, a->value, tc, var_list);
    if (preference_is_binary(a->preference_type))
      add_all_variables_in_rhs_value (thisAgent, a->referent, tc, var_list);
  } else {
    /* --- function call actions --- */
    add_all_variables_in_rhs_value (thisAgent, a->value, tc, var_list);
  }
}

void add_all_variables_in_action_list (agent* thisAgent, action *actions, tc_number tc,
                                       list **var_list) {
  action *a;

  for (a=actions; a!=NIL; a=a->next)
    add_all_variables_in_action (thisAgent, a, tc, var_list);
}

/* ====================================================================

              Transitive Closure for Conditions and Actions

   These routines do transitive closure calculations for tests,
   conditions, actions, etc.

   Usage:
     1. Set my_tc = get_new_tc_number() to start a new TC
     2. (optional) If you want linked lists of symbols in the TC, initialize
        id_list=NIL and var_list=NIL.
        If you're not using id_list and/or var_list, give NIL for "&id_list"
        and/or "&var_list" in the function calls below.
     3. (optional) setup any id's or var's that you want to include in the
        initial TC, by calling
           add_symbol_to_tc (sym, my_tc, &id_list, &var_list)
        (If not using id_list or var_list, you can just mark
         sym->{id,var}.tc_num = my_tc instead.)
     4. To do the work you want, use any of the following any number of times:
            add_cond_to_tc (cond, my_tc, &id_list, &var_list);
            add_action_to_tc (cond, my_tc, &id_list, &var_list);
            result = cond_is_in_tc (cond, my_tc);
            result = action_is_in_tc (action, my_tc);
     5. When finished, free the cons cells in id_list and var_list (but
        don't call symbol_remove_ref() on the symbols in them).

  Warning:  actions must not contain reteloc's or rhs unbound variables here.
==================================================================== */

void add_symbol_to_tc (agent* thisAgent, Symbol *sym, tc_number tc,
                       list **id_list, list **var_list) {
  if (sym->symbol_type==VARIABLE_SYMBOL_TYPE) {
    mark_variable_if_unmarked (thisAgent, sym, tc, var_list);
  } else if (sym->symbol_type==IDENTIFIER_SYMBOL_TYPE) {
    mark_identifier_if_unmarked (thisAgent, sym, tc, id_list);
  }
}

void add_test_to_tc (agent* thisAgent, test t, tc_number tc,
                     list **id_list, list **var_list) {
  cons *c;
  complex_test *ct;

  if (test_is_blank_test(t)) return;

  if (test_is_blank_or_equality_test(t)) {
    add_symbol_to_tc (thisAgent, referent_of_equality_test(t), tc, id_list, var_list);
    return;
  }

  ct = complex_test_from_test(t);
  if (ct->type == CONJUNCTIVE_TEST) {
    for (c=ct->data.conjunct_list; c!=NIL; c=c->rest)
      add_test_to_tc (thisAgent, static_cast<char *>(c->first), tc, id_list, var_list);
  }
}

void add_cond_to_tc (agent* thisAgent, condition *c, tc_number tc,
                     list **id_list, list **var_list) {
  if (c->type==POSITIVE_CONDITION) {
    add_test_to_tc (thisAgent, c->data.tests.id_test, tc, id_list, var_list);
    add_test_to_tc (thisAgent, c->data.tests.value_test, tc, id_list, var_list);
  }
}

void add_action_to_tc (agent* thisAgent, action *a, tc_number tc,
                       list **id_list, list **var_list) {
  if (a->type != MAKE_ACTION) return;
  add_symbol_to_tc (thisAgent, rhs_value_to_symbol(a->id), tc, id_list, var_list);
  if (rhs_value_is_symbol(a->value))
    add_symbol_to_tc (thisAgent, rhs_value_to_symbol(a->value), tc, id_list, var_list);
  if (preference_is_binary(a->preference_type))
    if (rhs_value_is_symbol(a->referent))
      add_symbol_to_tc (thisAgent, rhs_value_to_symbol(a->referent),tc,id_list,var_list);
}

bool symbol_is_in_tc (Symbol *sym, tc_number tc) {
  if (sym->symbol_type==VARIABLE_SYMBOL_TYPE)
    return (sym->tc_num == tc);
  if (sym->symbol_type==IDENTIFIER_SYMBOL_TYPE)
    return (sym->tc_num == tc);
  return false;
}

bool test_is_in_tc (test t, tc_number tc) {
  cons *c;
  complex_test *ct;

  if (test_is_blank_test(t)) return false;
  if (test_is_blank_or_equality_test(t)) {
    return symbol_is_in_tc (referent_of_equality_test(t), tc);
  }

  ct = complex_test_from_test(t);
  if (ct->type==CONJUNCTIVE_TEST) {
    for (c=ct->data.conjunct_list; c!=NIL; c=c->rest)
      if (test_is_in_tc (static_cast<char *>(c->first), tc)) return true;
    return false;
  }
  return false;
}

bool cond_is_in_tc (agent* thisAgent, condition *cond, tc_number tc) {
  condition *c;
  bool anything_changed;
  bool result;
  list *new_ids, *new_vars;

  if (cond->type != CONJUNCTIVE_NEGATION_CONDITION)
    return test_is_in_tc (cond->data.tests.id_test, tc);

  /* --- conjunctive negations:  keep trying to add stuff to the TC --- */
  new_ids = NIL;
  new_vars = NIL;
  for (c=cond->data.ncc.top; c!=NIL; c=c->next)
    c->already_in_tc = false;
  while (true) {
    anything_changed = false;
    for (c=cond->data.ncc.top; c!=NIL; c=c->next)
      if (! c->already_in_tc)
        if (cond_is_in_tc (thisAgent, c, tc)) {
          add_cond_to_tc (thisAgent, c, tc, &new_ids, &new_vars);
          c->already_in_tc = true;
          anything_changed = true;
        }
    if (! anything_changed) break;
  }

  /* --- complete TC found, look for anything that didn't get hit --- */
  result = true;
  for (c=cond->data.ncc.top; c!=NIL; c=c->next)
    if (! c->already_in_tc) result = false;

  /* --- unmark identifiers and variables that we just marked --- */
  unmark_identifiers_and_free_list (thisAgent, new_ids);
  unmark_variables_and_free_list (thisAgent, new_vars);

  return result;
}

bool action_is_in_tc (action *a, tc_number tc) {
  if (a->type != MAKE_ACTION) return false;
  return symbol_is_in_tc (rhs_value_to_symbol(a->id), tc);
}

/* *********************************************************************

                         Variable Generator

   These routines are used for generating new variables.  The variables
   aren't necessarily "completely" new--they might occur in some existing
   production.  But we usually need to make sure the new variables don't
   overlap with those already used in a *certain* production--for instance,
   when variablizing a chunk, we don't want to introduce a new variable that
   conincides with the name of a variable already in an NCC in the chunk.

   To use these routines, first call reset_variable_generator(), giving
   it lists of conditions and actions whose variables should not be
   used.  Then call generate_new_variable() any number of times; each
   time, you give it a string to use as the prefix for the new variable's
   name.  The prefix string should not include the opening "<".
********************************************************************* */


void reset_variable_generator (agent* thisAgent,
							   condition *conds_with_vars_to_avoid,
                               action *actions_with_vars_to_avoid) {
  tc_number tc;
  list *var_list;
  cons *c;
  int i;

  /* --- reset counts, and increment the gensym number --- */
  for (i=0; i<26; i++) thisAgent->gensymed_variable_count[i] = 1;
  thisAgent->current_variable_gensym_number++;
  if (thisAgent->current_variable_gensym_number==0) {
    reset_variable_gensym_numbers (thisAgent);
    thisAgent->current_variable_gensym_number = 1;
  }

  /* --- mark all variables in the given conds and actions --- */
  tc = get_new_tc_number(thisAgent);
  var_list = NIL;
  add_all_variables_in_condition_list (thisAgent, conds_with_vars_to_avoid,tc, &var_list);
  add_all_variables_in_action_list (thisAgent, actions_with_vars_to_avoid, tc, &var_list);
  for (c=var_list; c!=NIL; c=c->rest)
    static_cast<Symbol *>(c->first)->var->gensym_number = thisAgent->current_variable_gensym_number;
  free_list (thisAgent, var_list);
}

Symbol *generate_new_variable (agent* thisAgent, const char *prefix) {
#define GENERATE_NEW_VARIABLE_BUFFER_SIZE 200 /* that ought to be long enough! */
  char name[GENERATE_NEW_VARIABLE_BUFFER_SIZE];
  Symbol *New;
  char first_letter;

  first_letter = *prefix;
  if (isalpha(first_letter)) {
    if (isupper(first_letter)) first_letter = static_cast<char>(tolower(first_letter));
  } else {
    first_letter = 'v';
  }

  while (true) {
    SNPRINTF (name,GENERATE_NEW_VARIABLE_BUFFER_SIZE, "<%s%lu>", prefix,
             static_cast<long unsigned int>(thisAgent->gensymed_variable_count[first_letter-'a']++));
	name[GENERATE_NEW_VARIABLE_BUFFER_SIZE - 1] = 0; /* ensure null termination */

    New = make_variable (thisAgent, name);
    if (New->var->gensym_number != thisAgent->current_variable_gensym_number) break;
    symbol_remove_ref (thisAgent, New);
  }

  New->var->current_binding_value = NIL;
  New->var->gensym_number = thisAgent->current_variable_gensym_number;
  return New;
}

/* *********************************************************************

                         Production Management

    Make_production() does reordering, compile-time o-support calc's,
    and builds and returns a production structure for a new production.
    It does not enter the production into the Rete net, however.
    The "type" argument should be one of USER_PRODUCTION_TYPE, etc.
    The flag "reorder_nccs" tells whether to recursively reorder
    the subconditions of NCC's--this is not necessary for newly
    built chunks, as their NCC's are copies of other NCC's in SP's that
    have already been reordered.  If any error occurs, make_production()
    returns NIL.

    Deallocate_production() and excise_production() do just what they
    say.  Normally deallocate_production() should be invoked only via
    the production_remove_ref() macro.
********************************************************************* */

production *make_production (agent* thisAgent,
							        byte type,
                             Symbol *name,
                             condition **lhs_top,
                             condition **lhs_bottom,
                             action **rhs_top,
                             bool reorder_nccs) {
  production *p;
  tc_number tc;
  action *a;


  thisAgent->name_of_production_being_reordered = name->sc->name;

  if (type!=JUSTIFICATION_PRODUCTION_TYPE) {
    reset_variable_generator (thisAgent, *lhs_top, *rhs_top);
    tc = get_new_tc_number(thisAgent);
    add_bound_variables_in_condition_list (thisAgent, *lhs_top, tc, NIL);
    if (! reorder_action_list (thisAgent, rhs_top, tc)) return NIL;
    if (! reorder_lhs (thisAgent, lhs_top, lhs_bottom, reorder_nccs)) return NIL;

	if ( !smem_valid_production( *lhs_top, *rhs_top ) )
	{
		print( thisAgent, "ungrounded LTI in production\n" );
		return NIL;
	}

#ifdef DO_COMPILE_TIME_O_SUPPORT_CALCS
    calculate_compile_time_o_support (*lhs_top, *rhs_top);
#ifdef LIST_COMPILE_TIME_O_SUPPORT_FAILURES
    for (a = *rhs_top; a!=NIL; a=a->next)
      if ((a->type==MAKE_ACTION) && (a->support==UNKNOWN_SUPPORT)) break;
    if (a) print_with_symbols (thisAgent, "\nCan't classify %y\n", name);
#endif
#else
    for (a = *rhs_top; a!=NIL; a=a->next) a->support = UNKNOWN_SUPPORT;
#endif
  } else {
    /* --- for justifications --- */
    /* force run-time o-support (it'll only be done once) */
    for (a = *rhs_top; a!=NIL; a=a->next) a->support = UNKNOWN_SUPPORT;
  }

  allocate_with_pool (thisAgent, &thisAgent->production_pool, &p);
  p->name = name;
  if (name->sc->production) {
    print (thisAgent, "Internal error: make_production called with name %s\n",
           thisAgent->name_of_production_being_reordered);
    print (thisAgent, "for which a production already exists\n");
  }
  name->sc->production = p;
  p->documentation = NIL;
  p->filename = NIL;
  p->firing_count = 0;
  p->reference_count = 1;
  insert_at_head_of_dll (thisAgent->all_productions_of_type[type], p, next, prev);
  thisAgent->num_productions_of_type[type]++;
  p->type = type;
  p->declared_support = UNDECLARED_SUPPORT;
  p->trace_firings = false;
  p->p_node = NIL;               /* it's not in the Rete yet */
  p->action_list = *rhs_top;
  p->rhs_unbound_variables = NIL; /* the Rete fills this in */
  p->instantiations = NIL;
  p->interrupt = false;

  // Soar-RL stuff
  p->rl_update_count = 0.0;
  p->rl_delta_bar_delta_beta = -3.0;
  p->rl_delta_bar_delta_h = 0.0;
  p->rl_rule = false;
  p->rl_update_count = 0;
  p->rl_ref_count = 0;
  p->rl_ecr = 0.0;
  p->rl_efr = 0.0;
  if ( ( type != JUSTIFICATION_PRODUCTION_TYPE ) && ( type != TEMPLATE_PRODUCTION_TYPE ) )
  {
    p->rl_rule = rl_valid_rule( p );
	if ( p->rl_rule )
	{
	  p->rl_efr = get_number_from_symbol( rhs_value_to_symbol( p->action_list->referent ) );
	}
  }
  p->rl_template_conds = NIL;
  p->rl_template_instantiations = NIL;

  rl_update_template_tracking( thisAgent, name->sc->name );

  return p;
}

void deallocate_production (agent* thisAgent, production *prod) {
  if (prod->instantiations) {
    char msg[BUFFER_MSG_SIZE];
    strncpy (msg, "Internal error: deallocating prod. that still has inst's\n", BUFFER_MSG_SIZE);
    msg[BUFFER_MSG_SIZE - 1] = 0; /* ensure null termination */
    abort_with_fatal_error(thisAgent, msg);
  }
  deallocate_action_list (thisAgent, prod->action_list);
  /* RBD 3/28/95 the following line used to use free_list(), leaked memory */
  deallocate_symbol_list_removing_references (thisAgent, prod->rhs_unbound_variables);
  symbol_remove_ref (thisAgent, prod->name);
  if (prod->documentation) free_memory_block_for_string (thisAgent, prod->documentation);
  /* next line, kjh CUSP(B11) */
  if (prod->filename) free_memory_block_for_string (thisAgent, prod->filename);

  if (prod->rl_template_conds) deallocate_condition_list(thisAgent,prod->rl_template_conds);
  if (prod->rl_template_instantiations) delete prod->rl_template_instantiations;

  free_with_pool (&thisAgent->production_pool, prod);
}

void excise_production (agent* thisAgent, production *prod, bool print_sharp_sign) {
  if (prod->trace_firings) remove_pwatch (thisAgent, prod);
  remove_from_dll (thisAgent->all_productions_of_type[prod->type], prod, next, prev);

  // Remove reference from apoptosis object store
  if ( ( prod->type == CHUNK_PRODUCTION_TYPE ) && ( thisAgent->rl_params ) && ( thisAgent->rl_params->apoptosis->get_value() != rl_param_container::apoptosis_none ) )
    thisAgent->rl_prods->remove_object( prod );

  // Remove RL-related pointers to this production
  if ( prod->rl_rule )
    rl_remove_refs_for_prod( thisAgent, prod );

  thisAgent->num_productions_of_type[prod->type]--;
  if (print_sharp_sign) print (thisAgent, "#");
  if (prod->p_node) excise_production_from_rete (thisAgent, prod);
  prod->name->sc->production = NIL;
  production_remove_ref (thisAgent, prod);
}

void excise_all_productions_of_type(agent* thisAgent,
                                    byte type,
                                    bool print_sharp_sign) {

  // Iterating through the productions of the appropriate type and excising them
  while (thisAgent->all_productions_of_type[type]) {
    excise_production (thisAgent,
                       thisAgent->all_productions_of_type[type],
                       print_sharp_sign&&thisAgent->sysparams[TRACE_LOADING_SYSPARAM]);
  }
}

void excise_all_productions(agent* thisAgent,
                            bool print_sharp_sign) {

  // Excise all the productions of the four different types
  for (int i=0; i < NUM_PRODUCTION_TYPES; i++) {
    excise_all_productions_of_type(thisAgent,
                                   static_cast<byte>(i),
                                   print_sharp_sign&&thisAgent->sysparams[TRACE_LOADING_SYSPARAM]);
  }
}
