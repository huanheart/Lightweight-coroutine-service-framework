#ifndef __SYLAR_IOMANAGER_H__
#define __SYLAR_IOMANAGER_H__

#include "scheduler.h"
#include "timer.h"

namespace sylar {

// work flow
// 1 register one event -> 2 wait for it to ready -> 3 schedule the callback -> 4 unregister the event -> 5 run the callback
class IOManager : public Scheduler,public TimerManager
{
public:
    typedef std::shared_ptr<IOManager> ptr;
public:
    enum Event 
    {
        NONE = 0x0,
        // READ == EPOLLIN
        READ = 0x1,
        // WRITE == EPOLLOUT
        WRITE = 0x4
    };

private:
    struct FdContext 
    {
        struct EventContext 
        {
            Scheduler *scheduler = nullptr;
            std::shared_ptr<Fiber> fiber;
            std::function<void()> cb;
        };

        EventContext read;
        EventContext write;
        int fd = 0;
        Event events = NONE;
        //这里将原本sylar维护的锁进行删除了，将它内部自己封装的锁解耦出来
        std::mutex mutex;
        EventContext& getEventContext(Event event);
        void resetEventContext(EventContext &ctx);
        void triggerEvent(Event event);        
    };

public:
    IOManager(size_t threads = 1, bool use_caller = true, const std::string &name = "IOManager");
    ~IOManager();

    // add one event at a time
    int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
    // delete event
    bool delEvent(int fd, Event event);
    // delete the event and trigger its callback
    bool cancelEvent(int fd, Event event);
    // delete all events and trigger its callback
    bool cancelAll(int fd);

    static IOManager* GetThis();

public:
    //自己添加的函数，用于httpserver示例获取对应的m_epfd
    int get_epollfd(){
        return m_epfd;
    }

protected:
    void tickle() override;
    
    bool stopping() override;
    
    void idle() override;

    void onTimerInsertedAtFront() override;

    void contextResize(size_t size);

public:
    int m_epfd=-1;
private:
    // fd[0] read，fd[1] write
    int m_tickleFds[2];
    std::atomic<size_t> m_pendingEventCount = {0};
    std::shared_mutex m_mutex;
    // store fdcontexts for each fd
    std::vector<FdContext *> m_fdContexts;
};

} // end namespace sylar

#endif