set -e
rm -f backup.db
sqlite3 backup.db < backup.sql
