#include "thread.h"

#include <sys/syscall.h> 
#include <iostream>
#include <unistd.h>  

namespace sylar {

// 线程信息
static thread_local Thread* t_thread          = nullptr;
static thread_local std::string t_thread_name = "UNKNOWN";

pid_t Thread::GetThreadId()
{
    //用于用户空间的程序与操作系统内核进行交互
    //这里再讲一下另外一个stdcall 是一种函数调用约定，它定义了函数参数的传递顺序和栈的管理方式
    //syscall 函数可以通过不同的传参来执行各种系统调用
	return syscall(SYS_gettid);
}

Thread* Thread::GetThis()
{
    return t_thread;
}

const std::string& Thread::GetName() 
{
    return t_thread_name;
}

void Thread::SetName(const std::string &name) 
{
    if (t_thread) 
    {
        t_thread->m_name = name;
    }
    t_thread_name = name;
}

Thread::Thread(std::function<void()> cb, const std::string &name): 
m_cb(cb), m_name(name) 
{
    int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);
    if (rt) 
    {
        std::cerr << "pthread_create thread fail, rt=" << rt << " name=" << name;
        throw std::logic_error("pthread_create error");
    }
    // 等待线程函数完成初始化,主要是等待cb函数以前的部分
    m_semaphore.wait();
}

Thread::~Thread() 
{
    if (m_thread) 
    {
        pthread_detach(m_thread);
        m_thread = 0;
    }
}

void Thread::join() 
{
    if (m_thread) 
    {
        int rt = pthread_join(m_thread, nullptr);
        if (rt) 
        {
            std::cerr << "pthread_join failed, rt = " << rt << ", name = " << m_name << std::endl;
            throw std::logic_error("pthread_join error");
        }
        m_thread = 0;
    }
}

void* Thread::run(void* arg) 
{
    Thread* thread = (Thread*)arg;

    t_thread       = thread;
    t_thread_name  = thread->m_name;
    thread->m_id   = GetThreadId();
    //设置对应线程名字
    pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());

    std::function<void()> cb;
    cb.swap(thread->m_cb); // swap -> 可以减少m_cb中只能指针的引用计数
    
    // 初始化完成
    thread->m_semaphore.signal();

    cb();
    return 0;
}

} 

