sqlite3 data/scan.db .dump > data/scan.sql
chmod 664 data/scan.sql
chgrp wiki data/scan.sql
