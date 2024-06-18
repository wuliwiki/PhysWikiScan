sqlite3 scan.db .dump > scan.sql
./sql-stab.py scan.sql
cp stable-scan.sql scan.sql
mv stable-scan.sql scan-$(date +'%Y%m%d.%H%M%S').sql
