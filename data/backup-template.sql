-- PhysWiki backup database schema
-- Based on scan-template.sql conventions
-- All fields are NOT NULL unless explicitly noted.
-- Each record corresponds to a file: YYYYMMDDHHMM_author_entry.tex

CREATE TABLE "table_version" (
	"table"      TEXT    NOT NULL UNIQUE,
	"version"    TEXT    NOT NULL,
	"importance" INTEGER NOT NULL,
	PRIMARY KEY("table")
);

INSERT INTO "table_version" VALUES ('backup_files', '20260210', 1);

CREATE TABLE "backup_files" (
	"id"         INTEGER NOT NULL PRIMARY KEY, -- auto rowid
	"time"       TEXT    NOT NULL, -- YYYYMMDDHHMM
	"author"     INTEGER NOT NULL,
	"entry"      TEXT    NOT NULL,
	"size"       INTEGER NOT NULL, -- file size in bytes (64-bit)
	"hash"       TEXT    NOT NULL, -- first 16 chars of sha1
	"last_id"    INTEGER,          -- previous version id (NULL for first)
	"diff"       TEXT    NOT NULL, -- JSON diff from previous version of same article_id
	UNIQUE("time", "author", "entry"),
	FOREIGN KEY("last_id") REFERENCES "backup_files"("id")
);

CREATE INDEX idx_backup_files_time ON "backup_files"("time");
CREATE INDEX idx_backup_files_author ON "backup_files"("author");
CREATE INDEX idx_backup_files_entry ON "backup_files"("entry");
CREATE INDEX idx_backup_files_last_id ON "backup_files"("last_id");
