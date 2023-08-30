rm -f data/scan.db
sqlite3 data/scan.db < data/scan-template.sql
chmod 664 data/scan.db
