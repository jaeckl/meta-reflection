#include <reflect/reflect.hpp>

#include "check.hpp"

#include <string>
#include <vector>

struct Range {
    int min;
    int max;
};
struct Doc {
    reflect::static_string text;
};

struct Api {
    int connect([[=Doc{"server name"}]] std::string host,
                [[=Range{1, 65535}, =Doc{"tcp port"}]] int port = 80, bool = false) {
        return port + int(host.size());
    }
    void ping() {}
    static double scale(double value, double factor = 2.0) { return value * factor; }
};

constexpr auto connect = reflect::get_function_of<Api, "connect">();
constexpr auto ping = reflect::get_function_of<Api, "ping">();
constexpr auto scale = reflect::get_function_of<Api, "scale">();

using Host = decltype(reflect::get_parameter_of<0>(connect));
using Port = decltype(reflect::get_parameter_of<1>(connect));
using Flag = decltype(reflect::get_parameter_of<2>(connect));

static_assert(std::same_as<Host::type, std::string>);
static_assert(std::same_as<Port::type, int>);
static_assert(Host::index() == 0 && Port::index() == 1);
static_assert(Host::has_name() && Host::name() == "host");
static_assert(Port::name() == "port");
static_assert(!Flag::has_name());
static_assert(!Host::has_default_argument());
static_assert(Port::has_default_argument() && Flag::has_default_argument());
static_assert(std::meta::is_function_parameter(Host::native()));

// Annotations: as members ...
static_assert(Port::has_annotation<Range>());
static_assert(!Host::has_annotation<Range>());
static_assert(Port::get_annotation<Range>().max == 65535);
static_assert(Host::get_annotation<Doc>().text == "server name");
static_assert(!Flag::find_annotation<Doc>().has_value());
static_assert(Port::find_annotation<Doc>()->text == "tcp port");
static_assert(Port::get_annotations<Doc>().size() == 1);
static_assert(Port::annotations().size() == 2);
static_assert(Flag::annotations().empty());

// ... and through the same free functions as every other proxy.
static_assert(reflect::has_annotation<Range>(Port{}));
static_assert(reflect::get_annotation<Range>(Port{}).min == 1);
static_assert(!reflect::find_annotation<Range>(Host{}));
static_assert(reflect::get_annotations_of(Port{}).size() == 2);

static_assert(reflect::get_parameter_names_of(connect).size() == 3);
static_assert(reflect::get_parameter_names_of(connect)[0] == "host");
static_assert(reflect::get_parameter_names_of(connect)[2].empty());
static_assert(reflect::get_parameter_names_of(ping).empty());
static_assert(reflect::get_parameter_names_of(scale)[1] == "factor");

int main() {
    std::vector<std::string> seen;
    reflect::for_each_parameter(connect, [&](auto parameter) {
        std::string line = std::to_string(parameter.index()) + ":";
        if constexpr (parameter.has_name())
            line += parameter.name();
        else
            line += "?";
        if constexpr (parameter.has_default_argument())
            line += "=default";
        if constexpr (reflect::has_annotation<Range>(parameter)) {
            constexpr Range range = reflect::get_annotation<Range>(parameter);
            line += "[" + std::to_string(range.min) + ".." + std::to_string(range.max) + "]";
        }
        seen.push_back(line);
    });
    CHECK((seen == std::vector<std::string>{"0:host", "1:port=default[1..65535]",
                                            "2:?=default"}));

    int count = 0;
    reflect::for_each_parameter(ping, [&](auto) { ++count; });
    CHECK(count == 0);

    // The parameters of every function of a class.
    std::string signature;
    reflect::for_each_function<Api>([&](auto function) {
        signature += function.name();
        signature += '(';
        reflect::for_each_parameter(function, [&](auto parameter) {
            if constexpr (parameter.index() != 0)
                signature += ',';
            if constexpr (std::same_as<typename decltype(parameter)::type, std::string>)
                signature += "string";
            else
                signature += reflect::name_of<typename decltype(parameter)::type>();
        });
        signature += ") ";
    });
    CHECK(signature == "connect(string,int,bool) ping() scale(double,double) ");

    std::string doc;
    reflect::for_each_annotation<Doc>(Port{}, [&](const Doc& d) { doc += d.text.view(); });
    CHECK(doc == "tcp port");

    return test::result();
}
