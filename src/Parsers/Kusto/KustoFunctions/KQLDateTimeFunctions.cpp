#include <Parsers/IParserBase.h>
#include <Parsers/ParserSetQuery.h>
#include <Parsers/ASTExpressionList.h>
#include <Parsers/ASTSelectWithUnionQuery.h>
#include <Parsers/Kusto/ParserKQLQuery.h>
#include <Parsers/Kusto/ParserKQLStatement.h>
#include <Parsers/Kusto/KustoFunctions/IParserKQLFunction.h>
#include <Parsers/Kusto/KustoFunctions/KQLDateTimeFunctions.h>
#include <Parsers/Kusto/KustoFunctions/KQLStringFunctions.h>
#include <Parsers/Kusto/KustoFunctions/KQLDynamicFunctions.h>
#include <Parsers/Kusto/KustoFunctions/KQLCastingFunctions.h>
#include <Parsers/Kusto/KustoFunctions/KQLAggregationFunctions.h>
#include <Parsers/Kusto/KustoFunctions/KQLTimeSeriesFunctions.h>
#include <Parsers/Kusto/KustoFunctions/KQLIPFunctions.h>
#include <Parsers/Kusto/KustoFunctions/KQLBinaryFunctions.h>
#include <Parsers/Kusto/KustoFunctions/KQLGeneralFunctions.h>
#include <format>
#include <regex>

namespace DB::ErrorCodes
{
extern const int SYNTAX_ERROR;
}
namespace DB
{
bool Ago::convertImpl(String & out, IParser::Pos & pos)
{
   const String fn_name = getKQLFunctionName(pos);
    if (fn_name.empty())
        return false;

    ++pos;
    if (pos->type != TokenType::ClosingRoundBracket)
    {
        const auto offset = getConvertedArgument(fn_name, pos);
        out = std::format("now64(9,'UTC') - {}", offset);
    }
    else
        out = "now64(9,'UTC')";
    return true;
}

bool DatetimeAdd::convertImpl(String & out, IParser::Pos & pos)
{
    const String fn_name = getKQLFunctionName(pos);
    if (fn_name.empty())
        return false;

    ++pos;
    String period = getConvertedArgument(fn_name, pos);
    //remove quotes from period.
    trim(period);
    if (period.front() == '\"' || period.front() == '\'')
    {
        //period.remove
        period.erase( 0, 1); // erase the first quote
        period.erase( period.size() - 1); // erase the last quote
    }
    ++pos;
    const String offset = getConvertedArgument(fn_name, pos);
    ++pos;
    const String datetime = getConvertedArgument(fn_name, pos);
    
    out = std::format("date_add({}, {}, {})",period,offset,datetime);

    return true;
   
};

bool DatetimePart::convertImpl(String & out, IParser::Pos & pos)
{
    const String fn_name = getKQLFunctionName(pos);
    if (fn_name.empty())
        return false;

    ++pos;
    String part = Poco::toUpper(getConvertedArgument(fn_name, pos));
    trim(part);
    if (part.front() == '\"' || part.front() == '\'')
    {
        //period.remove
        part.erase( 0, 1); // erase the first quote
        part.erase( part.size() - 1); // erase the last quote
    }
    String date;
    if (pos->type == TokenType::Comma)
    {
         ++pos;
         date = getConvertedArgument(fn_name, pos);
    }
    String format;
    
    if (part == "YEAR")
        format = "%G";
    else if (part == "QUARTER") 
        format = "%Q";
    else if (part == "MONTH")
        format = "%m";
    else if (part == "WEEK_OF_YEAR")
        format = "%V";
    else if (part == "DAY")
        format = "%e";
    else if (part == "DAYOFYEAR")
        format = "%j";
    else if (part == "HOUR")
        format = "%I";
    else if (part  == "MINUTE")
        format = "%M";
    else if (part == "SECOND")
        format = "%S";
    else 
        throw Exception("Unexpected argument " + part + " for " + fn_name, ErrorCodes::SYNTAX_ERROR);

    out = std::format("formatDateTime({}, '{}')", date, format);
    return true;
}

bool DatetimeDiff::convertImpl(String & out, IParser::Pos & pos)
{
    const String fn_name = getKQLFunctionName(pos);
    if (fn_name.empty())
        return false;

    const auto period = getArgument(fn_name, pos);
    const auto datetime_lhs = getArgument(fn_name, pos);
    const auto datetime_rhs = getArgument(fn_name, pos);
    out = std::format("dateDiff({}, {}, {})", period, datetime_rhs, datetime_lhs);

    return true;
}

bool DayOfMonth::convertImpl(String & out, IParser::Pos & pos)
{
    return directMapping(out, pos, "toDayOfMonth");
}

bool DayOfWeek::convertImpl(String & out, IParser::Pos & pos)
{
    const auto fn_name = getKQLFunctionName(pos);
    if (fn_name.empty())
        return false;

    const auto datetime = getArgument(fn_name, pos);
    out = std::format("(toDayOfWeek({}) % 7) * {}", datetime, kqlCallToExpression("timespan", {"1d"}, pos.max_depth));

    return true;
}

bool DayOfYear::convertImpl(String & out, IParser::Pos & pos)
{
    return directMapping(out, pos, "toDayOfYear");
}

bool EndOfMonth::convertImpl(String & out, IParser::Pos & pos)
{

    const String fn_name = getKQLFunctionName(pos);
    if (fn_name.empty())
        return false;

    ++pos;
    const String datetime_str = getConvertedArgument(fn_name, pos);
    String offset = "0";

    if (pos->type == TokenType::Comma)
    {
         ++pos;
         offset = getConvertedArgument(fn_name, pos);
         if(offset.empty())
             throw Exception("Number of arguments do not match in function:" + fn_name, ErrorCodes::SYNTAX_ERROR);
    }
    out = std::format("toDateTime(toLastDayOfMonth(toDateTime({}, 9, 'UTC') + toIntervalMonth({})), 9, 'UTC') + toIntervalHour(23) + toIntervalMinute(59) + toIntervalSecond(60) - toIntervalMicrosecond(1)", datetime_str, toString(offset));
    
    return true;
}

bool EndOfDay::convertImpl(String & out, IParser::Pos & pos)
{

    const String fn_name = getKQLFunctionName(pos);
    if (fn_name.empty())
        return false;

    ++pos;
    const String datetime_str = getConvertedArgument(fn_name, pos);
    String offset = "0";

    if (pos->type == TokenType::Comma)
    {
         ++pos;
         offset = getConvertedArgument(fn_name, pos);
    }
    out = std::format("toDateTime(toStartOfDay({}),9,'UTC') + (INTERVAL {} +1 DAY) - (INTERVAL 1 microsecond)", datetime_str, toString(offset));

    return true;
   
}

bool EndOfWeek::convertImpl(String & out, IParser::Pos & pos)
{
    
    const String fn_name = getKQLFunctionName(pos);
    if (fn_name.empty())
        return false;

    ++pos;
    const String datetime_str = getConvertedArgument(fn_name, pos);
    String offset = "0";

    if (pos->type == TokenType::Comma)
    {
         ++pos;
         offset = getConvertedArgument(fn_name, pos);
    }
        out = std::format("toDateTime(toStartOfDay({}),9,'UTC') + (INTERVAL {} +1 WEEK) - (INTERVAL 1 microsecond)", datetime_str, toString(offset));

    return true;  
}

bool EndOfYear::convertImpl(String & out, IParser::Pos & pos)
{
    const String fn_name = getKQLFunctionName(pos);
    if (fn_name.empty())
        return false;

    ++pos;
    const String datetime_str = getConvertedArgument(fn_name, pos);

    if(datetime_str.empty())
        throw Exception("Number of arguments do not match in function:" + fn_name, ErrorCodes::SYNTAX_ERROR);

    String offset = "0";
    if (pos->type == TokenType::Comma)
    {
         ++pos;
         offset = getConvertedArgument(fn_name, pos);
         if(offset.empty())
            throw Exception("Number of arguments do not match in function:" + fn_name, ErrorCodes::SYNTAX_ERROR);
         offset.erase(remove(offset.begin(), offset.end(), ' '), offset.end());
    }

        out = std::format("(((((toDateTime(toString(toLastDayOfMonth(toDateTime({0}, 9, 'UTC') + toIntervalYear({1}) + toIntervalMonth(12 - toInt8(substring(toString(toDateTime({0}, 9, 'UTC')), 6, 2))))), 9, 'UTC') + toIntervalHour(23)) + toIntervalMinute(59)) + toIntervalSecond(60)) - toIntervalMicrosecond(1)))", datetime_str, toString(offset));

    return true;
}

bool FormatDateTime::convertImpl(String & out, IParser::Pos & pos)
{
    const String fn_name = getKQLFunctionName(pos);
    if (fn_name.empty())
        return false;
    String formatspecifier;
    ++pos;
    const auto datetime = getConvertedArgument(fn_name, pos);
    ++pos;
    auto format = getConvertedArgument(fn_name, pos);
    trim(format);

    //remove quotes and end space from format argument. 
    if (format.front() == '\"' || format.front() == '\'')
    {
        format.erase( 0, 1); // erase the first quote
        format.erase( format.size() - 1); // erase the last quote
    }

    std::vector<String> res;    
    getTokens(format, res);
    std::string::size_type i = 0;
    size_t decimal =0;
    while (i < format.size())
    {
        char c = format[i];
        if (!isalpha(c))
        {
            //delimeter 
            if (c == ' ' || c == '-' || c == '_' || c == '[' || c == ']' || c == '/' || c == ',' || c == '.' || c == ':')
                formatspecifier = formatspecifier + c;
            else
                throw Exception("Invalid format delimeter in function:" + fn_name, ErrorCodes::SYNTAX_ERROR);
            ++i;
        }
        else
        {
            //format specifier
            String arg = res.back();
           
            if (arg == "y" || arg == "yy")
              formatspecifier = formatspecifier + "%y";
            else if (arg == "yyyy")
                formatspecifier = formatspecifier + "%Y";
            else if (arg == "M" || arg == "MM")
                formatspecifier = formatspecifier + "%m";
            else if (arg == "s" || arg == "ss")
                formatspecifier = formatspecifier + "%S";
            else if (arg == "m" || arg == "mm")
                formatspecifier = formatspecifier + "%M";
            else if (arg == "h" || arg == "hh")
                formatspecifier = formatspecifier + "%I";
            else if (arg == "H" || arg == "HH")
                formatspecifier = formatspecifier + "%H";
            else if (arg == "d")
                formatspecifier = formatspecifier + "%e";
            else if (arg == "dd")
                formatspecifier = formatspecifier + "%d";
            else if (arg == "tt")
                formatspecifier = formatspecifier + "%p";
            else if (arg.starts_with('f') || arg.starts_with('F'))
                decimal = arg.size();
            else 
                throw Exception("Format specifier " + arg + " in function:" + fn_name + "is not supported", ErrorCodes::SYNTAX_ERROR);
            res.pop_back();
            i = i + arg.size();
        } 
    }
    if (decimal > 0 && formatspecifier.find('.') != String::npos)
    {   
    
    out = std::format("concat("
        "substring(toString(formatDateTime( {0} , '{1}')),1, position(toString(formatDateTime({0},'{1}')),'.')) ,"
        "substring(substring(toString({0}), position(toString({0}),'.')+1),1,{2}),"
        "substring(toString(formatDateTime( {0},'{1}')), position(toString(formatDateTime({0},'{1}')),'.')+1 ,length (toString(formatDateTime({0},'{1}'))))) ", datetime, formatspecifier,decimal);
    }
    else
        out = std::format("formatDateTime( {0},'{1}')",datetime, formatspecifier);
    
    return true;
}

bool FormatTimeSpan::convertImpl(String & out, IParser::Pos & pos)
{
    static const std::unordered_set<char> ALLOWED_DELIMITERS{' ', '/', '-', ':', ',', '.', '_', '[', ']'};
    static const std::unordered_map<char, std::tuple<std::string_view, std::optional<int>, bool, int, std::optional<std::string_view>>>
        ATTRIBUTES_BY_FORMAT_CHARACTER{
            {'d', {"1d", std::nullopt, false, 8, "leftPad"}},
            {'f', {"1tick", 10'000'000, true, 7, "rightPad"}},
            {'F', {"1tick", 10'000'000, true, 7, std::nullopt}},
            {'h', {"1h", 24, false, 2, "leftPad"}},
            {'H', {"1h", 24, false, 2, "leftPad"}},
            {'m', {"1m", 60, false, 2, "leftPad"}},
            {'s', {"1s", 60, false, 2, "leftPad"}}};

    const auto fn_name = getKQLFunctionName(pos);
    if (fn_name.empty())
        return false;

    const auto timespan = getArgument(fn_name, pos);
    const auto format = getArgument(fn_name, pos);
    if (std::ssize(format) < 3 || format.front() != format.back() || format.front() != '\'')
        throw Exception(ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT, "Expected non-empty string literal as the second argument to {}", fn_name);

    std::string current_streak;
    std::string delimited_parts;
    const auto convert_streak = [&current_streak, &timespan, &delimited_parts, &pos]
    {
        while (!current_streak.empty())
        {
            if (!delimited_parts.empty())
                delimited_parts.append(", ");

            const auto attributes_it = ATTRIBUTES_BY_FORMAT_CHARACTER.find(current_streak.front());
            if (attributes_it == ATTRIBUTES_BY_FORMAT_CHARACTER.cend())
                throw Exception(ErrorCodes::LOGICAL_ERROR, "Unexpected format character: {}", current_streak.front());

            const auto & [timespan_unit, modulus, should_truncate, max_length, pad_function] = attributes_it->second;
            const auto streak_length = std::ssize(current_streak);
            const auto part_length = std::min(streak_length, static_cast<std::ptrdiff_t>(max_length));
            current_streak.erase(current_streak.cbegin(), current_streak.cbegin() + part_length);

            auto expression = std::format("intDiv({}, {})", timespan, kqlCallToExpression("timespan", {timespan_unit}, pos.max_depth));
            expression = std::format("toString({})", modulus ? std::format("modulo({}, {})", expression, *modulus) : expression);
            if (should_truncate)
                expression = std::format("substring({}, 1, {})", expression, part_length);

            delimited_parts.append(
                pad_function ? std::format("if(length({1}) < {2}, {0}({1}, {2}, '0'), {1})", *pad_function, expression, part_length)
                             : expression);
        }
    };

    for (const auto & c : std::string_view(format.cbegin() + 1, format.cend() - 1))
    {
        if (ALLOWED_DELIMITERS.contains(c))
        {
            convert_streak();
            delimited_parts.append(std::format(", '{}'", c));
        }
        else if (ATTRIBUTES_BY_FORMAT_CHARACTER.contains(c))
        {
            if (!current_streak.empty() && current_streak.back() != c)
                convert_streak();

            current_streak.push_back(c);
        }
        else
            throw Exception(ErrorCodes::BAD_ARGUMENTS, "Unexpected character '{}' in format string of {}", c, fn_name);
    }

    convert_streak();
    out = "concat(" + delimited_parts + ", '')";
    return true;
}

bool GetMonth::convertImpl(String & out, IParser::Pos & pos)
{
  return directMapping(out, pos, "toMonth");
}

bool GetYear::convertImpl(String & out, IParser::Pos & pos)
{
   return directMapping(out, pos, "toYear");
}

bool HoursOfDay::convertImpl(String & out, IParser::Pos & pos)
{
     return directMapping(out, pos, "toHour");
}

bool MakeTimeSpan::convertImpl(String & out, IParser::Pos & pos)
{
     const String fn_name = getKQLFunctionName(pos);
     if (fn_name.empty())
         return false;

    const auto arg1 = getArgument(fn_name, pos);
    const auto arg2 = getArgument(fn_name, pos);
    const auto arg3 = getOptionalArgument(fn_name, pos);
    const auto arg4 = getOptionalArgument(fn_name, pos);

    const auto & [day, hour, minute, second]
        = std::invoke([&arg1, &arg2, &arg3, &arg4]
                      { return arg4 ? std::make_tuple(arg1, arg2, *arg3, *arg4) : std::make_tuple("0", arg1, arg2, arg3.value_or("0")); });

    out = std::format(
        "{} * {} + {} * {} + {} * {} + {} * {}",
        day,
        kqlCallToExpression("timespan", {"1d"}, pos.max_depth),
        hour,
        kqlCallToExpression("timespan", {"1h"}, pos.max_depth),
        minute,
        kqlCallToExpression("timespan", {"1m"}, pos.max_depth),
        second,
        kqlCallToExpression("timespan", {"1s"}, pos.max_depth));

    return true;
}

bool MakeDateTime::convertImpl(String & out, IParser::Pos & pos)
{
    const String fn_name = getKQLFunctionName(pos);

    if (fn_name.empty())
        return false;

    ++pos;
    String arguments;
    int arg_count = 0;
    
    while (!pos->isEnd() && pos->type != TokenType::ClosingRoundBracket)
    {
        String arg = getConvertedArgument(fn_name, pos);
        if (pos->type == TokenType::Comma)
            ++pos;
        arguments = arguments  + arg + ",";
        ++arg_count;
    }
    
    if (arg_count < 1 || arg_count > 7)
        throw Exception("argument count out of bound in function: " + fn_name, ErrorCodes::SYNTAX_ERROR);
    
    if (arg_count < 7)
    {
        for (int i = arg_count;i < 7; ++i)
            arguments = arguments + "0 ,";
    }    

    arguments = arguments + "7,'UTC'";
    out = std::format("makeDateTime64({})",arguments);

    return true;
}

bool Now::convertImpl(String & out, IParser::Pos & pos)
{
    const String fn_name = getKQLFunctionName(pos);
    if (fn_name.empty())
        return false;

    ++pos;
    if (pos->type != TokenType::ClosingRoundBracket)
    {
        const auto offset = getConvertedArgument(fn_name, pos);
        out = std::format("now64(9,'UTC') + {}", offset);
    }
    else
        out = "now64(9,'UTC')";
    return true;
}

bool StartOfDay::convertImpl(String & out, IParser::Pos & pos)
{
    const String fn_name = getKQLFunctionName(pos);

    if (fn_name.empty())
        return false;

    ++pos;
    const String datetime_str = getConvertedArgument(fn_name, pos);
    String offset = "0";

    if (pos->type == TokenType::Comma)
    {
         ++pos;
         offset = getConvertedArgument(fn_name, pos);

    }
    out = std::format("date_add(DAY,{}, parseDateTime64BestEffortOrNull(toString((toStartOfDay({}))) , 9 , 'UTC')) ", offset, datetime_str);
    return true;
}

bool StartOfMonth::convertImpl(String & out, IParser::Pos & pos)
{
    const String fn_name = getKQLFunctionName(pos);
    if (fn_name.empty())
        return false;

    ++pos;
    const String datetime_str = getConvertedArgument(fn_name, pos);
    String offset = "0";

    if (pos->type == TokenType::Comma)
    {
         ++pos;
         offset = getConvertedArgument(fn_name, pos);

    }
    out = std::format("date_add(MONTH,{}, parseDateTime64BestEffortOrNull(toString((toStartOfMonth({}))) , 9 , 'UTC')) ", offset, datetime_str);
    return true;
}

bool StartOfWeek::convertImpl(String & out, IParser::Pos & pos)
{
    const String fn_name = getKQLFunctionName(pos);
    if (fn_name.empty())
        return false;

    ++pos;
    const String datetime_str = getConvertedArgument(fn_name, pos);
    String offset = "0";

    if (pos->type == TokenType::Comma)
    {
         ++pos;
         offset = getConvertedArgument(fn_name, pos);

    }  
    out = std::format("date_add(Week,{}, parseDateTime64BestEffortOrNull(toString((toStartOfWeek({}))) , 9 , 'UTC')) ", offset, datetime_str);
    return true;
}

bool StartOfYear::convertImpl(String & out, IParser::Pos & pos)
{
    const String fn_name = getKQLFunctionName(pos);
    if (fn_name.empty())
        return false;

    ++pos;
    const String datetime_str = getConvertedArgument(fn_name, pos);
    String offset = "0";

    if (pos->type == TokenType::Comma)
    {
         ++pos;
         offset = getConvertedArgument(fn_name, pos);
    }
    out = std::format("date_add(YEAR,{}, parseDateTime64BestEffortOrNull(toString((toStartOfYear({}, 'UTC'))) , 9 , 'UTC'))", offset, datetime_str);
    return true;
}

bool UnixTimeMicrosecondsToDateTime::convertImpl(String & out, IParser::Pos & pos)
{
    const String fn_name = getKQLFunctionName(pos);
    if (fn_name.empty())
        return false;

    ++pos;
    const String value = getConvertedArgument(fn_name, pos);

    out = std::format("fromUnixTimestamp64Micro({},'UTC')", value);
    return true;
}

bool UnixTimeMillisecondsToDateTime::convertImpl(String & out, IParser::Pos & pos)
{
    const String fn_name = getKQLFunctionName(pos);
    if (fn_name.empty())
        return false;

    ++pos;
    const String value = getConvertedArgument(fn_name, pos);
    
    out = std::format("fromUnixTimestamp64Milli({},'UTC')", value);
    return true;
}

bool UnixTimeNanosecondsToDateTime::convertImpl(String & out, IParser::Pos & pos)
{
    const String fn_name = getKQLFunctionName(pos);
    if (fn_name.empty())
        return false;

    ++pos;
    const String value = getConvertedArgument(fn_name, pos);
     
    out = std::format("fromUnixTimestamp64Nano({},'UTC')", value);
    return true;
}

bool UnixTimeSecondsToDateTime::convertImpl(String & out, IParser::Pos & pos)
{
    const String fn_name = getKQLFunctionName(pos);
    if (fn_name.empty())
        return false;

    ++pos;
    if (pos->type == TokenType::QuotedIdentifier || pos->type == TokenType::StringLiteral)
        throw Exception(fn_name + " accepts only long, int and double type of arguments " , ErrorCodes::BAD_ARGUMENTS);
        
    String expression = getConvertedArgument(fn_name, pos);
    out = std::format(" if(toTypeName({0}) = 'Int64' OR toTypeName({0}) = 'Int32'OR toTypeName({0}) = 'Float64' OR  toTypeName({0}) = 'UInt32' OR  toTypeName({0}) = 'UInt64', toDateTime64({0}, 9, 'UTC') , toDateTime64(throwIf(true, '{1} only accepts Int , Long and double type of arguments'),9,'UTC'))", expression, fn_name);

    return true;
}

bool WeekOfYear::convertImpl(String & out, IParser::Pos & pos)
{
    const String fn_name = getKQLFunctionName(pos);
    if (fn_name.empty())
        return false;
    ++pos;
    const String time_str = getConvertedArgument(fn_name, pos);
    out = std::format("toWeek({},3,'UTC')", time_str);
    return true;
}

bool MonthOfYear::convertImpl(String & out, IParser::Pos & pos)
{
    return directMapping(out, pos, "toMonth");
}

}

