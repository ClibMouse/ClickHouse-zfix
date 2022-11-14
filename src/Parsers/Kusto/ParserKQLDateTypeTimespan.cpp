#include "Utilities.h"

#include <Parsers/ExpressionListParsers.h>
#include <Parsers/IParserBase.h>
#include <Parsers/Kusto/ParserKQLDateTypeTimespan.h>
#include <Parsers/Kusto/ParserKQLQuery.h>
#include <Common/StringUtils/StringUtils.h>

#include <boost/spirit/home/x3.hpp>

#include <cmath>
#include <cstdlib>
#include <format>
#include <type_traits>
#include <unordered_map>

namespace x3 = boost::spirit::x3;

namespace
{
enum class KQLTimespanUnit
{
    Day,
    Hour,
    Minute,
    Second,
    Millisecond,
    Microsecond,
    Nanosecond,
    Tick
};

template <typename T> concept arithmetic = std::is_arithmetic_v<T>;

template <arithmetic T>
Int64 kqlTimespanToTicks(const T value, const KQLTimespanUnit unit)
{
    static constexpr Int64 TICKS_PER_MICROSECOND = 10;
    static constexpr auto TICKS_PER_MILLISECOND = TICKS_PER_MICROSECOND * 1000;
    static constexpr auto TICKS_PER_SECOND = TICKS_PER_MILLISECOND * 1000;
    static constexpr auto TICKS_PER_MINUTE = TICKS_PER_SECOND * 60;
    static constexpr auto TICKS_PER_HOUR = TICKS_PER_MINUTE * 60;
    static constexpr auto TICKS_PER_DAY = TICKS_PER_HOUR * 24;

    switch (unit)
    {
        case KQLTimespanUnit::Day:
            return static_cast<Int64>(value * TICKS_PER_DAY);
        case KQLTimespanUnit::Hour:
            return static_cast<Int64>(value * TICKS_PER_HOUR);
        case KQLTimespanUnit::Minute:
            return static_cast<Int64>(value * TICKS_PER_MINUTE);
        case KQLTimespanUnit::Second:
            return static_cast<Int64>(value * TICKS_PER_SECOND);
        case KQLTimespanUnit::Millisecond:
            return static_cast<Int64>(value * TICKS_PER_MILLISECOND);
        case KQLTimespanUnit::Microsecond:
            return static_cast<Int64>(value * TICKS_PER_MICROSECOND);
        case KQLTimespanUnit::Tick:
            return static_cast<Int64>(value);
        case KQLTimespanUnit::Nanosecond:
            return static_cast<Int64>(value / 100);
    }
}

struct timespan_units_ : public x3::symbols<KQLTimespanUnit>
{
    timespan_units_()
    {
        add
            ("d", KQLTimespanUnit::Day)
            ("day", KQLTimespanUnit::Day)
            ("days", KQLTimespanUnit::Day)
            ("h", KQLTimespanUnit::Hour)
            ("hr", KQLTimespanUnit::Hour)
            ("hrs", KQLTimespanUnit::Hour)
            ("hour", KQLTimespanUnit::Hour)
            ("hours", KQLTimespanUnit::Hour)
            ("m", KQLTimespanUnit::Minute)
            ("min", KQLTimespanUnit::Minute)
            ("minute", KQLTimespanUnit::Minute)
            ("minutes", KQLTimespanUnit::Minute)
            ("s", KQLTimespanUnit::Second)
            ("sec", KQLTimespanUnit::Second)
            ("second", KQLTimespanUnit::Second)
            ("seconds", KQLTimespanUnit::Second)
            ("ms", KQLTimespanUnit::Millisecond)
            ("milli", KQLTimespanUnit::Millisecond)
            ("millis", KQLTimespanUnit::Millisecond)
            ("millisec", KQLTimespanUnit::Millisecond)
            ("millisecond", KQLTimespanUnit::Millisecond)
            ("milliseconds", KQLTimespanUnit::Millisecond)
            ("micro", KQLTimespanUnit::Microsecond)
            ("micros", KQLTimespanUnit::Microsecond)
            ("microsec", KQLTimespanUnit::Microsecond)
            ("microsecond", KQLTimespanUnit::Microsecond)
            ("microseconds", KQLTimespanUnit::Microsecond)
            ("nano", KQLTimespanUnit::Nanosecond)
            ("nanos", KQLTimespanUnit::Nanosecond)
            ("nanosec", KQLTimespanUnit::Nanosecond)
            ("nanosecond", KQLTimespanUnit::Nanosecond)
            ("nanoseconds", KQLTimespanUnit::Nanosecond)
            ("tick", KQLTimespanUnit::Tick)
            ("ticks", KQLTimespanUnit::Tick)
        ;
    }
};

const timespan_units_ timespan_units;

struct KQLTimespanComponents
{
    static constexpr auto MAX_SECONDS_FRACTIONAL = 10'000'000U;

    bool isValid() const { return hours < 24 && minutes < 60 && seconds < 60 && seconds_fractional < MAX_SECONDS_FRACTIONAL; }
    std::optional<Int64> toTicks() const
    {
        if (!isValid())
            return {};

        const auto sign = is_negative ? -1 : 1;
        auto seconds_fractional_in_ticks = seconds_fractional;
        while (seconds_fractional_in_ticks > 0 && seconds_fractional_in_ticks < (MAX_SECONDS_FRACTIONAL / 10))
            seconds_fractional_in_ticks *= 10;

        return sign * kqlTimespanToTicks(days, KQLTimespanUnit::Day) + kqlTimespanToTicks(hours, KQLTimespanUnit::Hour)
        + kqlTimespanToTicks(minutes, KQLTimespanUnit::Minute) + kqlTimespanToTicks(seconds, KQLTimespanUnit::Second)
        + kqlTimespanToTicks(seconds_fractional_in_ticks, KQLTimespanUnit::Tick);
    }

    bool is_negative = false;
    unsigned days = 0;
    unsigned hours = 0;
    unsigned minutes = 0;
    unsigned seconds = 0;
    unsigned seconds_fractional = 0;
};

using KQLTimespanValueWithUnit = std::pair<double, KQLTimespanUnit>;

using x3::double_;
using x3::lexeme;
using x3::lit;
using x3::int_;
using x3::uint_;
using x3::_val;
using x3::_attr;

const auto SET_DAYS = [](auto& ctx) { _val(ctx).days = _attr(ctx); };
const auto SET_HOURS_AND_MINUTES = [](auto& ctx)
{
    auto& kql_timespan_components = _val(ctx);
    const auto& attributes = _attr(ctx);
    kql_timespan_components.hours = at_c<0>(attributes);
    kql_timespan_components.minutes = at_c<1>(attributes);
};

const auto SET_NEGATIVE = [](auto& ctx) { _val(ctx).is_negative = true; };
const auto SET_SECONDS = [](auto& ctx) { _val(ctx).seconds = _attr(ctx); };
const auto SET_SECONDS_FRACTIONAL = [](auto& ctx) { _val(ctx).seconds_fractional = _attr(ctx); };

const x3::rule<class KQLTimespanLiteral, KQLTimespanComponents> KQL_TIMESPAN_SEPARATED_COMPONENTS = "KQL timespan separated components";
const auto KQL_TIMESPAN_SEPARATED_COMPONENTS_def =
    lexeme
    [
        -(lit('-')[SET_NEGATIVE] | lit('+'))
            >> -(uint_ >> lit('.'))[SET_DAYS]
            >> (uint_ >> lit(':') >> uint_)[SET_HOURS_AND_MINUTES]
            >> -(lit(':') >> uint_[SET_SECONDS] >> -(lit('.') >> uint_[SET_SECONDS_FRACTIONAL]))
    ];

const auto SET_VALUE_AND_UNIT = [](auto& ctx)
{
    const auto& value_and_unit = _attr(ctx);
    _val(ctx) = { at_c<0>(value_and_unit), at_c<1>(value_and_unit) };
};

const x3::rule<class KQLTimespanLiteral, KQLTimespanValueWithUnit> KQL_TIMESPAN_VALUE_WITH_UNIT = "KQL timespan value with unit";
const auto KQL_TIMESPAN_VALUE_WITH_UNIT_def = (double_ >> timespan_units)[SET_VALUE_AND_UNIT];

const x3::rule<class KQLTimespanLiteral, Int64> KQL_TIMESPAN_DAY_VALUE = "KQL timespan day value";
const auto KQL_TIMESPAN_DAY_VALUE_def = int_;

const x3::rule<class KQLTimespanLiteral, boost::variant<KQLTimespanComponents, KQLTimespanValueWithUnit, Int64>> KQL_TIMESPAN = "KQL timespan";
const auto KQL_TIMESPAN_def = KQL_TIMESPAN_SEPARATED_COMPONENTS | KQL_TIMESPAN_VALUE_WITH_UNIT | KQL_TIMESPAN_DAY_VALUE;

BOOST_SPIRIT_DEFINE(KQL_TIMESPAN_SEPARATED_COMPONENTS, KQL_TIMESPAN_VALUE_WITH_UNIT, KQL_TIMESPAN_DAY_VALUE, KQL_TIMESPAN);
}

namespace DB
{
bool ParserKQLDataTypeTimespan::parseImpl(Pos & pos, [[maybe_unused]] ASTPtr & node, [[maybe_unused]] Expected & expected)
{
    const auto token = extractTokenWithoutQuotes(pos);
    result = performParsing(token);
    return result.has_value();
}

std::optional<Int64> ParserKQLDataTypeTimespan::performParsing(const String & expression)
{
    auto first = expression.cbegin();
    auto last = expression.cend();
    
    boost::variant<KQLTimespanComponents, KQLTimespanValueWithUnit, Int64> kql_timespan_variant;
    const auto success = x3::parse(first, last, KQL_TIMESPAN, kql_timespan_variant);

    if (!success || first != last)
        return {};

    return boost::apply_visitor([](const auto& kql_timespan)
    {
        using Type = std::decay_t<decltype(kql_timespan)>;
        if constexpr (std::is_same_v<Type, KQLTimespanComponents>)
            return kql_timespan.toTicks();
        else if constexpr (std::is_same_v<Type, KQLTimespanValueWithUnit>)
            return kqlTimespanToTicks(kql_timespan.first, kql_timespan.second);
        else if constexpr (std::is_same_v<Type, Int64>)
            return kqlTimespanToTicks(kql_timespan, KQLTimespanUnit::Day);
    }, kql_timespan_variant);
}
}
