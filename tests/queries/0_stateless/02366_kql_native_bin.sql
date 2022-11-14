select '-- Numbers --';
select kqlBin(4.5, 1.5);
select kqlBin(4.5, 2);
select kqlBin(4, 3);
select kqlBin(5, 1.5);
select kqlBin(5, 0);
select kqlBin(4.5, 0);

select kqlBin(5, toIntervalNanosecond(1000)); -- { serverError ILLEGAL_TYPE_OF_ARGUMENT }
select kqlBin(5, toDateTime64('2022-11-08 12:34:56.7890123', 7)); -- { serverError ILLEGAL_TYPE_OF_ARGUMENT }

select '-- Intervals --';
select kqlBin(toIntervalWeek(1), toIntervalWeek(2)) as result, toTypeName(result);
select kqlBin(toIntervalNanosecond(2500000000), toIntervalNanosecond(1000000000));
select kqlBin(toIntervalNanosecond(2500000000), 1) as result, toTypeName(result);
select kqlBin(toIntervalNanosecond(2500000000), toIntervalNanosecond(0));

select kqlBin(toIntervalWeek(2), toIntervalHour(3)); -- { serverError ILLEGAL_TYPE_OF_ARGUMENT }
select kqlBin(toIntervalWeek(2), toDateTime64('2022-11-08 12:34:56.7890123', 7)); -- { serverError ILLEGAL_TYPE_OF_ARGUMENT }

select '-- DateTime64 --';
select kqlBin(toDateTime64('2022-11-08 12:34:56.7890123', 7), toIntervalNanosecond(100));
select kqlBin(toDateTime64('2022-11-08 12:34:56.7890123', 7), toIntervalNanosecond(1000));
select kqlBin(toDateTime64('2022-11-08 12:34:56.7890123', 7), toIntervalNanosecond(1000000));
select kqlBin(toDateTime64('2022-11-08 12:34:56.7890123', 7), toIntervalNanosecond(1000000000));
select kqlBin(toDateTime64('2022-11-08 12:34:56.7890123', 7), 1);
select kqlBin(toDateTime64('2022-11-08 12:34:56.7890123', 7), toIntervalNanosecond(60000000000));
select kqlBin(toDateTime64('2022-11-08 12:34:56.7890123', 7), toIntervalMinute(1));
select kqlBin(toDateTime64('2022-11-08 12:34:56.7890123', 7), toIntervalMinute(0));

select kqlBin(toDateTime64('2022-11-08 12:34:56.7890123', 7), toDateTime64('2022-11-08 12:34:56.7890123', 7)); -- { serverError ILLEGAL_TYPE_OF_ARGUMENT }
