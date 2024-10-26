#ifndef MEMORY_POOL_HPP
#define MEMORY_POOL_HPP


#include<climits> //定义了一些数字的库,比如INT_MAX
#include<cstddef> //有size_t的一些定义等

template<typename T,size_t BlockSize=4096>
class MemoryPool
{
public:
    typedef T* pointer;
    //定义rebind<U>::other接口
    template<typename U> struct rebind{
        using other=MemoryPool<U>;
    };

    //显式说明该函数不可能出现异常,增加可读性
    MemoryPool() noexcept{
        currentBlock_=nullptr;
        currentSlot_=nullptr;
        lastSlot_=nullptr;
        freeSlots_=nullptr;
    }

    ~MemoryPool()noexcept{
        //循环销毁所有内存池中的内存区块
        slot_pointer_ curr=currentBlock_;
        while(curr!=nullptr){
            slot_pointer_ prev=curr->next;
            operator delete(reinterpret_cast<void*>(curr) );
            curr=prev;
        }
    }

    //同一个时间只能分配一个对象,n和hint会被忽略
    pointer allocate(size_t n=1,const T*hint=0 );

    //销毁指针p指向的内存区域块
    void deallocate(pointer p,size_t n=1);

    //调用构造函数
    template<typename U,typename ...Args>
    void construct(U*p,Args&&...args);

    //销毁内存池中的对象，显式调用析构函数
    template<typename U>
    void destroy(U* p){
        p->~U();
    }



private:


    //要么实例化一个存放对象的槽，要么被实例化为一个指向对象槽的指针
    //共享内存，如果访问了element，那么next就不再有效，反之也成立
    union Slot_
    {
        T element; 
        Slot_ * next; //这个是指针，固然所占字节数固定8字节，在64位操作系统上
    };

    

    //数据指针
    using data_pointer_=char*;
    //对象槽
    using slot_type_=Slot_;
    //对象槽指针
    using slot_pointer_=Slot_*;

    //指向当前内存块
    slot_pointer_ currentBlock_;
    //指向当前内存块的一个对象槽：注意：一个内存块包含了多个对象槽
    slot_pointer_ currentSlot_;
    //指向当前内存区域块的最后一个对象槽
    slot_pointer_ lastSlot_;
    //指向当前内存块中的空闲对象槽
    slot_pointer_ freeSlots_;

    //静态断言，在编译的时候去触发判断，不像assert，是动态的，在运行时进行判断
    static_assert(BlockSize>=2*sizeof(slot_type_),"BlockSize too small " );
};

//调用构造函数,完美转发保证左右值的正确性
template<typename T,size_t BlockSize>
template<typename U, typename... Args>
void MemoryPool<T,BlockSize>::construct(U* p, Args&&... args) {
    //这里并没有开辟一个内存，而是placement_new操作
    //表示在已有的p内存中，调用对应的args参数的构造函数，构建对应的args对象
    new(p) U(std::forward<Args>(args)...);
}


//销毁指针p所指向的内存区块
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
    //如果本身的对象槽有空闲的，那么就直接返回即可
    if(freeSlots_!=nullptr){
        pointer result=reinterpret_cast<pointer>(freeSlots_);
        freeSlots_=freeSlots_->next;
        return result;
    }else{ 
        //如果对象槽够用，那么直接进入return ，进行一个返回
        if(currentSlot_>=lastSlot_){ 
            //否则分配一个新的内存区块，并指向前一个内存区块
            //data_pointer_为char*这个类型
            data_pointer_ newBlock=reinterpret_cast<data_pointer_>(operator new(BlockSize) ); //分配一个内存
            reinterpret_cast<slot_pointer_>(newBlock)->next=currentBlock_; //新开的一个内存块指向原本内存块
            currentBlock_=reinterpret_cast<slot_pointer_>(newBlock);     //然后将内存块指向新开内存块
            //填补整个区块来满足元素的内存区域对齐要求
            data_pointer_ body=newBlock+sizeof(slot_pointer_);
            uintptr_t result=reinterpret_cast<uintptr_t>(body);
            //alignof 是一个 C++ 中的运算符，用于获取指定类型的对齐要求
            size_t bodyPadding=( alignof(slot_type_) )-result%alignof(slot_type_);
            //这边可以拿一个char来举例（因为某种原因，本身一个字节占了4个字节） 10001111（第一个1表示char所占用的空间，000表示内存对齐的偏移量）
            //然后下一个int类型的数据就需要在1111的开头开始放入了，增加效率,所以这个body+bodyPadding可以表示例子中的int的真实位置了）
            currentSlot_=reinterpret_cast<slot_pointer_>(body+bodyPadding);
            lastSlot_=reinterpret_cast<slot_pointer_>(newBlock+BlockSize-sizeof(slot_type_)+1 );
        }
        return reinterpret_cast<pointer>(currentSlot_++);

    }

}


#endif