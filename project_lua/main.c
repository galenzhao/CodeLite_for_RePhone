
#include <stdio.h>
#include <string.h>

#include "vmtype.h"
#include "vmlog.h"
#include "vmsystem.h"
#include "vmgsm_tel.h"
#include "vmgsm_sim.h"
#include "vmtimer.h"
#include "vmdcl.h"
#include "vmdcl_kbd.h"
#include "vmkeypad.h"
#include "vmthread.h"

#include "shell.h"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "GATTServer.h"
#include "vmdatetime.h"


extern void retarget_setup();
extern int luaopen_audio(lua_State *L);
extern int luaopen_gsm(lua_State *L);
extern int luaopen_timer(lua_State *L);
extern int luaopen_gpio(lua_State *L);
extern int luaopen_screen(lua_State *L);
extern int luaopen_i2c(lua_State *L);
extern int luaopen_tcp(lua_State* L);
extern int luaopen_https(lua_State* L);
extern int luaopen_gatt(lua_State* L);

lua_State *L = NULL;

VM_TIMER_ID_PRECISE sys_timer_id = 0;

#define vm_log_debug(...) fprintf (stderr, __VA_ARGS__)

/* timer id of precise timer */

VM_TIMER_ID_PRECISE g_precise_id = 1;

/* the interval length of precise timer */

#define PRECISE_DELAY_TIME (120*1000)

/* the start time of each period of the precise timer  */

VM_TIME_UST_COUNT g_precise_start_count = 0;

/* the time out for each period of the precise timer */

VM_TIME_UST_COUNT g_precise_stop_count = 0;



/* a non precise timer */

VM_TIMER_ID_NON_PRECISE g_non_precise_id = 2;

/* the interval length of the non precise timer*/

#define NON_PRECISE_DELAY_TIME (60*1000)

/*  the start time of each period of the non precise timer  */

VM_TIME_UST_COUNT g_non_precise_start_count = 0;

/*  the time out for each period of the non precise timer */

VM_TIME_UST_COUNT g_non_precise_stop_count = 0;



/* the timer id of hisr timer */

VM_TIMER_ID_HISR g_hisr_id = NULL;

/* the interval length of hisr timer */

#define HISR_DELAY_TIME (180*1000)

/* the flag count the number hisr timer time out */

VMINT g_hisr_count = 0;

vm_bt_gatt_attribute_value_t gatt_attribute_value_t;

void key_init(void){
    VM_DCL_HANDLE kbd_handle;
    vm_dcl_kbd_control_pin_t kbdmap;

    kbd_handle = vm_dcl_open(VM_DCL_KBD,0);
    kbdmap.col_map = 0x09;
    kbdmap.row_map =0x05;
    vm_dcl_control(kbd_handle,VM_DCL_KBD_COMMAND_CONFIG_PIN, (void *)(&kbdmap));

    vm_dcl_close(kbd_handle);
}



void sys_timer_callback(VM_TIMER_ID_PRECISE sys_timer_id, void* user_data)
{
    vm_log_info("tick\n");

}



VMINT handle_keypad_event(VM_KEYPAD_EVENT event, VMINT code){
    /* output log to monitor or catcher */
    vm_log_info("key event=%d,key code=%d\n",event,code); /* event value refer to VM_KEYPAD_EVENT */

    if (code == 30) {
        if (event == 3) {   // long pressed

        } else if (event == 2) { // down
            printf("key is pressed\n");
        } else if (event == 1) { // up

        }
    }
    return 0;
}


static int msleep_c(lua_State *L)
{
    long ms = lua_tointeger(L, -1);
    vm_thread_sleep(ms);
    return 0;
}

void lua_setup()
{
    VM_THREAD_HANDLE handle;

    L = lua_open();
    lua_gc(L, LUA_GCSTOP, 0);  /* stop collector during initialization */
    luaL_openlibs(L);  /* open libraries */

    luaopen_audio(L);
    luaopen_gsm(L);
    luaopen_timer(L);
    luaopen_gpio(L);
    luaopen_screen(L);
    luaopen_i2c(L);
    luaopen_tcp(L);
    luaopen_https(L);
    luaopen_gatt(L);

    lua_register(L, "msleep", msleep_c);

    lua_gc(L, LUA_GCRESTART, 0);

    luaL_dofile(L, "init.lua");

    if (0)
    {
        const char *script = "audio.play('nokia.mp3')";
        int error;
        error = luaL_loadbuffer(L, script, strlen(script), "line") ||
                lua_pcall(L, 0, 0, 0);
        if (error) {
            fprintf(stderr, "%s", lua_tostring(L, -1));
            lua_pop(L, 1);  /* pop error message from the stack */
        }
    }

    handle = vm_thread_create(shell_thread, L, 0);
	vm_thread_change_priority(handle, 245);

}



/* precise timer time out function */

void customer_timer_precise_proc(VM_TIMER_ID_PRECISE timer_id, void* user_data){

    VMUINT32 duration = 0;

    /* get ust count */

    g_precise_stop_count = vm_time_ust_get_count();

    /* compute ust duration between start and time out */

    duration = vm_time_ust_get_duration(g_non_precise_start_count, g_precise_stop_count);

    vm_log_debug("precise proc duration %d stop count %d\n", duration, g_precise_stop_count);

    g_non_precise_start_count = g_precise_stop_count;

}



/* non precise timer time out function */

void customer_timer_non_precise_proc(VM_TIMER_ID_NON_PRECISE timer_id, void* user_data){

    VMUINT32 duration = 0;

    g_non_precise_stop_count = vm_time_ust_get_count();

    duration = vm_time_ust_get_duration(g_precise_start_count, g_non_precise_stop_count);


    // todo test gatt
    gatt_attribute_value_t.length = 1;
    gatt_attribute_value_t.data[0] = duration % 0xff;

    VM_RESULT result = app_send_indication(VM_FALSE, &gatt_attribute_value_t);
    vm_log_debug("non precise proc duration %d stop count %d\n", duration, g_non_precise_stop_count);
    vm_log_debug("%d, %d\n", gatt_attribute_value_t.data[0], result);

    g_precise_start_count = g_non_precise_stop_count;

}



/* hisr timer time out function */

void customer_timer_hisr_proc(void* user_data){

    g_hisr_count++;

}


/* how to create precise, non precise and hisr timer */

void customer_timer_create_timer(void){

    g_precise_id = vm_timer_create_precise(PRECISE_DELAY_TIME, customer_timer_precise_proc, NULL);

    vm_log_debug("customer timer g_precise_id = %d\n", g_precise_id);

    g_precise_start_count = vm_time_ust_get_count();

    vm_log_debug("customer timer precise start count %d\n", g_precise_start_count);

    //use code below to delete precise timer

    //vm_timer_delete_precise(g_precise_id);



    g_non_precise_id = vm_timer_create_non_precise(NON_PRECISE_DELAY_TIME, customer_timer_non_precise_proc, NULL);

    vm_log_debug("customer timer g_non_precise_id = %d\n", g_non_precise_id);

    g_non_precise_start_count = vm_time_ust_get_count();

    vm_log_debug("customer timer non precise start count %d\n", g_non_precise_start_count);

    // use code below to delete non precise timer

    //vm_timer_delete_non_precise(g_non_precise_id);





    g_hisr_id = vm_timer_create_hisr("HISR Timer");

    if(g_hisr_id == NULL){

    	vm_log_debug("create hisr fail\n");

    	return;

    }

    vm_timer_set_hisr(g_hisr_id, customer_timer_hisr_proc, NULL, HISR_DELAY_TIME, HISR_DELAY_TIME);

    //use code below to delete hisr timer

    //vm_timer_delete_hisr(g_hisr_id);

}

void handle_sysevt(VMINT message, VMINT param)
{
    switch (message) {
        case VM_EVENT_CREATE:
            // sys_timer_id = vm_timer_create_precise(1000, sys_timer_callback, NULL);
            lua_setup();

            /* create timer */

            customer_timer_create_timer();
            break;
        case SHELL_MESSAGE_ID:
            shell_docall(L);
            break;
        case VM_EVENT_QUIT:
            break;
    }
}

/* Entry point */
void vm_main(void)
{
    retarget_setup();
    fputs("hello, linkit assist\n", stdout);

    key_init();
    vm_keypad_register_event_callback(handle_keypad_event);

    /* register system events handler */
    vm_pmng_register_system_event_callback(handle_sysevt);

    VM_TIMER_ID_NON_PRECISE timer_id = 0;
    timer_id = vm_timer_create_non_precise(1000, (vm_timer_non_precise_callback)timer_cb, NULL);
    vm_log_debug("create timer [%d]\n", timer_id);
}
