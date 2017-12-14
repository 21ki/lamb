
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
) partition by range(create_time);

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
create table message_201812 partition of message for values from ('2018-12-01') to ('2019-01-01');

create index msg_idx_201801 ON message_201801(id, status);
create index msg_idx_201802 ON message_201802(id, status);
create index msg_idx_201803 ON message_201803(id, status);
create index msg_idx_201804 ON message_201804(id, status);
create index msg_idx_201805 ON message_201805(id, status);
create index msg_idx_201806 ON message_201806(id, status);
create index msg_idx_201807 ON message_201807(id, status);
create index msg_idx_201808 ON message_201808(id, status);
create index msg_idx_201809 ON message_201809(id, status);
create index msg_idx_201810 ON message_201810(id, status);
create index msg_idx_201811 ON message_201811(id, status);
create index msg_idx_201812 ON message_201812(id, status);

CREATE TABLE IF NOT EXISTS delivery (
   id bigint NOT NULL,
   spcode varchar(21) NOT NULL,
   phone varchar(21) NOT NULL,
   content text NOT NULL,
   create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);

CREATE TABLE IF NOT EXISTS report (
   id bigint NOT NULL,
   spcode varchar(21) NOT NULL,
   phone varchar(21) NOT NULL,
   status int NOT NULL,
   submit_time varchar(16),
   done_time varchar(16),
   account int NOT NULL,
   create_time timestamp without time zone NOT NULL default now()::timestamp(0) without time zone
);
