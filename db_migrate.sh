./db_dump.sh
mv data/scan.db data/scan-old.db
./db_init.sh
./PhysWikiScan --migrate-db data/scan-old.db data/scan.db
./db_dump.sh
