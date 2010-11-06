
drop table if exists programbank;

create table programbank (
  row INTEGER PRIMARY KEY AUTOINCREMENT,
  banknumber TINYINT,
  name VARCHAR(80),
  dir VARCHAR(80)
);

create unique index idx on programbank (row, banknumber);

drop table if exists instrument;

create table instrument (
  row INTEGER PRIMARY KEY AUTOINCREMENT,
  banknumber TINYINT,
  prognumber TINYINT,
  name VARCHAR(80),
  xml TEXT,
  PADsynth_used TINYINT
);

create unique index bankinstidx on instrument (row, banknumber, prognumber);

