
-- Lamb Database For PostgreSQL
-- Copyright (C) typefo <typefo@qq.com>

CREATE TABLE company (
    id serial PRIMARY KEY NOT NULL,
    name varchar(128) NOT NULL,
    money bigint NOT NULL default 0,
    paytype int NOT NULL,
    contacts varchar(64) NOT NULL,
    telephone varchar(32) NOT NULL,
    description varchar(128) NOT NULL,
    create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);

CREATE TABLE account (
    id serial PRIMARY KEY NOT NULL,
    username varchar(64) UNIQUE NOT NULL,
    password varchar(64) NOT NULL,
    spcode varchar(64) NOT NULL,
    company int NOT NULL,
    charge_type int NOT NULL,
    ip_addr varchar(32) NOT NULL,
    concurrent int NOT NULL,
    route int NOT NULL,
    extended int NOT NULL,
    policy int NOT NULL,
    check_template int NOT NULL,
    description varchar(64) NOT NULL,
    create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);

CREATE TABLE template (
    id serial PRIMARY KEY NOT NULL,
    name varchar(64) NOT NULL,
    contents varchar(512) NOT NULL,
    account int NOT NULL,
    description varchar(64) NOT NULL,
    create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);

CREATE TABLE channel (
    id serial PRIMARY KEY NOT NULL,
    name varchar(64) NOT NULL,
    type int NOT NULL,
    host varchar(32),
    port int NOT NULL,
    username varchar(32) NOT NULL,
    password varchar(64) NOT NULL,
    spid varchar(32) NOT NULL,
    spcode varchar(32) NOT NULL,
    encoded int NOT NULL,
    concurrent int NOT NULL,
    description varchar(64) NOT NULL,
    create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);

CREATE TABLE groups (
    id serial PRIMARY KEY NOT NULL,
    name varchar(64) NOT NULL,
    description varchar(64) NOT NULL,
    create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);

CREATE TABLE channels (
    id int NOT NULL,
    gid int NOT NULL,
    weight int NOT NULL
);

CREATE TABLE routes (
    id serial PRIMARY KEY NOT NULL,
    spcode varchar(32) NOT NULL,
    account varchar(32) NOT NULL,
    description varchar(64) NOT NULL,
    create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);

CREATE TABLE pay_logs (
    id serial NOT NULL,
    company varchar(64) NOT NULL,
    money bigint NOT NULL,
    operator varchar(64) NOT NULL,
    ip_addr bigint NOT NULL,
    create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);

CREATE TABLE blacklist (
    phone bigint UNIQUE NOT NULL
);

CREATE UNIQUE INDEX idx_blacklist on blacklist(phone);

CREATE TABLE whitelist (
    phone bigint UNIQUE NOT NULL
);

CREATE UNIQUE INDEX idx_whitelist on whitelist(phone);

CREATE TABLE message (
    id bigint UNIQUE NOT NULL,
    spid varchar(6) NOT NULL,
    spcode varchar(21) NOT NULL,
    phone varchar(21) NOT NULL,
    content text NOT NULL,
    status int NOT NULL,
    create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);

CREATE TABLE report (
   id bigint UNIQUE NOT NULL,
   spcode varchar(21) NOT NULL,
   status int NOT NULL,
   submit_time varchar(10),
   done_time varchar(10),
   phone varchar(21),
   create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);

CREATE TABLE deliver (
   id bigint UNIQUE NOT NULL,
   spcode varchar(21) NOT NULL,
   phone varchar(21) NOT NULL,
   content text NOT NULL,
   create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);
