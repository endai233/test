#ifndef CHARGECONTROLCCU_TASK_H
#define CHARGECONTROLCCU_TASK_H

#ifdef __cplusplus
extern "C"{
#endif 

void *Charge_Ccu_task(void *arg);
struct CCu_Charge_Data *Get_CCu_Charge_Data(void);


int ccu_init(void);
int ccu_start(void);
void ccu_release(void);

#ifdef __cplusplus
}
#endif

#endif
