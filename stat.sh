# get number of "history" records for an author in a time range from database

author=addis
time1=202303150000
time2=202305011300

##############################
sqlite3 data/scan.db "\
SELECT COUNT(1) FROM history WHERE \
author IN ( \
    SELECT id \
    FROM authors \
    WHERE name LIKE '${author}' \
) AND \
time >= ${time1} AND \
time <= ${time2} \
"
