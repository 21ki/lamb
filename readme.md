This a Open Source SMS Gateway Platform, Support the CMPP 2.0 protocol

### 基础环境说明

- CentOS 7.3
- nginx 1.12
- php 7.2
- yaf 3.0
- redis 4.0
- cmpp 1.0
- hiredis 0.13
- libiconv 1.15
- postgresql 10.2
- protobuf 3.5.0
- protobuf-c 1.3.0

### 依赖软件库

    yum install -y gcc gcc-c++ make automake autoconf libtool openssl-devel pcre-devel
    yum install -y re2c re2c-devl flex flex-devel bison bison-devl curl-devel libconfig-devel
    yum install -y readline-devel libxml2-devel hiredis-devel libpqxx-devel

### 内核参数配置 /etc/sysctl.conf

    net.ipv6.conf.all.disable_ipv6 = 1
    net.ipv6.conf.default.disable_ipv6 = 1
    net.ipv4.ip_forward = 1
    net.ipv4.tcp_syncookies = 1
    net.ipv4.tcp_tw_reuse = 1
    net.ipv4.tcp_tw_recycle = 1
    net.ipv4.tcp_fin_timeout = 15
    net.ipv4.tcp_synack_retries = 2
    fs.file-max = 2048000
    fs.nr_open = 2048000
    fs.file-max = 1024000
    fs.aio-max-nr = 1048576

### 内核参数配置 /etc/security/limits.conf

    * soft    nofile  1024000
    * hard    nofile  1024000
    * soft    nproc   unlimited
    * hard    nproc   unlimited
    * soft    core    unlimited
    * hard    core    unlimited
    * soft    memlock unlimited
    * hard    memlock unlimited

### nginx 编译安装

    $ wget http://nginx.org/download/nginx-1.12.2.tar.gz
    $ tar -zxvf nginx-1.12.2.tar.gz
    $ cd nginx-1.12.2
    $ ./configure --prefix=/usr/local/nginx
                  --with-threads
                  --with-file-aio
                  --with-http_ssl_module
                  --with-http_v2_module
                  --without-mail_pop3_module
                  --without-mail_imap_module
                  --without-mail_smtp_module
    $ make
    $ make install

### php 编译安装

    $ wget http://cn2.php.net/distributions/php-7.2.2.tar.gz
    $ tar -zxvf php-7.2.2.tar.gz
    $ cd php-7.2.2
    $ ./configure --prefix=/usr/local/php
                  --enable-fpm
                  --disable-ipv6
                  --enable-mbstring
                  --with-pdo-pgsql=/usr/local/postgresql
                  --with-pgsql=/usr/local/postgresql
    $ make
    $ make install


### protobuf 编译安装

    $ wget https://github.com/google/protobuf/releases/download/v3.5.0/protobuf-cpp-3.5.0.tar.gz
    $ tar -zxvf protobuf-cpp-3.5.0.tar.gz
    $ cd protobuf-3.5.0
    $ ./configure --prefix=/usr
    $ make
    $ make install

### protobuf-c 编译安装

    $ export PKG_CONFIG_PATH=/usr/lib/pkgconfig
    $ wget https://github.com/protobuf-c/protobuf-c/releases/download/v1.3.0/protobuf-c-1.3.0.tar.gz
    $ cd protobuf-c-1.3.0
    $ ./configure --prefix=/usr

修改 Makefile 文件将

    protobuf_LIBS = /usr/local/lib

修改为

    protobuf_LIBS = /usr/lib/libprotobuf.a

开始编译

    $ make
    $ make install
