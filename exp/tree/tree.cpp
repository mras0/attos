#include <iostream>
#include <assert.h>

#include <cstddef> // offsetof
#define CONTAINING_RECORD(ptr, type, field) *reinterpret_cast<type*>(reinterpret_cast<char*>(ptr) - offsetof(type, field))

namespace attos {

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


template<typename NodeTraits>
class tree {
public:
    explicit tree() {}
    ~tree() { check_tree(root_); }

    using container_type = decltype(NodeTraits::container(std::declval<tree_node&>()));

    void insert(container_type& c) {
        do_insert(&root_, NodeTraits::node(c));
        check_tree(root_);
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
            return NodeTraits::container(*node_);
        }

    private:
        explicit iterator(tree_node* node = nullptr) : node_(node) {
        }

        tree_node* node_;
        friend tree;
    };

    iterator begin() { return iterator{minimum_node(root_)}; }
    iterator end() { return iterator{}; }

    void remove(container_type& c) {
        auto& node = NodeTraits::node(c);
        tree_node* replacement_node = nullptr;
        for (;;) {
            std::cout << "Removing node " << c << std::endl;
            if (node.left) {
                if (node.right) {
                    // We have 2 children, swap places with our inorder successor and retry
                    auto successor = successor_node(&node);
                    assert(successor);
                    std::cout << "Replace with " << NodeTraits::container(*successor) << std::endl;

                    // Handle corner case
                    if (successor == node.right) {
                        // The successor is our right child
                        assert(successor->parent == &node);

                        // Update our parent
                        update_parent_of(node, successor);
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
                        update_parent_of(*successor, &node);
                        update_parent_of(node, successor);

                        // Swap children
                        std::swap(node.left, successor->left);
                        std::swap(node.right, successor->right);

                        // Swap parents
                        std::swap(node.parent, successor->parent);
                    }

                    // Update the children
                    reparent_child_nodes(node);
                    reparent_child_nodes(*successor);

                    // Check if we were the root node
                    if (&node == root_) {
                        root_ = successor;
                    }

                    check_tree(root_);

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
        update_parent_of(node, replacement_node);
        // Link new node to parent
        if (replacement_node) {
            replacement_node->parent = node.parent;
        }
        // Update root if necessary
        if (root_ == &node) {
            root_ = replacement_node;
        }
        node.parent = nullptr;
        check_tree(root_);
    }

private:
    tree_node* root_ = nullptr;

    static void do_insert(tree_node** root, tree_node& node) {
        assert(node.parent == nullptr);
        assert(node.left   == nullptr);
        assert(node.right  == nullptr);
        assert(root);

        tree_node* parent = nullptr;
        while (*root) {
            assert(root);
            parent = *root;
            root = NodeTraits::compare(NodeTraits::container(node), NodeTraits::container(*parent)) ? &parent->left : &parent->right;
        }
        node.parent = parent;
        *root = &node;
    }
};

std::ostream& operator<<(std::ostream& os, const tree_node& n) {
    return os << "tree_node{" << &n << ", parent=" << n.parent << ", left=" << n.left << ", " << n.right << "}";
}

} // namespace attos


using namespace attos;

struct test {
    explicit test(int value = 0) : value(value), node() {
    }
    test(const test&) = delete;
    test& operator=(const test&) = delete;

    int       value;
    tree_node node;
};

struct test_node_traits {
    static tree_node& node(test& t) {
        return t.node;
    }
    static test& container(tree_node& n) {
        return CONTAINING_RECORD(&n, test, node);
    }
    static bool compare(const test& l, const test& r) {
        return l.value < r.value;
    }
};

std::ostream& operator<<(std::ostream& os, const test& t) {
    return os << "test{value=" << t.value << ", node=" << t.node << "}";
}

using tree_type = tree<test_node_traits>;

#include <string>

void dump_tree(tree_node* root, int indent = 0)
{
    std::cout << std::string(indent*2, ' ');
    if (!root) {
        std::cout << "(null)\n";
        return;
    }
    std::cout << test_node_traits::container(*root) << "\n";
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

std::ostream& operator<<(std::ostream& os, const vt& v) {
    os << "{";
    for (const auto& e : v) os << " " << e->value;
    return os << " }";
}

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
