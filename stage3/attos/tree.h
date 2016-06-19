#ifndef ATTOS_TREE_H
#define ATTOS_TREE_H

namespace attos {

template<typename Container, typename T>
constexpr Container& containing_record(T& n, T Container::* field) {
    return *reinterpret_cast<Container*>(reinterpret_cast<char*>(&n) - (size_t)(char*)&(((Container*)0)->*field));
}

struct tree_node {
    tree_node* parent;
    tree_node* left;
    tree_node* right;
};

inline tree_node* minimum_node(tree_node* node) {
    while (node && node->left) {
        node = node->left;
    }
    return node;
}

inline tree_node* successor_node(tree_node* node) {
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
inline void update_parent_of(tree_node& removed_node, tree_node* replacement_node) {
    if (removed_node.parent) {
        if (removed_node.parent->left == &removed_node) {
            removed_node.parent->left = replacement_node;
        } else {
            assert(removed_node.parent->right == &removed_node);
            removed_node.parent->right = replacement_node;
        }
    }
}

inline void reparent_child_nodes(tree_node& n)
{
    if (n.left) n.left->parent = &n;
    if (n.right) n.right->parent = &n;
}

inline void check_tree(tree_node* root) {
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

        auto* operator->() {
            return &containing_record(*node_, Field);
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

    // Returns an iterator pointing to the first element that compares not less than key
    template<typename Cmp = Compare, typename K>
    iterator lower_bound(const K& k, Cmp compare = Cmp()) {
        tree_node* n = root_;
        tree_node* res = nullptr;
        while (n) {
            if (compare(containing_record(*n, Field), k)) {
                n = n->right; // n < k
            } else {
                res = n; // !(n < k), so this might be the result
                n = n->left;
            }
        }
        return iterator{res};
    }

    // Returns an iterator to the first element that is greater that the key
    template<typename Cmp = Compare, typename K>
    iterator upper_bound(const K& k, Cmp compare = Cmp()) {
        tree_node* n = root_;
        tree_node* res = nullptr;
        while (n) {
            if (compare(k, containing_record(*n, Field))) {
                res = n;  // k < n, best result so far
                n = n->left;
            } else {
                n = n->right;
            }
        }
        return iterator{res};
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

#endif
