#include "include.h"

u32 get_test_sbc_addr(void)
{
    //return RES_BUF_TEST_SBC;
    return 0;
}

u32 get_test_sbc_len(void)
{
    //return RES_LEN_TEST_SBC;
    return 0;
}

void sys_error_hook(u8 err_no)
{
    WDT_RST();
}

//正常启动Main函数
int main(void)
{
    printf("Hello AB5300\n");
    bsp_sys_init();
    func_run();
    return 0;
}

//升级完成
void update_complete(int mode)
{
    bsp_update_init();
    if (mode == 0) {
        WDT_DIS();
        while (1);
    }else if(mode == 1){
        WDT_RST();
    }else {  //mode = 2 OTA升级完成
        WDT_RST();
    }
}
