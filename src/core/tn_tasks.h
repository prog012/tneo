/*******************************************************************************
 *
 * TNeoKernel: real-time kernel initially based on TNKernel
 *
 *    TNKernel:                  copyright © 2004, 2013 Yuri Tiomkin.
 *    PIC32-specific routines:   copyright © 2013, 2014 Anders Montonen.
 *    TNeoKernel:                copyright © 2014       Dmitry Frank.
 *
 *    TNeoKernel was born as a thorough review and re-implementation of
 *    TNKernel. The new kernel has well-formed code, inherited bugs are fixed
 *    as well as new features being added, and it is tested carefully with
 *    unit-tests.
 *
 *    API is changed somewhat, so it's not 100% compatible with TNKernel,
 *    hence the new name: TNeoKernel.
 *
 *    Permission to use, copy, modify, and distribute this software in source
 *    and binary forms and its documentation for any purpose and without fee
 *    is hereby granted, provided that the above copyright notice appear
 *    in all copies and that both that copyright notice and this permission
 *    notice appear in supporting documentation.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE DMITRY FRANK AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *    PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL DMITRY FRANK OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *    THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

/**
 * \file
 *
 * Various task services: create, sleep, wake up, terminate, etc.
 *
 */

#ifndef _TN_TASKS_H
#define _TN_TASKS_H

/*******************************************************************************
 *    INCLUDED FILES
 ******************************************************************************/

#include "tn_list.h"
#include "tn_common.h"

#include "tn_eventgrp.h"
#include "tn_dqueue.h"
#include "tn_fmem.h"


/*******************************************************************************
 *    PUBLIC TYPES
 ******************************************************************************/

/**
 * Task state
 */
enum TN_TaskState {
   /// This state should never be publicly available.
   /// It may be stored in task_state only temporarily,
   /// while some system service is in progress.
   TN_TASK_STATE_NONE         = 0,
   ///
   /// Task is ready to run (it doesn't mean that it is running at the moment)
   TN_TASK_STATE_RUNNABLE     = (1 << 0),
   ///
   /// Task is waiting. The reason of waiting can be obtained from
   /// `task_wait_reason` field of the `struct TN_Task`.
   ///
   /// @see `enum TN_WaitReason`
   TN_TASK_STATE_WAIT         = (1 << 1),
   ///
   /// Task is suspended (by some other task)
   TN_TASK_STATE_SUSPEND      = (1 << 2),
   ///
   /// Task was previously waiting, and after this it was suspended
   TN_TASK_STATE_WAITSUSP     = (TN_TASK_STATE_WAIT | TN_TASK_STATE_SUSPEND),
   ///
   /// Task isn't yet activated or it was terminated by `tn_task_terminate()`.
   TN_TASK_STATE_DORMANT      = (1 << 3),
};


/**
 * Task wait reason
 */
enum TN_WaitReason {
   ///
   /// task isn't waiting for anything
   TN_WAIT_REASON_NONE,
   ///
   /// task has called `tn_task_sleep()`
   TN_WAIT_REASON_SLEEP,
   ///
   /// task waits to acquire a semaphore
   /// @see tn_sem.h
   TN_WAIT_REASON_SEM,
   ///
   /// task waits for some event in the event group to be set
   /// @see tn_eventgrp.h
   TN_WAIT_REASON_EVENT,
   ///
   /// task wants to put some data to the data queue, and there's no space
   /// in the queue.
   /// @see tn_dqueue.h
   TN_WAIT_REASON_DQUE_WSEND,
   ///
   /// task wants to receive some data to the data queue, and there's no data
   /// in the queue
   /// @see tn_dqueue.h
   TN_WAIT_REASON_DQUE_WRECEIVE,
   ///
   /// task wants to lock a mutex with priority ceiling
   /// @see tn_mutex.h
   TN_WAIT_REASON_MUTEX_C,
   ///
   /// task wants to lock a mutex with priority inheritance
   /// @see tn_mutex.h
   TN_WAIT_REASON_MUTEX_I,
   ///
   /// task wants to get memory block from memory pool, and there's no free
   /// memory blocks
   /// @see tn_fmem.h
   TN_WAIT_REASON_WFIXMEM,
};

/**
 * Options for `tn_task_create()`
 */
enum TN_TaskCreateOpt {
   ///
   /// whether task should be activated right after it is created.
   /// If this flag is not set, user must activate task manually by calling
   /// `tn_task_activate()`.
   TN_TASK_CREATE_OPT_START = (1 << 0),
   ///
   /// for internal kernel usage only: this option must be provided
   /// when creating idle task
   TN_TASK_CREATE_OPT_IDLE =  (1 << 1),
};

/**
 * Options for `tn_task_exit()`
 */
enum TN_TaskExitOpt {
   ///
   /// whether task should be deleted right after it is exited.
   /// If this flag is not set, user must either delete it manually by
   /// calling `tn_task_delete()` or re-activate it by calling
   /// `tn_task_activate()`.
   TN_TASK_EXIT_OPT_DELETE = (1 << 0),
};

/**
 * Prototype for task body function.
 */
typedef void (TN_TaskBody)(void *param);


/**
 * Task
 */
struct TN_Task {
   /// pointer to task's current stack pointer;
   /// Note that this field **must** be a first field in the struct,
   /// this fact is exploited by platform-specific routines.
   unsigned int *task_stk;   
   ///
   /// queue is used to include task in ready/wait lists
   struct TN_ListItem task_queue;     
   ///
   /// queue is used to include task in timer list
   struct TN_ListItem timer_queue;
   ///
   /// pointer to object's (semaphore, mutex, event, etc) wait list in which 
   /// task is included for waiting
   struct TN_ListItem *pwait_queue;
   ///
   /// queue is used to include task in creation list
   /// (currently, this list is used for statistics only)
   struct TN_ListItem create_queue;

#if TN_USE_MUTEXES
   ///
   /// list of all mutexes that are locked by task
   struct TN_ListItem mutex_queue;
#if TN_MUTEX_DEADLOCK_DETECT
   ///
   /// list of other tasks involved in deadlock. This list is non-empty
   /// only in emergency cases, and it is here to help you fix your bug
   /// that led to deadlock.
   ///
   /// @see `TN_MUTEX_DEADLOCK_DETECT`
   struct TN_ListItem deadlock_list;
#endif
#endif

   /// base address of task's stack space
   unsigned int *stk_start;
   ///
   /// size of task's stack (in `sizeof(unsigned int)`, not bytes)
   int stk_size;
   ///
   /// pointer to task's body function given to `tn_task_create()`
   TN_TaskBody *task_func_addr;
   ///
   /// pointer to task's parameter given to `tn_task_create()`
   void *task_func_param;
   ///
   /// base priority of the task (actual current priority may be higher than 
   /// base priority because of mutex)
   int base_priority;
   ///
   /// current task priority
   int priority;
   ///
   /// id for object validity verification
   enum TN_ObjId id_task;
   ///
   /// task state
   enum TN_TaskState task_state;
   ///
   /// reason for waiting (relevant if only `task_state` is `WAIT` or
   /// `WAITSUSP`)
   enum TN_WaitReason task_wait_reason;
   ///
   /// waiting result code (reason why waiting finished)
   enum TN_RCode task_wait_rc;
   ///
   /// remaining time until timeout; may be `TN_WAIT_INFINITE`.
   unsigned long tick_count;
   ///
   /// time slice counter
   int tslice_count;
   ///
   /// subsystem-specific fields that are used while task waits for something.
   /// Do note that these fields are grouped by union, so, they must not
   /// interfere with each other. It's quite ok here because task can't wait
   /// for different things.
   union {
      /// fields specific to tn_eventgrp.h
      struct TN_EGrpTaskWait eventgrp;
      ///
      /// fields specific to tn_dqueue.h
      struct TN_DQueueTaskWait dqueue;
      ///
      /// fields specific to tn_fmem.h
      struct TN_FMemTaskWait fmem;
   } subsys_wait;

#if TN_DEBUG
   /// task name for debug purposes, user may want to set it by hand
   const char *name;          
#endif

   //-- for the comments on the flag priority_already_updated,
   //   see file tn_mutex.c , function _mutex_do_unlock().
   unsigned          priority_already_updated : 1;


// Other implementation specific fields may be added below

};



/*******************************************************************************
 *    GLOBAL VARIABLES
 ******************************************************************************/

/*******************************************************************************
 *    DEFINITIONS
 ******************************************************************************/


#define get_task_by_tsk_queue(que)                                   \
   (que ? container_of(que, struct TN_Task, task_queue) : 0)

#define get_task_by_timer_queque(que)                                \
   (que ? container_of(que, struct TN_Task, timer_queue) : 0)

#define get_task_by_block_queque(que)                                \
   (que ? container_of(que, struct TN_Task, block_queue) : 0)




/*******************************************************************************
 *    PUBLIC FUNCTION PROTOTYPES
 ******************************************************************************/

/**
 * Construct task and probably start it (depends on options, see below).
 * `id_task` member should not contain `TN_ID_TASK`, otherwise, `TN_RC_WPARAM`
 * is returned.
 *
 * Usage example:
 *
 *     #define MY_TASK_STACK_SIZE   256
 *     #define MY_TASK_PRIORITY     5
 *
 *     struct TN_Task my_task;
 *
 *     //-- notice special architecture-dependent macros we use here,
 *     //   they are needed to make sure that all requirements
 *     //   regarding to stack are met.
 *     TN_ARCH_STK_ATTR_BEFORE
 *     int my_task_stack[ MY_TASK_STACK_SIZE ]
 *     TN_ARCH_STK_ATTR_AFTER;
 *
 *     void my_task_body(void *param)
 *     {
 *        //-- an endless loop
 *        for (;;){
 *           tn_task_sleep(1);
 *
 *           //-- probably do something useful
 *        }
 *     }
 *
 *
 *
 *     // ... and then, somewhere from other task:
 *     void some_different_task_body(void *param)
 *     {
 *        // ........
 *        enum TN_RCode rc = tn_task_create(
 *              &my_task,
 *              my_task_body,
 *              MY_TASK_PRIORITY,
 *              my_task_stack,
 *              MY_TASK_STACK_SIZE,
 *              NULL,                     //-- parameter isn't used
 *              TN_TASK_CREATE_OPT_START  //-- start task on creation
 *              );
 *        if (rc != TN_RC_OK){
 *           //-- handle error
 *        }
 *        // ........
 *     }
 *
 * @param task 
 *    Ready-allocated struct TN_Task structure. `id_task` member should not
 *    contain `TN_ID_TASK`, otherwise `TN_RC_WPARAM` is returned.
 * @param task_func  
 *    Pointer to task body function.
 * @param priority 
 *    Priority for new task. NOTE: the lower value, the higher priority.  Must
 *    be > 0 and < `(TN_PRIORITIES_CNT - 1)`.
 * @param task_stack_low_addr    
 *    Pointer to the stack for task. A stack must be allocated as an array of
 *    `int`.  Actually, the size of stack array element must be identical to
 *    the processor register size (for most 32-bits and 16-bits processors a
 *    register size equals `sizeof(int)`).
 * @param task_stack_size 
 *    Size of task stack, in `int`-s, not in bytes. (i.e., size of array that
 *    is used for `task_stack_low_addr`).
 * @param param 
 *    Parameter that is passed to `task_func`.
 * @param opts 
 *    Options for task creation
 *
 * @see `enum TN_TaskCreateOpt`
 * @see `TN_ARCH_STK_ATTR_BEFORE`
 * @see `TN_ARCH_STK_ATTR_AFTER`
 */
enum TN_RCode tn_task_create(
      struct TN_Task         *task,
      TN_TaskBody            *task_func,
      int                     priority,
      unsigned int           *task_stack_low_addr,
      int                     task_stack_size,
      void                   *param,
      enum TN_TaskCreateOpt   opts
      );


/**
 * If the task is runnable, it is moved to the `SUSPEND` state. If the task
 * is in the `WAIT` state, it is moved to the `WAITSUSP` state.
 * (waiting + suspended)
 *
 * @param task    Task to suspend
 *
 * @see enum TN_TaskState
 */
enum TN_RCode tn_task_suspend(struct TN_Task *task);

/**
 * Release task from `SUSPEND` state. If the given task is in the `SUSPEND`
 * state, it is moved to `RUNNABLE` state; afterwards it has the lowest
 * precedence among runnable tasks with the same priority. If the task is in
 * `WAITSUSP` state, it is moved to `WAIT` state.
 *
 * @param task    Task to release from suspended state
 *
 * @see enum TN_TaskState
 */
enum TN_RCode tn_task_resume(struct TN_Task *task);

/**
 * Put current task to sleep for at most timeout ticks. When the timeout
 * expires and the task was not suspended during the sleep, it is switched to
 * runnable state. If the timeout value is `TN_WAIT_INFINITE` and the task was
 * not suspended during the sleep, the task will sleep until another function
 * call (like `tn_task_wakeup()` or similar) will make it runnable.
 *
 * @returns
 *    * `TN_RC_TIMEOUT` if task has slept specified timeout;
 *    * `TN_RC_OK` if task was woken up from other task by `tn_task_wakeup()`
 *    * `TN_RC_FORCED` if task was released from wait forcibly by 
 *       `tn_task_release_wait()`
 *
 * @see TN_Timeout
 */
enum TN_RCode tn_task_sleep(TN_Timeout timeout);

/**
 * Wake up task from sleep.
 *
 * Task is woken up if only it sleeps because of call to `tn_task_sleep()`.
 * If task sleeps for some another reason, task won't be woken up, 
 * and `tn_task_wakeup()` returns `TN_RC_WSTATE`.
 *
 * After this call, `tn_task_sleep()` returns `TN_RC_OK`.
 *
 * @return
 *    * `TN_RC_OK` if successful
 *    * `TN_RC_WSTATE` task is not sleeping, or it is sleeping for
 *       some reason other than `tn_task_sleep()` call.
 *
 */
enum TN_RCode tn_task_wakeup(struct TN_Task *task);

/**
 * The same as `tn_task_wakeup()` but for using in the ISR.
 */
enum TN_RCode tn_task_iwakeup(struct TN_Task *task);

/**
 * Activate task that is in `DORMANT` state, i.e. it was either just created by
 * `tn_task_create()` without `TN_TASK_START_ON_CREATION` option, or 
 * terminated.
 *
 * Task is moved from `DORMANT` state to the `RUNNABLE` state.
 *
 * @see TN_TaskState
 */
enum TN_RCode tn_task_activate(struct TN_Task *task);

/**
 * The same as `tn_task_activate()` but for using in the ISR.
 */
enum TN_RCode tn_task_iactivate(struct TN_Task *task);

/**
 * Release task from `WAIT` state, independently of the reason of waiting.
 *
 * If task is in `WAITING` state, it is moved to `RUNNABLE` state.
 * If task is in `WAITSUSP` state, it is moved to `SUSPEND` state.
 *
 * `TERR_FORCED` is returned to the waiting task.
 *
 * @see TN_TaskState
 */
enum TN_RCode tn_task_release_wait(struct TN_Task *task);

/**
 * The same as `tn_task_release_wait()` but for using in the ISR.
 */
enum TN_RCode tn_task_irelease_wait(struct TN_Task *task);

/**
 * This function terminates the currently running task. The task is moved to
 * the `DORMANT` state.
 *
 * After exiting, the task may be either deleted by the `tn_task_delete()`
 * function call or reactivated by the `tn_task_activate()` /
 * `tn_task_iactivate()` function call. In this case task starts execution from
 * beginning (as after creation/activation).  The task will have the lowest
 * precedence among all tasks with the same priority in the `RUNNABLE` state.
 *
 * If this function is invoked with `TN_TASK_EXIT_OPT_DELETE` option set,
 * the task will be deleted after termination and cannot be reactivated (needs
 * recreation).
 * 
 * This function cannot be invoked from interrupts.
 *
 * @see `TN_TASK_EXIT_OPT_DELETE`
 * @see `tn_task_delete()`
 * @see `tn_task_activate()`
 * @see `tn_task_iactivate()`
 */
void tn_task_exit(enum TN_TaskExitOpt opts);


/**
 * This function is similar to `tn_task_exit()` but it terminates any task
 * other than currently running one.
 *
 * After task is terminated, the task may be either deleted by the
 * `tn_task_delete()` function call or reactivated by the `tn_task_activate()`
 * / `tn_task_iactivate()` function call. In this case task starts execution
 * from beginning (as after creation/activation).  The task will have the
 * lowest precedence among all tasks with the same priority in the `RUNNABLE`
 * state.
 */
enum TN_RCode tn_task_terminate(struct TN_Task *task);

/**
 * This function deletes the task specified by the task. The task must be in
 * the `DORMANT` state, otherwise `TN_RC_WCONTEXT` will be returned.
 *
 * This function resets the `id_task` field in the task structure to 0 and
 * removes the task from the system tasks list. The task can not be
 * reactivated after this function call (the task must be recreated).
 *
 * This function cannot be invoked from interrupts.
 */
enum TN_RCode tn_task_delete(struct TN_Task *task);

/**
 * Set new priority for task.
 * If priority is 0, then task's base_priority is set.
 */
enum TN_RCode tn_task_change_priority(struct TN_Task *task, int new_priority);

#endif // _TN_TASKS_H


/*******************************************************************************
 *    end of file
 ******************************************************************************/

