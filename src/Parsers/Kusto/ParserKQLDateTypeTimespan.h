#pragma once

#include <Parsers/IParserBase.h>
#include <Parsers/Kusto/ParserKQLQuery.h>

namespace DB
{
class ParserKQLDataTypeTimespan : public ParserKQLBase
{
public:
    static std::optional<Int64> performParsing(const String & expression);
    std::optional<Int64> retrieveResult() { return std::exchange(result, {}); }

protected:
    const char * getName() const override { return "KQLDateTypeTimespan"; }
    bool parseImpl(Pos & pos, ASTPtr & node, Expected & expected) override;

private:
    std::optional<Int64> result;
};

}
