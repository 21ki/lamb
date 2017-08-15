
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

CREATE TABLE pay_logs (
       id serial NOT NULL,
       company varchar(64) NOT NULL,
       money bigint NOT NULL,
       operator varchar(64) NOT NULL,
       ip_addr bigint NOT NULL,
       create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);
