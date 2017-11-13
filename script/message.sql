
-- Lamb Database For PostgreSQL
-- Copyright (C) typefo <typefo@qq.com>

CREATE TABLE IF NOT EXISTS message (
    id bigint NOT NULL,
    spid varchar(6) NOT NULL,
    spcode varchar(21) NOT NULL,
    phone varchar(21) NOT NULL,
    content text NOT NULL,
    status int NOT NULL,
    account int NOT NULL,
    company int NOT NULL,
    create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);

create index msg_idx ON message(id, status);

CREATE TABLE delivery (
   id bigint NOT NULL,
   spcode varchar(21) NOT NULL,
   phone varchar(21) NOT NULL,
   content text NOT NULL,
   create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);
