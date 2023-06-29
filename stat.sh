# get number of "history" records for an author in a time range from database
# ignore CC license and `Use` license

author=AcertainUser
time1=202303150000
time2=202305011300

##############################
sqlite3 data/scan.db "\
SELECT COUNT(1) FROM history WHERE \
author IN ( \
    SELECT id \
    FROM authors \
    WHERE name LIKE '%${author}%' \
) AND \
time >= ${time1} AND \
time <= ${time2} AND \
NOT license LIKE 'CCBY%' AND \
license != 'Use' \
"
