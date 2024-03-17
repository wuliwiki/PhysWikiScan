-- Zotero 数据库的结构参考

PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;

CREATE TABLE fieldFormats (    fieldFormatID INTEGER PRIMARY KEY,    regex TEXT,    isInteger INT);
INSERT INTO fieldFormats VALUES(1,'.*',0);
INSERT INTO fieldFormats VALUES(2,'[0-9]*',1);
INSERT INTO fieldFormats VALUES(3,'[0-9]{4}',1);

CREATE TABLE charsets (    charsetID INTEGER PRIMARY KEY,    charset TEXT UNIQUE);
INSERT INTO charsets VALUES(1,'utf-8');
INSERT INTO charsets VALUES(2,'big5');
INSERT INTO charsets VALUES(3,'euc-jp');
INSERT INTO charsets VALUES(4,'euc-kr');
INSERT INTO charsets VALUES(5,'gb18030');
INSERT INTO charsets VALUES(6,'gbk');

CREATE TABLE fileTypes (    fileTypeID INTEGER PRIMARY KEY,    fileType TEXT UNIQUE);
INSERT INTO fileTypes VALUES(1,'webpage');
INSERT INTO fileTypes VALUES(2,'image');
INSERT INTO fileTypes VALUES(3,'pdf');
INSERT INTO fileTypes VALUES(4,'audio');
INSERT INTO fileTypes VALUES(5,'video');
INSERT INTO fileTypes VALUES(6,'document');
INSERT INTO fileTypes VALUES(7,'presentation');

CREATE TABLE fileTypeMimeTypes (    fileTypeID INT,    mimeType TEXT,    PRIMARY KEY (fileTypeID, mimeType),    FOREIGN KEY (fileTypeID) REFERENCES fileTypes(fileTypeID));
INSERT INTO fileTypeMimeTypes VALUES(1,'text/html');
INSERT INTO fileTypeMimeTypes VALUES(2,'image/');
INSERT INTO fileTypeMimeTypes VALUES(2,'application/vnd.oasis.opendocument.graphics');
INSERT INTO fileTypeMimeTypes VALUES(2,'application/vnd.oasis.opendocument.image');
INSERT INTO fileTypeMimeTypes VALUES(3,'application/pdf');

CREATE TABLE syncObjectTypes (    syncObjectTypeID INTEGER PRIMARY KEY,    name TEXT);
INSERT INTO syncObjectTypes VALUES(1,'collection');
INSERT INTO syncObjectTypes VALUES(2,'creator');
INSERT INTO syncObjectTypes VALUES(3,'item');
INSERT INTO syncObjectTypes VALUES(4,'search');

CREATE TABLE itemTypes (    itemTypeID INTEGER PRIMARY KEY,    typeName TEXT,    templateItemTypeID INT,    display INT DEFAULT 1 );
INSERT INTO itemTypes VALUES(1,'annotation',NULL,1);
INSERT INTO itemTypes VALUES(2,'artwork',NULL,1);
INSERT INTO itemTypes VALUES(3,'attachment',NULL,1);
INSERT INTO itemTypes VALUES(4,'audioRecording',NULL,1);
INSERT INTO itemTypes VALUES(5,'bill',NULL,1);
INSERT INTO itemTypes VALUES(6,'blogPost',NULL,1);

CREATE TABLE itemTypesCombined (    itemTypeID INT NOT NULL,    typeName TEXT NOT NULL,    display INT DEFAULT 1 NOT NULL,    custom INT NOT NULL,    PRIMARY KEY (itemTypeID));
INSERT INTO itemTypesCombined VALUES(1,'annotation',1,0);
INSERT INTO itemTypesCombined VALUES(2,'artwork',1,0);
INSERT INTO itemTypesCombined VALUES(3,'attachment',1,0);
INSERT INTO itemTypesCombined VALUES(4,'audioRecording',1,0);
INSERT INTO itemTypesCombined VALUES(5,'bill',1,0);

CREATE TABLE fields (    fieldID INTEGER PRIMARY KEY,    fieldName TEXT,    fieldFormatID INT,    FOREIGN KEY (fieldFormatID) REFERENCES fieldFormats(fieldFormatID));
INSERT INTO fields VALUES(1,'title',NULL);
INSERT INTO fields VALUES(2,'abstractNote',NULL);
INSERT INTO fields VALUES(3,'artworkMedium',NULL);
INSERT INTO fields VALUES(4,'medium',NULL);
INSERT INTO fields VALUES(5,'artworkSize',NULL);
INSERT INTO fields VALUES(6,'date',NULL);
INSERT INTO fields VALUES(7,'language',NULL);

CREATE TABLE fieldsCombined (    fieldID INT NOT NULL,    fieldName TEXT NOT NULL,    label TEXT,    fieldFormatID INT,    custom INT NOT NULL,    PRIMARY KEY (fieldID));
INSERT INTO fieldsCombined VALUES(1,'title',NULL,NULL,0);
INSERT INTO fieldsCombined VALUES(2,'abstractNote',NULL,NULL,0);
INSERT INTO fieldsCombined VALUES(3,'artworkMedium',NULL,NULL,0);
INSERT INTO fieldsCombined VALUES(4,'medium',NULL,NULL,0);
INSERT INTO fieldsCombined VALUES(5,'artworkSize',NULL,NULL,0);

CREATE TABLE itemTypeFields (    itemTypeID INT,    fieldID INT,    hide INT,    orderIndex INT,    PRIMARY KEY (itemTypeID, orderIndex),    UNIQUE (itemTypeID, fieldID),    FOREIGN KEY (itemTypeID) REFERENCES itemTypes(itemTypeID),    FOREIGN KEY (fieldID) REFERENCES fields(fieldID));
INSERT INTO itemTypeFields VALUES(2,1,0,0);
INSERT INTO itemTypeFields VALUES(2,2,0,1);
INSERT INTO itemTypeFields VALUES(2,3,0,2);
INSERT INTO itemTypeFields VALUES(2,5,0,3);
INSERT INTO itemTypeFields VALUES(2,6,0,4);

CREATE TABLE itemTypeFieldsCombined (    itemTypeID INT NOT NULL,    fieldID INT NOT NULL,    hide INT,    orderIndex INT NOT NULL,    PRIMARY KEY (itemTypeID, orderIndex),    UNIQUE (itemTypeID, fieldID));
INSERT INTO itemTypeFieldsCombined VALUES(2,1,0,0);
INSERT INTO itemTypeFieldsCombined VALUES(2,2,0,1);
INSERT INTO itemTypeFieldsCombined VALUES(2,3,0,2);
INSERT INTO itemTypeFieldsCombined VALUES(2,5,0,3);
INSERT INTO itemTypeFieldsCombined VALUES(2,6,0,4);
INSERT INTO itemTypeFieldsCombined VALUES(2,7,0,5);

CREATE TABLE baseFieldMappings (    itemTypeID INT,    baseFieldID INT,    fieldID INT,    PRIMARY KEY (itemTypeID, baseFieldID, fieldID),    FOREIGN KEY (itemTypeID) REFERENCES itemTypes(itemTypeID),    FOREIGN KEY (baseFieldID) REFERENCES fields(fieldID),    FOREIGN KEY (fieldID) REFERENCES fields(fieldID));
INSERT INTO baseFieldMappings VALUES(2,4,3);
INSERT INTO baseFieldMappings VALUES(4,4,17);
INSERT INTO baseFieldMappings VALUES(4,23,22);
INSERT INTO baseFieldMappings VALUES(5,27,26);
INSERT INTO baseFieldMappings VALUES(5,19,29);
INSERT INTO baseFieldMappings VALUES(5,32,31);

CREATE TABLE baseFieldMappingsCombined (    itemTypeID INT,    baseFieldID INT,    fieldID INT,    PRIMARY KEY (itemTypeID, baseFieldID, fieldID));
INSERT INTO baseFieldMappingsCombined VALUES(2,4,3);
INSERT INTO baseFieldMappingsCombined VALUES(4,4,17);
INSERT INTO baseFieldMappingsCombined VALUES(4,23,22);
INSERT INTO baseFieldMappingsCombined VALUES(5,19,29);
INSERT INTO baseFieldMappingsCombined VALUES(5,27,26);

CREATE TABLE creatorTypes (    creatorTypeID INTEGER PRIMARY KEY,    creatorType TEXT);
INSERT INTO creatorTypes VALUES(1,'artist');
INSERT INTO creatorTypes VALUES(2,'contributor');
INSERT INTO creatorTypes VALUES(3,'performer');
INSERT INTO creatorTypes VALUES(4,'composer');
INSERT INTO creatorTypes VALUES(5,'wordsBy');
INSERT INTO creatorTypes VALUES(6,'sponsor');

CREATE TABLE itemTypeCreatorTypes (    itemTypeID INT,    creatorTypeID INT,    primaryField INT,    PRIMARY KEY (itemTypeID, creatorTypeID),    FOREIGN KEY (itemTypeID) REFERENCES itemTypes(itemTypeID),    FOREIGN KEY (creatorTypeID) REFERENCES creatorTypes(creatorTypeID));
INSERT INTO itemTypeCreatorTypes VALUES(2,1,1);
INSERT INTO itemTypeCreatorTypes VALUES(2,2,0);
INSERT INTO itemTypeCreatorTypes VALUES(4,3,1);
INSERT INTO itemTypeCreatorTypes VALUES(4,2,0);
INSERT INTO itemTypeCreatorTypes VALUES(4,4,0);

CREATE TABLE version (    schema TEXT PRIMARY KEY,    version INT NOT NULL);
INSERT INTO version VALUES('globalSchema',28);
INSERT INTO version VALUES('system',32);
INSERT INTO version VALUES('userdata',121);
INSERT INTO version VALUES('triggers',18);
INSERT INTO version VALUES('compatibility',7);
INSERT INTO version VALUES('delete',74);
INSERT INTO version VALUES('translators',1709846562);

CREATE TABLE settings (    setting TEXT,    key TEXT,    value,    PRIMARY KEY (setting, key));
INSERT INTO settings VALUES('globalSchema','data',X'aaa6fefc6775fa8ab063.............52dc5e3744291859aa7e07a');
INSERT INTO settings VALUES('client','lastCompatibleVersion','6.0.30');
INSERT INTO settings VALUES('account','localUserKey','AwnNJ02e');
INSERT INTO settings VALUES('account','username','shi3');
INSERT INTO settings VALUES('account','userID',13685696);
INSERT INTO settings VALUES('client','lastVersion','6.0.35');

CREATE TABLE syncedSettings (    setting TEXT NOT NULL,    libraryID INT NOT NULL,    value NOT NULL,    version INT NOT NULL DEFAULT 0,    synced INT NOT NULL DEFAULT 0,    PRIMARY KEY (setting, libraryID),    FOREIGN KEY (libraryID) REFERENCES libraries(libraryID) ON DELETE CASCADE);
INSERT INTO syncedSettings VALUES('lastPageIndex_u_GRGF2XJ4',1,'2',2,1);
INSERT INTO syncedSettings VALUES('lastPageIndex_u_4CBMC4LW',1,'0',712,1);
INSERT INTO syncedSettings VALUES('lastPageIndex_u_QYKS8PH9',1,'0',765,1);
INSERT INTO syncedSettings VALUES('lastPageIndex_u_NER5Z7XE',1,'0',1566,1);
INSERT INTO syncedSettings VALUES('lastPageIndex_u_A7SGJ3KX',1,'2',824,1);
INSERT INTO syncedSettings VALUES('lastPageIndex_u_BNUPGPEV',1,'0',908,1);
INSERT INTO syncedSettings VALUES('lastPageIndex_u_LFDLGCHJ',1,'1',942,1);
INSERT INTO syncedSettings VALUES('lastPageIndex_u_T3ENTEMT',1,'7',1205,1);
INSERT INTO syncedSettings VALUES('lastPageIndex_u_KGVQHCXH',1,'0',1311,1);

CREATE TABLE items (    itemID INTEGER PRIMARY KEY,    itemTypeID INT NOT NULL,    dateAdded TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,    dateModified TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,    clientDateModified TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,    libraryID INT NOT NULL,    key TEXT NOT NULL,    version INT NOT NULL DEFAULT 0,    synced INT NOT NULL DEFAULT 0,    UNIQUE (libraryID, key),    FOREIGN KEY (libraryID) REFERENCES libraries(libraryID) ON DELETE CASCADE);
INSERT INTO items VALUES(4,22,'2024-02-22 15:51:38','2024-03-05 21:41:53','2024-03-05 21:41:53',1,'EL7F6NM7',1559,1);
INSERT INTO items VALUES(5,3,'2024-02-22 15:51:40','2024-02-22 15:59:03','2024-02-22 16:33:43',1,'GRGF2XJ4',7,1);
INSERT INTO items VALUES(6,22,'2024-02-22 15:54:04','2024-02-24 00:07:25','2024-02-26 20:14:54',1,'Z5NQUXX3',1452,1);
INSERT INTO items VALUES(7,3,'2024-02-22 15:54:07','2024-02-22 15:58:57','2024-02-22 16:33:51',1,'IC67NNHP',8,1);
INSERT INTO items VALUES(8,28,'2024-02-22 15:56:28','2024-02-22 15:57:25','2024-02-22 16:39:22',1,'U69PYDVL',14,1);
INSERT INTO items VALUES(9,28,'2024-02-22 15:57:43','2024-02-22 15:57:51','2024-02-22 16:39:29',1,'4SR8Y4ZZ',15,1);
INSERT INTO items VALUES(10,28,'2024-02-22 15:58:00','2024-02-22 15:58:00','2024-02-22 15:58:11',1,'NPMD2RQV',3,1);
INSERT INTO items VALUES(11,28,'2024-02-22 15:58:23','2024-02-22 15:58:23','2024-02-22 15:58:26',1,'XTEM62XH',3,1);

CREATE TABLE itemDataValues (    valueID INTEGER PRIMARY KEY,    value UNIQUE);
INSERT INTO itemDataValues VALUES(1,'Attosecond correlation dynamics');
INSERT INTO itemDataValues VALUES(12,'2017-03-00 3/2017');
INSERT INTO itemDataValues VALUES(13,'en');
INSERT INTO itemDataValues VALUES(14,'DOI.org (Crossref)');
INSERT INTO itemDataValues VALUES(15,'https://www.nature.com/articles/nphys3941');
INSERT INTO itemDataValues VALUES(16,'2024-02-22 15:51:38');
INSERT INTO itemDataValues VALUES(17,'13');
INSERT INTO itemDataValues VALUES(18,'280-285');
INSERT INTO itemDataValues VALUES(19,'Nature Physics');
INSERT INTO itemDataValues VALUES(20,'10.1038/nphys3941');

CREATE TABLE itemData (    itemID INT,    fieldID INT,    valueID,    PRIMARY KEY (itemID, fieldID),    FOREIGN KEY (itemID) REFERENCES items(itemID) ON DELETE CASCADE,    FOREIGN KEY (fieldID) REFERENCES fieldsCombined(fieldID),    FOREIGN KEY (valueID) REFERENCES itemDataValues(valueID));
INSERT INTO itemData VALUES(4,1,1);
INSERT INTO itemData VALUES(4,7,13);
INSERT INTO itemData VALUES(4,11,14);
INSERT INTO itemData VALUES(4,13,15);
INSERT INTO itemData VALUES(4,14,16);
INSERT INTO itemData VALUES(4,19,17);
INSERT INTO itemData VALUES(4,32,18);
INSERT INTO itemData VALUES(4,38,19);

CREATE TABLE itemNotes (    itemID INTEGER PRIMARY KEY,    parentItemID INT,    note TEXT,    title TEXT,    FOREIGN KEY (itemID) REFERENCES items(itemID) ON DELETE CASCADE,    FOREIGN KEY (parentItemID) REFERENCES items(itemID) ON DELETE CASCADE);
INSERT INTO itemNotes VALUES(5,NULL,replace('<div class="zotero-note znv1"><div data-schema-version="8"><p>what is this?</p>\n</div></div>','\n',char(10)),'what is this?');
INSERT INTO itemNotes VALUES(7,NULL,replace('<div class="zotero-note znv1"><div data-schema-version="8"><p>what is this?</p>\n</div></div>','\n',char(10)),'what is this?');
INSERT INTO itemNotes VALUES(8,6,replace('<div class="zotero-note znv1"><div data-schema-version="9"><p>10 year debate <span class="math">$a^2$</span> wow!</p>\n<p><span class="math">$a^2 + b^2 = c^2$</span></p>\n<p>nice!</p>\n</div></div>','\n',char(10)),'10 year debate $a^2$ wow!');
INSERT INTO itemNotes VALUES(9,4,replace('<div class="zotero-note znv1"><div data-schema-version="8"><p>child note</p>\n</div></div>','\n',char(10)),'child note');
INSERT INTO itemNotes VALUES(10,NULL,'<div class="zotero-note znv1"></div>','');
INSERT INTO itemNotes VALUES(11,NULL,'<div class="zotero-note znv1"></div>','');
INSERT INTO itemNotes VALUES(189,188,'<div class="zotero-note znv1">Comment: Published in the journal of Applied Mathematics and Computation 217 (2011) 5360-5365</div>','Comment: Published in the journal of Applied Mathematics and Computation 217 (2011) 5360-5365');
INSERT INTO itemNotes VALUES(431,19,replace('<div class="zotero-note znv1"><div data-schema-version="8"><p>123</p>\n</div></div>','\n',char(10)),'123');
INSERT INTO itemNotes VALUES(433,6,replace('<div class="zotero-note znv1"><div data-schema-version="8"><p>- milestone for as streaking delay, debated for 10 years</p>\n<p>- 21 ± 5 as between ionization of the 2p and 2s shells of neonv</p>\n<p></p>\n</div></div>','\n',char(10)),'- milestone for as streaking delay, debated for 10 years');

CREATE TABLE itemAttachments (    itemID INTEGER PRIMARY KEY,    parentItemID INT,    linkMode INT,    contentType TEXT,    charsetID INT,    path TEXT,    syncState INT DEFAULT 0,    storageModTime INT,    storageHash TEXT,    lastProcessedModificationTime INT,    FOREIGN KEY (itemID) REFERENCES items(itemID) ON DELETE CASCADE,    FOREIGN KEY (parentItemID) REFERENCES items(itemID) ON DELETE CASCADE,    FOREIGN KEY (charsetID) REFERENCES charsets(charsetID) ON DELETE SET NULL);
INSERT INTO itemAttachments VALUES(5,4,1,'application/pdf',NULL,'storage:Ossiander et al. - 2017 - Attosecond correlation dynamics.pdf',2,1708617100117,'12797892c00a389f0803b44b1e466b64',1708617100);
INSERT INTO itemAttachments VALUES(7,6,1,'application/pdf',NULL,'storage:Schultze et al. - 2010 - Delay in Photoemission.pdf',2,1708617247703,'216c2389f7ada8e16fe583b1711c48a1',NULL);
INSERT INTO itemAttachments VALUES(12,4,2,'application/pdf',NULL,'G:\MacroUniverse\research-Uwe\papers\in-db\[Ossi16] Supplementary Material.pdf',0,NULL,NULL,NULL);
INSERT INTO itemAttachments VALUES(13,6,2,'application/pdf',NULL,'G:\MacroUniverse\research-Uwe\papers\in-db\Schultze et al. - 2010 - Delay in Photoemission.pdf',0,NULL,NULL,NULL);
INSERT INTO itemAttachments VALUES(14,6,2,'application/pdf',NULL,'G:\github\papers\[Schu10] Delay in Photoemission.pdf',0,NULL,NULL,1708619843);
INSERT INTO itemAttachments VALUES(16,15,1,'application/pdf',NULL,'storage:Agostini et al. - 1979 - Free-Free Transitions Following Six-Photon Ionizat.pdf',2,1708620218089,'f1314fc8dceaae4a35c82a9c3fb42397',NULL);
INSERT INTO itemAttachments VALUES(18,17,1,'application/pdf',NULL,'storage:Ahmed et al. - 2012 - A review of one-way and two-way experiments to tes.pdf',2,1708620327445,'301b09e0a26e2bc89a55099564c3b5f3',NULL);

CREATE TABLE itemAnnotations (    itemID INTEGER PRIMARY KEY,    parentItemID INT NOT NULL,    type INTEGER NOT NULL,    authorName TEXT,    text TEXT,    comment TEXT,    color TEXT,    pageLabel TEXT,    sortIndex TEXT NOT NULL,    position TEXT NOT NULL,    isExternal INT NOT NULL,    FOREIGN KEY (itemID) REFERENCES items(itemID) ON DELETE CASCADE,    FOREIGN KEY (parentItemID) REFERENCES itemAttachments(itemID));
INSERT INTO itemAnnotations VALUES(441,440,1,NULL,'main axes of the polarization ellipse do not depend on the CEP',NULL,'#ffff00','567','00002|000792|00521','{"pageIndex":2,"rects":[[39.687,251.483,287.551,261.36]]}',1);
INSERT INTO itemAnnotations VALUES(472,471,1,NULL,'Krylovsubspace method',NULL,'#ffff00','1','00000|000998|00455','{"pageIndex":0,"rects":[[230.4,290.572,263.865,300.75],[45.36,278.812,115.092,288.99]]}',1);
INSERT INTO itemAnnotations VALUES(473,471,1,NULL,'Taylor series',NULL,'#ffff00','4','00003|001665|00529','{"pageIndex":3,"rects":[[72.553,215.644,124.273,226.089]]}',1);
INSERT INTO itemAnnotations VALUES(474,471,1,NULL,'Krylov subspace',NULL,'#ffff00','4','00003|003415|00471','{"pageIndex":3,"rects":[[314.48,273.72,382.128,284.165]]}',1);
INSERT INTO itemAnnotations VALUES(475,471,1,NULL,'MPI',NULL,'#ffff00','12','00011|001209|00422','{"pageIndex":11,"rects":[[237.6,323.16,254.24,333.605]]}',1);
INSERT INTO itemAnnotations VALUES(684,430,1,NULL,'dye-doped photoresist (DDPR)',NULL,'#ffd400','772','00000|003534|00563','{"pageIndex":0,"rects":[[484.383,217.09,561.26,227.27],[306.142,206.34,349.67,216.859]]}',0);
INSERT INTO itemAnnotations VALUES(685,430,1,NULL,'aggregation-induced emission (AIE)',NULL,'#ffd400','772','00000|003565|00574','{"pageIndex":0,"rects":[[369.766,206.34,508.143,216.859]]}',0);

CREATE TABLE tags (    tagID INTEGER PRIMARY KEY,    name TEXT NOT NULL UNIQUE);
INSERT INTO tags VALUES(29,'Differential Cross Section');
INSERT INTO tags VALUES(30,'Excess Energy');
INSERT INTO tags VALUES(31,'Momentum Distribution');
INSERT INTO tags VALUES(32,'Photon Energy');
INSERT INTO tags VALUES(33,'Total Cross Section');
INSERT INTO tags VALUES(74,'shi24');
INSERT INTO tags VALUES(75,'RABBITT');
INSERT INTO tags VALUES(76,'delay');

CREATE TABLE itemRelations (    itemID INT NOT NULL,    predicateID INT NOT NULL,    object TEXT NOT NULL,    PRIMARY KEY (itemID, predicateID, object),    FOREIGN KEY (itemID) REFERENCES items(itemID) ON DELETE CASCADE,    FOREIGN KEY (predicateID) REFERENCES relationPredicates(predicateID) ON DELETE CASCADE);
INSERT INTO itemRelations VALUES(389,1,'http://zotero.org/users/13685696/items/SRX93UC9');
INSERT INTO itemRelations VALUES(390,1,'http://zotero.org/users/13685696/items/JDYIPCRN');
INSERT INTO itemRelations VALUES(388,1,'http://zotero.org/users/13685696/items/3VVG46CG');
INSERT INTO itemRelations VALUES(21,1,'http://zotero.org/users/13685696/items/2XUHXWAD');
INSERT INTO itemRelations VALUES(386,1,'http://zotero.org/users/13685696/items/WXA26TBG');
INSERT INTO itemRelations VALUES(387,1,'http://zotero.org/users/13685696/items/6B3AIVPR');

CREATE TABLE itemTags (    itemID INT NOT NULL,    tagID INT NOT NULL,    type INT NOT NULL,    PRIMARY KEY (itemID, tagID),    FOREIGN KEY (itemID) REFERENCES items(itemID) ON DELETE CASCADE,    FOREIGN KEY (tagID) REFERENCES tags(tagID) ON DELETE CASCADE);
INSERT INTO itemTags VALUES(306,29,1);
INSERT INTO itemTags VALUES(306,30,1);
INSERT INTO itemTags VALUES(306,31,1);
INSERT INTO itemTags VALUES(306,32,1);
INSERT INTO itemTags VALUES(306,33,1);

CREATE TABLE creators (    creatorID INTEGER PRIMARY KEY,    firstName TEXT,    lastName TEXT,    fieldMode INT,    UNIQUE (lastName, firstName, fieldMode));
INSERT INTO creators VALUES(2,'M.','Ossiander',0);
INSERT INTO creators VALUES(3,'F.','Siegrist',0);
INSERT INTO creators VALUES(4,'V.','Shirvanyan',0);
INSERT INTO creators VALUES(5,'R.','Pazourek',0);
INSERT INTO creators VALUES(6,'A.','Sommer',0);
INSERT INTO creators VALUES(7,'T.','Latka',0);

CREATE TABLE itemCreators (    itemID INT NOT NULL,    creatorID INT NOT NULL,    creatorTypeID INT NOT NULL DEFAULT 1,    orderIndex INT NOT NULL DEFAULT 0,    PRIMARY KEY (itemID, creatorID, creatorTypeID, orderIndex),    UNIQUE (itemID, orderIndex),    FOREIGN KEY (itemID) REFERENCES items(itemID) ON DELETE CASCADE,    FOREIGN KEY (creatorID) REFERENCES creators(creatorID) ON DELETE CASCADE,    FOREIGN KEY (creatorTypeID) REFERENCES creatorTypes(creatorTypeID));
INSERT INTO itemCreators VALUES(4,2,8,0);
INSERT INTO itemCreators VALUES(4,3,8,1);
INSERT INTO itemCreators VALUES(4,4,8,2);
INSERT INTO itemCreators VALUES(4,5,8,3);

CREATE TABLE collections (    collectionID INTEGER PRIMARY KEY,    collectionName TEXT NOT NULL,    parentCollectionID INT DEFAULT NULL,    clientDateModified TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,    libraryID INT NOT NULL,    key TEXT NOT NULL,    version INT NOT NULL DEFAULT 0,    synced INT NOT NULL DEFAULT 0,    UNIQUE (libraryID, key),    FOREIGN KEY (libraryID) REFERENCES libraries(libraryID) ON DELETE CASCADE,    FOREIGN KEY (parentCollectionID) REFERENCES collections(collectionID) ON DELETE CASCADE);

CREATE TABLE collectionItems (    collectionID INT NOT NULL,    itemID INT NOT NULL,    orderIndex INT NOT NULL DEFAULT 0,    PRIMARY KEY (collectionID, itemID),    FOREIGN KEY (collectionID) REFERENCES collections(collectionID) ON DELETE CASCADE,    FOREIGN KEY (itemID) REFERENCES items(itemID) ON DELETE CASCADE);

CREATE TABLE collectionRelations (    collectionID INT NOT NULL,    predicateID INT NOT NULL,    object TEXT NOT NULL,    PRIMARY KEY (collectionID, predicateID, object),    FOREIGN KEY (collectionID) REFERENCES collections(collectionID) ON DELETE CASCADE,    FOREIGN KEY (predicateID) REFERENCES relationPredicates(predicateID) ON DELETE CASCADE);

CREATE TABLE feeds (    libraryID INTEGER PRIMARY KEY,    name TEXT NOT NULL,    url TEXT NOT NULL UNIQUE,    lastUpdate TIMESTAMP,    lastCheck TIMESTAMP,    lastCheckError TEXT,    cleanupReadAfter INT,    cleanupUnreadAfter INT,    refreshInterval INT,    FOREIGN KEY (libraryID) REFERENCES libraries(libraryID) ON DELETE CASCADE);

CREATE TABLE feedItems (    itemID INTEGER PRIMARY KEY,    guid TEXT NOT NULL UNIQUE,    readTime TIMESTAMP,    translatedTime TIMESTAMP,    FOREIGN KEY (itemID) REFERENCES items(itemID) ON DELETE CASCADE);

CREATE TABLE savedSearches (    savedSearchID INTEGER PRIMARY KEY,    savedSearchName TEXT NOT NULL,    clientDateModified TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,    libraryID INT NOT NULL,    key TEXT NOT NULL,    version INT NOT NULL DEFAULT 0,    synced INT NOT NULL DEFAULT 0,    UNIQUE (libraryID, key),    FOREIGN KEY (libraryID) REFERENCES libraries(libraryID) ON DELETE CASCADE);
INSERT INTO savedSearches VALUES(1,'streaking - full text search','2024-02-26 19:58:07',1,'NK7J5S3N',1425,1);

CREATE TABLE savedSearchConditions (    savedSearchID INT NOT NULL,    searchConditionID INT NOT NULL,    condition TEXT NOT NULL,    operator TEXT,    value TEXT,    required NONE,    PRIMARY KEY (savedSearchID, searchConditionID),    FOREIGN KEY (savedSearchID) REFERENCES savedSearches(savedSearchID) ON DELETE CASCADE);
INSERT INTO savedSearchConditions VALUES(1,0,'fulltextContent','contains','streaking',NULL);
INSERT INTO savedSearchConditions VALUES(1,1,'includeParentsAndChildren','true',NULL,NULL);
INSERT INTO savedSearchConditions VALUES(1,2,'noChildren','false',NULL,NULL);

CREATE TABLE deletedCollections (    collectionID INTEGER PRIMARY KEY,    dateDeleted DEFAULT CURRENT_TIMESTAMP NOT NULL,    FOREIGN KEY (collectionID) REFERENCES collections(collectionID) ON DELETE CASCADE);

CREATE TABLE deletedItems (    itemID INTEGER PRIMARY KEY,    dateDeleted DEFAULT CURRENT_TIMESTAMP NOT NULL,    FOREIGN KEY (itemID) REFERENCES items(itemID) ON DELETE CASCADE);
INSERT INTO deletedItems VALUES(5,'2024-02-22 16:33:39');
INSERT INTO deletedItems VALUES(7,'2024-02-22 16:33:47');
INSERT INTO deletedItems VALUES(8,'2024-02-22 16:39:19');
INSERT INTO deletedItems VALUES(9,'2024-02-22 16:39:26');
INSERT INTO deletedItems VALUES(10,'2024-02-22 15:58:11');
INSERT INTO deletedItems VALUES(11,'2024-02-22 15:58:26');

CREATE TABLE deletedSearches (    savedSearchID INTEGER PRIMARY KEY,    dateDeleted DEFAULT CURRENT_TIMESTAMP NOT NULL,    FOREIGN KEY (savedSearchID) REFERENCES savedSearches(savedSearchID) ON DELETE CASCADE);

CREATE TABLE libraries (    libraryID INTEGER PRIMARY KEY,    type TEXT NOT NULL,    editable INT NOT NULL,    filesEditable INT NOT NULL,    version INT NOT NULL DEFAULT 0,    storageVersion INT NOT NULL DEFAULT 0,    lastSync INT NOT NULL DEFAULT 0,    archived INT NOT NULL DEFAULT 0);
INSERT INTO libraries VALUES(1,'user',1,1,1617,1617,1710639232,0);

CREATE TABLE users (    userID INTEGER PRIMARY KEY,    name TEXT NOT NULL);
INSERT INTO users VALUES(13685696,'shi3');

CREATE TABLE groups (    groupID INTEGER PRIMARY KEY,    libraryID INT NOT NULL UNIQUE,    name TEXT NOT NULL,    description TEXT NOT NULL,    version INT NOT NULL,    FOREIGN KEY (libraryID) REFERENCES libraries(libraryID) ON DELETE CASCADE);

CREATE TABLE groupItems (    itemID INTEGER PRIMARY KEY,    createdByUserID INT,    lastModifiedByUserID INT,    FOREIGN KEY (itemID) REFERENCES items(itemID) ON DELETE CASCADE,    FOREIGN KEY (createdByUserID) REFERENCES users(userID) ON DELETE SET NULL,    FOREIGN KEY (lastModifiedByUserID) REFERENCES users(userID) ON DELETE SET NULL);

CREATE TABLE publicationsItems (    itemID INTEGER PRIMARY KEY);

CREATE TABLE retractedItems (	itemID INTEGER PRIMARY KEY,	data TEXT,	flag INT DEFAULT 0,	FOREIGN KEY (itemID) REFERENCES items(itemID) ON DELETE CASCADE);

CREATE TABLE fulltextItems (    itemID INTEGER PRIMARY KEY,    indexedPages INT,    totalPages INT,    indexedChars INT,    totalChars INT,    version INT NOT NULL DEFAULT 0,    synced INT NOT NULL DEFAULT 0,    FOREIGN KEY (itemID) REFERENCES items(itemID) ON DELETE CASCADE);
INSERT INTO fulltextItems VALUES(5,7,7,NULL,NULL,0,0);
INSERT INTO fulltextItems VALUES(7,6,6,NULL,NULL,0,0);
INSERT INTO fulltextItems VALUES(12,19,19,NULL,NULL,0,0);
INSERT INTO fulltextItems VALUES(13,6,6,NULL,NULL,0,0);
INSERT INTO fulltextItems VALUES(14,6,6,NULL,NULL,0,0);
INSERT INTO fulltextItems VALUES(16,4,4,NULL,NULL,0,0);

CREATE TABLE fulltextWords (    wordID INTEGER PRIMARY KEY,    word TEXT UNIQUE);
INSERT INTO fulltextWords VALUES(1,'0');
INSERT INTO fulltextWords VALUES(14,'113');
INSERT INTO fulltextWords VALUES(15,'115');
INSERT INTO fulltextWords VALUES(16,'118');
INSERT INTO fulltextWords VALUES(296,'wavepacket');
INSERT INTO fulltextWords VALUES(297,'we');
INSERT INTO fulltextWords VALUES(298,'with');
INSERT INTO fulltextWords VALUES(299,'ﬁeld');
INSERT INTO fulltextWords VALUES(300,'ﬁndings');
INSERT INTO fulltextWords VALUES(301,'1950s');

CREATE TABLE fulltextItemWords (    wordID INT,    itemID INT,    PRIMARY KEY (wordID, itemID),    FOREIGN KEY (wordID) REFERENCES fulltextWords(wordID),    FOREIGN KEY (itemID) REFERENCES items(itemID) ON DELETE CASCADE);
INSERT INTO fulltextItemWords VALUES(1,5);
INSERT INTO fulltextItemWords VALUES(2,5);
INSERT INTO fulltextItemWords VALUES(3,5);
INSERT INTO fulltextItemWords VALUES(4,5);
INSERT INTO fulltextItemWords VALUES(5,5);
INSERT INTO fulltextItemWords VALUES(6,5);
INSERT INTO fulltextItemWords VALUES(7,5);

CREATE TABLE syncCache (    libraryID INT NOT NULL,    key TEXT NOT NULL,    syncObjectTypeID INT NOT NULL,    version INT NOT NULL,    data TEXT,    PRIMARY KEY (libraryID, key, syncObjectTypeID, version),    FOREIGN KEY (libraryID) REFERENCES libraries(libraryID) ON DELETE CASCADE,    FOREIGN KEY (syncObjectTypeID) REFERENCES syncObjectTypes(syncObjectTypeID));
INSERT INTO syncCache VALUES(1,'8QIUP4JT',3,3,'{"key":"8QIUP4JT","version":3,"library":{"type":"user","id":13685696,"name":"shi3","links":{"alternate":{"href":"https://www.zotero.org/shi3","type":"text/html"}}},"links":{"self":{"href":"https://api.zotero.org/users/13685696/items/8QIUP4JT","type":"application/json"},"alternate":{"href":"https://www.zotero.org/shi3/items/8QIUP4JT","type":"text/html"},"up":{"href":"https://api.zotero.org/users/13685696/items/EL7F6NM7","type":"application/json"}},"meta":{"numChildren":0},"data":{"key":"8QIUP4JT","version":3,"parentItem":"EL7F6NM7","itemType":"attachment","linkMode":"linked_file","title":"[Ossi16] Supplementary Material.pdf","accessDate":"","url":"","note":"","contentType":"application/pdf","charset":"","path":"G:\\MacroUniverse\\research-Uwe\\papers\\in-db\\[Ossi16] Supplementary Material.pdf","tags":[],"relations":{},"dateAdded":"2024-02-22T16:17:08Z","dateModified":"2024-02-22T16:17:08Z"}}');
INSERT INTO syncCache VALUES(1,'NPMD2RQV',3,3,'{"key":"NPMD2RQV","version":3,"library":{"type":"user","id":13685696,"name":"shi3","links":{"alternate":{"href":"https://www.zotero.org/shi3","type":"text/html"}}},"links":{"self":{"href":"https://api.zotero.org/users/13685696/items/NPMD2RQV","type":"application/json"},"alternate":{"href":"https://www.zotero.org/shi3/items/NPMD2RQV","type":"text/html"}},"meta":{"numChildren":0},"data":{"key":"NPMD2RQV","version":3,"itemType":"note","note":"","deleted":1,"tags":[],"collections":[],"relations":{},"dateAdded":"2024-02-22T15:58:00Z","dateModified":"2024-02-22T15:58:00Z"}}');
INSERT INTO syncCache VALUES(1,'XTEM62XH',3,3,'{"key":"XTEM62XH","version":3,"library":{"type":"user","id":13685696,"name":"shi3","links":{"alternate":{"href":"https://www.zotero.org/shi3","type":"text/html"}}},"links":{"self":{"href":"https://api.zotero.org/users/13685696/items/XTEM62XH","type":"application/json"},"alternate":{"href":"https://www.zotero.org/shi3/items/XTEM62XH","type":"text/html"}},"meta":{"numChildren":0},"data":{"key":"XTEM62XH","version":3,"itemType":"note","note":"","deleted":1,"tags":[],"collections":[],"relations":{},"dateAdded":"2024-02-22T15:58:23Z","dateModified":"2024-02-22T15:58:23Z"}}');

CREATE TABLE syncDeleteLog (    syncObjectTypeID INT NOT NULL,    libraryID INT NOT NULL,    key TEXT NOT NULL,    dateDeleted TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,    UNIQUE (syncObjectTypeID, libraryID, key),    FOREIGN KEY (syncObjectTypeID) REFERENCES syncObjectTypes(syncObjectTypeID),    FOREIGN KEY (libraryID) REFERENCES libraries(libraryID) ON DELETE CASCADE);

CREATE TABLE syncQueue (    libraryID INT NOT NULL,    key TEXT NOT NULL,    syncObjectTypeID INT NOT NULL,    lastCheck TIMESTAMP,    tries INT,    PRIMARY KEY (libraryID, key, syncObjectTypeID),    FOREIGN KEY (libraryID) REFERENCES libraries(libraryID) ON DELETE CASCADE,    FOREIGN KEY (syncObjectTypeID) REFERENCES syncObjectTypes(syncObjectTypeID) ON DELETE CASCADE);

CREATE TABLE storageDeleteLog (    libraryID INT NOT NULL,    key TEXT NOT NULL,    dateDeleted TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,    PRIMARY KEY (libraryID, key),    FOREIGN KEY (libraryID) REFERENCES libraries(libraryID) ON DELETE CASCADE);

CREATE TABLE proxies (    proxyID INTEGER PRIMARY KEY,    multiHost INT,    autoAssociate INT,    scheme TEXT);

CREATE TABLE proxyHosts (    hostID INTEGER PRIMARY KEY,    proxyID INTEGER,    hostname TEXT,    FOREIGN KEY (proxyID) REFERENCES proxies(proxyID));

CREATE TABLE relationPredicates (    predicateID INTEGER PRIMARY KEY,    predicate TEXT UNIQUE);
INSERT INTO relationPredicates VALUES(1,'dc:replaces');
INSERT INTO relationPredicates VALUES(2,'dc:relation');

CREATE TABLE customItemTypes (    customItemTypeID INTEGER PRIMARY KEY,    typeName TEXT,    label TEXT,    display INT DEFAULT 1,     icon TEXT);

CREATE TABLE customFields (    customFieldID INTEGER PRIMARY KEY,    fieldName TEXT,    label TEXT);

CREATE TABLE customItemTypeFields (    customItemTypeID INT NOT NULL,    fieldID INT,    customFieldID INT,    hide INT NOT NULL,    orderIndex INT NOT NULL,    PRIMARY KEY (customItemTypeID, orderIndex),    FOREIGN KEY (customItemTypeID) REFERENCES customItemTypes(customItemTypeID),    FOREIGN KEY (fieldID) REFERENCES fields(fieldID),    FOREIGN KEY (customFieldID) REFERENCES customFields(customFieldID));

CREATE TABLE customBaseFieldMappings (    customItemTypeID INT,    baseFieldID INT,    customFieldID INT,    PRIMARY KEY (customItemTypeID, baseFieldID, customFieldID),    FOREIGN KEY (customItemTypeID) REFERENCES customItemTypes(customItemTypeID),    FOREIGN KEY (baseFieldID) REFERENCES fields(fieldID),    FOREIGN KEY (customFieldID) REFERENCES customFields(customFieldID));

CREATE TABLE translatorCache (    fileName TEXT PRIMARY KEY,    metadataJSON TEXT,    lastModifiedTime INT);
INSERT INTO translatorCache VALUES('Ab Imperio.js','{"translatorID":"f3e31f93-c18d-4ba3-9aa6-1963702b5762","translatorType":4,"label":"Ab Imperio","creator":"Avram Lyon","target":"^https?://(www\\.)?abimperio\\.net/","priority":100,"lastUpdated":"2013-02-28 14:52:44","browserSupport":"gcs","minVersion":"2.0","maxVersion":"","inRepository":true}',1698962673000);
INSERT INTO translatorCache VALUES('ABC News Australia.js','{"translatorID":"92d45016-5f7b-4bcf-bb63-193033f02f2b","translatorType":4,"label":"ABC News Australia","creator":"Joyce Chia","target":"https?://(www\\.)?abc\\.net\\.au/news/","priority":100,"lastUpdated":"2021-07-23 00:29:10","browserSupport":"gcsibv","minVersion":"3.0","maxVersion":"","inRepository":true}',1698962673000);
INSERT INTO translatorCache VALUES('Access Engineering.js','{"translatorID":"d120a8a7-9d45-446e-8c18-ad9ef0a6bf47","translatorType":4,"label":"Access Engineering","creator":"Vinoth K - highwirepress.com","target":"^https?://www\\.accessengineeringlibrary\\.com/","priority":100,"lastUpdated":"2023-09-09 09:42:36","browserSupport":"gcsibv","minVersion":"3.0","maxVersion":"","inRepository":true}',1698962673000);
INSERT INTO translatorCache VALUES('Access Medicine.js','{"translatorID":"60e55b65-08cb-4a8f-8a61-c36338ec8754","translatorType":4,"label":"Access Medicine","creator":"Jaret M. Karnuta","target":"^https?://(0-)?(access(anesthesiology|cardiology|emergencymedicine|medicine|pediatrics|surgery)|neurology)\\.mhmedical\\.com","priority":100,"lastUpdated":"2017-01-12 22:14:02","browserSupport":"gcsibv","minVersion":"3.0","maxVersion":"","inRepository":true}',1698962673000);
INSERT INTO translatorCache VALUES('Access Science.js','{"translatorID":"558330ca-3531-467a-8003-86cd9602cc48","translatorType":4,"label":"Access Science","creator":"Vinoth K - highwirepress.com","target":"^https?://www\\.accessscience\\.com/","priority":100,"lastUpdated":"2023-10-17 20:19:39","browserSupport":"gcsibv","minVersion":"5.0","maxVersion":"","inRepository":true}',1698962673000);
INSERT INTO translatorCache VALUES('ACLS Humanities EBook.js','{"translatorID":"2553b683-dc1b-4a1e-833a-7a7755326186","translatorType":4,"label":"ACLS Humanities EBook","creator":"Abe Jellinek","target":"^https?://www\\.fulcrum\\.org/","priority":100,"lastUpdated":"2021-08-03 01:54:15","browserSupport":"gcsibv","minVersion":"3.0","maxVersion":"","inRepository":true}',1698962673000);

CREATE TABLE dbDebug1 (    a INTEGER PRIMARY KEY);
CREATE INDEX charsets_charset ON charsets(charset);
CREATE INDEX fileTypes_fileType ON fileTypes(fileType);
CREATE INDEX fileTypeMimeTypes_mimeType ON fileTypeMimeTypes(mimeType);
CREATE INDEX syncObjectTypes_name ON syncObjectTypes(name);
CREATE INDEX itemTypeFields_fieldID ON itemTypeFields(fieldID);
CREATE INDEX itemTypeFieldsCombined_fieldID ON itemTypeFieldsCombined(fieldID);
CREATE INDEX baseFieldMappings_baseFieldID ON baseFieldMappings(baseFieldID);
CREATE INDEX baseFieldMappings_fieldID ON baseFieldMappings(fieldID);
CREATE INDEX baseFieldMappingsCombined_baseFieldID ON baseFieldMappingsCombined(baseFieldID);
CREATE INDEX baseFieldMappingsCombined_fieldID ON baseFieldMappingsCombined(fieldID);
CREATE INDEX itemTypeCreatorTypes_creatorTypeID ON itemTypeCreatorTypes(creatorTypeID);
CREATE INDEX schema ON version(schema);
CREATE INDEX items_synced ON items(synced);
CREATE INDEX itemData_fieldID ON itemData(fieldID);
CREATE INDEX itemNotes_parentItemID ON itemNotes(parentItemID);
CREATE INDEX itemAttachments_parentItemID ON itemAttachments(parentItemID);
CREATE INDEX itemAttachments_charsetID ON itemAttachments(charsetID);
CREATE INDEX itemAttachments_contentType ON itemAttachments(contentType);
CREATE INDEX itemAttachments_syncState ON itemAttachments(syncState);
CREATE INDEX itemAttachments_lastProcessedModificationTime ON itemAttachments(lastProcessedModificationTime);
CREATE INDEX itemAnnotations_parentItemID ON itemAnnotations(parentItemID);
CREATE INDEX itemRelations_predicateID ON itemRelations(predicateID);
CREATE INDEX itemRelations_object ON itemRelations(object);
CREATE INDEX itemTags_tagID ON itemTags(tagID);
CREATE INDEX itemCreators_creatorTypeID ON itemCreators(creatorTypeID);
CREATE INDEX collections_synced ON collections(synced);
CREATE INDEX collectionItems_itemID ON collectionItems(itemID);
CREATE INDEX collectionRelations_predicateID ON collectionRelations(predicateID);
CREATE INDEX collectionRelations_object ON collectionRelations(object);
CREATE INDEX savedSearches_synced ON savedSearches(synced);
CREATE INDEX deletedCollections_dateDeleted ON deletedCollections(dateDeleted);
CREATE INDEX deletedItems_dateDeleted ON deletedItems(dateDeleted);
CREATE INDEX deletedSearches_dateDeleted ON deletedItems(dateDeleted);
CREATE INDEX fulltextItems_synced ON fulltextItems(synced);
CREATE INDEX fulltextItems_version ON fulltextItems(version);
CREATE INDEX fulltextItemWords_itemID ON fulltextItemWords(itemID);
CREATE INDEX proxyHosts_proxyID ON proxyHosts(proxyID);
CREATE INDEX customItemTypeFields_fieldID ON customItemTypeFields(fieldID);
CREATE INDEX customItemTypeFields_customFieldID ON customItemTypeFields(customFieldID);
CREATE INDEX customBaseFieldMappings_baseFieldID ON customBaseFieldMappings(baseFieldID);
CREATE INDEX customBaseFieldMappings_customFieldID ON customBaseFieldMappings(customFieldID);
CREATE TRIGGER insert_creators BEFORE INSERT ON creators  FOR EACH ROW WHEN NEW.firstName='' AND NEW.lastName=''  BEGIN    SELECT RAISE (ABORT, 'Creator names cannot be empty');  END;
CREATE TRIGGER update_creators BEFORE UPDATE ON creators  FOR EACH ROW WHEN NEW.firstName='' AND NEW.lastName=''  BEGIN    SELECT RAISE (ABORT, 'Creator names cannot be empty');  END;
CREATE TRIGGER fki_collections_parentCollectionID_libraryID  BEFORE INSERT ON collections  FOR EACH ROW BEGIN    SELECT RAISE(ABORT, 'insert on table "collections" violates foreign key constraint "fki_collections_parentCollectionID_libraryID"')    WHERE NEW.parentCollectionID IS NOT NULL AND    NEW.libraryID != (SELECT libraryID FROM collections WHERE collectionID = NEW.parentCollectionID);  END;
CREATE TRIGGER fku_collections_parentCollectionID_libraryID  BEFORE UPDATE ON collections  FOR EACH ROW BEGIN    SELECT RAISE(ABORT, 'update on table "collections" violates foreign key constraint "fku_collections_parentCollectionID_libraryID"')    WHERE NEW.parentCollectionID IS NOT NULL AND    NEW.libraryID != (SELECT libraryID FROM collections WHERE collectionID = NEW.parentCollectionID);  END;
CREATE TRIGGER fki_collectionItems_libraryID  BEFORE INSERT ON collectionItems  FOR EACH ROW BEGIN    SELECT RAISE(ABORT, 'insert on table "collectionItems" violates foreign key constraint "fki_collectionItems_libraryID"')    WHERE (SELECT libraryID FROM collections WHERE collectionID = NEW.collectionID) != (SELECT libraryID FROM items WHERE itemID = NEW.itemID);  END;
CREATE TRIGGER fku_collectionItems_libraryID  BEFORE UPDATE ON collectionItems  FOR EACH ROW BEGIN    SELECT RAISE(ABORT, 'update on table "collectionItems" violates foreign key constraint "fku_collectionItems_libraryID"')    WHERE (SELECT libraryID FROM collections WHERE collectionID = NEW.collectionID) != (SELECT libraryID FROM items WHERE itemID = NEW.itemID);  END;
CREATE TRIGGER fki_collectionItems_itemID_parentItemID  BEFORE INSERT ON collectionItems  FOR EACH ROW BEGIN    SELECT RAISE(ABORT, 'insert on table "collectionItems" violates foreign key constraint "fki_collectionItems_itemID_parentItemID"')    WHERE NEW.itemID IN (SELECT itemID FROM itemAttachments WHERE parentItemID IS NOT NULL UNION SELECT itemID FROM itemNotes WHERE parentItemID IS NOT NULL);  END;
CREATE TRIGGER fku_collectionItems_itemID_parentItemID  BEFORE UPDATE OF itemID ON collectionItems  FOR EACH ROW BEGIN    SELECT RAISE(ABORT, 'update on table "collectionItems" violates foreign key constraint "fku_collectionItems_itemID_parentItemID"')    WHERE NEW.itemID IN (SELECT itemID FROM itemAttachments WHERE parentItemID IS NOT NULL UNION SELECT itemID FROM itemNotes WHERE parentItemID IS NOT NULL);  END;
CREATE TRIGGER fku_itemAttachments_parentItemID_collectionItems_itemID  BEFORE UPDATE OF parentItemID ON itemAttachments  FOR EACH ROW WHEN OLD.parentItemID IS NULL AND NEW.parentItemID IS NOT NULL BEGIN    DELETE FROM collectionItems WHERE itemID = NEW.itemID;  END;
CREATE TRIGGER fku_itemNotes_parentItemID_collectionItems_itemID  BEFORE UPDATE OF parentItemID ON itemNotes  FOR EACH ROW WHEN OLD.parentItemID IS NULL AND NEW.parentItemID IS NOT NULL BEGIN    DELETE FROM collectionItems WHERE itemID = NEW.itemID;  END;
CREATE TRIGGER fki_tagsBEFORE INSERT ON tags  FOR EACH ROW BEGIN    SELECT RAISE(ABORT, 'Tag cannot be blank')    WHERE TRIM(NEW.name)='';  END;
CREATE TRIGGER fku_tags  BEFORE UPDATE OF name ON tags  FOR EACH ROW BEGIN      SELECT RAISE(ABORT, 'Tag cannot be blank')      WHERE TRIM(NEW.name)='';  END;
COMMIT;
