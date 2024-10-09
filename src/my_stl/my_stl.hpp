#include"../memorypool/memorypool.hpp"

using namespace std;

//在我的webserver中，只需要增加以及查询，所以根据了一定的webserver的需求来设计这个数据结构，不过还是做了pop等公有接口

#define f_end -1

template<typename P>
class BackNode
{
public:
    int index;
    P passwd;
};


template<typename T,typename Alloc=std::allocator<T> >
class FindCache    //实现增，删，改，查。以及删除所有内存
{
private:
    template<typename U>
    class FindNode
    {
    public:
        FindNode(U key,U value){
            this->key=key;
            this->value=value;
            next=nullptr;     //不允许用户创建这个结点对象，只有友元类可以(或者可以用一个匿名的内部命名空间也可以做到其他类不访问的了)
        }
        FindNode(){
            next=nullptr;
        }
    public:
        U key;
        U value;
        FindNode * next;
    };

private:
    void clear();
public:
    typedef FindNode<T> Node;
    typedef typename Alloc::template rebind<Node>::other allocator;
    //默认构造
    FindCache(){
        aider=allocator_.allocate(1); //分配一个T类型大小的空间
        allocator_.construct(aider,Node() ); //然后调用其构造函数
        head=nullptr;     //这里我们选择使用头插法
        size_=0;
    }
    ~FindCache(){
        clear();
    }

    bool push(T key,T value);

    bool pop(T key,T value);

    bool update(T key,T value); //这里保证key是不能被改的

    BackNode<T> find(T key);  //返回一个下标

    long long get_size(){
        return size_;
    }
private:
    allocator allocator_;
    Node * head;  //首元结点
    Node * aider; //辅助结点(一般顺序是辅助结点指向第一个结点即首元结点)
    long long size_; //存放数组大小
};

template<typename T,typename Alloc>
void FindCache<T,Alloc>::clear()
{
    Node * temp=aider->next;

    while(temp)
    {
        Node * temp_2=temp;
        temp=temp->next;
        aider->next=temp;
        cout<<temp_2->key<<' '<<temp_2->value<<endl;
        //先调用析构函数，再来释放内存
        allocator_.destroy(temp_2);
        allocator_.deallocate(temp_2,1);
        
    }
    //最后释放辅助结点
    allocator_.destroy(aider);
    allocator_.deallocate(aider,1);

}

template<typename T,typename Alloc>
bool FindCache<T,Alloc>::push(T key,T value)  //增加元素
{
    BackNode<T> back;
    back=find(key);
    if(back.index!=f_end) //不允许插入具有相同key的元素，key值唯一
        return false;
    Node *newnode=allocator_.allocate(1); //分配一个T类型大小的空间
    allocator_.construct(newnode,Node(key,value) ); //然后调用其构造函数
    //现在开始进行插入数据
    newnode->next=head;
    aider->next=newnode;
    head=newnode;
    size_++;
    return true;
}

template<typename T,typename Alloc>
bool FindCache<T,Alloc>::pop(T key,T value) //要去查找对应键值是否有相同的结点
{
    //从辅助结点出发
    Node * temp=aider->next;
    Node * last_temp=aider;
    while(temp){
        if(temp->key==key&&temp->value==value){
            //进行删除
            last_temp->next=temp->next;
            size_--;
            //这个特判的原因：head由于是指向辅助结点的下一个结点，那么如果删除了head，我们需要做一些操作，即将head给重新指向
            //否则head会变成野指针
            if(head==temp){ 
                head=last_temp->next;
            }
            //先调用析构函数，再来释放内存
            allocator_.destroy(temp);            
            allocator_.deallocate(temp,1);
            return true;
        }
        temp=temp->next;
        last_temp=last_temp->next;
    }

    return false; //说明没有找到
}


template<typename T,typename Alloc>
BackNode<T> FindCache<T,Alloc>::find(T key)
{
    Node * temp=aider->next;
    int index=1; //注意：从1开始表示第一个元素
    BackNode<T> back;
    while(temp)
    {
        if(temp->key==key){
            back.index=index;
            back.passwd=temp->value;
            return back;
        }
        temp=temp->next;
        index++;
    }
    back.index=f_end;
    return back;
}

template<typename T,typename Alloc>
bool FindCache<T,Alloc>::update(T key,T value) 
{
    Node* temp=aider->next;
    while(temp){
        if(temp->key==key){
            temp->value=value;
            return true;
        }
        temp=temp->next;
    }
    return false;
};