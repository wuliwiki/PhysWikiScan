sqlite3 data/scan.db .dump > data/scan.sql
cp data/scan.sql data/scan-$(date +'%Y%m%d.%H%M%S').sql

python3 data/sqlite-reorder.py data/scan.sql
cp data/stable-scan.sql data/stable-scan-$(date +'%Y%m%d.%H%M%S').sql
