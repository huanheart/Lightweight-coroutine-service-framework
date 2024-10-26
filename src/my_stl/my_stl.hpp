#include"../memorypool/memorypool.hpp"

using namespace std;

//���ҵ�webserver�У�ֻ��Ҫ�����Լ���ѯ�����Ը�����һ����webserver�����������������ݽṹ��������������pop�ȹ��нӿ�

#define f_end -1

template<typename P>
class BackNode
{
public:
    int index;
    P passwd;
};


template<typename T,typename Alloc=std::allocator<T> >
class FindCache    //ʵ������ɾ���ģ��顣�Լ�ɾ�������ڴ�
{
private:
    template<typename U>
    class FindNode
    {
    public:
        FindNode(U key,U value){
            this->key=key;
            this->value=value;
            next=nullptr;     //�������û��������������ֻ����Ԫ�����(���߿�����һ���������ڲ������ռ�Ҳ�������������಻���ʵ���)
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
    //Ĭ�Ϲ���
    FindCache(){
        aider=allocator_.allocate(1); //����һ��T���ʹ�С�Ŀռ�
        allocator_.construct(aider,Node() ); //Ȼ������乹�캯��
        head=nullptr;     //��������ѡ��ʹ��ͷ�巨
        size_=0;
    }
    ~FindCache(){
        clear();
    }

    bool push(T key,T value);

    bool pop(T key,T value);

    bool update(T key,T value); //���ﱣ֤key�ǲ��ܱ��ĵ�

    BackNode<T> find(T key);  //����һ���±�

    long long get_size(){
        return size_;
    }
private:
    allocator allocator_;
    Node * head;  //��Ԫ���
    Node * aider; //�������(һ��˳���Ǹ������ָ���һ����㼴��Ԫ���)
    long long size_; //��������С
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
        //�ȵ������������������ͷ��ڴ�
        allocator_.destroy(temp_2);
        allocator_.deallocate(temp_2,1);
        
    }
    //����ͷŸ������
    allocator_.destroy(aider);
    allocator_.deallocate(aider,1);

}

template<typename T,typename Alloc>
bool FindCache<T,Alloc>::push(T key,T value)  //����Ԫ��
{
    BackNode<T> back;
    back=find(key);
    if(back.index!=f_end) //��������������ͬkey��Ԫ�أ�keyֵΨһ
        return false;
    Node *newnode=allocator_.allocate(1); //����һ��T���ʹ�С�Ŀռ�
    allocator_.construct(newnode,Node(key,value) ); //Ȼ������乹�캯��
    //���ڿ�ʼ���в�������
    newnode->next=head;
    aider->next=newnode;
    head=newnode;
    size_++;
    return true;
}

template<typename T,typename Alloc>
bool FindCache<T,Alloc>::pop(T key,T value) //Ҫȥ���Ҷ�Ӧ��ֵ�Ƿ�����ͬ�Ľ��
{
    //�Ӹ���������
    Node * temp=aider->next;
    Node * last_temp=aider;
    while(temp){
        if(temp->key==key&&temp->value==value){
            //����ɾ��
            last_temp->next=temp->next;
            size_--;
            //������е�ԭ��head������ָ����������һ����㣬��ô���ɾ����head��������Ҫ��һЩ����������head������ָ��
            //����head����Ұָ��
            if(head==temp){ 
                head=last_temp->next;
            }
            //�ȵ������������������ͷ��ڴ�
            allocator_.destroy(temp);            
            allocator_.deallocate(temp,1);
            return true;
        }
        temp=temp->next;
        last_temp=last_temp->next;
    }

    return false; //˵��û���ҵ�
}


template<typename T,typename Alloc>
BackNode<T> FindCache<T,Alloc>::find(T key)
{
    Node * temp=aider->next;
    int index=1; //ע�⣺��1��ʼ��ʾ��һ��Ԫ��
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