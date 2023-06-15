#! /bin/bash
chgrp wiki -R .
find . -type d -exec chmod 775 {} \;
find . -type f -exec chmod 664 {} \;
find . -type f -name '*.sh' -exec chmod +x *.sh {} \;
chmod +x PhysWikiScan
chmod +x NoteScan

