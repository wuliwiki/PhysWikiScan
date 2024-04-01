sqlite3 data/scan.db .dump > data/scan.sql
cp data/scan.sql data/scan-$(date +'%Y%m%d.%H%M%S').sql

python3 data/sqlite-reorder.py data/scan.sql
cp data/stable-scan.sql data/stable-scan-$(date +'%Y%m%d.%H%M%S').sql

lines1=$(wc -l < "data/scan.sql")
lines2=$(wc -l < "data/stable-scan.sql")
if [ "$lines1" -ne "$lines2" ]; then
  echo "==== ERROR: reordered sql has different # of lines! ===="
fi
