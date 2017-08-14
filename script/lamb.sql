
-- Lamb Database For PostgreSQL
-- Copyright (C) typefo <typefo@qq.com>

CREATE TABLE company (
    id serial PRIMARY KEY NOT NULL,
    name varchar(128) NOT NULL,
    money bigint NOT NULL,
    paytype int NOT NULL,
    contacts varchar(64) NOT NULL,
    telephone varchar(32) NOT NULL,
    description varchar(128) NOT NULL,
    create_time timestamp without time zone NOT NULL
);
