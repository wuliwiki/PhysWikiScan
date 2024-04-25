#!/bin/bash
# backup all contents/*.tex for a user
# user 900 is a system user for this purpose
for filepath in ../PhysWiki/contents/*.tex; do
	filename=$(basename -- "$filepath");
	base="${filename%.*}";
	./PhysWikiScan --backup $base 900;
done
