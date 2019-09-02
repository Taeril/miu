CREATE TABLE paths (
	id INTEGER PRIMARY KEY ASC,
	name TEXT UNIQUE NOT NULL
);
CREATE UNIQUE INDEX uniq_paths_name ON paths(name);

CREATE TABLE entries (
	id INTEGER PRIMARY KEY ASC,
	type INT NOT NULL,

	source TEXT NOT NULL,

	path INT NOT NULL,
	slug TEXT DEFAULT "",
	file TEXT NOT NULL,

	title TEXT DEFAULT NULL,

	created TEXT NOT NULL,
	updated TEXT DEFAULT NULL,

	FOREIGN KEY(path) REFERENCES paths(id),
	UNIQUE(path, slug, file)
);
CREATE UNIQUE INDEX uniq_entries ON entries(path, slug, file);

CREATE TABLE tags (
	id INTEGER PRIMARY KEY ASC,
	name TEXT UNIQUE NOT NULL
);
CREATE UNIQUE INDEX uniq_tags_name ON tags(name);

CREATE TABLE tagged_entries (
	tag INT NOT NULL,
	entry INT NOT NULL,

	FOREIGN KEY(tag) REFERENCES tags(id),
	FOREIGN KEY(entry) REFERENCES entries(id),
	UNIQUE(tag, entry)
);
CREATE UNIQUE INDEX uniq_tag_entry ON tagged_entries(tag, entry);

