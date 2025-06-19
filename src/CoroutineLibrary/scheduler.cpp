#include "scheduler.h"

static bool debug = false;

namespace sylar {

    //保存当前线程的调度器
static thread_local Scheduler* t_scheduler = nullptr;

Scheduler* Scheduler::GetThis()
{
	return t_scheduler;
}

void Scheduler::SetThis()
{
	t_scheduler = this;
}

Scheduler::Scheduler(size_t threads, bool use_caller, const std::string &name):
m_useCaller(use_caller), m_name(name)
{
//	assert(threads>0 && Scheduler::GetThis()==nullptr);
    assert(threads>0);
	SetThis();

	Thread::SetName(m_name);

	// 使用主线程当作工作线程
	if(use_caller)
	{
        Fiber::GetThis();
        threads --;
		// 创建主协程
        //这个调度协程其实就是主线程的这部分
		// 创建调度协程，并且将当前类的this传进去，以便访问当前这个类的成员函数,（成员函数默认是有个this参数的）
		m_schedulerFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, false)); // false -> 该调度协程退出后将返回主协程
        //保存当前协程的调度器，将刚刚m_schedulerFiber这个东西，赋值给当前线程fiber类的调度器
		Fiber::SetSchedulerFiber(m_schedulerFiber.get());
		
		m_rootThread = Thread::GetThreadId();
		m_threadIds.push_back(m_rootThread);
	}
    //注意一个小细节：假设当前要求2个工作线程
    // 当使用主线程当作工作线程的时候，将其threads--,此时应该还没有开辟其他线程，固然还需要开辟一个
    // 如果不设置为工作线程的话，那么就说明等会还要开辟两个，就是通过m_threadCount到继承后的子类中去开辟的
	m_threadCount = threads;
	if(debug) std::cout << "Scheduler::Scheduler() success\n";
}

Scheduler::~Scheduler()
{
	assert(stopping()==true);
	if (GetThis() == this) 
	{
        t_scheduler = nullptr;
    }
    if(debug) std::cout << "Scheduler::~Scheduler() success\n";
}

void Scheduler::start()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if(m_stopping)
	{
		std::cerr << "Scheduler is stopped" << std::endl;
		return;
	}

	assert(m_threads.empty());
	m_threads.resize(m_threadCount);
	for(size_t i=0;i<m_threadCount;i++)
	{
		m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this), m_name + "_" + std::to_string(i)));
		m_threadIds.push_back(m_threads[i]->getId());
	}
	if(debug) std::cout << "Scheduler::start() success\n";
}

void Scheduler::run()
{
	int thread_id = Thread::GetThreadId();
	if(debug)
        std::cout << "Schedule::run() starts in thread: " << thread_id << std::endl;
	
	set_hook_enable(true);

	SetThis();

	// 运行在新创建的线程 -> 需要创建主协程
	if(thread_id != m_rootThread)
	{
		Fiber::GetThis();
	}

	std::shared_ptr<Fiber> idle_fiber = std::make_shared<Fiber>(std::bind(&Scheduler::idle, this));
	ScheduleTask task;
	
	while(true)
	{
		task.reset();
		bool tickle_me = false;

		{
			std::lock_guard<std::mutex> lock(m_mutex);
			auto it = m_tasks.begin();
			// 1 遍历任务队列
			while(it!=m_tasks.end())
			{
				if(it->thread!=-1&&it->thread!=thread_id)
				{
					it++;
					tickle_me = true;
					continue;
				}

				// 2 取出任务
				assert(it->fiber||it->cb);
				task = *it;
				m_tasks.erase(it); 
				m_activeThreadCount++;
				break;
			}	
			tickle_me = tickle_me || (it != m_tasks.end());
		}

		if(tickle_me)
		{
			tickle();
		}

		// 3 执行任务
		if(task.fiber)
		{
			{					
				std::lock_guard<std::mutex> lock(task.fiber->m_mutex);
				if(task.fiber->getState()!=Fiber::TERM)
				{
					task.fiber->resume();	
				}
			}
			m_activeThreadCount--;
			task.reset();
		}
		else if(task.cb)
		{
			std::shared_ptr<Fiber> cb_fiber = std::make_shared<Fiber>(task.cb);
			{
				std::lock_guard<std::mutex> lock(cb_fiber->m_mutex);
				cb_fiber->resume();			
			}
			m_activeThreadCount--;
			task.reset();	
		}
		// 4 无任务 -> 执行空闲协程
		else
		{		
			// 系统关闭 -> idle协程将从死循环跳出并结束 -> 此时的idle协程状态为TERM -> 再次进入将跳出循环并退出run()
            if (idle_fiber->getState() == Fiber::TERM) 
            {
            	if(debug) std::cout << "Schedule::run() ends in thread: " << thread_id << std::endl;
                break;
            }
			m_idleThreadCount++;
			idle_fiber->resume();				
			m_idleThreadCount--;
		}
	}
	
}

void Scheduler::stop()
{
//	if(debug)
//        std::cout << "是主线程吗 ？Schedule::stop() starts in thread: " << Thread::GetThreadId() << std::endl;

	if(stopping())
	{
		return;
	}
	m_stopping = true;
    if (m_useCaller) 
    {
        assert(GetThis() == this);
    } 
    else 
    {
        assert(GetThis() != this);
    }
	
	for (size_t i = 0; i < m_threadCount; i++) 
	{
		tickle();
	}

	if (m_schedulerFiber) 
	{
		tickle();
	}

	if(m_schedulerFiber)
	{
		m_schedulerFiber->resume();       //似乎主线程的协程调取器也参与到这个线程池里面了
		if(debug) std::cout << "m_schedulerFiber ends in thread:" << Thread::GetThreadId() << std::endl;
	}
    //创建局部智能指针的线程池，方便后面直接通过stop函数释放掉这些指针
	std::vector<std::shared_ptr<Thread>> thrs;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		thrs.swap(m_threads);
	}
    //阻塞这些线程，保证线程先退出
	for(auto &i : thrs)
	{
		i->join();
	}
	if(debug) std::cout << "Schedule::stop() ends in thread:" << Thread::GetThreadId() << std::endl;
}

void Scheduler::tickle()
{
}

void Scheduler::idle()
{
	while(!stopping())
	{
		if(debug) std::cout << "Scheduler::idle(), sleeping in thread: " << Thread::GetThreadId() << std::endl;	
		sleep(1);	
		Fiber::GetThis()->yield();
	}
}

bool Scheduler::stopping() 
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stopping && m_tasks.empty() && m_activeThreadCount == 0;
}


}