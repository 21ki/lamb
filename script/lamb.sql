
-- Lamb Database For PostgreSQL
-- Copyright (C) typefo <typefo@qq.com>

CREATE TABLE company (
    id serial PRIMARY KEY NOT NULL,
    name varchar(128) NOT NULL,
    money bigint NOT NULL default 0,
    paytype int NOT NULL,
    contacts varchar(64) NOT NULL,
    telephone varchar(32) NOT NULL,
    description text NOT NULL,
    create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);

CREATE TABLE account (
    id serial PRIMARY KEY NOT NULL,
    username varchar(8) UNIQUE NOT NULL,
    password varchar(32) NOT NULL,
    spcode varchar(21) UNIQUE NOT NULL,
    company int NOT NULL,
    charge int NOT NULL,
    address varchar(32) NOT NULL,
    concurrent int NOT NULL,
    dbase int NOT NULL,
    template int NOT NULL,
    keyword int NOT NULL,
    description text NOT NULL,
    create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);

CREATE TABLE template (
    id serial PRIMARY KEY NOT NULL,
    rexp varchar(128) NOT NULL,
    name varchar(64) NOT NULL,
    content varchar(512) NOT NULL
);

CREATE TABLE gateway (
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
    extended int NOT NULL,
    concurrent int NOT NULL,
    description text NOT NULL,
    create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);

CREATE TABLE routing (
    id serial PRIMARY KEY NOT NULL,
    rexp varchar(128) NOT NULL,
    target int UNIQUE NOT NULL,
    description text NOT NULL
);

CREATE TABLE channels (
    id int NOT NULL,
    gid int NOT NULL,
    weight int NOT NULL,
    operator int NOT NULL
);

CREATE TABLE delivery (
    id serial PRIMARY KEY NOT NULL,
    rexp varchar(128) NOT NULL,
    target int NOT NULL,
    description text NOT NULL
);

CREATE TABLE keyword (
    id serial PRIMARY KEY NOT NULL,
    val text NOT NULL,
    tag text NOT NULL
);

CREATE TABLE pay_logs (
    id serial NOT NULL,
    company varchar(64) NOT NULL,
    money bigint NOT NULL,
    operator varchar(64) NOT NULL,
    description text NOT NULL,
    ip_addr bigint NOT NULL,
    create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);

CREATE TABLE statistical (
    gid int NOT NULL,
    submit bigint NOT NULL,
    delivrd bigint NOT NULL,
    expired bigint NOT NULL,
    deleted bigint NOT NULL,
    undeliv bigint NOT NULL,
    acceptd bigint NOT NULL,
    unknown bigint NOT NULL,
    rejectd bigint NOT NULL,
    datetime date NOT NULL
);

CREATE TABLE database (
    id int UNIQUE NOT NULL,
    name varchar(128) NOT NULL,
    type int NOT NULL,
    total bigint NOT NULL,
    description text NOT NULL,
);
