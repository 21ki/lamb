
-- Lamb Database For PostgreSQL
-- Copyright (C) typefo <typefo@qq.com>

DROP DATABASE IF EXISTS `lamb`;

CREATE TABLE company (
    id integer NOT NULL,
    name character varying(36) NOT NULL,
    concurrent integer NOT NULL,
    billing character varying(32) NOT NULL,
    level integer NOT NULL,
    sound_check integer NOT NULL,
    create_time timestamp without time zone NOT NULL,
    data_filter integer
);



