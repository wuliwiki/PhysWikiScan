sqlite3 backup.db .dump > backup.sql
./sql-stab.py backup.sql
cp stable-backup.sql backup.sql
mv stable-backup.sql backup-$(date +'%Y%m%d.%H%M%S').sql
