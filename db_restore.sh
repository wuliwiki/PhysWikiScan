rm -f data/scan.db
sqlite3 data/scan.db < data/scan.sql
chmod 664 data/scan.db
chgrp wiki data/scan.db
