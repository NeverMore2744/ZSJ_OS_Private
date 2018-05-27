

/*
 **** 系统调用的重构:系统调用的静态注册 ****
办法：直接初始化静态函数指针数组

以4号中断为例
旧办法为
register_syscall(4, syscall4);
其中4为系统调用号，syscall4为处理函数的函数指针。
存在以下缺点：
•	使得程序冗长，添加系统调用的负担重
•	注册时机十分重要，稍不注意就出错
•	可读性差
改进后的做法为
直接初始化静态函数指针数组

//  [宏定义：系统调用号]    sys_函数指针,
[SYS_printstring]       syscall4,
其中syscall4为处理函数的函数指针，SYS_printstring 为一个宏定义
#define SYS_printstring      50
宏定义在unistd.h中
存在以下优点：
•	程序简洁，编程方便
•	由编译器保证注册时机
•	可读性好，以表格形式呈现系统调用
*/

#ifndef __PRIVATE_SYSCALL_H__
#define __PRIVATE_SYSCALL_H__

void syscall(void);

#endif /* !__PRIVATE_SYSCALL_H__ */

/*
  *** 致命：MIPS编译器的错误 ***
以下办法会产生错误，原因是内嵌汇编导致$v0绑定错误。

	Fatal Warnnig: $a0 is not correct
	It seems that only li is reliable.
	
	asm volatile(                                //try system call
        "li $v0, 4\n\t"   // $v0: system call code
		"li $a0, 0\n\t"
		"add $a0, %0,$a0\n\t"// $a0: 传入参数, mov $a0, %0
		"syscall\n\t"
        :      //output
		: "r"(iAddressOfString)		//input
        );

希望获得正确结果的解决办法应该是：
		
	To get right answer, you should write:
	unsigned int iAddressOfString = 100;
	asm volatile(                                //try system call
		"li $a0, 0\n\t"
		"add $a0, %0,$a0\n\t"// $a0: 传入参数, mov $a0, %0
		
        :      //output
		: "r"( iAddressOfString )		//input
        );
	asm volatile(                                //try system call
        "li $v0, 4\n\t"   // $v0: system call code
		"syscall\n\t"
        );		
*/