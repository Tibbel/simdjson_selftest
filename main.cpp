/**
 * compile with :
 *
 * g++ -Wall -O3 -std=c++17 main.cpp \
 * <...>/simdjson/singleheader/simdjson.cpp \
 * -I<...>/simdjson/
 * <...> replace with path to simdjson install dir
 *
**/
#include <iostream>
#include <string>
#include <algorithm>
#include <signal.h>

#include <singleheader/simdjson.h>

///// Config
#define NULL_AT_END
#define TYPE_ENUM_UNKNOWN

using namespace simdjson;


void segfault_sigaction(int /*signal*/, siginfo_t *si, void */*arg*/) {
    std::cout << "Caught segfault at address " <<  si->si_addr << std::endl;
    exit(-1);
}

void install_seg_handler() {
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(struct sigaction));
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction= segfault_sigaction;
    sa.sa_flags= SA_SIGINFO;
    sigaction(SIGSEGV, &sa, nullptr);
}


class ValueConst
{
    using t= simdjson::dom::element_type;
public:

    ValueConst( int64_t const i )
        : m_type{ t::INT64 }
        , m_data{ .l= i }
    {
    }

    ValueConst( ondemand::array const a )
        : m_type{ t::ARRAY }
        ,         m_data{ .a= a }
    {
    }

    ValueConst( ondemand::object const o )
        : m_type{ t::INT64 }
        , m_data{ .o= o }
    {
    }

    ValueConst( uint64_t const u )
        : m_type{ t::UINT64 }
        , m_data{ .u= u }
    {
    }

    ValueConst( double const d )
        : m_type{ t::DOUBLE }
        , m_data{ .d= d }
    {
    }

    ValueConst( bool const t )
        : m_type{ t::BOOL }
        , m_data{ .l= t }
    {
    }

    ValueConst( std::string_view const s )
        : m_type{ t::STRING }
        , m_data{ .s= s }
    {
    }

    ValueConst( void const*const /*ptr*/)
        : m_type{ t::NULL_VALUE }
        , m_data{ .ptr= nullptr }
    {
    }
#ifdef TYPE_ENUM_UNKNOWN
    ValueConst()
        : m_type{ t::UNKNOWN }
    {
    }
#endif

    t getType() const {return m_type; };

    bool getBool() const {
        if ( m_type != t::BOOL ) { throw __PRETTY_FUNCTION__; }
        return ( m_data.l != 0 );
    }

    int64_t getInt64() const {
        if ( m_type != t::INT64 ) { throw __PRETTY_FUNCTION__;  }
        return m_data.l;
    }

    uint64_t getUInt64() const {
        if ( m_type != t::UINT64 ) { throw __PRETTY_FUNCTION__; }
        return m_data.u;
    }

    uint64_t getDouble() const {
        if ( m_type != t::DOUBLE ) { throw __PRETTY_FUNCTION__; }
        return m_data.d;
    }

    std::string_view getStringView() const {
        if ( m_type != t::DOUBLE ) { throw __PRETTY_FUNCTION__; }
        return m_data.s;
    }

    std::string getString() const {
        if ( m_type != t::STRING ) {
            throw __PRETTY_FUNCTION__;
        }
        return std::string(m_data.s);
    }

    ondemand::array getArray() const {
        if ( m_type != t::ARRAY ) {
            throw __PRETTY_FUNCTION__;
        }
        return m_data.a;
    }

    ondemand::object getObject() const {
        if ( m_type != t::OBJECT ) {
            throw __PRETTY_FUNCTION__;
        }
        return m_data.o;
    }

    inline bool isNull() const noexcept { return ( m_type == t::NULL_VALUE ); }

#ifdef TYPE_ENUM_UNKNOWN
    inline bool hasType() const noexcept { return ( m_type != t::UNKNOWN ); }
    inline bool hasError() const noexcept { return ( m_type == t::UNKNOWN ); }
#endif

private:
    static constexpr uint64_t create_check( char const*const text )
    {
        using T= simdjson::dom::element_type;
        uint64_t ret= 0;
        size_t shift= 0;
        for ( size_t i= 0; i < 8; ++i )
        {
            char const c = text[i];
            auto const rt= static_cast<T>( c );
            switch( rt )
            {
            case T::ARRAY : [[fallthrough]];
            case T::STRING : [[fallthrough]];
            case T::UINT64: [[fallthrough]];
            case T::INT64: [[fallthrough]];
            case T::DOUBLE: [[fallthrough]];
            case T::OBJECT: [[fallthrough]];
            case T::BOOL: [[fallthrough]];
            case T::NULL_VALUE:
                ret += (static_cast<size_t>(c) << shift);
                shift += 8;
                break;
#ifdef TYPE_ENUM_UNKNOWN
            case T::UNKNOWN : [[fallthrough]];
#endif
            default:
                break;
            }
        }
        return ret;
    }

public:

    static ValueConst create( ondemand::value &val, char const*const text )
    {
        uint64_t const check= create_check( text );
        return create( val, check );
    }

    static ValueConst create( ondemand::value &val, uint64_t const check=0x6E74646C75737B5B )
    {
        using T= simdjson::dom::element_type;
#ifdef TYPE_ENUM_UNKNOWN
        ValueConst ret;
#else
        ValueConst ret{ nullptr };
#endif
#ifdef NULL_AT_END
        bool check_null = false;
#endif
        simdjson::error_code error = simdjson::INCORRECT_TYPE;
        for ( uint64_t run= check; run > 0 ; run >>= 8 )
        {
            //            ValueConst rt;
            char const c= 0xFF & run;
            ret.m_type= static_cast<T>( c );
            switch( ret.m_type )
            {
#ifdef TYPE_ENUM_UNKNOWN
            case T::UNKNOWN :   run = 0; break;
#endif
            case T::ARRAY : error= val.get( ret.m_data.a ); break;
            case T::STRING : error= val.get(ret.m_data.s); break;
            case T::UINT64: error= val.get(ret.m_data.u); break;
            case T::INT64: error= val.get(ret.m_data.l); break;
            case T::DOUBLE: error= val.get(ret.m_data.d); break;
            case T::OBJECT: error= val.get(ret.m_data.o); break;
            case T::BOOL: {
                bool t= false;
                error= val.get( t );
                ret.m_data.l = t;
                break;
            }
            case T::NULL_VALUE: {
#ifdef NULL_AT_END
                check_null = true;
#else
                bool const null= val.is_null();
                if ( null ) {
                    ret.m_type= t::NULL_VALUE;
                    ret.m_data.ptr= nullptr;
                }
#endif
                break;
            }

            default:
                run= 0;
                //throw std::string{"PANIC: Unknown Type"};
                break;
            }
            if ( error == simdjson::SUCCESS ){
                return ret;//run = 0;// break;
            }
        }
#ifdef NULL_AT_END
        if ( error != simdjson::SUCCESS && check_null == true ) {
            bool const null= val.is_null();
            if ( null ) {
                ret.m_type= T::NULL_VALUE;
                ret.m_data.ptr= nullptr;
            }
        }
#endif
        return ret;
    }

private:
    t m_type
#ifdef TYPE_ENUM_UNKNoWN
    = t::UNKNOWN
        #endif
            ;
    union type_u
    {
        void * ptr;
        //bool t;
        double d;
        std::string_view s;
        int64_t l;
        uint64_t u;
        ondemand::array a;
        ondemand::object o;
    };
    type_u m_data = {nullptr};
};


int test_order( simdjson::padded_string const& abstract_json, std::string const& order)
{

    ondemand::parser parser;

    auto doc = parser.iterate(abstract_json);

    ondemand::value val;
    simdjson::error_code error= simdjson::SUCCESS;
    doc["str"].tie(val, error);
    //char szc[9]= "[{\"uldtn";
    //    char szc[9]= "[{\"uldtn";
    auto const ele= ValueConst::create( val, order.c_str() );

    using t= simdjson::dom::element_type;
    switch ( ele.getType() )
    {
    case t::STRING :
        std::cout << ele.getString();
        break;
    case t::OBJECT: break;
    default :
        break;
    }

    return 0;
}



/**
 * @brief main
 * @param argc
 * @param argv
 * first integer to start position
 * second integer to end position
 * @return
 */
int main( int argc, char *argv[])
{
    install_seg_handler();

    int permutation_start= 0;
    int permutation_end= 0xFFFFFF;
    if ( argc >= 2 )
    {
        permutation_start= std::atol( argv[1] );
    }
    if ( argc >= 3 )
    {
        permutation_end= std::atol( argv[2] );
    }


    if ( permutation_start < 0 )
    {
        return 0;
    }

    //std::vector<simdjson::padded_string const> const vec= {
    /// @todo Loading test cases from json file would be better
    simdjson::padded_string const vec[]= {
        R"(
        { "str" : { "123" : 1 }, "1":"abc" }
        )"_padded,

        R"(
        { "str" : { "123" : -1 }, "1":"abc" }
        )"_padded,

        R"(
        { "str" : { "123" : -0.01 }, "1":"abc" }
        )"_padded,

        R"(
        { "str" : { "123" : { "abc" : 3.14 } }, "1":"abc" }
        )"_padded,

        R"(
        { "str" : { "123" : [ 1, 2, 3 ], "1":"abc" }
        )"_padded,

        R"(
        { "str" : { "123" : null, "1":"abc" }
        )"_padded,

        R"(
        { "str" : { "123" : true, "1":"abc" }
        )"_padded,

        R"(
        { "str" : { "123" : false, "1":"abc" }
        )"_padded,

        R"(
        { "str" : { "123" : "string", "1":"abc" }
        )"_padded,

    };

    std::string s = "[{\"uldtn";
    std::sort(s.begin(), s.end());
    try {
        int i= 0;
        do {
            ++i;
            if ( i > permutation_end ) {
                break;
            } else if ( i < permutation_start ) {
                continue;
            }
            //            for ( auto const &it : vec ) {
            auto const n= sizeof(vec) / sizeof(simdjson::padded_string);
            for ( size_t i= 0; i < n ; ++i ) {
                std::cout << s << "\t" << i << std::endl;//'\n';
                test_order( vec[i], s );
                std::cout << "passed\n";
            }
        } while(std::next_permutation(s.begin(), s.end()));
        return 0;
    } catch ( std::exception const &e ){
        std::cout << e.what() << std::endl;
    } catch ( std::string const &e ){
        std::cout << e << std::endl;
    } catch (...) {
        std::cout << "Unknown exception" << std::endl;
    }

    return -1;
}


