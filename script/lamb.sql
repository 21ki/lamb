
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
    check_keyword int NOT NULL,
    description text NOT NULL,
    create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);

CREATE TABLE template (
    id serial PRIMARY KEY NOT NULL,
    name varchar(64) NOT NULL,
    contents varchar(512) NOT NULL,
    account int NOT NULL,
    description text NOT NULL,
    create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
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
    name varchar(128) NOT NULL,
    target varchar(64) NOT NULL,
    description text NOT NULL,
    create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);

CREATE TABLE groups (
    id serial PRIMARY KEY NOT NULL,
    name varchar(64) NOT NULL,
    description text NOT NULL
)

CREATE TABLE channels (
    id int NOT NULL,
    gid int NOT NULL,
    weight int NOT NULL,
    operator int NOT NULL
);

CREATE TABLE delivery (
    id serial PRIMARY KEY NOT NULL,
    spcode varchar(32) NOT NULL,
    account int NOT NULL,
    description text NOT NULL,
    create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);

CREATE TABLE keyword (
    id serial PRIMARY KEY NOT NULL,
    val varchar(36) NOT NULL
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
    name varchar(64) NOT NULL,
    type int NOT NULL,
    total bigint NOT NULL,
    description text NOT NULL,
);
