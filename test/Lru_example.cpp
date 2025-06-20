
#include <iostream>
#include<list>
#include<unordered_map>

#include "../src/memorypool/memorypool.hpp"

using namespace std;

//用于查找账号密码的数据结构，替换掉基于链表的缓存数据结构
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
    //当容量为-1的时候，表示不限制LRU缓存大小,有多少存多少

private:
    void clear();
public:
    typedef LruNode<T, U> Node;
    typedef LruNode<T, U>* NodePtr;
    //老写法（不兼容新标准库）：但是c++11可以兼容
    //这里用 Alloc 的 rebind，把 Alloc 从 std::allocator<T> 变成能分配 Node 的 allocator
    //注意（从开始到结束，只要用户用了新内存池，那么它的T就跟内存池没有任何关联，因为分配器绑定的是我的Node)
    //所以不管怎么样都不会出错
    typedef typename Alloc::template rebind<Node>::other allocator;
    // 在 C++11 及之后，推荐使用 std::allocator_traits 来代替 rebind。
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
    //先查看是否已经存在，如果存在，那么修改原先的那个值
    if (map.find(key) != map.end()) {
        list.splice(list.begin(), list, map[key]);
        list.front()->value = value;
        return;
    }
    //如果限制了大小，那么当数据量超过容量的时候需要将其删除
    if (capacity != -1 && list.size() >= capacity) {
        //获取最后一个元素，进行销毁
        map.erase(list.back()->key);
        NodePtr temp = list.back();
        //先调用析构函数
        allocator_.destroy(temp);
        //再释放内存
        allocator_.deallocate(temp, 1);
        list.pop_back();
    }
    //先分配内存
    NodePtr newnode = allocator_.allocate(1);
    //然后调用其构造函数
    allocator_.construct(newnode, Node(key, value));
    //然后将这个结点加入到list中
    list.emplace_front(newnode);
    map[key] = list.begin();
}

template<typename T, typename U, typename Alloc>
pair<bool, U> LRUCache<T, U, Alloc> ::get(T key) {
    if (map.find(key) == map.end()) {
        //这边U一定需要空参构造函数,否则内部LRU缓存数据结构会报错
        return pair<bool, U>(false, U{});
    }
    list.splice(list.begin(), list, map[key]);
    return pair<bool, U>(true, list.front()->value);
}

template<typename T, typename U, typename Alloc>
void LRUCache<T, U, Alloc>::clear() {
    //需要将链表中的指针元素全部消除
    map.clear();
    while (list.size()) {
        NodePtr temp = list.front();
        cout << "Node delete " << temp->key << " " << temp->value << endl;
        //先调用析构函数
        allocator_.destroy(temp);
        //再释放内存
        allocator_.deallocate(temp, 1);
        list.pop_front();
    }
}

void test() {
    //MemoryPool<string>中的string无效，内部使用的是自定义Node为内存分配单位的,传入原因 : 方便传参
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

//标准输出(符合Lru逻辑）
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
