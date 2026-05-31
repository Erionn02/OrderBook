#include <meta>
#include <iostream>

#include <simd>

struct User {
    std::string name;
    int age;
    bool is_active;
};

template<typename T>
consteval decltype(auto) getMembers() {
    return std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()));
}

template <typename T>
void print_struct_fields(const T& obj) {
    std::cout << "Iterating over struct: "
              << std::meta::identifier_of(^^T) << "\n";

    // Wrap the vector returning function in std::define_static_array
    template for (constexpr auto member : getMembers<T>()) {
        std::cout << "  - " << std::meta::identifier_of(member)
                  << ":+ " << obj.[:member:] << '\n';
    }
}


int main() {
    User my_user{"Alice", 28, true};

    print_struct_fields(my_user);


    return 0;
}
