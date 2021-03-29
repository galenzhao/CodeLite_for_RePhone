#ifndef _LINKIT_APP_WIZARDTEMPLATE_
#define	_LINKIT_APP_WIZARDTEMPLATE_
#include "vmtimer.h"
#include "vmbt_gatt.h"
#include "vmbt_cm.h"

void timer_cb(VM_TIMER_ID_NON_PRECISE tid, void* user_data);
VM_RESULT app_send_indication(
        VMBOOL need_confirm, vm_bt_gatt_attribute_value_t* value);

#endif

