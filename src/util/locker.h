#ifndef LOCKER_H
#define LOCKER_H

#include<exception>
#include<pthread.h>
#include<semaphore.h>


class Sem
{
private:
    sem_t m_sem;


public:
    Sem()
    {
        if(sem_init(&m_sem,0,0)!=0)
        {

            throw std::exception();
        }
    }

    Sem(int num)
    {
        if(sem_init(&m_sem,0,num)!=0)        //��ʼ���ź�����ֵ,���������ô�ͱ��쳣
        {
            throw std::exception();
        }
    }
    ~Sem()
    {
        sem_destroy(&m_sem);

    }
    bool wait()
    {
        //�ȴ��ź���������ȴ��ɹ�����ô�ͷ���0
        //�൱�ڻ����ź�����ֵ-1��ע�⣺�ź����ǿ����ö���̷߳���ͬ����Դ������ֻ��һ���̷߳���һ����Դ
        return sem_wait(&m_sem)==0;   
    }
    bool post()
    {
        return sem_post(&m_sem)==0;
    }
};


class Cond
{
private:
    pthread_cond_t m_cond;

public:
    Cond()
    {
        if(pthread_cond_init(&m_cond,nullptr)!=0)
        {
            throw std::exception();
        }

    }

    ~Cond()
    {
        pthread_cond_destroy(&m_cond);
    }

    bool wait(pthread_mutex_t* m_mutex)
    {
        int ret=0;
        ret=pthread_cond_wait(&m_cond,m_mutex); //�ڵȴ���ͬʱ�ͷŵ��������������ֹ��������
        return ret==0;
    }

    // ��ʱ�����Ƶĵȴ�
    bool time_wait(pthread_mutex_t*m_mutex,struct timespec t)
    {
        int ret=0;
        ret=pthread_cond_timedwait(&m_cond,m_mutex,&t);
        return ret==0;
    }

    //���ѵ����߳�
    bool signal()
    {
        return pthread_cond_signal(&m_cond)==0;
    }

    //���Ѷ���߳�
    bool broadcast()
    {
        return pthread_cond_broadcast(&m_cond)==0;
    }

};

#endif

