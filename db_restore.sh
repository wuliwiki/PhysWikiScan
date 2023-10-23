rm -f data/scan.db
sqlite3 data/scan.db < data/scan.sql
echo please run:
echo ". /mnt/drive/editor/stop.sh && . /mnt/drive/editor/start.sh"
