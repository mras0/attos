#include <assert.h>
#include "../../stage3/attos/tree.h"

#define CONTAINING_RECORD(ptr, type, field) *reinterpret_cast<type*>(reinterpret_cast<char*>(ptr) - offsetof(type, field))

#include <iostream>

using namespace attos;

std::ostream& operator<<(std::ostream& os, const tree_node& n) {
    return os << "tree_node{" << &n << ", parent=" << n.parent << ", left=" << n.left << ", " << n.right << "}";
}

struct test {
    explicit test(int value = 0) : value(value), node() {
    }
    test(const test&) = delete;
    test& operator=(const test&) = delete;

    int       value;
    tree_node node;
};

struct test_node_compare {
    bool operator()(const test& l, const test& r) const {
        return l.value < r.value;
    }
};

std::ostream& operator<<(std::ostream& os, const test& t) {
    return os << "test{value=" << t.value << ", node=" << t.node << "}";
}

using tree_type = tree<test, &test::node, test_node_compare>;

#include <string>

void dump_tree(tree_node* root, int indent = 0)
{
    std::cout << std::string(indent*2, ' ');
    if (!root) {
        std::cout << "(null)\n";
        return;
    }
    std::cout << containing_record(*root, &test::node) << "\n";
    dump_tree(root->left, indent+1);
    dump_tree(root->right, indent+1);
}

void do_print(const char* title, tree_type& t)
{
    const auto line = std::string(30, '-') + "\n";
    std::cout << line << title << "\n" << line;
    dump_tree(t.debug_get_root());
    std::cout << line;
    for (const auto& e : t) {
        std::cout << e << std::endl;
    }
    std::cout << "\n\n";
}
#define PR(x) do { std::cout << #x " = " << x << std::endl; } while(0)
#undef PR

#include <vector>
using vt = std::vector<const test*>;

vt conv(tree_type& t)
{
    vt v;
    for (const auto& e : t) v.push_back(&e);
    return v;
}

template<typename... T>
vt V(const T&... ts) {
    return vt{&ts...};
}

#undef CONTAINING_RECORD
#define CATCH_CONFIG_MAIN
#include "catch.hpp"
TEST_CASE("tree") {
    test a{42}, b{12}, c{30}, d{7}, e{31};

    REQUIRE(V(a) == V(a));

    tree_type t;
    REQUIRE(conv(t) == V());

    t.insert(a);
    REQUIRE(conv(t) == V(a));

    t.insert(b);
    REQUIRE(conv(t) == V(b, a));

    t.insert(c);
    REQUIRE(conv(t) == V(b, c, a));

    t.insert(d);
    REQUIRE(conv(t) == V(d, b, c, a));

    t.insert(e);
    REQUIRE(conv(t) == V(d, b, c, e, a));

    SECTION("remove a") {
        t.remove(a);
        REQUIRE(conv(t) == V(d, b, c, e));
    }

    SECTION("remove b") {
        t.remove(b);
        REQUIRE(conv(t) == V(d, c, e, a));
    }

    SECTION("remove c") {
        t.remove(c);
        REQUIRE(conv(t) == V(d, b, e, a));
    }

    SECTION("remove d") {
        t.remove(d);
        REQUIRE(conv(t) == V(b, c, e, a));
    }

    SECTION("remove e") {
        t.remove(e);
        REQUIRE(conv(t) == V(d, b, c, a));
    }
}

TEST_CASE("more complicated removal") {
    test t5{5}, t9{9}, t12{12}, t13{13}, t14{14}, t16{16}, t16b{16}, t18{18}, t19{19};
    tree_type t;
    t.insert(t13);
    t.insert(t9);
    t.insert(t18);
    t.insert(t5);
    t.insert(t12);
    t.insert(t16);
    t.insert(t16b);
    t.insert(t14);
    t.insert(t19);
    REQUIRE(conv(t) == V(t5, t9, t12, t13, t14, t16, t16b, t18, t19));

    SECTION("remove t5") {
        t.remove(t5);
        REQUIRE(conv(t) == V(t9, t12, t13, t14, t16, t16b, t18, t19));
    }
    SECTION("remove t9") {
        t.remove(t9);
        REQUIRE(conv(t) == V(t5, t12, t13, t14, t16, t16b, t18, t19));
    }
    SECTION("remove t12") {
        t.remove(t12);
        REQUIRE(conv(t) == V(t5, t9, t13, t14, t16, t16b, t18, t19));
    }
    SECTION("remove t13") {
        t.remove(t13);
        REQUIRE(conv(t) == V(t5, t9, t12, t14, t16, t16b, t18, t19));
    }
    SECTION("remove t14") {
        t.remove(t14);
        REQUIRE(conv(t) == V(t5, t9, t12, t13, t16, t16b, t18, t19));
    }
    SECTION("remove t16") {
        t.remove(t16);
        REQUIRE(conv(t) == V(t5, t9, t12, t13, t14, t16b, t18, t19));
    }
    SECTION("remove t16b") {
        t.remove(t16b);
        REQUIRE(conv(t) == V(t5, t9, t12, t13, t14, t16, t18, t19));
    }
    SECTION("remove t18") {
        t.remove(t18);
        REQUIRE(conv(t) == V(t5, t9, t12, t13, t14, t16, t16b, t19));
    }
    SECTION("remove t19") {
        t.remove(t19);
        REQUIRE(conv(t) == V(t5, t9, t12, t13, t14, t16, t16b, t18));
    }
}

TEST_CASE("lower_bound") {
    test x3a{3}, x4a{4}, x4b{4}, x4c{4}, x4d{4}, x5a{5}, x7a{7}, x7b{7}, x7c{7}, x7d{7}, x8a{8};
    tree_type t;
//    x3a x4a x4b x4c x4d x5a x7a x7b x7c x7d x8a
    t.insert(x3a);
    t.insert(x4a);
    t.insert(x4b);
    t.insert(x4c);
    t.insert(x4d);
    t.insert(x5a);
    t.insert(x7a);
    t.insert(x7b);
    t.insert(x7c);
    t.insert(x7d);
    t.insert(x8a);
    REQUIRE(conv(t) == V(x3a, x4a, x4b, x4c, x4d, x5a, x7a, x7b, x7c, x7d, x8a));
#define RL(n, x) REQUIRE(&*t.lower_bound(test{n}) == &x)
#define RU(n, x) REQUIRE(&*t.upper_bound(test{n}) == &x)
    RL(2, x3a);
    RU(2, x3a);
    RL(4, x4a);
    RU(4, x5a);
    RL(6, x7a);
    RU(6, x7a);
    RL(8, x8a);
    REQUIRE(t.upper_bound(test{8}) == t.end());
    REQUIRE(t.lower_bound(test{9}) == t.end());
#undef RU
#undef RL
}

class memory_area {
private:
    uint64_t    addr_;
    uint64_t    length_;
    tree_node   link_;

public:
    explicit memory_area() : addr_(0), length_(0), link_() {
    }
    explicit memory_area(uint64_t addr, uint64_t length) : addr_(addr), length_(length), link_() {
        assert(length);
    }
    memory_area(const memory_area&) = delete;
    memory_area& operator=(const memory_area&) = delete;

    uint64_t address() const { return addr_; }
    uint64_t length() const { return length_; }

    struct compare {
        bool operator()(const memory_area& l, const memory_area& r) const {
            return l.addr_ < r.addr_;
        }
    };
    using tree_type = tree<memory_area, &memory_area::link_, compare>;
};

std::ostream& operator<<(std::ostream& os, memory_area::tree_type& t) {
    for (const auto& fl : t) {
        os << fl.address() << " " << fl.length() << "\n";
    }
    return os;
}

TEST_CASE("memory_area test") {
    memory_area area0{0, 1<<20};
    memory_area area1{1<<20, 15<<20};
    memory_area::tree_type free_list;
    memory_area::tree_type reserved_list;
    free_list.insert(area1);
    reserved_list.insert(area0);
    std::cout << free_list << "\n";
    std::cout << reserved_list << "\n";
}
