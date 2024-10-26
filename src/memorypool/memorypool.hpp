#ifndef MEMORY_POOL_HPP
#define MEMORY_POOL_HPP


#include<climits> //������һЩ���ֵĿ�,����INT_MAX
#include<cstddef> //��size_t��һЩ�����

template<typename T,size_t BlockSize=4096>
class MemoryPool
{
public:
    typedef T* pointer;
    //����rebind<U>::other�ӿ�
    template<typename U> struct rebind{
        using other=MemoryPool<U>;
    };

    //��ʽ˵���ú��������ܳ����쳣,���ӿɶ���
    MemoryPool() noexcept{
        currentBlock_=nullptr;
        currentSlot_=nullptr;
        lastSlot_=nullptr;
        freeSlots_=nullptr;
    }

    ~MemoryPool()noexcept{
        //ѭ�����������ڴ���е��ڴ�����
        slot_pointer_ curr=currentBlock_;
        while(curr!=nullptr){
            slot_pointer_ prev=curr->next;
            operator delete(reinterpret_cast<void*>(curr) );
            curr=prev;
        }
    }

    //ͬһ��ʱ��ֻ�ܷ���һ������,n��hint�ᱻ����
    pointer allocate(size_t n=1,const T*hint=0 );

    //����ָ��pָ����ڴ������
    void deallocate(pointer p,size_t n=1);

    //���ù��캯��
    template<typename U,typename ...Args>
    void construct(U*p,Args&&...args);

    //�����ڴ���еĶ�����ʽ������������
    template<typename U>
    void destroy(U* p){
        p->~U();
    }



private:


    //Ҫôʵ����һ����Ŷ���Ĳۣ�Ҫô��ʵ����Ϊһ��ָ�����۵�ָ��
    //�����ڴ棬���������element����ônext�Ͳ�����Ч����֮Ҳ����
    union Slot_
    {
        T element; 
        Slot_ * next; //�����ָ�룬��Ȼ��ռ�ֽ����̶�8�ֽڣ���64λ����ϵͳ��
    };

    

    //����ָ��
    using data_pointer_=char*;
    //�����
    using slot_type_=Slot_;
    //�����ָ��
    using slot_pointer_=Slot_*;

    //ָ��ǰ�ڴ��
    slot_pointer_ currentBlock_;
    //ָ��ǰ�ڴ���һ������ۣ�ע�⣺һ���ڴ������˶�������
    slot_pointer_ currentSlot_;
    //ָ��ǰ�ڴ����������һ�������
    slot_pointer_ lastSlot_;
    //ָ��ǰ�ڴ���еĿ��ж����
    slot_pointer_ freeSlots_;

    //��̬���ԣ��ڱ����ʱ��ȥ�����жϣ�����assert���Ƕ�̬�ģ�������ʱ�����ж�
    static_assert(BlockSize>=2*sizeof(slot_type_),"BlockSize too small " );
};

//���ù��캯��,����ת����֤����ֵ����ȷ��
template<typename T,size_t BlockSize>
template<typename U, typename... Args>
void MemoryPool<T,BlockSize>::construct(U* p, Args&&... args) {
    //���ﲢû�п���һ���ڴ棬����placement_new����
    //��ʾ�����е�p�ڴ��У����ö�Ӧ��args�����Ĺ��캯����������Ӧ��args����
    new(p) U(std::forward<Args>(args)...);
}


//����ָ��p��ָ����ڴ�����
template<typename T,size_t BlockSize>
void MemoryPool<T,BlockSize>::deallocate(pointer p,size_t n)
{
    if(p!=nullptr){
        reinterpret_cast<slot_pointer_>(p)->next=freeSlots_;
        freeSlots_=reinterpret_cast<slot_pointer_>(p);
    }

}

template<typename T,size_t BlockSize>
typename MemoryPool<T,BlockSize>::pointer MemoryPool<T,BlockSize>::allocate(size_t n,const T*hint)
{
    //�������Ķ�����п��еģ���ô��ֱ�ӷ��ؼ���
    if(freeSlots_!=nullptr){
        pointer result=reinterpret_cast<pointer>(freeSlots_);
        freeSlots_=freeSlots_->next;
        return result;
    }else{ 
        //�������۹��ã���ôֱ�ӽ���return ������һ������
        if(currentSlot_>=lastSlot_){ 
            //�������һ���µ��ڴ����飬��ָ��ǰһ���ڴ�����
            //data_pointer_Ϊchar*�������
            data_pointer_ newBlock=reinterpret_cast<data_pointer_>(operator new(BlockSize) ); //����һ���ڴ�
            reinterpret_cast<slot_pointer_>(newBlock)->next=currentBlock_; //�¿���һ���ڴ��ָ��ԭ���ڴ��
            currentBlock_=reinterpret_cast<slot_pointer_>(newBlock);     //Ȼ���ڴ��ָ���¿��ڴ��
            //���������������Ԫ�ص��ڴ��������Ҫ��
            data_pointer_ body=newBlock+sizeof(slot_pointer_);
            uintptr_t result=reinterpret_cast<uintptr_t>(body);
            //alignof ��һ�� C++ �е�����������ڻ�ȡָ�����͵Ķ���Ҫ��
            size_t bodyPadding=( alignof(slot_type_) )-result%alignof(slot_type_);
            //��߿�����һ��char����������Ϊĳ��ԭ�򣬱���һ���ֽ�ռ��4���ֽڣ� 10001111����һ��1��ʾchar��ռ�õĿռ䣬000��ʾ�ڴ�����ƫ������
            //Ȼ����һ��int���͵����ݾ���Ҫ��1111�Ŀ�ͷ��ʼ�����ˣ�����Ч��,�������body+bodyPadding���Ա�ʾ�����е�int����ʵλ���ˣ�
            currentSlot_=reinterpret_cast<slot_pointer_>(body+bodyPadding);
            lastSlot_=reinterpret_cast<slot_pointer_>(newBlock+BlockSize-sizeof(slot_type_)+1 );
        }
        return reinterpret_cast<pointer>(currentSlot_++);

    }

}


#endif