# get number of "history" records for an author in a time range from database
# ignore CC license and `Use` license

author=Acertain
time1=202307010000
time2=202307312359

##############################
sqlite3 data/scan.db " \
SELECT COUNT(1) FROM history \
JOIN entries ON history.entry = entries.id \
WHERE \
history.author IN ( \
    SELECT id \
    FROM authors \
    WHERE name LIKE '%${author}%' \
) AND \
history.time >= '${time1}' AND \
history.time <= '${time2}' AND \
entries.license = 'Xiao' \
"

