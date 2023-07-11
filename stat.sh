# get number of "history" records for an author in a time range from database
# ignore CC license and `Use` license

author=零
time1=202303150000
time2=202306302359

##############################
sqlite3 data/scan.db "\
SELECT COUNT(1) FROM history WHERE \
author IN ( \
    SELECT id \
    FROM authors \
    WHERE name LIKE '%${author}%' \
) AND \
time >= ${time1} AND \
time <= ${time2}  \
"
