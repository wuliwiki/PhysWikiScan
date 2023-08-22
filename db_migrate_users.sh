rm -rf ../user-notes/note-template/cmd_data/scan.db
sqlite3 ../user-notes/note-template/cmd_data/scan.db < data/scan-template.sql
./PhysWikiScan --migrate-user-db
