#include <assert.h>

#define CONTAINING_RECORD(ptr, type, field) *reinterpret_cast<type*>(reinterpret_cast<char*>(ptr) - offsetof(type, field))

namespace attos {

template<typename Container, typename T>
inline constexpr Container& containing_record(T& n, T Container::* field) {
    return *reinterpret_cast<Container*>(reinterpret_cast<char*>(&n) - (size_t)(char*)&(((Container*)0)->*field));
}

struct tree_node {
    tree_node* parent;
    tree_node* left;
    tree_node* right;
};

tree_node* minimum_node(tree_node* node) {
    while (node && node->left) {
        node = node->left;
    }
    return node;
}

tree_node* successor_node(tree_node* node) {
    // We have visitied all the smaller (left) nodes
    // and `node' and want the successor
    if (!node) {
        return nullptr;
    }

    // If there is right child, advance to the smallest
    // node in that subtree
    if (node->right) {
        return minimum_node(node->right);
    }

    // Search upwards until we're our parent's left child
    tree_node* parent;
    while ((parent = node->parent) != nullptr && parent->right == node) {
        node = parent;
    }
    assert(!parent || parent->left == node);
    return parent;
}

namespace detail {

// Updates the parent of `removed_node' to point to `replacement_node'
void update_parent_of(tree_node& removed_node, tree_node* replacement_node) {
    if (removed_node.parent) {
        if (removed_node.parent->left == &removed_node) {
            removed_node.parent->left = replacement_node;
        } else {
            assert(removed_node.parent->right == &removed_node);
            removed_node.parent->right = replacement_node;
        }
    }
}

void reparent_child_nodes(tree_node& n)
{
    if (n.left) n.left->parent = &n;
    if (n.right) n.right->parent = &n;
}

void check_tree(tree_node* root) {
    if (!root) return;
    if (root->left) {
        assert(root->left->parent == root);
        check_tree(root->left);
    }
    if (root->right) {
        assert(root->right->parent == root);
        check_tree(root->right);
    }
}

} // namespace detail


template<typename Container, tree_node Container::*Field, typename Compare>
class tree {
public:
    explicit tree() {}
    ~tree() { detail::check_tree(root_); }

    void insert(Container& c) {
        do_insert(&root_, node(c));
    }

    tree_node* debug_get_root() { return root_; }

    class iterator {
    public:
        bool operator==(const iterator& rhs) const { return node_ == rhs.node_; }
        bool operator!=(const iterator& rhs) const { return !(*this == rhs); }

        iterator& operator++() {
            node_ = successor_node(node_);
            return *this;
        }

        auto& operator*() {
            return containing_record(*node_, Field);
        }

    private:
        explicit iterator(tree_node* node = nullptr) : node_(node) {
        }

        tree_node* node_;
        friend tree;
    };

    iterator begin() { return iterator{minimum_node(root_)}; }
    iterator end() { return iterator{}; }

    void remove(Container& c) {
        auto& node = tree::node(c);
        tree_node* replacement_node = nullptr;
        for (;;) {
            if (node.left) {
                if (node.right) {
                    // We have 2 children, swap places with our inorder successor and retry
                    auto successor = successor_node(&node);
                    assert(successor);

                    // Handle corner case
                    if (successor == node.right) {
                        // The successor is our right child
                        assert(successor->parent == &node);

                        // Update our parent
                        detail::update_parent_of(node, successor);
                        // The successor gets our parent
                        successor->parent = node.parent;

                        // The left child is easy
                        std::swap(node.left, successor->left);

                        // Grab the right child of the successor
                        node.right = successor->right;

                        // We're now the successors right child
                        successor->right = &node;

                    } else {
                        // The successor is some other node (it can't be our left child)
                        // and it can't be our parent, because the only way it can be our
                        // direct parent is if we don't have any right child (in case we
                        // wouldn't be in this code path)
                        assert(successor != node.left);
                        assert(successor != node.parent);

                        // Update the parents
                        detail::update_parent_of(*successor, &node);
                        detail::update_parent_of(node, successor);

                        // Swap children
                        std::swap(node.left, successor->left);
                        std::swap(node.right, successor->right);

                        // Swap parents
                        std::swap(node.parent, successor->parent);
                    }

                    // Update the children
                    detail::reparent_child_nodes(node);
                    detail::reparent_child_nodes(*successor);

                    // Check if we were the root node
                    if (&node == root_) {
                        root_ = successor;
                    }

                    // Continue search
                } else {
                    // Only left child
                    std::swap(replacement_node, node.left);
                    break;
                }
            } else if (node.right) {
                // Only right child
                std::swap(replacement_node, node.right);
                break;
            } else {
                // No children - Simply unhook from parent
                break;
            }
        }
        assert(node.left == nullptr);
        assert(node.right == nullptr);

        // Unlink from parent
        detail::update_parent_of(node, replacement_node);
        // Link new node to parent
        if (replacement_node) {
            replacement_node->parent = node.parent;
        }
        // Update root if necessary
        if (root_ == &node) {
            root_ = replacement_node;
        }
        node.parent = nullptr;
    }

private:
    tree_node* root_ = nullptr;

    static void do_insert(tree_node** root, tree_node& node) {
        assert(node.parent == nullptr);
        assert(node.left   == nullptr);
        assert(node.right  == nullptr);
        assert(root);

        Compare compare;
        tree_node* parent = nullptr;
        while (*root) {
            assert(root);
            parent = *root;
            root = compare(containing_record(node, Field), containing_record(*parent, Field)) ? &parent->left : &parent->right;
        }
        node.parent = parent;
        *root = &node;
    }

    static constexpr tree_node& node(Container& t) {
        return t.*Field;
    }
};

} // namespace attos

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
