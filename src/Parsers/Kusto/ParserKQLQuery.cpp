#include <Parsers/ASTLiteral.h>
#include <Parsers/ASTSelectQuery.h>
#include <Parsers/IParserBase.h>
#include <Parsers/Kusto/ParserKQLQuery.h>
#include <Parsers/Kusto/ParserKQLTable.h>
#include <Parsers/Kusto/ParserKQLProject.h>
#include <Parsers/Kusto/ParserKQLFilter.h>
#include <Parsers/Kusto/ParserKQLSort.h>
#include <Parsers/Kusto/ParserKQLSummarize.h>
#include <Parsers/Kusto/ParserKQLLimit.h>
#include <Parsers/Kusto/ParserKQLStatement.h>
#include <Parsers/Kusto/KustoFunctions/KQLFunctionFactory.h>
#include <Parsers/Kusto/ParserKQLOperators.h>
namespace DB
{

namespace ErrorCodes
{
    extern const int UNKNOWN_FUNCTION;
}

bool ParserKQLBase :: parsePrepare(Pos & pos)
{
    op_pos.push_back(pos);
    return true;
}

String ParserKQLBase :: getExprFromToken(Pos &pos)
{
    String res;
    std::vector<String> tokens;
    std::unique_ptr<IParserKQLFunction> kql_function;
    String alias;

    while (!pos->isEnd() && pos->type != TokenType::PipeMark && pos->type != TokenType::Semicolon)
    {
        String token = String(pos->begin,pos->end);
        String new_token;
        if (token == "=")
        {
            ++pos;
            if (String(pos->begin,pos->end) != "~" )
            {
                alias = tokens.back();
                tokens.pop_back();
            }
            --pos;
        }
        else if (!KQLOperators().convert(tokens,pos))
        {
            if (pos->type == TokenType::BareWord )
            {
                kql_function = KQLFunctionFactory::get(token);
                if (kql_function && kql_function->convert(new_token,pos))
                    token = new_token;
             /*   else if (!kql_function)
                { 
                    if ((++pos)->type == TokenType::OpeningRoundBracket)
                        throw Exception("Unknown function  " + token, ErrorCodes::UNKNOWN_FUNCTION);
                    --pos;
                }*/
            }
            tokens.push_back(token);
        }

        if (pos->type == TokenType::Comma && !alias.empty())
        {
            tokens.pop_back();
            tokens.push_back("AS");
            tokens.push_back(alias);
            tokens.push_back(",");
            alias.clear();
        }
        ++pos;
    }

    if (!alias.empty())
    {
        tokens.push_back("AS");
        tokens.push_back(alias);
    }

    for (auto token:tokens) 
        res = res + token +" ";
    return res;
}

bool ParserKQLQuery::parseImpl(Pos & pos, ASTPtr & node, Expected & expected)
{
    auto select_query = std::make_shared<ASTSelectQuery>();
    node = select_query;

    ParserKQLFilter kql_filter_p;
    ParserKQLLimit kql_limit_p;
    ParserKQLProject kql_project_p;
    ParserKQLSort kql_sort_p;
    ParserKQLSummarize kql_summarize_p;
    ParserKQLTable kql_table_p;

    ASTPtr select_expression_list;
    ASTPtr tables;
    ASTPtr where_expression;
    ASTPtr group_expression_list;
    ASTPtr order_expression_list;
    ASTPtr limit_length;

    std::unordered_map<std::string, ParserKQLBase * > kql_parser = {
        { "filter",&kql_filter_p},
        { "where",&kql_filter_p},
        { "limit",&kql_limit_p},
        { "take",&kql_limit_p},
        { "project",&kql_project_p},
        { "sort",&kql_sort_p},
        { "order",&kql_sort_p},
        { "summarize",&kql_summarize_p},
        { "table",&kql_table_p}
    };

    std::vector<std::pair<String, Pos>> operation_pos;

    operation_pos.push_back(std::make_pair("table",pos));
    String table_name(pos->begin,pos->end);

    ++pos;
    while (!pos->isEnd() && pos->type != TokenType::Semicolon)
    {
        if (pos->type == TokenType::PipeMark)
        {
            ++pos;
            String kql_operator(pos->begin,pos->end);
            if (pos->type != TokenType::BareWord || kql_parser.find(kql_operator) == kql_parser.end())
                return false;
            ++pos;
            operation_pos.push_back(std::make_pair(kql_operator,pos));
            kql_parser[kql_operator]->getExprFromToken(pos);
        }
        else
            ++pos;
    }

    for (auto &op_pos : operation_pos)
    {
        auto kql_operator = op_pos.first;
        auto npos = op_pos.second;
        if (!npos.isValid())
            return false;

        if (!kql_parser[kql_operator]->parsePrepare(npos))
            return false;
    }

    if (!kql_table_p.parse(pos, tables, expected))
        return false;

    if (!kql_project_p.parse(pos, select_expression_list, expected))
        return false;

    if (!kql_limit_p.parse(pos, limit_length, expected))
        return false;

    if (!kql_filter_p.parse(pos, where_expression, expected))
        return false;

    if (!kql_sort_p.parse(pos, order_expression_list, expected))
         return false;

    kql_summarize_p.setTableName(table_name);
    kql_summarize_p.setFilterPos(kql_filter_p.op_pos);
    if (!kql_summarize_p.parse(pos, select_expression_list, expected))
         return false;
    else
    {
        group_expression_list = kql_summarize_p.group_expression_list;
        if (kql_summarize_p.tables)
            tables = kql_summarize_p.tables;

        if (kql_summarize_p.where_expression)
            where_expression = kql_summarize_p.where_expression;
    }

    select_query->setExpression(ASTSelectQuery::Expression::SELECT, std::move(select_expression_list));
    select_query->setExpression(ASTSelectQuery::Expression::TABLES, std::move(tables));
    select_query->setExpression(ASTSelectQuery::Expression::WHERE, std::move(where_expression));
    select_query->setExpression(ASTSelectQuery::Expression::GROUP_BY, std::move(group_expression_list));
    select_query->setExpression(ASTSelectQuery::Expression::ORDER_BY, std::move(order_expression_list));
    select_query->setExpression(ASTSelectQuery::Expression::LIMIT_LENGTH, std::move(limit_length));

    return true;
}

}
