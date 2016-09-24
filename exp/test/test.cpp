#include <assert.h>
#include <attos/containers.h>

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"
TEST_CASE("kvector<int>") {
    using vec = attos::kvector<int>;
    vec v;
    REQUIRE(v.empty());
    REQUIRE(v.size() == 0);

    v.push_back(42);
    REQUIRE(!v.empty());
    REQUIRE(v.size() == 1);
    REQUIRE(v.capacity() >= v.size());
    REQUIRE(v.front() == 42);
    REQUIRE(v.back() == 42);
    REQUIRE(v[0] == 42);
    REQUIRE(*v.begin() == 42);
    REQUIRE(*(v.end()-1) == 42);

    v.push_back(60);
    REQUIRE(!v.empty());
    REQUIRE(v.size() == 2);
    REQUIRE(v.capacity() >= v.size());
    REQUIRE(v.front() == 42);
    REQUIRE(v.back() == 60);
    REQUIRE(v[0] == 42);
    REQUIRE(v[1] == 60);
    REQUIRE(*v.begin() == 42);
    REQUIRE(*(v.begin()+1) == 60);
    REQUIRE(*(v.end()-2) == 42);
    REQUIRE(*(v.end()-1) == 60);

    v.push_back(80);
    REQUIRE(!v.empty());
    REQUIRE(v.size() == 3);
    REQUIRE(v.capacity() >= v.size());
    REQUIRE(v.front() == 42);
    REQUIRE(v.back() == 80);
    REQUIRE(v[0] == 42);
    REQUIRE(v[1] == 60);
    REQUIRE(v[2] == 80);

    SECTION("clear") {
        v.clear();
        REQUIRE(v.empty());
        REQUIRE(v.size() == 0);
    }

    SECTION("pop_back") {
        v.pop_back();
        REQUIRE(v.size() == 2);
        REQUIRE(v[0] == 42);
        REQUIRE(v[1] == 60);
    }

    SECTION("erase last") {
        v.erase(v.end()-1);
        REQUIRE(v.size() == 2);
        REQUIRE(v[0] == 42);
        REQUIRE(v[1] == 60);
    }

    SECTION("erase middle") {
        v.erase(v.begin()+1);
        REQUIRE(v.size() == 2);
        REQUIRE(v[0] == 42);
        REQUIRE(v[1] == 80);
    }

    SECTION("erase first") {
        v.erase(v.begin());
        REQUIRE(v.size() == 2);
        REQUIRE(v[0] == 60);
        REQUIRE(v[1] == 80);
    }

    SECTION("insert at end") {
        int ints[] = { 1, 2 };
        v.insert(v.end(), std::begin(ints), std::end(ints));
        REQUIRE(v.size() == 5);
        REQUIRE(v[0] == 42);
        REQUIRE(v[1] == 60);
        REQUIRE(v[2] == 80);
        REQUIRE(v[3] == 1);
        REQUIRE(v[4] == 2);
    }
}

struct movable_obj {
    explicit movable_obj(int id) : id(id) {
        ++count;
    }
    movable_obj(movable_obj&& rhs) : id(rhs.id) {
        ++count;
        rhs.id = -1;
    }
    ~movable_obj() {
        --count;
    }
    movable_obj& operator=(movable_obj&& rhs) {
        if (this != &rhs) {
            id = rhs.id;
            rhs.id = -1;
        }
        return *this;
    }

    int id;

    static int count;
};
int movable_obj::count = 0;

TEST_CASE("kvector<movable_obj>") {
    using vec = attos::kvector<movable_obj>;

    vec v;
    REQUIRE(v.empty());
    v.push_back(movable_obj{42});
    v.push_back(movable_obj{60});
    v.push_back(movable_obj{80});
    REQUIRE(movable_obj::count == 3);
    REQUIRE(v[0].id == 42);
    REQUIRE(v[1].id == 60);
    REQUIRE(v[2].id == 80);
    for (int i = 0; i < 100; ++i) {
        v.push_back(movable_obj{1000 + i});
    }
    REQUIRE(movable_obj::count == 103);
    REQUIRE(v.size() == 103);
    REQUIRE(v[102].id == 1099);
    REQUIRE(v.back().id == 1099);

    SECTION("clear") {
        v.clear();
        REQUIRE(movable_obj::count == 0);
    }

    SECTION("erase") {
        v.erase(v.begin());
        REQUIRE(movable_obj::count == 102);
        REQUIRE(v.begin()->id == v[0].id);
        REQUIRE(v[0].id == 60);
        REQUIRE(v.back().id == 1099);
    }
}
