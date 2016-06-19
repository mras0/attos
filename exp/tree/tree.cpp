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

template<typename Extract, typename Compare>
class tree {
public:
    explicit tree() {}

    void insert(tree_node& node) {
        do_insert(&root_, node);
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

        auto operator*() {
            return Extract()(*node_);
        }

    private:
        explicit iterator(tree_node* node = nullptr) : node_(node) {
        }

        tree_node* node_;
        friend tree;
    };

    iterator begin() { return iterator{minimum_node(root_)}; }
    iterator end() { return iterator{}; }

private:
    tree_node* root_ = nullptr;

    static void do_insert(tree_node** root, tree_node& node) {
        assert(root);

        Compare compare;
        Extract extract;
        tree_node* parent = nullptr;
        while (*root) {
            assert(root);
            parent = *root;
            root = compare(extract(node), extract(*parent)) ? &parent->left : &parent->right;
        }
        node.parent = parent;
        *root = &node;
    }
};

std::ostream& operator<<(std::ostream& os, const tree_node& n) {
    return os << "tree_node{parent=" << n.parent << ", left=" << n.left << ", " << n.right << "}";
}

} // namespace attos


using namespace attos;

struct test {
    explicit test(int value) : value(value), node() {
    }

    int       value;
    tree_node node;
};

struct test_node_extract {
    test& operator()(tree_node& n) const {
        return CONTAINING_RECORD(&n, test, node);
    }
};

struct test_node_less {
    bool operator()(const test& l, const test& r) const {
        return l.value < r.value;
    }
};

std::ostream& operator<<(std::ostream& os, const test& t) {
    return os << "test{value=" << t.value << ", node=" << t.node << "}";
}

#include <string>

void dump_tree(tree_node* root, int indent = 0)
{
    std::cout << std::string(indent*2, ' ');
    if (!root) {
        std::cout << "(null)\n";
        return;
    }
    std::cout << test_node_extract()(*root) << "\n";
    dump_tree(root->left, indent+1);
    dump_tree(root->right, indent+1);
}

template<typename T>
void do_print(const char* title, T& t)
{
    const auto line = std::string(30, '-') + "\n";
    std::cout << line << title << line;
    dump_tree(t.debug_get_root());
    std::cout << line;
    for (const auto& e : t) {
        std::cout << e << std::endl;
    }
}

int main()
{
    test a{42}, b{12}, c{30};
#define PR(x) do { std::cout << #x " = " << x << std::endl; } while(0)
    PR(a);
    PR(b);
    PR(c);
    PR(CONTAINING_RECORD(&a.node, test, node));
#undef PR
    tree<test_node_extract, test_node_less> t;
    do_print("Initial\n", t);

    t.insert(a.node);
    do_print("A inserted\n", t);

    t.insert(b.node);
    do_print("B inserted\n", t);

    t.insert(c.node);
    do_print("C inserted\n", t);
}
