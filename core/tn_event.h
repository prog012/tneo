/*******************************************************************************
 *   Description:   TODO
 *
 ******************************************************************************/

#ifndef _TN_EVENT_H
#define _TN_EVENT_H

/*******************************************************************************
 *    INCLUDED FILES
 ******************************************************************************/

#include "tn_list.h"
#include "tn_common.h"



/*******************************************************************************
 *    PUBLIC TYPES
 ******************************************************************************/

enum TN_EGrpWaitMode {
   TN_EGRP_WMODE_OR        = (1 << 0),    //-- any set bit is enough for event
   TN_EGRP_WMODE_AND       = (1 << 1),    //-- all bits should be set for event
};

enum TN_EGrpOp {
   TN_EGRP_OP_SET,      //-- set flags that are set in pattern argument
   TN_EGRP_OP_CLEAR,    //-- clear flags that are set in pattern argument
   TN_EGRP_OP_TOGGLE,   //-- toggle flags that are set in pattern argument
};

struct TN_Event {
   struct TN_ListItem   wait_queue; //-- task wait queue
   unsigned int         pattern;    //-- current flags pattern
   enum TN_ObjId        id_event;   //-- id for verification
};



/*******************************************************************************
 *    GLOBAL VARIABLES
 ******************************************************************************/

/*******************************************************************************
 *    DEFINITIONS
 ******************************************************************************/



/*******************************************************************************
 *    PUBLIC FUNCTION PROTOTYPES
 ******************************************************************************/

enum TN_Retval tn_eventgrp_create(
      struct TN_Event * evf,
      unsigned int initial_pattern
      );

enum TN_Retval tn_eventgrp_delete(struct TN_Event * evf);

enum TN_Retval tn_eventgrp_wait(
      struct TN_Event     *evf,
      unsigned int         wait_pattern,
      enum TN_EGrpWaitMode wait_mode,
      unsigned int        *p_flags_pattern,
      unsigned long        timeout
      );

enum TN_Retval tn_eventgrp_wait_polling(
      struct TN_Event     *evf,
      unsigned int         wait_pattern,
      enum TN_EGrpWaitMode wait_mode,
      unsigned int        *p_flags_pattern
      );

enum TN_Retval tn_eventgrp_iwait(
      struct TN_Event     *evf,
      unsigned int         wait_pattern,
      enum TN_EGrpWaitMode wait_mode,
      unsigned int        *p_flags_pattern
      );

enum TN_Retval tn_eventgrp_modify(
      struct TN_Event     *evf,
      enum TN_EGrpOp       operation,
      unsigned int         pattern
      );

enum TN_Retval tn_eventgrp_imodify(
      struct TN_Event     *evf,
      enum TN_EGrpOp       operation,
      unsigned int         pattern
      );

#endif // _TN_EVENT_H

/*******************************************************************************
 *    end of file
 ******************************************************************************/


