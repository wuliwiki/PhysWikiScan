sqlite3 data/scan.db .dump > data/scan.sql
cp data/scan.sql data/scan-$(date +'%Y%m%d.%H%M').sql
