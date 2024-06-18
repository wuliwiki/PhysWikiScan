rm -f scan.db
sqlite3 scan.db < scan.sql
echo please run:
echo ". /mnt/drive/editor/stop.sh && . /mnt/drive/editor/start.sh"
