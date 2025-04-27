#include <iostream>
#include <thread>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include "debug.hpp"
#include "task.hpp"
#include "timer_loop.hpp"
#include "when_all.hpp"
#include "when_any.hpp"
#include "and_then.hpp"
/*
 * 规定名称格式：
 * 形如promise_type这样的使用匈牙利命名法的函数名称对应标准库
 * 使用小驼峰命名法的为自定义函数
 * 使用大驼峰命名法的为自定义类
 */


int main()
{
	//将0号数据流设置为非阻塞状态，当read没有数据时，返回EWOULDBLOCK
	int attr = 1;
	ioctl(0, FIONBIO, &attr);

	//创建异步控制器
	int epfd = epoll_create1(0);
	
	//创建异步监听器
	struct epoll_event event;
	event.events = EPOLLIN; //设置时间为有输入数据
	event.data.fd = 0; // 设置监听目标为0号输入流
	epoll_ctl(epfd, EPOLL_CTL_ADD, 0, &event);

	while (true)
	{
		epoll_event ebuf[10];
		int res = epoll_wait(epfd, ebuf, 10, 1000);
		if(res == -1)
		{
			debug(), "epoll 出错了: ", strerror(errno);
		}
		if(res == 0)
		{
			debug(), "epoll超时了， 1秒内没有输入";
		}
		debug(), "有", res, "个事件触发"; 
		for(int i = 0; i < res; i++){
			int fd = ebuf[i].data.fd;
			char c;
			while (true)
			{
				int len = read(fd, &c, 1);
				if(len <= 0){ 
					// 表示此时需要阻塞等待
					if(errno == EWOULDBLOCK){
						debug(), "此时不能读取数据";
						break;
					}
					debug(), "read 出错了", strerror(errno);
					break;
				}
				debug(), c;
			}
		}
	}
	
	return 0;
}
