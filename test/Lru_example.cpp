
#include <iostream>
#include<list>
#include<unordered_map>

#include "../src/memorypool/memorypool.hpp"

using namespace std;

//���ڲ����˺���������ݽṹ���滻����������Ļ������ݽṹ
template<typename T, typename U, typename Alloc = std::allocator<T> >
class LRUCache {
public:
    template<typename K, typename P>
    class LruNode {
    public:
        LruNode(K key, P value) {
            this->key = key;
            this->value = value;
        }
        ~LruNode() {
            std::cout << "LruNode is delete: " << this->key << " " << this->value << std::endl;
        }
    public:
        K key;
        P value;
    };
    //������Ϊ-1��ʱ�򣬱�ʾ������LRU�����С,�ж��ٴ����

private:
    void clear();
public:
    typedef LruNode<T, U> Node;
    typedef LruNode<T, U>* NodePtr;
    //��д�����������±�׼�⣩������c++11���Լ���
    //������ Alloc �� rebind���� Alloc �� std::allocator<T> ����ܷ��� Node �� allocator
    //ע�⣨�ӿ�ʼ��������ֻҪ�û��������ڴ�أ���ô����T�͸��ڴ��û���κι�������Ϊ�������󶨵����ҵ�Node)
    //���Բ�����ô�����������
    typedef typename Alloc::template rebind<Node>::other allocator;
    // �� C++11 ��֮���Ƽ�ʹ�� std::allocator_traits ������ rebind��
    // typedef typename std::allocator_traits<Alloc>::template rebind_alloc<Node> allocator;

    LRUCache() {
        capacity = -1;
    }
    LRUCache(int capacity) {
        this->capacity = capacity;
    }
    ~LRUCache() {
        clear();
    }
    void push(T key, U value);
    pair<bool, U> get(T key);
private:
    int capacity;
    unordered_map<T, typename list<NodePtr>::iterator>  map;
    list<NodePtr> list;
    allocator allocator_;
};


template<typename T, typename U, typename Alloc>
void LRUCache<T, U, Alloc>::push(T key, U value) {
    //�Ȳ鿴�Ƿ��Ѿ����ڣ�������ڣ���ô�޸�ԭ�ȵ��Ǹ�ֵ
    if (map.find(key) != map.end()) {
        list.splice(list.begin(), list, map[key]);
        list.front()->value = value;
        return;
    }
    //��������˴�С����ô������������������ʱ����Ҫ����ɾ��
    if (capacity != -1 && list.size() >= capacity) {
        //��ȡ���һ��Ԫ�أ���������
        map.erase(list.back()->key);
        NodePtr temp = list.back();
        //�ȵ�����������
        allocator_.destroy(temp);
        //���ͷ��ڴ�
        allocator_.deallocate(temp, 1);
        list.pop_back();
    }
    //�ȷ����ڴ�
    NodePtr newnode = allocator_.allocate(1);
    //Ȼ������乹�캯��
    allocator_.construct(newnode, Node(key, value));
    //Ȼ����������뵽list��
    list.emplace_front(newnode);
    map[key] = list.begin();
}

template<typename T, typename U, typename Alloc>
pair<bool, U> LRUCache<T, U, Alloc> ::get(T key) {
    if (map.find(key) == map.end()) {
        //���Uһ����Ҫ�ղι��캯��,�����ڲ�LRU�������ݽṹ�ᱨ��
        return pair<bool, U>(false, U{});
    }
    list.splice(list.begin(), list, map[key]);
    return pair<bool, U>(true, list.front()->value);
}

template<typename T, typename U, typename Alloc>
void LRUCache<T, U, Alloc>::clear() {
    //��Ҫ�������е�ָ��Ԫ��ȫ������
    map.clear();
    while (list.size()) {
        NodePtr temp = list.front();
        cout << "Node delete " << temp->key << " " << temp->value << endl;
        //�ȵ�����������
        allocator_.destroy(temp);
        //���ͷ��ڴ�
        allocator_.deallocate(temp, 1);
        list.pop_front();
    }
}

void test() {
    //MemoryPool<string>�е�string��Ч���ڲ�ʹ�õ����Զ���NodeΪ�ڴ���䵥λ��,����ԭ�� : ���㴫��
    LRUCache<string, string, MemoryPool<string> > lru(5);
    for (int i = 0; i < 5; i++) {
        lru.push(to_string(i), to_string(i));
    }
    pair<bool, string> res1 = lru.get("3");
    cout << "test backNode1 " << res1.first << ' ' << res1.second << endl;

    for (int i = 5; i < 10; i++) {
        lru.push(to_string(i), to_string(i));
    }
    pair<bool, string> res2 = lru.get("15");
    cout << "test backNode2 " << res2.first << ' ' << res2.second << endl;

}

int main() {
    test();
    return 0;
}

//��׼���(����Lru�߼���
/*
LruNode is delete: 0 0
LruNode is delete: 1 1
LruNode is delete: 2 2
LruNode is delete: 3 3
LruNode is delete: 4 4
test backNode1 1 3
LruNode is delete: 0 0
LruNode is delete: 5 5
LruNode is delete: 1 1
LruNode is delete: 6 6
LruNode is delete: 2 2
LruNode is delete: 7 7
LruNode is delete: 4 4
LruNode is delete: 8 8
LruNode is delete: 3 3
LruNode is delete: 9 9
test backNode2 0
Node delete 9 9
LruNode is delete: 9 9
Node delete 8 8
LruNode is delete: 8 8
Node delete 7 7
LruNode is delete: 7 7
Node delete 6 6
LruNode is delete: 6 6
Node delete 5 5
LruNode is delete: 5 5
*/
