sqlite3 PhysWiki-backup.db .dump > PhysWiki-backup.sql
./sql-stab.py PhysWiki-backup.sql
cp stable-PhysWiki-backup.sql PhysWiki-backup.sql
mv stable-PhysWiki-backup.sql PhysWiki-backup-$(date +'%Y%m%d.%H%M%S').sql
