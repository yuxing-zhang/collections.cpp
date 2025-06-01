/*
 * C++ implementation of Python collections
 *
 * defaultdict(f[, initializer_list]) behaves as expected, f supplying the
 * default value, with the signature of V (*)().
 *
 * Counter([initializer_list]) is like a defaultdict with a default value of
 * 0, additionally paired with the following methods:
 *   - elements() Return a range over elements repeating each as many times
 *   as its count. If an element’s count is less than one, elements() will
 *   ignore it.
 *   - most_common([n]) Return a vector of the n most common elements and their
 *   counts from the most common to the least. If n is omitted, most_common()
 *   returns all elements in the counter.
 *   - update(initializer_list<key_type>) Count the elements in the list and
 *   update the counter accordingly.
 *   - +/-/+=/-= operators behave as expected.
 * 
 * ChainMap(map[, maps...]) Groups multiple mappings together to create a
 * single, updateable view. The following methods are supported. 
 *   - get_map(n) Return the n-th map, by reference.
 *   - new_child(map) Create a new ChainMap containing a new map followed by
 *   all of the maps in the current instance.
 *   - at(k) const Search for the value associated with the given key. Throws
 *   exception on failure. Read only.
 *   - operator[k] Can read and write, but all modifications only apply to
 *   the first mapping.
 *   - one_map() Return a single materialized map containing all the keys
 *   and their values as if returned by at().
 */

#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>

template<class Iter>
struct range {
    Iter b, e;
    Iter begin() { return b; }
    Iter end() { return e; }
};

//-----defaultdict-----
template<class K, class V>
struct defaultdict : std::unordered_map<K, V> {
    V (*f)();

    defaultdict(V (*f)()) : f(f) {}
    defaultdict(V (*f)(), std::initializer_list<std::pair<const K, V>> il) :
            f(f), std::unordered_map<K, V>(il) {}

    V& operator[](const K& k) {
        try { return std::unordered_map<K, V>::at(k); }
        catch (const std::out_of_range &) {
            return std::unordered_map<K, V>::operator[](k) = f();
        }
    }

    V& at(const K& k) { return (*this)[k]; }
};

//-----Counter-----
template<class T>
struct Counter : std::unordered_map<T, int> {
    typedef typename std::unordered_map<T, int>::iterator iterator;
    typedef typename std::unordered_map<T, int>::value_type value_type;

    Counter(std::initializer_list<std::pair<const T, int>> il) :
            std::unordered_map<T, int>(il) {}
    Counter() = default;

    struct Iter {
        iterator curr, end;
        int count;
        Iter(iterator i, iterator e) : curr(i), end(e),
                                       count(i == e ? 0 : i->second) {}
        const T& operator*() { return curr->first; }
        Iter& operator++() {
            --count;
            while (count <= 0 && ++curr != end)
                count = curr->second;
            return *this;
        }
        Iter operator++(int) {
            Iter i = *this;
            ++*this;
            return i;
        }
        bool operator!=(const Iter &i) { return curr != i.curr; }
    };

    range<Iter> elements() {
        iterator i = this->begin(), j = this->end();
        while (i != j && i->second <= 0) ++i;
        return {{i, j}, {j, j}};
    }

    std::vector<std::pair<T, int>> most_common(int n=0) {
        if (n == 0) n = this->size();
        std::vector<std::pair<T, int>> v(this->begin(), this->end());
        std::function cmp = [](const std::pair<T, int> &l,
                               const std::pair<T, int> &r) {
                                   return l.second < r.second;
                               };
        std::make_heap(v.begin(), v.end(), cmp);
        for (int i = 0; i < n; ++i) std::pop_heap(v.begin(), v.end() - i, cmp);
        return {std::reverse_iterator(v.end()),
                std::reverse_iterator(v.end() - n)};
    }

    int& at(const T& t) = delete;

    Counter& update(std::initializer_list<T> l) {
        for (const T& t : l) ++(*this)[t];
        return *this;
    }

    Counter& operator+=(const Counter &c) {
        for (auto &t : c) (*this)[t.first] += t.second;
        return *this;
    }

    Counter& operator-=(const Counter &c) {
        for (auto &t : c) (*this)[t.first] -= t.second;
        return *this;
    }

    Counter operator+(const Counter &c) {
        Counter ct = *this;
        return ct += c;
    }

    Counter operator-(const Counter &c) {
        Counter ct = *this;
        return ct -= c;
    }

    int total() { 
        int s = 0;
        for (auto &t : *this) s += t.second;
        return s;
    }
};

//-----ChainMap-----
template<class Map, class... Maps>
struct ChainMap : ChainMap<Maps...> {
    typedef typename Map::key_type K;
    typedef typename Map::mapped_type V;
    typedef ChainMap<Maps...> B;

    Map& map;

    ChainMap(Map& map, Maps&... maps) : map(map), B(maps...) {}
    ChainMap(Map& map, const B& cmp) : map(map), B(cmp) {}

    Map& get_map(size_t i) { return i ? B::get_map(i - 1) : map; }

    template<class NewMap>
    ChainMap<NewMap, Map, Maps...> new_child(NewMap& new_map) {
        return {new_map, *this};
    }

    const V& at(const K& k) const {
        try { return map.at(k); }
        catch (const std::out_of_range &) {
            return static_cast<const B*>(this)->at(k);
        }
    }

    V& operator[](const K& k) {
        try { return map[k] = at(k); }
        catch (const std::out_of_range &) {
            return map[k];
        }
    }

    size_t erase(const K& k) { return map.erase(k); }

    void one_map(Map& one) {
        one.insert(map.begin(), map.end());
        B::one_map(one);
    }

    Map one_map() {
        Map one;
        one_map(one);
        return one;
    }
};

template<class Map>
struct ChainMap<Map> {
    typedef typename Map::key_type K;
    typedef typename Map::mapped_type V;

    Map& map;

    ChainMap(Map& map) : map(map) {}

    Map& get_map(size_t i) {
        if (i) throw std::out_of_range("ChainMap::get_map: index out of range");
        return map;
    }

    template<class NewMap>
    ChainMap<NewMap, Map> new_child(NewMap& new_map) { return {new_map, map}; }

    const V& at(const K& k) const { return map.at(k); }

    V& operator[](const K& k) { return map[k]; }

    size_t erase(const K& k) { return map.erase(k); }

    void one_map(Map& one) { one.insert(map.begin(), map.end()); }

    Map one_map() { return map; }
};

int main() {
    //-----defaultdict tests-----
    std::cout << "defaultdict tests:\n";
    defaultdict<char, int> dd([]() { return -1; }, {{'a', 1}});

    // defaultdict value access: 1-1
    std::cout << dd['a'] << dd.at('b') << '\n';

    //-----Counter tests-----
    std::cout << "\nCounter tests:\n";
    Counter<char> ct{{'a', 1}, {'b', 1}};
    
    // Counter updates: ccbba dccbbaaa
    ct += {{'b', 1}, {'c', 2}};
    for (char c : ct.elements()) std::cout << c;
    ct.update({'a', 'd', 'a'});
    std::cout << ' ';
    for (char c : ct.elements()) std::cout << c;

    // Counter::most_common: a3 a3c2b2d1
    std::vector<std::pair<char, int>> v = ct.most_common(1);
    std::cout << '\n' << v[0].first << v[0].second << ' '; 
    v = ct.most_common();
    for (const auto& t : v) std::cout << t.first << t.second;

    // Counter::elements handling non-positive counts: ccaaa
    ct -= {{'b', 2}, {'d', 2}};
    std::cout << '\n';
    for (char c : ct.elements()) std::cout << c;
    
    // Counter::total: 4
    std::cout << '\n' << ct.total() << '\n';

    //-----ChainMap tests-----
    std::cout << "\nChainMap tests:\n";
    std::map<char, int> mp1{{'a', 1}, {'b', 2}},
                        mp2{{'b', 3}, {'c', 4}},
                        mp3{{'c', 5}, {'d', 6}};
    ChainMap cmp(mp2, mp3);

    // ChainMap::new_child: a1b2c4d6
    ChainMap n_cmp = cmp.new_child(mp1);
    for (auto &t : n_cmp.one_map()) std::cout << t.first << t.second;

    // ChainMap::at: 346
    std::cout << '\n' << cmp.at('b') << cmp.at('c') << cmp.at('d') << '\n';

    // ChainMap::at bounds checking
    try { cmp.at('a'); } catch (const std::out_of_range &) {
        std::cout << "Bounds checked\n";
    }

    // ChainMap modifications only operate on the first mapping: b3d7 c5d6
    cmp.erase('c'), cmp['d'] += 1;
    for (auto &t : mp2) std::cout << t.first << t.second;
    std::cout << ' ';
    for (auto &t : mp3) std::cout << t.first << t.second;

    // ChainMap::get_map returns reference: c6d6
    ++cmp.get_map(1)['c'];
    std::cout << '\n';
    for (auto &t : mp3) std::cout << t.first << t.second;

    // ChainMap::get_map bounds checking
    try { cmp.get_map(2); } catch (const std::out_of_range &) {
        std::cout << "\nBounds checked\n";
    }
}
