// SQL statements generated from annotated structs.
//
// The table name and the column details live on the struct. The two
// generators below work for any struct that carries a Table annotation.

#include <reflect/reflect.hpp>

#include <iostream>
#include <string>
#include <type_traits>

struct Table {
    reflect::static_string name;
};
struct Column {
    reflect::static_string name;
};
struct PrimaryKey {};
struct Transient {};  // not stored

struct [[=Table{"users"}]] User {
    [[=PrimaryKey{}]] int id;
    [[=Column{"display_name"}]] std::string name;
    double balance;
    bool active;
    [[=Transient{}]] std::string session_token;
};

struct [[=Table{"products"}]] Product {
    [[=PrimaryKey{}]] long sku;
    std::string title;
};

template <typename T>
constexpr const char* sql_type() {
    if constexpr (std::is_same_v<T, bool>)
        return "BOOLEAN";
    else if constexpr (std::is_integral_v<T>)
        return "INTEGER";
    else if constexpr (std::is_floating_point_v<T>)
        return "REAL";
    else
        return "TEXT";
}

// Only types that opted in with a Table annotation are accepted.
template <reflect::annotated_with<Table> T>
std::string create_table() {
    std::string sql = "CREATE TABLE ";
    sql += reflect::get_annotation<Table, T>().name.view();
    sql += " (";

    bool first = true;
    // without<Transient>() drops the fields that should not be stored.
    reflect::get_fields_of<T>().template without<Transient>().for_each([&](auto field) {
        // Column{} is optional; the member name is the fallback.
        Column column = reflect::get_annotation_or<Column>(field, Column{field.name()});

        sql += first ? "" : ", ";
        sql += column.name.view();
        sql += ' ';
        sql += sql_type<typename decltype(field)::type>();
        if constexpr (reflect::has_annotation<PrimaryKey>(field))
            sql += " PRIMARY KEY";
        first = false;
    });
    return sql + ");";
}

template <reflect::annotated_with<Table> T>
std::string insert_statement() {
    constexpr auto columns = reflect::get_fields_of<T>().template without<Transient>();

    std::string names;
    std::string marks;
    columns.for_each([&](auto field) {
        names += names.empty() ? "" : ", ";
        names += reflect::get_annotation_or<Column>(field, Column{field.name()}).name.view();
        marks += marks.empty() ? "?" : ", ?";
    });

    std::string sql = "INSERT INTO ";
    sql += reflect::get_annotation<Table, T>().name.view();
    return sql + " (" + names + ") VALUES (" + marks + ");";
}

int main() {
    std::cout << create_table<User>() << '\n';
    std::cout << insert_statement<User>() << '\n';
    std::cout << create_table<Product>() << '\n';
    std::cout << insert_statement<Product>() << '\n';
}
