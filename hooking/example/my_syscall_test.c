#include <linux/module.h>
#include <linux/kernel.h>
//#include <linux/proc_> // proc 파일 시스템 관련헤더?
//#include <linux/seq> // 시퀀스 파일 처리 위한 헤더 
#include <linux/kprobes.h>
#include <linux/version.h>


MODULE_LICENSE("GPL"); 
MODULE_AUTHOR("Chkim"); // 
MODULE_DESCRIPTION("Simple mkdir Hooker using Kprobe"); 

static struct kprobe kp = {
    .symbol_name = "sys_mkdir" // hooking kernel function name
};

static int handler_pre(struct kprobe *p, struct pt_regs *regs) {
    printk(KERN_INFO "[HOOK] sys_mkdir detected! Someone is creating a dir.\n");

    return 0;
}

// 모듈 적재 시 (insmod)
static int __init hook_init(void) {
    int ret;
    
    // 핸들러 연결
    kp.pre_handler = handler_pre;

    // Kprobe 등록
    ret = register_kprobe(&kp);
    if (ret < 0) {
        printk(KERN_ERR "[HOOK] register_kprobe failed, returned %d\n", ret);
        return ret;
    }
    
    printk(KERN_INFO "[HOOK] Module inserted. Hooking %s\n", kp.symbol_name);
    return 0;
}

// 모듈 제거 시 (rmmod)
static void __exit hook_exit(void) {
    unregister_kprobe(&kp); // Kprobe 해제 
    printk(KERN_INFO "[HOOK] Module removed.\n");
}

module_init(hook_init);
module_exit(hook_exit);