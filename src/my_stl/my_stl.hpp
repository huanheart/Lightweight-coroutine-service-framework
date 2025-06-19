#include"../memorypool/memorypool.hpp"
#include <iostream>
#include<list>
#include<unordered_map>
#include<climits> //定义了一些数字的库,比如INT_MAX
#include<cstddef> //有size_t的一些定义等
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


//更换缓存数据结构为更权威的LRU
//用于查找账号密码的数据结构，替换掉基于链表的缓存数据结构
template<typename T,typename U,typename Alloc=std::allocator<T> >
class LRUCache {
private:
    template<typename K,typename P>
    class LruNode {
    public:
        LruNode(K key,P value) {
            this->key = key;
            this->value = value;
        }
    public:
        K key;
        P value;
    };
    //当容量为-1的时候，表示不限制LRU缓存大小,有多少存多少

private:
    void clear();
public:
    typedef LruNode<T,U> Node;
    typedef LruNode<T,U>* NodePtr;
    //老写法（不兼容新标准库）：但是c++11可以兼容
    //这里用 Alloc 的 rebind，把 Alloc 从 std::allocator<T> 变成能分配 Node 的 allocator
    //注意（从开始到结束，只要用户用了新内存池，那么它的T就跟内存池没有任何关联，因为分配器绑定的是我的Node)
    //所以不管怎么样都不会出错
    typedef typename Alloc::template rebind<Node>::other allocator;
    // 在 C++11 及之后，推荐使用 std::allocator_traits 来代替 rebind。
    // typedef typename std::allocator_traits<Alloc>::template rebind_alloc<Node> allocator;

    LRUCache() {
        capacity=-1;
    }
    LRUCache(int capacity) {
        this->capacity = capacity;
    }
    ~LRUCache() {
        clear();
    }
    void push(T key,U value);
    pair<bool,U> get(T key);
private:
    int capacity;
    unordered_map<T,typename list<NodePtr>::iterator>  map;
    list<NodePtr> my_list;
    allocator allocator_;
};


template<typename T, typename U, typename Alloc>
void LRUCache<T, U, Alloc>::push(T key, U value) {
    //先查看是否已经存在，如果存在，那么修改原先的那个值
    if (map.find(key)!=map.end()) {
        my_list.splice(my_list.begin(),my_list,map[key]);
        my_list.front()->value=value;
        return ;
    }
    //如果限制了大小，那么当数据量超过容量的时候需要将其删除
    if (capacity!=-1 && my_list.size()>=capacity) {
        //获取最后一个元素，进行销毁
        map.erase(my_list.back()->key);
        NodePtr temp=my_list.back();
        //先调用析构函数
        allocator_.destroy(temp);
        //再释放内存
        allocator_.deallocate(temp,1);
        my_list.pop_back();
    }
    //先分配内存
    NodePtr newnode=allocator_.allocate(1);
    //然后调用其构造函数
    allocator_.construct(newnode,Node(key,value) );
    //然后将这个结点加入到list中
    my_list.emplace_front(newnode);
    map[key]=my_list.begin();
}

template<typename T, typename U, typename Alloc>
pair<bool,U> LRUCache<T,U,Alloc> :: get(T key) {
    if (map.find(key) == map.end()) {
        //这边U一定需要空参构造函数,否则内部LRU缓存数据结构会报错
        return pair<bool,U>(false,U{} );
    }
    my_list.splice( my_list.begin(),my_list,map[key]);
    return pair<bool,U>(true,my_list.front()->value);
}

template<typename T, typename U, typename Alloc>
void LRUCache<T, U, Alloc>::clear() {
    //需要将链表中的指针元素全部消除
    map.clear();
    while (my_list.size() ) {
        NodePtr temp=my_list.front();
        cout<<"Node delete "<<temp->key<<" "<<temp->value<<endl;
        //先调用析构函数
        allocator_.destroy(temp);
        //再释放内存
        allocator_.deallocate(temp,1);
        my_list.pop_front();
    }
}





//end
