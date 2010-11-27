
drop table if exists banks;

create table banks (
  row INTEGER PRIMARY KEY AUTOINCREMENT,
  banknum TINYINT,
  name VARCHAR(80)
);

create unique index idx on banks (row, banknum);

drop table if exists programs;

create table programs (
  row INTEGER PRIMARY KEY AUTOINCREMENT,
  banknum TINYINT,
  prognum TINYINT,
  name VARCHAR(80),
  xml TEXT
);

create unique index bankprogidx on programs (row, banknum, prognum);
