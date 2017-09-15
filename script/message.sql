
-- Lamb Database For PostgreSQL
-- Copyright (C) typefo <typefo@qq.com>

CREATE TABLE message (
    id bigint NOT NULL,
    msgid bigint NOT NULL,
    spid varchar(6) NOT NULL,
    spcode varchar(21) NOT NULL,
    phone varchar(21) NOT NULL,
    content text NOT NULL,
    status int NOT NULL,
    account int NOT NULL,
    company int NOT NULL,
    create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
) partition by range(create_time);

create table message_201709 partition of message for values from ('2017-09-01') to ('2017-10-01');
create table message_201710 partition of message for values from ('2017-10-01') to ('2017-11-01');
create table message_201711 partition of message for values from ('2017-11-01') to ('2017-12-01');
create table message_201712 partition of message for values from ('2017-12-01') to ('2018-01-01');
create table message_201801 partition of message for values from ('2018-01-01') to ('2018-02-01');
create table message_201802 partition of message for values from ('2018-02-01') to ('2018-03-01');
create table message_201803 partition of message for values from ('2018-03-01') to ('2018-04-01');
create table message_201804 partition of message for values from ('2018-04-01') to ('2018-05-01');
create table message_201805 partition of message for values from ('2018-05-01') to ('2018-06-01');
create table message_201806 partition of message for values from ('2018-06-01') to ('2018-07-01');
create table message_201807 partition of message for values from ('2018-07-01') to ('2018-08-01');
create table message_201808 partition of message for values from ('2018-08-01') to ('2018-09-01');
create table message_201809 partition of message for values from ('2018-09-01') to ('2018-10-01');
create table message_201810 partition of message for values from ('2018-10-01') to ('2018-11-01');
create table message_201811 partition of message for values from ('2018-11-01') to ('2018-12-01');

CREATE TABLE report (
   id bigint UNIQUE NOT NULL,
   spcode varchar(21) NOT NULL,
   status int NOT NULL,
   submit_time varchar(10),
   done_time varchar(10),
   phone varchar(21),
   account int NOT NULL,
   company int NOT NULL,
   create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);

CREATE TABLE deliver (
   id bigint UNIQUE NOT NULL,
   spcode varchar(21) NOT NULL,
   phone varchar(21) NOT NULL,
   content text NOT NULL,
   create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);
